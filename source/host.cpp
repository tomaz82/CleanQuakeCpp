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
// host.c -- coordinates spawning and killing of local servers

#include	"quakedef.h"
#include	"qCWad.h"
#include	"qCNetwork.h"
#include	"client.h"
#include	"qCClient.h"

#include	"qCMessage.h"
#include	"vid.h"
#include	"sys.h"
#include	"qCHunk.h"
#include	"qCZone.h"
#include	"qCCVar.h"
#include	"screen.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"qCSBar.h"
#include	"sound.h"
#include	"render.h"
#include	"qCInput.h"
#include	"qCDemo.h"
#include	"qCSky.h"
#include	"server.h"
#include	"gl_model.h"
#include	"console.h"
#include	"qCView.h"
#include	"menu.h"

/*

A server can allways be started, even if the system started out as a client
to a remote system.

Memory is cleared / released when a server or client begins, not when they end.

*/
qVector3 vec3_origin = {0,0,0};

quakeparms_t host_parms;

qBool	host_initialized;		// TRUE if into command execution

double		host_frametime;
double		host_time;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

qUInt8		*host_basepal;
qUInt8		*host_colormap;

qSCVar	fpsshow = {"fpsshow","0", true};
qSCVar	fpsmax = {"fpsmax","60", true};

qSCVar	host_framerate = {"host_framerate","0"};	// set for slow motion
qSCVar	host_speeds = {"host_speeds","0"};			// set for running times

qSCVar	sys_ticrate = {"sys_ticrate","0.05"};

qSCVar	fraglimit = {"fraglimit","0",FALSE,TRUE};
qSCVar	timelimit = {"timelimit","0",FALSE,TRUE};
qSCVar	teamplay = {"teamplay","0",FALSE,TRUE};

qSCVar	samelevel = {"samelevel","0"};
qSCVar	noexit = {"noexit","0",FALSE,TRUE};

qSCVar	developer = {"developer","0"};

qSCVar	skill = {"skill","1"};						// 0 - 3
qSCVar	deathmatch = {"deathmatch","0"};			// 0, 1, or 2
qSCVar	coop = {"coop","0"};			// 0 or 1

qSCVar	pausable = {"pausable","1"};

qSCVar	temp1 = {"temp1","0"};


/*
================
Host_EndGame
================
*/
void Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,message);
	vsnprintf (string,sizeof(string),message,argptr);
	va_end (argptr);
	Con_DPrintf ("Host_EndGame: %s\n",string);

	if (sv.active)
		Host_ShutdownServer (FALSE);

	if (cls.demonum != -1)
		qCDemo::Next();
	else
		qCClient::DisconnectCB();

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static	qBool inerror = FALSE;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = TRUE;

	SCR_EndLoadingPlaque ();		// reenable screen updates

	va_start (argptr,error);
	vsnprintf (string,sizeof(string),error,argptr);
	va_end (argptr);
	Con_Printf ("Host_Error: %s\n",string);

	if (sv.active)
		Host_ShutdownServer (FALSE);

	qCClient::DisconnectCB();
	cls.demonum = -1;

	inerror = FALSE;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
	int		i;

	svs.maxclients = 1;

	cls.state = ca_disconnected;

	i = COM_CheckParm ("-listen");
	if (i)
	{
		if (i != (com_argc - 1))
			svs.maxclients = atoi (com_argv[i+1]);
		else
			svs.maxclients = 8;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 8)
		svs.maxclientslimit = 8;
	svs.clients = (client_t*)qCHunk::AllocName (svs.maxclientslimit*sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		qCCVar::Set ("deathmatch", 1.0);
	else
		qCCVar::Set ("deathmatch", 0.0);
}


