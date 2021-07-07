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
#include	"qCInput.h"
#include	"client.h"
#include	"qCClient.h"

#include	"qCMessage.h"
#include	"sys.h"
#include	"qCZone.h"
#include	"qCMath.h"
#include	"qCCVar.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"console.h"
#include	"menu.h"

typedef struct qSKeyName
{
	qChar*	pName;
	qSInt32	KeyNum;
} qSKeyName;

qSKeyName g_KeyNames[] =
{
	{ "TAB",		K_TAB			},
	{ "ENTER",		K_ENTER			},
	{ "ESCAPE",		K_ESCAPE		},
	{ "SPACE",		K_SPACE			},
	{ "BACKSPACE",	K_BACKSPACE		},
	{ "UPARROW",	K_UPARROW		},
	{ "DOWNARROW",	K_DOWNARROW		},
	{ "LEFTARROW",	K_LEFTARROW		},
	{ "RIGHTARROW",	K_RIGHTARROW	},

	{ "ALT",		K_ALT			},
	{ "CTRL",		K_CTRL			},
	{ "SHIFT",		K_SHIFT}		,

	{ "F1",			K_F1			},
	{ "F2",			K_F2			},
	{ "F3",			K_F3			},
	{ "F4",			K_F4			},
	{ "F5",			K_F5			},
	{ "F6",			K_F6			},
	{ "F7",			K_F7			},
	{ "F8",			K_F8			},
	{ "F9",			K_F9			},
	{ "F10",		K_F10			},
	{ "F11",		K_F11			},
	{ "F12",		K_F12			},

	{ "INS",		K_INS			},
	{ "DEL",		K_DEL			},
	{ "PGDN",		K_PGDN			},
	{ "PGUP",		K_PGUP			},
	{ "HOME",		K_HOME			},
	{ "END",		K_END			},

	{ "MOUSE1",		K_MOUSE1		},
	{ "MOUSE2",		K_MOUSE2		},
	{ "MOUSE3",		K_MOUSE3		},

	{ "PAUSE",		K_PAUSE			},

	{ "MWHEELUP",	K_MWHEELUP		},
	{ "MWHEELDOWN",	K_MWHEELDOWN	},

	{ "SEMICOLON",	';'				},	// because a raw semicolon seperates commands

	{ NULL,			0				}
};

static qUInt32			g_MouseButtonStateOld;
static qBool			g_MouseActive;
static qBool			g_MouseInitialized;

static qChar*			g_pKeyBindings[ 256 ];
static qUInt32			g_KeyRepeats[ 256 ];
static qUInt32			g_KeyShifts[ 256 ];
static qBool			g_KeyDowns[ 256 ];
static qBool			g_ConsoleKeys[ 256 ];
static qBool			g_MenuKeys[ 256 ];
static qEKeyDestination	g_KeyDestination;
static qSInt32			g_KeyCount;
static qSInt32			g_KeyLastPress;
static qBool			g_ShiftDown	= FALSE;

static qSButton			g_Forward;
static qSButton			g_Back;
static qSButton			g_StrafeLeft;
static qSButton			g_StrafeRight;
static qSButton			g_Jump;
static qSButton			g_Attack;

static qSInt32			g_Impulse;

qSCVar					g_CVarMouseSensitivity	= { "sensitivity",	"3",		TRUE };
qSCVar					g_CVarMousePitch		= { "m_pitch",		"0.022",	TRUE };
qSCVar					g_CVarMouseYaw			= { "m_yaw",		"0.022",	TRUE };

//////////////////////////////////////////////////////////////////////////

