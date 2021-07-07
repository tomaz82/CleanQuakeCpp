/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include	"quakedef.h"
#include	"winquake.h"

#include	"sound.h"
#include	"console.h"

#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

static qBool	dsound_init;
static qBool	snd_firsttime = true, snd_isdirect;
static qBool	primary_format_set;

static int	sample16;

/*
 * Global variables. Must be visible to window-procedure function
 *  so it can unlock and free the data block after it has been played.
 */

HPSTR		lpData;

DWORD	gSndBufSize;

MMTIME		mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

sndinitstat SNDDMA_InitDirect (void);

/*
==================
FreeSound
==================
*/
void FreeSound (void)
{
	if (pDSBuf)
	{
		pDSBuf->Stop();
		pDSBuf->Release();
	}

// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		pDSPBuf->Release();
	}

	if (pDS)
	{
		pDS->SetCooperativeLevel (mainwindow, DSSCL_NORMAL);
		pDS->Release();
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	lpData = NULL;
	dsound_init = false;
}


/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect (void)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	DSCAPS			dscaps;
	WAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;

	memset ((void *)&sn, 0, sizeof (sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 48000;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = shm->channels;
    format.wBitsPerSample = shm->samplebits;
    format.nSamplesPerSec = shm->speed;
    format.nBlockAlign = format.nChannels
		*format.wBitsPerSample / 8;
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec
		*format.nBlockAlign;

	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_SafePrintf ("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}
	}

	while ((hresult = DirectSoundCreate(NULL, &pDS, NULL)) != DS_OK)
	{
		if (hresult != DSERR_ALLOCATED)
		{
			Con_SafePrintf ("DirectSound create failed\n");
			return SIS_FAILURE;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
					    "Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf ("DirectSoundCreate failure\n"
							"  hardware already in use\n");
			return SIS_NOTAVAIL;
		}
	}

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != pDS->GetCaps (&dscaps))
	{
		Con_SafePrintf ("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf ("No DirectSound driver installed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

	if (DS_OK != pDS->SetCooperativeLevel (mainwindow, DSSCL_EXCLUSIVE))
	{
		Con_SafePrintf ("Set coop level failed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

// get access to the primary buffer, if possible, so we can set the
// sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm ("-snoforceformat"))
	{
		if (DS_OK == pDS->CreateSoundBuffer(&dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != pDSPBuf->SetFormat (&pformat))
			{
				if (snd_firsttime)
					Con_SafePrintf ("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_SafePrintf ("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

	if (!primary_format_set || !COM_CheckParm ("-primarysound"))
	{
	// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (DS_OK != pDS->CreateSoundBuffer(&dsbuf, &pDSBuf, NULL))
		{
			Con_SafePrintf ("DS:CreateSoundBuffer Failed");
			FreeSound ();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->speed = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->GetCaps (&dsbcaps))
		{
			Con_SafePrintf ("DS:GetCaps failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (snd_firsttime)
			Con_SafePrintf ("Using secondary sound buffer\n");
	}
	else
	{
		if (DS_OK != pDS->SetCooperativeLevel (mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf ("Set coop level failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (DS_OK != pDSPBuf->GetCaps (&dsbcaps))
		{
			Con_Printf ("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf ("Using primary sound buffer\n");
	}

	// Make sure mixer is active
	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Con_SafePrintf("   %d channel(s)\n"
		               "   %d bits/sample\n"
					   "   %d bytes/sec\n",
					   shm->channels, shm->samplebits, shm->speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

// initialize the buffer
	reps = 0;

	while ((hresult = pDSBuf->Lock(0, gSndBufSize, (LPVOID*)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound ();
			return SIS_FAILURE;
		}

	}

	memset(lpData, 0, dwSize);
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

	pDSBuf->Unlock(lpData, dwSize, NULL, 0);

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	pDSBuf->Stop();
	pDSBuf->GetCurrentPosition(&mmstarttime.u.sample, &dwWrite);
	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

int SNDDMA_Init(void)
{
	sndinitstat	stat;

	dsound_init = 0;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	/* Init DirectSound */
	if (snd_firsttime || snd_isdirect)
	{
		stat = SNDDMA_InitDirect ();

		if (stat == SIS_SUCCESS)
		{
			snd_isdirect = true;

			if (snd_firsttime)
				Con_SafePrintf ("DirectSound initialized\n");
		}
		else
		{
			snd_isdirect = false;
			Con_SafePrintf ("DirectSound failed to init\n");
		}
	}

	snd_firsttime = false;

	if (!dsound_init)
	{
		if (snd_firsttime)
			Con_SafePrintf ("No sound device initialized\n");

		return 0;
	}

	return 1;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	MMTIME	mmtime;
	int		s;
	DWORD	dwWrite;

	if (dsound_init)
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->GetCurrentPosition(&mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}

	s >>= sample16;

	s &= (shm->samples-1);

	return s;
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	FreeSound ();
}
