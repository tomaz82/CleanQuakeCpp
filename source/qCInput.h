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

#ifndef __QCINPUT_H__
#define __QCINPUT_H__

#include	"qTypes.h"

#define	K_TAB			9
#define	K_ENTER			13
#define	K_ESCAPE		27
#define	K_SPACE			32
#define	K_BACKSPACE		127
#define	K_UPARROW		128
#define	K_DOWNARROW		129
#define	K_LEFTARROW		130
#define	K_RIGHTARROW	131
#define	K_ALT			132
#define	K_CTRL			133
#define	K_SHIFT			134
#define	K_F1			135
#define	K_F2			136
#define	K_F3			137
#define	K_F4			138
#define	K_F5			139
#define	K_F6			140
#define	K_F7			141
#define	K_F8			142
#define	K_F9			143
#define	K_F10			144
#define	K_F11			145
#define	K_F12			146
#define	K_INS			147
#define	K_DEL			148
#define	K_PGDN			149
#define	K_PGUP			150
#define	K_HOME			151
#define	K_END			152
#define	K_MOUSE1		200
#define	K_MOUSE2		201
#define	K_MOUSE3		202
#define K_MWHEELUP		239
#define K_MWHEELDOWN	240
#define K_PAUSE			255

extern qSCVar	g_CVarMouseSensitivity;
extern qSCVar	g_CVarMousePitch;
extern qSCVar	g_CVarMouseYaw;

typedef struct qSButton
{
	qUInt32	Down[ 2 ];
	qUInt32	State;
} qSButton;

typedef enum qEKeyDestination
{
	qGAME,
	qCONSOLE,
	qMESSAGE,
	qMENU
} qEKeyDestination;

class qCInput
{
public:

	// Constructor / Destructor
	explicit qCInput	( void )	{ }	/**< Constructor */
			~qCInput	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void				Init				( void );
	static void				Shutdown			( void );
	static void				MouseShow			( const qBool Show );
	static void				MouseActivate		( const qBool Activate );
	static void				MouseClipCursor		( void );
	static void				MouseEvent			( const qUInt32 State );
	static qBool			MouseIsActive		( void );
	static qFloat			ButtonGetState		( qSButton* pButton );
	static void				KeyEvent			( qUInt32 KeyNum, const qBool Down );
	static qChar*			KeyNumToString		( const qSInt32 KeyNum );
	static void				SetBinding			( const qSInt32 KeyNum, const qChar* pBinding );
	static void				WriteBindings		( FILE* pFile );
	static void				ClearStates			( void );
	static void				SendMoveToServer	( void );
	static void				SetKeyDestination	( const qEKeyDestination KeyDestination );
	static qEKeyDestination	GetKeyDestination	( void );
	static void				SetKeyCount			( const qSInt32 KeyCount );
	static qSInt32			GetKeyCount			( void );
	static qSInt32			GetKeyLastPress		( void );
	static qChar*			GetKeyBinding		( const qUInt32 Index );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCInput

#endif // __QCINPUT_H__