/*
=========
MouseMove
=========
*/
static void MouseMove( void )
{
	if( !g_MouseActive || !ActiveApp || Minimized )
		return;

	POINT	CursorPos;
	qSInt32	MouseX;
	qSInt32	MouseY;

	GetCursorPos( &CursorPos );

	MouseX	= ( qSInt32 )( ( CursorPos.x - window_center_x ) * g_CVarMouseSensitivity.Value );
	MouseY	= ( qSInt32 )( ( CursorPos.y - window_center_y ) * g_CVarMouseSensitivity.Value );

	cl.viewangles[ YAW ]	-= g_CVarMouseYaw.Value * MouseX;
	cl.viewangles[ PITCH ]	+= g_CVarMousePitch.Value * MouseY;

	if( MouseX || MouseY )
		SetCursorPos( window_center_x, window_center_y );

//////////////////////////////////////////////////////////////////////////

	cl.viewangles[ YAW ]	= qCMath::AngleMod( cl.viewangles[ YAW ] );

	if( cl.viewangles[ PITCH ] >  80 )	cl.viewangles[ PITCH ]	=  80;
	if( cl.viewangles[ PITCH ] < -70 )	cl.viewangles[ PITCH ]	= -70;
	if( cl.viewangles[ ROLL ]  >  50 )	cl.viewangles[ ROLL ]	=  50;
	if( cl.viewangles[ ROLL ]  < -50 )	cl.viewangles[ ROLL ]	= -50;

}	// MouseMove

/*
==========
ButtonDown
==========
*/
static void ButtonDown( qSButton* pButton )
{
	qChar*	pCommand;
	qSInt32	Key;

	pCommand	= Cmd_Argv( 1 );

	if( pCommand[ 0 ] )	Key	= atoi( pCommand );
	else				Key	= -1;	// typed manually at the console for continuous down

	if( Key == pButton->Down[ 0 ] ||
		Key == pButton->Down[ 1 ] )
		return;	// repeating key

	if(		 !pButton->Down[ 0 ] )	pButton->Down[ 0 ] = Key;
	else if( !pButton->Down[ 1 ] )	pButton->Down[ 1 ] = Key;
	else
	{
		Con_Printf( "Three keys down for a button!\n" );
		return;
	}

	if( pButton->State & 1 )
		return;	// still down

	pButton->State |= 1 + 2;	// down + impulse down

}	// ButtonDown

/*
========
ButtonUp
========
*/
static void ButtonUp( qSButton* pButton )
{
	qChar*	pCommand;
	qSInt32	Key;

	pCommand	= Cmd_Argv( 1 );

	if( pCommand[ 0 ] )	Key	= atoi( pCommand );
	else
	{
		// typed manually at the console, assume for unsticking, so clear all
		pButton->Down[ 0 ]	=
		pButton->Down[ 1 ]	= 0;
		pButton->State		= 4;	// impulse up
		return;
	}

	if(		 pButton->Down[ 0 ]	== Key )	pButton->Down[ 0 ] = 0;
	else if( pButton->Down[ 1 ]	== Key )	pButton->Down[ 1 ] = 0;
	else									return;	// key up without coresponding down ( menu pass through )

	if( pButton->Down[ 0 ] || pButton->Down[ 1 ] )
		return;	// some other key is still holding it down

	if( !( pButton->State & 1 ) )
		return;	// still up ( this should not happen )

	pButton->State	&= ~1;	// now up
	pButton->State	|=  4;	// impulse up

}	// ButtonUp

static void	ButtonForwardDownCB( void )		{ ButtonDown( &g_Forward );				}
static void ButtonForwardUpCB( void )		{ ButtonUp( &g_Forward );				}
static void ButtonBackDownCB( void )		{ ButtonDown( &g_Back );				}
static void ButtonBackUpCB( void )			{ ButtonUp( &g_Back );					}
static void ButtonStrafeLeftDownCB( void )	{ ButtonDown( &g_StrafeLeft );			}
static void ButtonStrafeLeftUpCB( void )	{ ButtonUp( &g_StrafeLeft );			}
static void ButtonStrafeRightDownCB( void )	{ ButtonDown( &g_StrafeRight );			}
static void ButtonStrafeRightUpCB( void )	{ ButtonUp( &g_StrafeRight );			}
static void ButtonAttackDownCB( void )		{ ButtonDown( &g_Attack );				}
static void ButtonAttackUpCB( void )		{ ButtonUp( &g_Attack );				}
static void ButtonJumpDownCB( void )		{ ButtonDown( &g_Jump );				}
static void ButtonJumpUpCB( void )			{ ButtonUp( &g_Jump );					}
static void ImpulseCB( void )				{ g_Impulse = atoi( Cmd_Argv( 1 ) );	}
static void	NothingCB( void )				{										}