/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
	Host_InitCommands ();

	qCCVar::Register (&fpsshow);
	qCCVar::Register (&fpsmax);

	qCCVar::Register (&host_framerate);
	qCCVar::Register (&host_speeds);

	qCCVar::Register (&sys_ticrate);

	qCCVar::Register (&fraglimit);
	qCCVar::Register (&timelimit);
	qCCVar::Register (&teamplay);
	qCCVar::Register (&samelevel);
	qCCVar::Register (&noexit);
	qCCVar::Register (&skill);
	qCCVar::Register (&developer);
	qCCVar::Register (&deathmatch);
	qCCVar::Register (&coop);

	qCCVar::Register (&pausable);

	qCCVar::Register (&temp1);

	Host_FindMaxClients ();

	host_time = 1.0;		// so a think at time 0 won't get called
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (void)
{
	FILE	*f;

// config.cfg cvars
	if (host_initialized)
	{
		f = fopen (va("%s/config.cfg",com_gamedir), "w");
		if (!f)
		{
			Con_Printf ("Couldn't write config.cfg.\n");
			return;
		}

		qCInput::WriteBindings (f);
		qCCVar::WriteVariables (f);

		fclose (f);
	}
}


/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	vsnprintf (string,sizeof(string),fmt,argptr);
	va_end (argptr);

	qCMessage::WriteByte (&host_client->message, svc_print);
	qCMessage::WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	qUInt32			i;

	va_start (argptr,fmt);
	vsnprintf (string,sizeof(string),fmt,argptr);
	va_end (argptr);

	for (i=0 ; i<svs.maxclients ; i++)
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			qCMessage::WriteByte (&svs.clients[i].message, svc_print);
			qCMessage::WriteString (&svs.clients[i].message, string);
		}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	vsnprintf (string,sizeof(string),fmt,argptr);
	va_end (argptr);

	qCMessage::WriteByte (&host_client->message, svc_stufftext);
	qCMessage::WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = TRUE), don't bother sending signofs
=====================
*/
void SV_DropClient (qBool crash)
{
	int		saveSelf;
	qUInt32		i;
	client_t *client;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			qCMessage::WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned)
		{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}
	}

// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = FALSE;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

// send notification to all clients
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		qCMessage::WriteByte (&client->message, svc_updatename);
		qCMessage::WriteByte (&client->message, host_client - svs.clients);
		qCMessage::WriteString (&client->message, "");
		qCMessage::WriteByte (&client->message, svc_updatefrags);
		qCMessage::WriteByte (&client->message, host_client - svs.clients);
		qCMessage::WriteShort (&client->message, 0);
		qCMessage::WriteByte (&client->message, svc_updatecolors);
		qCMessage::WriteByte (&client->message, host_client - svs.clients);
		qCMessage::WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qBool crash)
{
	qUInt32		i;
	int		count;
	sizebuf_t	buf;
	char		message[4];
	double	start;

	if (!sv.active)
		return;

	sv.active = FALSE;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		qCClient::DisconnectCB();

// flush any pending messages - like the score!!!
	start = Sys_FloatTime();
	do
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_FloatTime() - start) > 3.0)
			break;
	}
	while (count);

// make sure all the clients know we're disconnecting
	buf.data = (qUInt8*)message;
	buf.maxsize = 4;
	buf.cursize = 0;
	qCMessage::WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

