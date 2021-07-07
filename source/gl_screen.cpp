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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include	"quakedef.h"
#include	"client.h"
#include	"qCClient.h"

#include	"vid.h"
#include	"sys.h"
#include	"qCMath.h"
#include	"draw.h"
#include	"qCCVar.h"
#include	"screen.h"
#include	"cmd.h"
#include	"qCSBar.h"
#include	"sound.h"
#include	"render.h"
#include	"qCInput.h"
#include	"console.h"
#include	"qCView.h"
#include	"menu.h"
#include	"glquake.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/


int			glwidth, glheight;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov;
qSCVar		scr_viewsize = {"viewsize","100", TRUE};
qSCVar		scr_fov = {"fov","90"};	// 10 - 170
qSCVar		scr_conspeed = {"scr_conspeed","3000"};
qSCVar		scr_centertime = {"scr_centertime","2"};
qSCVar		scr_showpause = {"showpause","1"};
qSCVar		scr_printspeed = {"scr_printspeed","8"};

qSCVar	crosshair = {"crosshair", "0", TRUE};

qBool	scr_initialized;		// ready to draw

qpic_t		*scr_net;

qBool	scr_disabled_for_loading;
qBool	scr_drawloading;
float		scr_disabled_time;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.Value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.Value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (qCInput::GetKeyDestination() != qGAME)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFovY
====================
*/
float CalcFovY (float fov_x, float width, float height)
{
	float   a;
	float   x;

	// bound, don't crash
	if (fov_x < 1) fov_x = 1;
	if (fov_x > 179) fov_x = 179;

	x = width / tan (fov_x / 360 * Q_PI);

	a = atan (height / x);

	a = a * 360 / Q_PI;

	return a;
}


/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	vid.recalc_refdef = 0;

//========================================

// bound viewsize
	if (scr_viewsize.Value < 100)	qCCVar::Set ("viewsize","100");
	if (scr_viewsize.Value > 120)	qCCVar::Set ("viewsize","120");

// bound field of view
	if (scr_fov.Value < 10)		qCCVar::Set ("fov","10");
	if (scr_fov.Value > 170)	qCCVar::Set ("fov","170");

	if (cl.intermission || scr_viewsize.Value >= 120)	qCSBar::SetNumLines( 0 );
	else if (scr_viewsize.Value >= 110)					qCSBar::SetNumLines( 24 );
	else												qCSBar::SetNumLines( 24 + 16 + 8 );

	r_refdef.width = vid.width;
	r_refdef.height = vid.height;
	r_refdef.fov_x = scr_fov.Value;
	r_refdef.fov_y = CalcFovY (r_refdef.fov_x, glwidth, glheight);
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	qCCVar::Set ("viewsize",scr_viewsize.Value+10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	qCCVar::Set ("viewsize",scr_viewsize.Value-10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	qCCVar::Register (&scr_fov);
	qCCVar::Register (&scr_viewsize);
	qCCVar::Register (&scr_conspeed);
	qCCVar::Register (&scr_showpause);
	qCCVar::Register (&scr_centertime);
	qCCVar::Register (&scr_printspeed);
	qCCVar::Register( &crosshair );

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_net = Draw_PicFromWad ("net");

	scr_initialized = TRUE;
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (64, 0, scr_net);
}

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
	static double	currtime;
	static float	counter;
	static char		text[ 16 ];
	double			newtime;
	int				calc;

	if ((cl.time + 1) < counter)
		counter = cl.time + 0.1;

	newtime		= Sys_FloatTime();
	calc		= (int) ((1.0 / (newtime - currtime)) + 0.5);
	currtime	= newtime;

	if (cl.time > counter)
	{
		snprintf( text, sizeof( text ), "%3i FPS", calc);
		counter = cl.time + 0.1;
	}


	Draw_String( vid.width - 64, 0, text);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!scr_showpause.Value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (vid.width - pic->width)/2,
		(vid.height - 48 - pic->height)/2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2,
		(vid.height - 48 - pic->height)/2, pic);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	if (scr_drawloading)
		return;		// never a console with loading plaque

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (qCInput::GetKeyDestination() == qCONSOLE)
		scr_conlines = vid.height/2;	// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.Value*host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.Value*host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	con_notifylines = 0;
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current, TRUE);
	}
	else
	{
		if (qCInput::GetKeyDestination() == qGAME || qCInput::GetKeyDestination() == qMESSAGE)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
	qUInt8		*buffer;
	char		tganame[80];
	char		checkname[MAX_OSPATH];
	int			i, c, temp;
//
// find a file name to save it to
//
	snprintf( checkname, sizeof( checkname ), "%s/screenshots", com_gamedir);
	Sys_mkdir (checkname);
	strcpy(tganame,"clean00.tga");

	for (i=0 ; i<=99 ; i++)
	{
		tganame[5] = i/10 + '0';
		tganame[6] = i%10 + '0';
		snprintf( checkname, sizeof( checkname ), "%s/screenshots/%s", com_gamedir, tganame);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	}
	if (i==100)
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
 	}


	buffer = (qUInt8*)malloc(glwidth*glheight*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = glwidth&255;
	buffer[13] = glwidth>>8;
	buffer[14] = glheight&255;
	buffer[15] = glheight>>8;
	buffer[16] = 24;	// pixel size

	glReadPixels (0, 0, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 );

	// swap rgb to bgr
	c = 18+glwidth*glheight*3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	COM_WriteFile (tganame, buffer, glwidth*glheight*3 + 18 );

	free (buffer);
	Con_Printf ("Wrote %s\n", tganame);
}


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (TRUE);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = TRUE;
	SCR_UpdateScreen ();
	scr_drawloading = FALSE;

	scr_disabled_for_loading = TRUE;
	scr_disabled_time = realtime;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = FALSE;
	Con_ClearNotify ();
}

