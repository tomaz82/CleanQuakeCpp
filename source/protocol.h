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
// protocol.h -- communications protocols

#define	PROTOCOL_VERSION	15

// if the high bit of the servercmd is set, the low bits are fast update flags:
#define	U_MOREBITS	(1<<0)
#define	U_ORIGIN1	(1<<1)
#define	U_ORIGIN2	(1<<2)
#define	U_ORIGIN3	(1<<3)
#define	U_ANGLE2	(1<<4)
#define	U_NOLERP	(1<<5)		// don't interpolate movement
#define	U_FRAME		(1<<6)
#define U_SIGNAL	(1<<7)		// just differentiates from other updates

// svc_update can pass all of the fast update bits, plus more
#define	U_ANGLE1	(1<<8)
#define	U_ANGLE3	(1<<9)
#define	U_MODEL		(1<<10)
#define	U_COLORMAP	(1<<11)
#define	U_SKIN		(1<<12)
#define	U_EFFECTS	(1<<13)
#define	U_LONGENTITY	(1<<14)


#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
//define	SU_AIMENT		(1<<8)  AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)

// a sound with no channel is a local only sound
#define	SND_VOLUME		(1<<0)		// a qUInt8
#define	SND_ATTENUATION	(1<<1)		// a qUInt8
#define	SND_LOOPING		(1<<2)		// a long


// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	22


// game types sent by serverinfo
// these determine which intermission screen plays
#define	GAME_COOP			0
#define	GAME_DEATHMATCH		1

//==================
// note that there are some defs.qc that mirror to these numbers
// also related to svc_strings[] in cl_parse
//==================

//
// server to client
//
#define	svc_bad				0
#define	svc_nop				1
#define	svc_disconnect		2
#define	svc_updatestat		3	// [qUInt8] [long]
#define	svc_version			4	// [long] server version
#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
#define	svc_time			7	// [float] server time
#define	svc_print			8	// [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer, the string should be \n terminated
#define	svc_setangle		10	// [angle3] set the view angle to this absolute value
#define	svc_serverinfo		11	// [long] version [string] signon string [string]..[0]model cache [string]...[0]sounds cache
#define	svc_lightstyle		12	// [qUInt8] [string]
#define	svc_updatename		13	// [qUInt8] [string]
#define	svc_updatefrags		14	// [qUInt8] [short]
#define	svc_clientdata		15	// <shortbits + data>
#define	svc_stopsound		16	// <see code>
#define	svc_updatecolors	17	// [qUInt8] [qUInt8]
#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19
#define	svc_spawnstatic		20
//	svc_spawnbinary		21
#define	svc_spawnbaseline	22
#define	svc_temp_entity		23
#define	svc_setpause		24	// [qUInt8] on / off
#define	svc_signonnum		25	// [qUInt8]  used for the signon sequence
#define	svc_centerprint		26	// [string] to put in center of the screen
#define	svc_killedmonster	27
#define	svc_foundsecret		28
#define	svc_spawnstaticsound	29	// [coord3] [qUInt8] samp [qUInt8] vol [qUInt8] aten
#define	svc_intermission	30		// [string] music
#define	svc_finale			31		// [string] music [string] text
#define	svc_cdtrack			32		// [qUInt8] track [qUInt8] looptrack
#define svc_sellscreen		33
#define svc_cutscene		34

//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
#define	clc_disconnect	2
#define	clc_move		3			// [usercmd_t]
#define	clc_stringcmd	4		// [string] message
