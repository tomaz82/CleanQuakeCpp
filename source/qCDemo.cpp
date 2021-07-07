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
#include	"qCDemo.h"
#include	"client.h"
#include	"qCClient.h"

#include	"qCMessage.h"
#include	"qCEndian.h"
#include	"screen.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"server.h"
#include	"console.h"

/*
========
RecordCB
========
*/
static void RecordCB( void )
{
	if( cmd_source != src_command )
		return;

	const qUInt32	NumArgc	= Cmd_Argc();

	if( NumArgc < 2 )
	{
		Con_Printf( "record <demoname> <map>\n" );
		return;
	}

	if( strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed\n" );
		return;
	}

	if( NumArgc == 2 && cls.state == ca_connected )
	{
		Con_Printf( "Can not record - already connected to server\nClient demo recording must be started before connecting\n" );
		return;
	}

	qChar	Name[ MAX_OSPATH ];
	snprintf( Name, sizeof( Name ), "%s/%s", com_gamedir, Cmd_Argv( 1 ) );

	// start the map up
	if( NumArgc > 2 )
		Cmd_ExecuteString ( va( "map %s", Cmd_Argv( 2 ) ), src_command );

	// open the demo file
	COM_DefaultExtension( Name, ".dem" );

	Con_Printf( "Recording to %s\n", Name );

	if( ( cls.demofile = fopen( Name, "wb" ) ) == NULL )
	{
		Con_Printf( "ERROR: couldn't open\n" );
		return;
	}

	cls.forcetrack	= -1;
	fprintf( cls.demofile, "%d\n", cls.forcetrack );

	cls.demorecording = TRUE;

}	// RecordCB

