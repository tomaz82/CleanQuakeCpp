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

#ifndef __WINQUAKE_H__
#define __WINQUAKE_H__

#include <windows.h>
#include <winsock.h>
#include <mmsystem.h>

#include <dsound.h>

extern	HINSTANCE	global_hInstance;

extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;

extern DWORD gSndBufSize;

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

extern modestate_t	modestate;

extern HWND			mainwindow;
extern qBool		ActiveApp, Minimized;

extern qSCVar	_windowed_mouse;

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

void VID_SetDefaultMode (void);

#endif // __WINQUAKE_H__