/*
==============
StringToKeyNum
==============
*/
static qSInt32 StringToKeyNum( const qChar* pString )
{
	qSKeyName*	pKeyName;

	if( !pString || !pString[ 0 ] )
		return -1;

	if(! pString[ 1 ] )
		return pString[ 0 ];

	for( pKeyName=g_KeyNames; pKeyName->pName; ++pKeyName )
	{
		if( !stricmp( pString, pKeyName->pName ) )
			return pKeyName->KeyNum;
	}

	return -1;

}	// StringToKeyNum

/*
======
BindCB
======
*/
static void BindCB( void )
{
	qChar	Command[ 1024 ];
	qUInt32	NumArgc	= Cmd_Argc();
	qUInt32	KeyNum;

	if( NumArgc != 2 && NumArgc != 3 )
	{
		Con_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	KeyNum	= StringToKeyNum( Cmd_Argv( 1 ) );

	if( KeyNum == -1 )
	{
		Con_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if( NumArgc == 2 )
	{
		if( g_pKeyBindings[ KeyNum ] )	Con_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), g_pKeyBindings[ KeyNum ] );
		else							Con_Printf( "\"%s\" is not bound\n", Cmd_Argv( 1 ) );
		return;
	}

	// copy the rest of the command line
	Command[ 0 ]	= 0;		// start out with a null string

	for( qUInt32 i=2; i<NumArgc; ++i )
	{
		if( i > 2 )
			strcat( Command, " " );

		strcat( Command, Cmd_Argv( i ) );
	}

	qCInput::SetBinding( KeyNum, Command);

}	// BindCB