//=============================================================================

char	*scr_notifystring;
qBool	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage (char *text)
{
	scr_notifystring = text;

// draw a fresh screen
	scr_drawdialog = TRUE;
	SCR_UpdateScreen ();
	scr_drawdialog = FALSE;

	S_ClearBuffer ();		// so dma doesn't loop current sound

	do
	{
		qCInput::SetKeyCount( -1 );		// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (qCInput::GetKeyLastPress() != 'y' && qCInput::GetKeyLastPress() != 'n' && qCInput::GetKeyLastPress() != K_ESCAPE);

	SCR_UpdateScreen ();

	return qCInput::GetKeyLastPress() == 'y';
}

//=============================================================================


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	static float	oldscr_viewsize;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = FALSE;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet


	GL_BeginRendering (&glwidth, &glheight);

	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.Value)
	{
		oldfov = scr_fov.Value;
		vid.recalc_refdef = TRUE;
	}

	if (oldscreensize != scr_viewsize.Value)
	{
		oldscreensize = scr_viewsize.Value;
		vid.recalc_refdef = TRUE;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	qCView::Render();

	GL_Set2D ();

	if (scr_drawdialog)
	{
		qCSBar::Render();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();
		qCSBar::Render();
	}
	else if (cl.intermission == 1 && qCInput::GetKeyDestination() == qGAME)
	{
		qCSBar::OverlayIntermission();
	}
	else if (cl.intermission == 2 && qCInput::GetKeyDestination() == qGAME)
	{
		qCSBar::OverlayFinale();
		SCR_CheckDrawCenterString ();
	}
	else
	{
		if (crosshair.Value)
			Draw_Character( ( r_refdef.width / 2 ) - 4 , ( r_refdef.height / 2 ) - 4, '+' );

		SCR_DrawNet ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		qCSBar::Render();
		SCR_DrawConsole ();
		M_Draw ();
	}

	if( fpsshow.Value )
		SCR_DrawFPS();

	qCView::UpdatePalette();

	GL_EndRendering ();
}