/*
==========
PlayDemoCB
==========
*/
static void PlayDemoCB( void )
{
	qChar	Name[ MAX_OSPATH ];
	qUInt32	Char;
	qBool	Negative	= FALSE;

	if( cmd_source != src_command )
		return;

	if( Cmd_Argc() != 2 )
	{
		Con_Printf( "play <demoname> : plays a demo\n" );
		return;
	}

	// disconnect from server
	qCClient::DisconnectCB();

	// open the demo file
	snprintf( Name, sizeof( Name ), "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( Name, ".dem" );

	Con_Printf( "Playing demo from %s\n", Name );
	COM_FOpenFile( Name, &cls.demofile );

	if( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open\n" );
		cls.demonum	= -1;		// stop demo loop
		return;
	}

	cls.demoplayback	= TRUE;
	cls.state			= ca_connected;
	cls.forcetrack		= 0;

	while( ( Char = getc( cls.demofile ) ) != '\n' )
	{
		if( Char == '-' )	Negative		= TRUE;
		else				cls.forcetrack	= cls.forcetrack * 10 + ( Char - '0' );
	}

	if( Negative )
		cls.forcetrack	= -cls.forcetrack;

}	// PlayDemoCB

/*
==========
TimeDemoCB
==========
*/
static void TimeDemoCB( void )
{
	if( cmd_source != src_command )
		return;

	if( Cmd_Argc() != 2 )
	{
		Con_Printf( "timedemo <demoname> : gets demo speeds\n" );
		return;
	}

	PlayDemoCB();

	// cls.td_starttime will be grabbed at the second frame of the demo, so all the loading time doesn't get counted
	cls.timedemo		= TRUE;
	cls.td_startframe	= host_framecount;
	cls.td_lastframe	= -1;		// get a new message this frame

}	// TimeDemoCB

/*
==============
FinishTimeDemo
==============
*/
static void FinishTimeDemo( void )
{
	// the first frame didn't count
	const qUInt32	NumFrames	= ( host_framecount - cls.td_startframe ) - 1;
	qDouble			Time		= realtime - cls.td_starttime;

	cls.timedemo	= FALSE;

	if( !Time )
		Time	= 1;

	Con_Printf( "%d frames %7.3f seconds %7.3f fps\n", NumFrames, Time, NumFrames / Time );

}	// FinishTimeDemo

/*
============
StartDemosCB
============
*/
static void StartDemosCB( void )
{
	qUInt32	NumDemos	= Cmd_Argc() - 1;

	if( NumDemos > MAX_DEMOS )
	{
		Con_Printf( "Max %d demos in demoloop\n", MAX_DEMOS );
		NumDemos	= MAX_DEMOS;
	}

	Con_Printf( "%d demo(s) in loop\n", NumDemos );

	for( qUInt32 i=1; i<NumDemos+1; ++i )
		strncpy( cls.demos[ i - 1 ], Cmd_Argv( i ), sizeof( cls.demos[ 0 ] ) - 1 );

	if( !sv.active && cls.demonum != -1 && !cls.demoplayback )
	{
		cls.demonum	= 0;
		qCDemo::Next();
	}
	else
	{
		cls.demonum = -1;
	}

}	// StartDemosCB

/*
=======
DemosCB
=======
*/
static void DemosCB( void )
{
	if( cls.demonum == -1 )
		cls.demonum = 1;

	qCClient::DisconnectCB();

	qCDemo::Next();

}	// DemosCB

/*
==========
StopDemoCB
==========
*/
static void StopDemoCB( void )
{
	if (!cls.demoplayback)
		return;

	qCDemo::StopPlayback();

	qCClient::DisconnectCB();

}	// StopDemoCB

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCDemo::Init( void )
{
	Cmd_AddCommand( "record",		RecordCB );
	Cmd_AddCommand( "stop",			StopRecordingCB );
	Cmd_AddCommand( "playdemo",		PlayDemoCB );
	Cmd_AddCommand( "timedemo",		TimeDemoCB );
	Cmd_AddCommand ("startdemos",	StartDemosCB );
	Cmd_AddCommand ("demos",		DemosCB );
	Cmd_AddCommand ("stopdemo",		StopDemoCB );

}	// Init

/*
============
StopPlayback
============
*/
void qCDemo::StopPlayback( void )
{
	if( !cls.demoplayback )
		return;

	fclose( cls.demofile );
	cls.demoplayback	= FALSE;
	cls.demofile		= NULL;
	cls.state			= ca_disconnected;

	if( cls.timedemo )
		FinishTimeDemo();

}	// StopPlayback

/*
===============
StopRecordingCB
===============
*/
void qCDemo::StopRecordingCB( void )
{
	if( cmd_source != src_command )
		return;

	if( !cls.demorecording )
	{
		Con_Printf( "Not recording a demo\n" );
		return;
	}

	// write a disconnect message to the demo file
	SZ_Clear( &net_message );
	qCMessage::WriteByte( &net_message, svc_disconnect );
	qCDemo::WriteMessage();

	// finish up
	fclose( cls.demofile );

	cls.demofile		= NULL;
	cls.demorecording	= FALSE;

	Con_Printf( "Completed demo\n" );

}	// StopRecordingCB

/*
====
Next
====
*/
void qCDemo::Next( void )
{
	qChar	String[ 1024 ];

	if( cls.demonum == -1 )
		return;

	SCR_BeginLoadingPlaque();

	if( !cls.demos[ cls.demonum ][ 0 ] || cls.demonum == MAX_DEMOS )
	{
		cls.demonum	= 0;

		if( !cls.demos[ cls.demonum ][ 0 ] )
		{
			Con_Printf( "No demos listed with startdemos\n" );
			cls.demonum	= -1;
			return;
		}
	}

	snprintf( String, sizeof( String ), "playdemo %s\n", cls.demos[ cls.demonum ] );
	Cbuf_InsertText( String );
	++cls.demonum;

}	// Next

/*
============
WriteMessage
============
*/
void qCDemo::WriteMessage( void )
{
	const qUInt32	Length		= qCEndian::LittleInt32( net_message.cursize );
	const qVector3	ViewAngles	= { qCEndian::LittleFloat( cl.viewangles[ 0 ] ), qCEndian::LittleFloat( cl.viewangles[ 1 ] ), qCEndian::LittleFloat( cl.viewangles[ 2 ] ) };

	fwrite( &Length,			4,						1, cls.demofile );
	fwrite( &ViewAngles,		12,						1, cls.demofile );
	fwrite( net_message.data,	net_message.cursize,	1, cls.demofile );
	fflush( cls.demofile );

}	// WriteMessage