/*
========
UnbindCB
========
*/
static void UnbindCB( void )
{
	qUInt32	KeyNum;

	if( Cmd_Argc() != 2 )
	{
		Con_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	KeyNum	= StringToKeyNum( Cmd_Argv( 1 ) );

	if( KeyNum == -1 )
	{
		Con_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	qCInput::SetBinding( KeyNum, "" );

}	// UnbindCB

/*
===========
UnbindAllCB
===========
*/
static void UnbindAllCB( void )
{
	for( qUInt32 i=0; i<256; ++i )
		if( g_pKeyBindings[ i ] )
			qCInput::SetBinding( i, "" );

}	// UnbindAllCB

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCInput::Init( void )
{
	qCCVar::Register( &g_CVarMouseSensitivity );
	qCCVar::Register( &g_CVarMousePitch );
	qCCVar::Register( &g_CVarMouseYaw );

	Cmd_AddCommand( "bind",			BindCB );
	Cmd_AddCommand( "unbind",		UnbindCB );
	Cmd_AddCommand( "unbindall",	UnbindAllCB );

	Cmd_AddCommand( "+forward",		ButtonForwardDownCB );
	Cmd_AddCommand( "-forward",		ButtonForwardUpCB );
	Cmd_AddCommand( "+back",		ButtonBackDownCB );
	Cmd_AddCommand( "-back",		ButtonBackUpCB );
	Cmd_AddCommand( "+moveleft",	ButtonStrafeLeftDownCB );
	Cmd_AddCommand( "-moveleft",	ButtonStrafeLeftUpCB );
	Cmd_AddCommand( "+moveright",	ButtonStrafeRightDownCB );
	Cmd_AddCommand( "-moveright",	ButtonStrafeRightUpCB );
	Cmd_AddCommand( "+attack",		ButtonAttackDownCB );
	Cmd_AddCommand( "-attack",		ButtonAttackUpCB );
	Cmd_AddCommand( "+jump",		ButtonJumpDownCB );
	Cmd_AddCommand( "-jump",		ButtonJumpUpCB );
	Cmd_AddCommand( "impulse",		ImpulseCB );

	Cmd_AddCommand( "+lookup",		NothingCB );
	Cmd_AddCommand( "-lookup",		NothingCB );
	Cmd_AddCommand( "+lookdown",	NothingCB );
	Cmd_AddCommand( "-lookdown",	NothingCB );
	Cmd_AddCommand( "+moveup",		NothingCB );
	Cmd_AddCommand( "-moveup",		NothingCB );
	Cmd_AddCommand( "+movedown",	NothingCB );
	Cmd_AddCommand( "-movedown",	NothingCB );
	Cmd_AddCommand( "+speed",		NothingCB );
	Cmd_AddCommand( "-speed",		NothingCB );
	Cmd_AddCommand( "+strafe",		NothingCB );
	Cmd_AddCommand( "-strafe",		NothingCB );
	Cmd_AddCommand( "+left",		NothingCB );
	Cmd_AddCommand( "-left",		NothingCB );
	Cmd_AddCommand( "+right",		NothingCB );
	Cmd_AddCommand( "-right",		NothingCB );
	Cmd_AddCommand( "+klook",		NothingCB );
	Cmd_AddCommand( "-klook",		NothingCB );
	Cmd_AddCommand( "+mlook",		NothingCB );
	Cmd_AddCommand( "-mlook",		NothingCB );
	Cmd_AddCommand( "centerview",	NothingCB );

//////////////////////////////////////////////////////////////////////////

	for( qUInt16 i=32; i<128; ++i )
		g_ConsoleKeys[ i ]	= TRUE;

	g_ConsoleKeys[ K_ENTER ]		= TRUE;
	g_ConsoleKeys[ K_TAB ]			= TRUE;
	g_ConsoleKeys[ K_LEFTARROW ]	= TRUE;
	g_ConsoleKeys[ K_RIGHTARROW ]	= TRUE;
	g_ConsoleKeys[ K_UPARROW ]		= TRUE;
	g_ConsoleKeys[ K_DOWNARROW ]	= TRUE;
	g_ConsoleKeys[ K_BACKSPACE ]	= TRUE;
	g_ConsoleKeys[ K_DEL ]			= TRUE;
	g_ConsoleKeys[ K_INS ]			= TRUE;
	g_ConsoleKeys[ K_PGUP ]			= TRUE;
	g_ConsoleKeys[ K_PGDN ]			= TRUE;
	g_ConsoleKeys[ K_SHIFT ]		= TRUE;
	g_ConsoleKeys[ K_MWHEELUP ]		= TRUE;
	g_ConsoleKeys[ K_MWHEELDOWN ]	= TRUE;
	g_ConsoleKeys[ '`' ]			= FALSE;
	g_ConsoleKeys[ '~' ]			= FALSE;

	for( qUInt32 i=0; i<256; ++i )
		g_KeyShifts[ i ]	= i;

	for( qUInt32 i='a'; i<='z'; ++i )
		g_KeyShifts[ i ]	= i - 'a' + 'A';

	g_KeyShifts[ '1' ]	= '!';
	g_KeyShifts[ '2' ]	= '@';
	g_KeyShifts[ '3' ]	= '#';
	g_KeyShifts[ '4' ]	= '$';
	g_KeyShifts[ '5' ]	= '%';
	g_KeyShifts[ '6' ]	= '^';
	g_KeyShifts[ '7' ]	= '&';
	g_KeyShifts[ '8' ]	= '*';
	g_KeyShifts[ '9' ]	= '(';
	g_KeyShifts[ '0' ]	= ')';
	g_KeyShifts[ '-' ]	= '_';
	g_KeyShifts[ '=' ]	= '+';
	g_KeyShifts[ ',' ]	= '<';
	g_KeyShifts[ '.' ]	= '>';
	g_KeyShifts[ '/' ]	= '?';
	g_KeyShifts[ ';' ]	= ':';
	g_KeyShifts[ '\'' ]	= '"';
	g_KeyShifts[ '[' ]	= '{';
	g_KeyShifts[ ']' ]	= '}';
	g_KeyShifts[ '`' ]	= '~';
	g_KeyShifts[ '\\' ]	= '|';

	for( qUInt32 i=0; i<12; ++i )
		g_MenuKeys[ K_F1 + i ]	= TRUE;

	g_MenuKeys[ K_ESCAPE ]	= TRUE;

//////////////////////////////////////////////////////////////////////////

	if( COM_CheckParm( "-nomouse" ) )
		return;

	g_MouseInitialized	= TRUE;

	MouseActivate( TRUE );
	MouseShow( FALSE );

}	// Init

/*
========
Shutdown
========
*/
void qCInput::Shutdown( void )
{
	MouseActivate( FALSE );
	MouseShow( TRUE );

	g_MouseInitialized	= FALSE;

}	// Shutdown

/*
=========
MouseShow
=========
*/
void qCInput::MouseShow( const qBool Show )
{
	qSInt32	Counter	= -2;

	while( Counter < ( -1 + Show ) )	Counter	= ShowCursor( TRUE );
	while( Counter > ( -1 + Show ) )	Counter	= ShowCursor( FALSE );

}	// MouseShow

/*
=============
MouseActivate
=============
*/
void qCInput::MouseActivate( const qBool Activate )
{
	if( g_MouseInitialized )
	{
		if( Activate )
		{
			SetCursorPos( window_center_x, window_center_y );
			SetCapture( mainwindow );
			ClipCursor( &window_rect );

			g_MouseActive	= TRUE;
		}
		else
		{
			ReleaseCapture();
			ClipCursor( NULL );

			g_MouseActive	= FALSE;
		}
	}

}	// MouseActivate

/*
===============
MouseClipCursor
===============
*/
void qCInput::MouseClipCursor( void )
{
	if( g_MouseInitialized && g_MouseActive )
		ClipCursor( &window_rect );

}	// MouseClipCursor

/*
==========
MouseEvent
==========
*/
void qCInput::MouseEvent( const qUInt32 State )
{
	if( g_MouseActive )
	{
		for( qUInt32 i=0; i<3; ++i )
		{
			if(  ( State & ( 1 << i ) ) && !( g_MouseButtonStateOld & ( 1 << i ) ) )
				KeyEvent( K_MOUSE1 + i, TRUE );

			if( !( State & ( 1 << i ) ) &&  ( g_MouseButtonStateOld & ( 1 << i ) ) )
				KeyEvent( K_MOUSE1 + i, FALSE );
		}

		g_MouseButtonStateOld	= State;
	}

}	// MouseEvent

/*
=============
MouseIsActive
=============
*/
qBool qCInput::MouseIsActive( void )
{
	return g_MouseActive;

}	// MouseIsActive

/*
==============
ButtonGetState
==============
*/
qFloat qCInput::ButtonGetState( qSButton* pButton )
{
	qFloat		Value		= 0.0f;
	qBool		Down		= pButton->State & 1;
	qBool		ImpulseDown	= pButton->State & 2;
	qBool		ImpulseUp	= pButton->State & 4;

	if( ImpulseDown )
	{
		if( ImpulseUp )
		{
			if( Down)	Value	= 0.75f;	// released and re-pressed this frame
			else		Value	= 0.25f;	// pressed and released this frame
		}
		else
		{
			if( Down)	Value	= 0.5f;		// pressed and held this frame
			else		Value	= 0.0f;
		}
	}
	else if( !ImpulseUp )
	{
		if( Down )	Value	= 1.0f;			// held the entire frame
		else		Value	= 0.0f;			// up the entire frame
	}

	pButton->State &= 1;	// clear impulses

	return Value;

}	// ButtonGetState

/*
========
KeyEvent
========
*/
void qCInput::KeyEvent( qUInt32 KeyNum, const qBool Down )
{
	g_KeyCount++;
	g_KeyLastPress			= KeyNum;
	g_KeyDowns[ KeyNum ]	= Down;

	if( !Down )
		g_KeyRepeats[ KeyNum ]	= 0;

	if( g_KeyCount <= 0 )
		return;		// just catching keys for Con_NotifyBox

	// update auto-repeat status
	if( Down )
	{
		g_KeyRepeats[ KeyNum ]++;

		if( g_KeyRepeats[ KeyNum ] > 1	&&
			KeyNum != K_BACKSPACE		&&
			KeyNum != K_PAUSE )
			return;	// ignore most autorepeats

		if( KeyNum >= 200 && !g_pKeyBindings[ KeyNum ] )
			Con_Printf( "%s is unbound, hit F4 to set.\n", KeyNumToString( KeyNum ) );
	}

	if( KeyNum == K_SHIFT )
		g_ShiftDown	= Down;

	// handle escape specialy, so the user can never unbind it
	if( KeyNum == K_ESCAPE )
	{
		if( !Down )
			return;

		switch( g_KeyDestination )
		{
			case qGAME:		{ M_ToggleMenu_f();						} break;
			case qCONSOLE:	{ M_ToggleMenu_f();						} break;
			case qMESSAGE:	{ Key_Message( KeyNum );				} break;
			case qMENU:		{ M_Keydown( KeyNum );					} break;
			default:		{ Sys_Error( "Bad Key destination");	} break;
		}

		return;
	}

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	if(! Down )
	{
		qChar*	pKeyBinding	= g_pKeyBindings[ KeyNum ];
		qChar	Command[ 1024 ];

		if( pKeyBinding && pKeyBinding[ 0 ] == '+' )
		{
			snprintf( Command, sizeof( Command ), "-%s %d\n", pKeyBinding + 1, KeyNum );
			Cbuf_AddText( Command );
		}

		if( g_KeyShifts[ KeyNum ] != KeyNum )
		{
			pKeyBinding	= g_pKeyBindings[ g_KeyShifts[ KeyNum ] ];

			if( pKeyBinding && pKeyBinding[ 0 ] == '+' )
			{
				snprintf( Command, sizeof( Command ), "-%s %d\n", pKeyBinding + 1, KeyNum );
				Cbuf_AddText( Command );
			}
		}

		return;
	}

	// during demo playback, most keys bring up the main menu
	if( cls.demoplayback && Down && g_ConsoleKeys[ KeyNum ] && g_KeyDestination == qGAME )
	{
		M_ToggleMenu_f();
		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if( ( g_KeyDestination == qMENU		&& g_MenuKeys[ KeyNum ] )		||
		( g_KeyDestination == qCONSOLE	&& !g_ConsoleKeys[ KeyNum ] )	||
		( g_KeyDestination == qGAME		&& ( !con_forcedup || !g_ConsoleKeys[ KeyNum ] ) ) )
	{
		qChar*	pKeyBinding	= g_pKeyBindings[ KeyNum ];
		qChar	Command[ 1024 ];

		if( pKeyBinding )
		{
			if( pKeyBinding[ 0 ] == '+' )	// button commands add keynum as a parm
			{
				snprintf( Command, sizeof( Command ), "%s %d\n", pKeyBinding, KeyNum );
				Cbuf_AddText( Command );
			}
			else
			{
				Cbuf_AddText( pKeyBinding );
				Cbuf_AddText( "\n" );
			}
		}

		return;
	}

	if(! Down )
		return;		// other systems only care about key down events

	if( g_ShiftDown )
		KeyNum	= g_KeyShifts[ KeyNum ];

	switch( g_KeyDestination )
	{
		case qGAME:		{ Key_Console( KeyNum );				} break;
		case qCONSOLE:	{ Key_Console( KeyNum );				} break;
		case qMESSAGE:	{ Key_Message( KeyNum );				} break;
		case qMENU:		{ M_Keydown( KeyNum );					} break;
		default:		{ Sys_Error( "Bad Key destination");	} break;
	}

}	// KeyEvent

/*
==============
KeyNumToString
==============
*/
qChar* qCInput::KeyNumToString( const qSInt32 KeyNum )
{
	qSKeyName*		pKeyName;
	static qChar	TinyString[ 2 ];

	if( KeyNum == -1 )
		return "<KEY NOT FOUND>";

	if( KeyNum > 32 && KeyNum < 127 )	// printable ascii
	{
		TinyString[ 0 ] = KeyNum;
		TinyString[ 1 ] = 0;
		return TinyString;
	}

	for( pKeyName=g_KeyNames; pKeyName->pName; ++pKeyName )
		if( KeyNum == pKeyName->KeyNum )
			return pKeyName->pName;

	return "<UNKNOWN KEYNUM>";

}	// KeyNumToString

/*
==========
SetBinding
==========
*/
void qCInput::SetBinding( const qSInt32 KeyNum, const qChar* pBinding )
{
	qChar*	pNewBinding;
	qUInt32	Length;

	if( KeyNum == -1 )
		return;

	if( g_pKeyBindings[ KeyNum ] )
	{
		qCZone::Free( g_pKeyBindings[ KeyNum ] );
		g_pKeyBindings[ KeyNum ]	= NULL;
	}

	// allocate memory for new binding
	Length		= strlen( pBinding );
	pNewBinding	= ( qChar* )qCZone::Alloc( Length + 1 );
	strcpy( pNewBinding, pBinding );
	pNewBinding[ Length ]		= 0;
	g_pKeyBindings[ KeyNum ]	= pNewBinding;

}	// SetBinding

/*
=============
WriteBindings
=============
*/
void qCInput::WriteBindings( FILE* pFile )
{
	for( qUInt32 i=0; i<256; ++i )
		if( g_pKeyBindings[ i ] )
			if( *g_pKeyBindings[ i ] )
				fprintf( pFile, "bind \"%s\" \"%s\"\n", KeyNumToString( i ), g_pKeyBindings[ i ] );

}	// WriteBindings

/*
===========
ClearStates
===========
*/
void qCInput::ClearStates( void )
{
	for( qUInt32 i=0; i<256; ++i )
	{
		g_KeyDowns[ i ]		= FALSE;
		g_KeyRepeats[ i ]	= 0;
	}

	if( g_MouseActive )
		g_MouseButtonStateOld	= 0;

}	// ClearStates

/*
================
SendMoveToServer
================
*/
void qCInput::SendMoveToServer( void )
{
	qUInt8		Data[ 128 ];
	sizebuf_t	Buffer	= { FALSE, FALSE, Data, 128, 0 };
	qUInt32		Bits	= 0;

	MouseMove();

	// set button bits
	if( g_Attack.State & 3 )	Bits |= 1;
	if( g_Jump.State & 3 )		Bits |= 2;

	g_Attack.State	&= ~2;
	g_Jump.State	&= ~2;

	// send the movement message
	qCMessage::WriteByte(  &Buffer, clc_move );
	qCMessage::WriteFloat( &Buffer, ( qFloat )cl.mtime[ 0 ] );
	if( !cls.demoplayback && cls.netcon->Mod == MOD_PROQUAKE )
	{
		qCMessage::WriteAngleProQuake( &Buffer, cl.viewangles[ 0 ] );
		qCMessage::WriteAngleProQuake( &Buffer, cl.viewangles[ 1 ] );
		qCMessage::WriteAngleProQuake( &Buffer, cl.viewangles[ 2 ] );
	}
	else
	{
		qCMessage::WriteAngle( &Buffer, cl.viewangles[ 0 ] );
		qCMessage::WriteAngle( &Buffer, cl.viewangles[ 1 ] );
		qCMessage::WriteAngle( &Buffer, cl.viewangles[ 2 ] );
	}
	qCMessage::WriteShort( &Buffer, ( qSInt16 )( ButtonGetState( &g_Forward )		* g_CVarClientSpeedForward.Value	- ButtonGetState( &g_Back )			* g_CVarClientSpeedBack.Value ) );
	qCMessage::WriteShort( &Buffer, ( qSInt16 )( ButtonGetState( &g_StrafeRight )	* g_CVarClientSpeedSide.Value		- ButtonGetState( &g_StrafeLeft )	* g_CVarClientSpeedSide.Value ) );
	qCMessage::WriteShort( &Buffer, 0 );
	qCMessage::WriteByte(  &Buffer, Bits );
	qCMessage::WriteByte(  &Buffer, g_Impulse );

	g_Impulse	= 0;

	// deliver the message
	if( cls.demoplayback )
		return;

	// always dump the first two message, because it may contain leftover inputs from the last level
	if( ++cl.movemessages <= 2 )
		return;

	if( NET_SendUnreliableMessage (cls.netcon, &Buffer ) == -1 )
	{
		Con_Printf( "qCInput::SendMoveToServer: Lost server connection\n" );
		qCClient::DisconnectCB();
	}

}	// SendMoveToServer

/*
=================
SetKeyDestination
=================
*/
void qCInput::SetKeyDestination( const qEKeyDestination KeyDestination )
{
	g_KeyDestination	= KeyDestination;

}	// SetKeyDestination

/*
=================
GetKeyDestination
=================
*/
qEKeyDestination qCInput::GetKeyDestination( void )
{
	return g_KeyDestination;

}	// GetKeyDestination

/*
===========
SetKeyCount
===========
*/
void qCInput::SetKeyCount( const qSInt32 KeyCount )
{
	g_KeyCount	= KeyCount;

}	// SetKeyCount

/*
===========
GetKeyCount
===========
*/
qSInt32 qCInput::GetKeyCount( void )
{
	return g_KeyCount;

}	// GetKeyCount

/*
===============
GetKeyLastPress
===============
*/
qSInt32 qCInput::GetKeyLastPress( void )
{
	return g_KeyLastPress;

}	// GetKeyLastPress

/*
===========
GetKeyBinding
===========
*/
qChar* qCInput::GetKeyBinding( const qUInt32 Index )
{
	return g_pKeyBindings[ Index ];

}	// GetKeyBinding