//
// clear structures
//
	memset (&sv, 0, sizeof(sv));
	memset (svs.clients, 0, svs.maxclientslimit*sizeof(client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrintf ("Clearing memory\n");
	Mod_ClearAll ();
	if (host_hunklevel)
		qCHunk::FreeToLowMark (host_hunklevel);

	cls.signon = 0;
	memset (&sv, 0, sizeof(sv));
	memset (&cl, 0, sizeof(cl));
}


//============================================================================


/*
===================
Host_FilterTime

Returns FALSE if the time is too short to run a frame
===================
*/
qBool Host_FilterTime (float time)
{
	realtime += time;

	if( fpsmax.Value < 1 )
		qCCVar::Set( "max_fps", 80 );

	if( !cls.timedemo && realtime - oldrealtime < 1.0 / fpsmax.Value )
		return FALSE;		// framerate is too high

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if (host_framerate.Value > 0)
		host_frametime = host_framerate.Value;
	else
	{	// don't allow really long or short frames
		if (host_frametime > 0.1)
			host_frametime = 0.1;
	}

	return TRUE;
}

/*
==================
Host_ServerFrame

==================
*/
void Host_ServerFrame (void)
{
// run the world state
	pr_global_struct->frametime = host_frametime;

// set the time and clear the general datagram
	SV_ClearDatagram ();

// check for new clients
	SV_CheckForNewClients ();

// read client messages
	SV_RunClients ();

// move things around and think
// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || qCInput::GetKeyDestination() == qGAME) )
		SV_Physics ();

// send all messages to the clients
	SV_SendClientMessages ();
}

/*
==================
Host_Frame

Runs all active servers
==================
*/
void Host_Frame (float time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;

	if (setjmp (host_abortserver) )
		return;			// something bad happened, or the server disconnected

// keep the random time dependent
	rand ();

// decide the simulation time
	if (!Host_FilterTime (time))
		return;			// don't run too fast, or packets will flood out

// get new key events
	Sys_SendKeyEvents ();

// process console commands
	Cbuf_Execute ();

	NET_Poll();

// if running the server locally, make intentions now
	if (sv.active)
		qCClient::SendCommand();

//-------------------
//
// server operations
//
//-------------------

	if (sv.active)
		Host_ServerFrame ();

//-------------------
//
// client operations
//
//-------------------

// if running the server remotely, send intentions now after
// the incoming messages have been read
	if (!sv.active)
		qCClient::SendCommand();

	host_time += host_frametime;

// fetch results from server
	if (cls.state == ca_connected)
	{
		qCClient::ReadFromServer();
	}

// update video
	if (host_speeds.Value)
		time1 = Sys_FloatTime ();

	SCR_UpdateScreen ();

	if (host_speeds.Value)
		time2 = Sys_FloatTime ();

// update audio
	if (cls.signon == SIGNONS)
	{
		S_Update (r_origin, vpn, vright, vup);
		qCClient::DecayDynamicLights();
	}
	else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	if (host_speeds.Value)
	{
		pass1 = (time1 - time3)*1000;
		time3 = Sys_FloatTime ();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1+pass2+pass3, pass1, pass2, pass3);
	}

	host_framecount++;
}

//============================================================================

/*
====================
Host_Init
====================
*/
void Host_Init( quakeparms_t* pParms )
{
	minimum_memory	= MINIMUM_MEMORY;

	if( COM_CheckParm( "-minmemory" ) )
		pParms->memsize	= minimum_memory;

	host_parms	= *pParms;

	if( pParms->memsize < minimum_memory )
		Sys_Error( "Only %4.1f megs of memory available, can't execute game", pParms->memsize / ( float )0x100000 );

	com_argc	= pParms->argc;
	com_argv	= pParms->argv;

	qCHunk::Init( pParms->membase, pParms->memsize );
	qCCache::Init();
	qCZone::Init();
	Cbuf_Init();
	Cmd_Init();
	qCView::Init();
	COM_Init();
	Host_InitLocal();
	qCWad::LoadFile( "gfx.wad" );
	qCInput::Init();
	Con_Init();
	M_Init();
	PR_Init();
	Mod_Init();
	NET_Init();
	SV_Init();

	Con_Printf( "Exe: %s %s\n", __TIME__, __DATE__ );
	Con_Printf( "%4.1f megabyte heap\n", pParms->memsize / ( 1024.0 * 1024.0 ) );

	host_basepal	= ( qUInt8* )COM_LoadHunkFile( "gfx/palette.lmp" );
	if( !host_basepal )
		Sys_Error( "Couldn't load gfx/palette.lmp" );

	host_colormap	= ( qUInt8* )COM_LoadHunkFile( "gfx/colormap.lmp" );
	if( !host_colormap )
		Sys_Error( "Couldn't load gfx/colormap.lmp" );

	VID_Init( host_basepal );

	qCTextures::Init();
	Draw_Init ();
	SCR_Init ();
	R_InitParticles();
	S_Init ();
	qCSBar::Init();
	qCClient::Init();
	qCDemo::Init();
	qCSky::Init();

	Cbuf_InsertText ("exec quake.rc\n");

	qCHunk::AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = qCHunk::GetLowMark();

	host_initialized = TRUE;
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static qBool isdown = FALSE;

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = TRUE;

// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = TRUE;

	Host_WriteConfiguration ();

	NET_Shutdown ();
	S_Shutdown();
	qCInput::Shutdown();
	VID_Shutdown();
}
