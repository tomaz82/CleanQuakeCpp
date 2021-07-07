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
#include	"client.h"
#include	"qCClient.h"

#include	"qCMessage.h"
#include	"vid.h"
#include	"sys.h"
#include	"qCHunk.h"
#include	"qCMath.h"
#include	"qCEndian.h"
#include	"qCCVar.h"
#include	"screen.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"sound.h"
#include	"render.h"
#include	"qCInput.h"
#include	"qCEntityFragments.h"
#include	"qCDemo.h"
#include	"qCTempEntities.h"
#include	"server.h"
#include	"console.h"
#include	"qCView.h"
#include	"glquake.h"

qSCVar	g_CVarClientName			= { "_cl_name",			"player",	TRUE };
qSCVar	g_CVarClientColor			= { "_cl_color",		"0",		TRUE };

qSCVar	g_CVarClientSpeedForward	= { "cl_forwardspeed",	"200",		TRUE };
qSCVar	g_CVarClientSpeedBack		= { "cl_backspeed",		"200",		TRUE };
qSCVar	g_CVarClientSpeedSide		= { "cl_sidespeed",		"350" };

qSLightStyle	g_LightStyles[ MAX_LIGHTSTYLES ];

client_static_t	cls;
client_state_t	cl;
// FIXME: put these on hunk?
qSEntityFragment	cl_efrags[MAX_EFRAGS];
entity_t		cl_entities[MAX_EDICTS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];

dlight_t		cl_dlights[MAX_DLIGHTS];

int				cl_numvisedicts;
entity_t		*cl_visedicts[MAX_VISEDICTS];

/*
==========
ClearState
==========
*/
static void ClearState( void )
{
	qUInt32	i;

	if( !sv.active )
		Host_ClearMemory();

	// wipe the entire cl structure
	memset( &cl, 0, sizeof( cl ) );

	SZ_Clear( &cls.message );

	// clear other arrays
	memset( cl_efrags,		0, sizeof( cl_efrags ) );
	memset( cl_entities,	0, sizeof( cl_entities ) );
	memset( cl_dlights,		0, sizeof( cl_dlights ) );
	memset( g_LightStyles,	0, sizeof( g_LightStyles ) );
	qCTempEntities::Clear();

	// allocate the efrags and chain together into a free list
	cl.free_efrags	= cl_efrags;

	for( i=0; i<MAX_EFRAGS-1; ++i )
		cl.free_efrags[ i ].pEntityNext	= &cl.free_efrags[ i + 1 ];

	cl.free_efrags[ i ].pEntityNext	= NULL;

}	// ClearState

/*
===========
SignonReply
===========
*/
static void SignonReply( void )
{
	Con_DPrintf( "SignonReply: %d\n", cls.signon );

	switch( cls.signon )
	{
		case 1:
		{
			qCMessage::WriteByte(	&cls.message, clc_stringcmd);
			qCMessage::WriteString(	&cls.message, "prespawn");
		}
		break;

		case 2:
		{
			qCMessage::WriteByte(	&cls.message, clc_stringcmd );
			qCMessage::WriteString(	&cls.message, va( "name \"%s\"\n", g_CVarClientName.pString ) );

			qCMessage::WriteByte(	&cls.message, clc_stringcmd );
			qCMessage::WriteString(	&cls.message, va( "color %d %d\n", ( ( qUInt32 )g_CVarClientColor.Value ) >> 4, ( ( qUInt32 )g_CVarClientColor.Value ) & 15 ) );

			qCMessage::WriteByte(	&cls.message, clc_stringcmd );
			qCMessage::WriteString(	&cls.message, va( "spawn %s", cls.spawnparms ) );
		}
		break;

		case 3:
		{
			qCMessage::WriteByte(	&cls.message, clc_stringcmd );
			qCMessage::WriteString(	&cls.message, "begin" );
			qCCache::Report();		// print remaining memory
		}
		break;

		case 4:
		{
			SCR_EndLoadingPlaque();	// allow normal screen updates
		}
		break;
	}

}	// SignonReply

/*
=============
GetNewMessage
=============
*/
static qSInt32 GetNewMessage( void )
{
	if( cls.demoplayback )
	{
		// decide if it is time to grab the next message
		if( cls.signon == SIGNONS )	// always grab until fully connected
		{
			if( cls.timedemo )
			{
				if( host_framecount == cls.td_lastframe )
					return 0;		// already read this frame's message

				cls.td_lastframe	= host_framecount;
				// if this is the second frame, grab the real td_starttime
				// so the bogus time on the first frame doesn't count
				if( host_framecount == cls.td_startframe + 1 )
					cls.td_starttime	= realtime;
			}
			else if( cl.time <= cl.mtime[ 0 ] )
			{
				return 0;	// don't need another message yet
			}
		}

		// get the next message
		fread( &net_message.cursize, 4, 1, cls.demofile );
		VectorCopy( cl.mviewangles[ 0 ], cl.mviewangles[ 1 ] );

		for( qUInt32 i=0; i<3; ++i )
		{
			qFloat	Angle;
			fread( &Angle, 4, 1, cls.demofile );
			cl.mviewangles[ 0 ][ i ] = qCEndian::LittleFloat( Angle );
		}

		net_message.cursize	= qCEndian::LittleInt32( net_message.cursize );

		if( net_message.cursize > MAX_MSGLEN )
			Sys_Error( "Demo message > MAX_MSGLEN" );

		if( fread( net_message.data, net_message.cursize, 1, cls.demofile ) != 1 )
		{
			qCDemo::StopPlayback();
			return 0;
		}

		return 1;
	}

//////////////////////////////////////////////////////////////////////////

	qSInt32	Value;

	while( 1 )
	{
		Value	= NET_GetMessage( cls.netcon );

		if( Value != 1 && Value != 2 )
			return Value;

		// discard nop keepalive message
		if( net_message.cursize == 1 && net_message.data[ 0 ] == svc_nop )	Con_Printf( "<-- server to client keepalive\n" );
		else																break;
	}

	if( cls.demorecording )
		qCDemo::WriteMessage();

	return Value;

}	// GetNewMessage

/*
=========
LerpPoint
=========
*/
static qFloat LerpPoint( void )
{
	qFloat	Time	= ( qFloat )( cl.mtime[ 0 ] - cl.mtime[ 1 ] );

	if( !Time || cls.timedemo || sv.active )
	{
		cl.time	= cl.mtime[ 0 ];

		return 1.0f;
	}

	if( Time > 0.1f )
	{
		// dropped packet, or start of demo
		cl.mtime[ 1 ]	= cl.mtime[ 0 ] - 0.1f;
		Time			= 0.1f;
	}

	qFloat	Fraction	= ( qFloat )( ( cl.time - cl.mtime[ 1 ] ) / Time );

	if( Fraction < 0.0f )
	{
		if( Fraction < -0.01f )
			cl.time	= cl.mtime[ 1 ];

		Fraction	= 0.0f;
	}
	else if( Fraction > 1.0f )
	{
		if( Fraction > 1.01f )
			cl.time = cl.mtime[ 0 ];

		Fraction	= 1.0f;
	}

	return Fraction;

}	// LerpPoint

/*
==============
RelinkEntities
==============
*/
static void RelinkEntities( void )
{
	qVector3		OldOrigin;
	const qFloat	Fraction	= LerpPoint();	// determine partial update time

	cl_numvisedicts	= 0;

	// interpolate player info
	cl.velocity[ 0 ]	= cl.mvelocity[ 1 ][ 0 ] + Fraction * ( cl.mvelocity[ 0 ][ 0 ] - cl.mvelocity[ 1 ][ 0 ] );
	cl.velocity[ 1 ]	= cl.mvelocity[ 1 ][ 1 ] + Fraction * ( cl.mvelocity[ 0 ][ 1 ] - cl.mvelocity[ 1 ][ 1 ] );
	cl.velocity[ 2 ]	= cl.mvelocity[ 1 ][ 2 ] + Fraction * ( cl.mvelocity[ 0 ][ 2 ] - cl.mvelocity[ 1 ][ 2 ] );

	if(cls.demoplayback )
	{
		// interpolate the angles
		for( qUInt32 i=0; i<3; ++i )
		{
			qFloat	Delta	= cl.mviewangles[ 0 ][ i ] - cl.mviewangles[ 1 ][ i ];
			if( Delta >  180.0f )	Delta -= 360;
			if( Delta < -180.0f )	Delta += 360;

			cl.viewangles[ i ]	= cl.mviewangles[ 1 ][ i ] + Fraction * Delta;
		}
	}

	qFloat	Rotate	= qCMath::AngleMod( ( qFloat )( cl.time * 100.0 ) );

//////////////////////////////////////////////////////////////////////////

	// start on the entity after the world
	entity_t*	pEntity	= cl_entities + 1;

	for( qUInt32 i=1; i<cl.num_entities; ++i, ++pEntity )
	{
		if( !pEntity->model )
		{
			if( pEntity->forcelink )
				qCEntityFragments::Remove( pEntity );

			continue;
		}

		// if the object wasn't included in the last packet, remove it
		if( pEntity->msgtime != cl.mtime[ 0 ] )
		{
			pEntity->model					= NULL;
			pEntity->translate_start_time	= 0.0f;
			pEntity->rotate_start_time		= 0.0f;
			VectorClear( pEntity->last_light );
			continue;
		}

		VectorCopy( pEntity->origin, OldOrigin );

		if( pEntity->forcelink )
		{
			// the entity was not updated in the last message so move to the final spot
			VectorCopy( pEntity->msg_origins[ 0 ], pEntity->origin );
			VectorCopy( pEntity->msg_angles[ 0 ], pEntity->angles );
		}
		else
		{
			// if the delta is large, assume a teleport and don't lerp
			qVector3	DeltaVector;
			qFloat		Value	= Fraction;

			for( qUInt32 j=0; j<3; ++j )
			{
				DeltaVector[ j ]	= pEntity->msg_origins[ 0 ][ j ] - pEntity->msg_origins[ 1 ][ j ];

				if( DeltaVector[ j ] > 100.0f || DeltaVector[ j ] < -100.0f )
					Value	= 1.0f;		// assume a teleportation, not a motion
			}

			if( Value >= 1.0f )
			{
				pEntity->translate_start_time	= 0.0f;
				pEntity->rotate_start_time		= 0.0f;
				VectorClear( pEntity->last_light );
			}

			// interpolate the origin and angles
			for( qUInt32 j=0; j<3; ++j )
			{
				qFloat	Delta			= pEntity->msg_angles[ 0 ][ j ] - pEntity->msg_angles[ 1 ][ j ];
				pEntity->origin[ j ]	= pEntity->msg_origins[ 1 ][ j ] + Value * DeltaVector[ j ];

				if( Delta >  180.0f )	Delta -= 360;
				if( Delta < -180.0f )	Delta += 360;

				pEntity->angles[ j ]	= pEntity->msg_angles[ 1 ][ j ] + Value * Delta;
			}
		}

//////////////////////////////////////////////////////////////////////////

		dlight_t*	pDynamicLight;

		// rotate binary objects locally
		if( pEntity->model->flags )
		{
			if( pEntity->model->flags & EF_ROTATE )
			{
				pEntity->angles[ 1 ]	 = Rotate;
				pEntity->origin[ 2 ]	+= ( qFloat )( ( sin( Rotate / 90.0 * Q_PI ) * 5.0 ) + 5.0 );
			}

			if(			pEntity->model->flags & EF_GRENADE )	R_RocketTrail( OldOrigin, pEntity->origin, 1 );
			else if(	pEntity->model->flags & EF_GIB )		R_RocketTrail( OldOrigin, pEntity->origin, 2 );
			else if(	pEntity->model->flags & EF_ZOMGIB )		R_RocketTrail( OldOrigin, pEntity->origin, 4 );
			else if(	pEntity->model->flags & EF_TRACER )
			{
				R_RocketTrail( OldOrigin, pEntity->origin, 3 );
				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 250.0f;
				pDynamicLight->die			= ( qFloat )( cl.time + 0.01 );
				pDynamicLight->color[ 0 ]	= 0.42f;
				pDynamicLight->color[ 1 ]	= 0.42f;
				pDynamicLight->color[ 2 ]	= 0.06f;
				VectorCopy( pEntity->origin, pDynamicLight->origin );
			}
			else if( pEntity->model->flags & EF_TRACER2 )
			{
				R_RocketTrail( OldOrigin, pEntity->origin, 5 );
				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 250.0f;
				pDynamicLight->die			= ( qFloat )( cl.time + 0.01 );
				pDynamicLight->color[ 0 ]	= 0.88f;
				pDynamicLight->color[ 1 ]	= 0.58f;
				pDynamicLight->color[ 2 ]	= 0.31f;
				VectorCopy( pEntity->origin, pDynamicLight->origin );
			}
			else if( pEntity->model->flags & EF_TRACER3 )
			{
				R_RocketTrail( OldOrigin, pEntity->origin, 6 );
				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 250.0f;
				pDynamicLight->die			= ( qFloat )( cl.time + 0.01 );
				pDynamicLight->color[ 0 ]	= 0.73f;
				pDynamicLight->color[ 1 ]	= 0.45f;
				pDynamicLight->color[ 2 ]	= 0.62f;
				VectorCopy( pEntity->origin, pDynamicLight->origin );
			}
			else if( pEntity->model->flags & EF_ROCKET )
			{
				R_RocketTrail( OldOrigin, pEntity->origin, 0 );
				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 250.0f;
				pDynamicLight->die			= ( qFloat )( cl.time + 0.01 );
				VectorCopy( pEntity->origin, pDynamicLight->origin );
			}
		}

		if( pEntity->effects )
		{
			if( pEntity->effects & EF_BRIGHTFIELD )
				R_EntityParticles( pEntity );

			if( pEntity->effects & EF_MUZZLEFLASH )
			{
				qVector3	Forward;

				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 200.0f + ( rand() & 31 );
				pDynamicLight->die			= ( qFloat )( cl.time + 0.1 );
				pDynamicLight->minlight		= 32.0f;
				VectorCopy( pEntity->origin, pDynamicLight->origin );
				pDynamicLight->origin[ 2 ]	+= 16.0f;
				qCMath::AngleVectors( pEntity->angles, Forward, NULL, NULL );
				VectorMultiplyAdd( pDynamicLight->origin, 18.0f, Forward, pDynamicLight->origin );
			}

			if( pEntity->effects & EF_BRIGHTLIGHT )
			{
				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 400.0f + ( rand() & 31 );
				pDynamicLight->die			= ( qFloat )( cl.time + 0.001 );
				pDynamicLight->color[ 0 ]	=
				pDynamicLight->color[ 1 ]	=
				pDynamicLight->color[ 2 ]	= 1.0f;
				VectorCopy( pEntity->origin, pDynamicLight->origin );
				pDynamicLight->origin[ 2 ]	+= 16.0f;
			}

			if( pEntity->effects & EF_DIMLIGHT )
			{
				pDynamicLight				= qCClient::CreateDynamicLight( i );
				pDynamicLight->radius		= 200.0f + ( rand() & 31 );
				pDynamicLight->die			= ( qFloat )( cl.time + 0.001 );
				pDynamicLight->color[ 0 ]	=
				pDynamicLight->color[ 1 ]	=
				pDynamicLight->color[ 2 ]	= 1.0f;
				VectorCopy( pEntity->origin, pDynamicLight->origin );
			}
		}

		pEntity->forcelink	= FALSE;

		if( i == cl.viewentity )
			continue;

		if( cl_numvisedicts < MAX_VISEDICTS )
		{
			cl_visedicts[ cl_numvisedicts ]	= pEntity;
			++cl_numvisedicts;
		}
	}

}	// RelinkEntities

/*
=========
GetEntity
=========
*/
static entity_t* GetEntity( const qUInt32 Num )
{
	if( Num >= cl.num_entities )
	{
		if( Num >= MAX_EDICTS )
			Host_Error( "GetEntity: %d is an invalid number", Num );

		while( cl.num_entities <= Num )
		{
			cl_entities[ cl.num_entities ].colormap	= vid.colormap;
			++cl.num_entities;
		}
	}

	return &cl_entities[ Num ];

}	// GetEntity

/*
==============
NewTranslation
==============
*/
static void NewTranslation( const qUInt32 Slot )
{
	if( Slot > cl.maxclients )
		Sys_Error( "NewTranslation: slot > cl.maxclients" );

	qUInt8*	pDest	= cl.scores[ Slot ].translations;
	qUInt8*	pSource	= vid.colormap;
	qUInt32	Top		= ( cl.scores[ Slot ].colors & 0xF0 );
	qUInt32	Bottom	= ( cl.scores[ Slot ].colors & 15 ) << 4;

	memcpy( pDest, vid.colormap, sizeof( cl.scores[ Slot ].translations ) );

	qCTextures::TranslatePlayerSkin( Slot );

	for( qUInt32 i=0; i<VID_GRADES; ++i, pDest += 256, pSource += 256 )
	{
		// the artists made some backwards ranges
		if( Top < 128 )
		{
			memcpy( pDest + TOP_RANGE, pSource + Top, 16 );
		}
		else
		{
			for( qUInt8 j=0; j<16; ++j )
				pDest[ TOP_RANGE + j ]	= pSource[ Top + 15 - j ];
		}

		if( Bottom < 128 )
		{
			memcpy( pDest + BOTTOM_RANGE, pSource + Bottom, 16 );
		}
		else
		{
			for( qUInt8 j=0; j<16; ++j )
				pDest[ BOTTOM_RANGE + j ]	= pSource[ Bottom + 15 - j ];
		}
	}

}	// NewTranslation


/*
======
NewMap
======
*/
void static NewMap( void )
{
	for( qUInt32 i=0 ; i<256; ++i )
		d_lightstylevalue[ i ]	= 264;		// normal light value

	memset( &r_worldentity, 0, sizeof( r_worldentity ) );
	r_worldentity.model	= cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	for( qUInt32 i=0; i<cl.worldmodel->numleafs; ++i )
		cl.worldmodel->leafs[ i ].efrags	= NULL;

	r_viewleaf	= NULL;
	R_ClearParticles();

	GL_BuildLightmaps();

}	// NewMap


/*
================
KeepAliveMessage
================
*/
static void KeepAliveMessage( void )
{
	if( sv.active || cls.demoplayback )
		return;

	static qFloat	LastMessage;
	const sizebuf_t	OldMessage	= net_message;
	qUInt8			OldData[ 8192 ];
	qSInt32			Value;

	// read messages from server, should just be nops
	memcpy( OldData, net_message.data, net_message.cursize );

	do
	{
		Value	= GetNewMessage();

		switch( Value )
		{
			case 0:
			{
			}
			break;	// nothing waiting

			case 1:
			{
				Host_Error( "KeepAliveMessage: received a message" );
			}
			break;

			case 2:
			{
				if( qCMessage::ReadByte() != svc_nop )
					Host_Error( "KeepAliveMessage: datagram wasn't a nop" );
			}
			break;

			default:
			{
				Host_Error( "KeepAliveMessage: GetMessage failed" );
			}
			break;
		}
	} while( Value );

	net_message	= OldMessage;
	memcpy( net_message.data, OldData, net_message.cursize );

	// check time
	qFloat	Time	= ( qFloat )Sys_FloatTime();

	if(	Time - LastMessage < 5.0f )
		return;

	LastMessage	= Time;

	// write out a nop
	Con_Printf( "--> client to server keepalive\n" );

	qCMessage::WriteByte( &cls.message, clc_nop );
	NET_SendMessage( cls.netcon, &cls.message );
	SZ_Clear( &cls.message );

}	// KeepAliveMessage

/*
=====================
ParseStartSoundPacket
=====================
*/
static void ParseStartSoundPacket( void )
{
	qVector3		Position;
	const qUInt8	FieldMask	= qCMessage::ReadByte();
	qFloat			Attenuation	= DEFAULT_SOUND_PACKET_ATTENUATION;
	qUInt8			Volume		= DEFAULT_SOUND_PACKET_VOLUME;

	if( FieldMask & SND_VOLUME )
		Volume	= qCMessage::ReadByte();

	if( FieldMask & SND_ATTENUATION )
		Attenuation	= ( qFloat )( qCMessage::ReadByte () * ( 1.0 / 64.0 ) );

	qUInt16			Channel		 = qCMessage::ReadShort();
	const qUInt32	Entity		 = Channel >> 3;
	const qUInt8	SoundNum	 = qCMessage::ReadByte();
	Channel						&= 7;

	if( Entity > MAX_EDICTS )
		Host_Error( "ParseStartSoundPacket: ent = %d", Entity );

	for( qUInt8 i=0; i<3; ++i )
		Position[ i ]	= qCMessage::ReadCoord();

	S_StartSound( Entity, Channel, cl.sound_precache[ SoundNum ], Position, Volume / 255.0f, Attenuation );

}	// ParseStartSoundPacket

/*
===============
ParseServerInfo
===============
*/
static void ParseServerInfo( void )
{
	Con_DPrintf( "Serverinfo packet received\n" );

	// wipe the client_state_t struct
	ClearState();

	// parse protocol version number
	const qUInt32	Version	= qCMessage::ReadLong();

	if( Version != PROTOCOL_VERSION )
	{
		Con_Printf( "Server returned version %d, not %d", Version, PROTOCOL_VERSION );
		return;
	}

	// parse maxclients
	cl.maxclients	= qCMessage::ReadByte();
	if( cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD )
	{
		Con_Printf( "Bad maxclients (%d) from server\n", cl.maxclients );
		return;
	}

	cl.scores	= ( scoreboard_t* )qCHunk::AllocName( cl.maxclients * sizeof( *cl.scores ), "scores" );

	// parse gametype
	cl.gametype	= qCMessage::ReadByte();

	// parse signon message
	const qChar* pString	= qCMessage::ReadString();
	strncpy( cl.levelname, pString, sizeof( cl.levelname ) - 1 );

	// seperate the printfs so the server message can have a color
	Con_Printf( "\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n" );
	Con_Printf( "%c%s\n", 2, pString );

//////////////////////////////////////////////////////////////////////////

	qChar	ModelPrecache[ MAX_MODELS ][ MAX_QPATH ];
	qChar	SoundPrecache[ MAX_SOUNDS ][ MAX_QPATH ];
	qUInt32	NumModels;
	qUInt32	NumSounds;

	// first we go through and touch all of the precache data that still happens to be in the cache, so precaching
	// something else doesn't needlessly purge it

	// precache models
	memset( cl.model_precache, 0, sizeof( cl.model_precache ) );

	for( NumModels=1; ; ++NumModels )
	{
		pString	= qCMessage::ReadString();

		if(! pString[ 0 ] )
			break;

		if( NumModels == MAX_MODELS )
		{
			Con_Printf( "Server sent too many model precaches\n" );
			return;
		}

		strcpy( ModelPrecache[ NumModels ], pString );
		Mod_TouchModel( pString );
	}

	// precache sounds
	memset( cl.sound_precache, 0, sizeof( cl.sound_precache ) );

	for( NumSounds=1; ; ++NumSounds )
	{
		pString	= qCMessage::ReadString();

		if(! pString[ 0 ] )
			break;

		if( NumSounds == MAX_SOUNDS )
		{
			Con_Printf( "Server sent too many sound precaches\n" );
			return;
		}

		strcpy( SoundPrecache[ NumSounds ], pString );
		S_TouchSound( pString );
	}

//////////////////////////////////////////////////////////////////////////

	// now we try to load everything else until a cache allocation fails

	for( qUInt32 i=1; i<NumModels; ++i )
	{
		cl.model_precache[ i ]	= Mod_ForName( ModelPrecache[ i ], FALSE );

		if( cl.model_precache[ i ] == NULL )
		{
			Con_Printf( "Model %s not found\n", ModelPrecache[ i ] );
			return;
		}

		KeepAliveMessage();
	}

	for( qUInt32 i=1; i<NumSounds; ++i )
	{
		cl.sound_precache[ i ]	= S_PrecacheSound( SoundPrecache[ i ] );
		KeepAliveMessage();
	}

	// local state
	cl_entities[ 0 ].model	=
	cl.worldmodel			= cl.model_precache[ 1 ];

	NewMap();

	qCHunk::Check();	// make sure nothing is hurt

}	// ParseServerInfo

/*
===========
ParseUpdate
===========
*/
static void ParseUpdate( qUInt32 Bits )
{
	model_t*	pModel;
	qUInt32		EntityNum;
	qUInt32		ModelNum;
	qUInt32		ColorMap;
	qUInt32		Skin;
	qBool		ForceLink;

	if( cls.signon == SIGNONS - 1 )
	{
		// first update is the final signon stage
		cls.signon	= SIGNONS;
		SignonReply();
	}

	if( Bits & U_MOREBITS )
		Bits	|= ( qCMessage::ReadByte() << 8 );

	if( Bits & U_LONGENTITY )	EntityNum	= qCMessage::ReadShort();
	else						EntityNum	= qCMessage::ReadByte();

	entity_t*	pEntity	= GetEntity( EntityNum );

	if( pEntity->msgtime != cl.mtime[ 1 ] )	ForceLink	= TRUE;	// no previous frame to lerp from
	else									ForceLink	= FALSE;

	pEntity->msgtime	= cl.mtime[ 0 ];

	if( Bits & U_MODEL )
	{
		ModelNum	= qCMessage::ReadByte();

		if( ModelNum >= MAX_MODELS )
			Host_Error( "ParseUpdate: bad modnum" );
	}
	else
	{
		ModelNum	= pEntity->baseline.modelindex;
	}

	pModel	= cl.model_precache[ ModelNum ];

	if( pModel != pEntity->model )
	{
		pEntity->model	= pModel;

		// automatic animation ( torches, etc ) can be either all together or randomized
		if( pModel )
		{
			if( pModel->synctype == ST_RAND )	pEntity->syncbase	= ( qFloat )( rand() & 0x7fff ) / 0x7fff;
			else								pEntity->syncbase	= 0.0f;
		}
		else
		{
			ForceLink	= TRUE;	// hack to make null model players work
		}

		if( EntityNum > 0 && EntityNum <= cl.maxclients )
			qCTextures::TranslatePlayerSkin( EntityNum - 1 );
	}

	if( Bits & U_FRAME )	pEntity->frame	= qCMessage::ReadByte();
	else					pEntity->frame	= pEntity->baseline.frame;

	if( Bits & U_COLORMAP )	ColorMap		= qCMessage::ReadByte();
	else					ColorMap		= pEntity->baseline.colormap;

	if( !ColorMap )
	{
		pEntity->colormap	= vid.colormap;
	}
	else
	{
		if( ColorMap > cl.maxclients )
			Sys_Error(" ColorMap >= cl.maxclients" );

		pEntity->colormap	= cl.scores[ ColorMap - 1 ].translations;
	}

	if( Bits & U_SKIN )		Skin	= qCMessage::ReadByte();
	else					Skin	= pEntity->baseline.skin;

	if( Skin != pEntity->skinnum )
	{
		pEntity->skinnum	= Skin;

		if( EntityNum > 0 && EntityNum <= cl.maxclients )
			qCTextures::TranslatePlayerSkin( EntityNum - 1 );
	}

	if( Bits & U_EFFECTS )	pEntity->effects	= qCMessage::ReadByte();
	else					pEntity->effects	= pEntity->baseline.effects;

	// shift the known values for interpolation
	VectorCopy( pEntity->msg_origins[ 0 ], pEntity->msg_origins[ 1 ] );
	VectorCopy( pEntity->msg_angles[ 0 ], pEntity->msg_angles[ 1 ] );

	if( Bits & U_ORIGIN1 )	pEntity->msg_origins[ 0 ][ 0 ]	= qCMessage::ReadCoord();
	else					pEntity->msg_origins[ 0 ][ 0 ]	= pEntity->baseline.origin[ 0 ];
	if( Bits & U_ANGLE1 )	pEntity->msg_angles[ 0 ][ 0 ]	= qCMessage::ReadAngle();
	else					pEntity->msg_angles[ 0 ][ 0 ]	= pEntity->baseline.angles[ 0 ];
	if( Bits & U_ORIGIN2 )	pEntity->msg_origins[ 0 ][ 1 ]	= qCMessage::ReadCoord();
	else					pEntity->msg_origins[ 0 ][ 1 ]	= pEntity->baseline.origin[ 1 ];
	if( Bits & U_ANGLE2 )	pEntity->msg_angles[ 0 ][ 1 ]	= qCMessage::ReadAngle();
	else					pEntity->msg_angles[ 0 ][ 1 ]	= pEntity->baseline.angles[ 1 ];
	if( Bits & U_ORIGIN3 )	pEntity->msg_origins[ 0 ][ 2 ]	= qCMessage::ReadCoord();
	else					pEntity->msg_origins[ 0 ][ 2 ]	= pEntity->baseline.origin[ 2 ];
	if( Bits & U_ANGLE3 )	pEntity->msg_angles[ 0 ][ 2 ]	= qCMessage::ReadAngle();
	else					pEntity->msg_angles[ 0 ][ 2 ]	= pEntity->baseline.angles[ 2 ];

	if( Bits & U_NOLERP )
		pEntity->forcelink	= TRUE;

	if( ForceLink )
	{
		// didn't have an update last message
		VectorCopy( pEntity->msg_origins[ 0 ], pEntity->msg_origins[ 1 ] );
		VectorCopy( pEntity->msg_origins[ 0 ], pEntity->origin );
		VectorCopy( pEntity->msg_angles[ 0 ], pEntity->msg_angles[ 1 ] );
		VectorCopy( pEntity->msg_angles[ 0 ], pEntity->angles );
		pEntity->forcelink	= TRUE;
	}

}	// ParseUpdate

/*
=============
ParseBaseline
=============
*/
static void ParseBaseline( entity_t* pEntity )
{
	pEntity->baseline.modelindex	= qCMessage::ReadByte();
	pEntity->baseline.frame			= qCMessage::ReadByte();
	pEntity->baseline.colormap		= qCMessage::ReadByte();
	pEntity->baseline.skin			= qCMessage::ReadByte();
	pEntity->baseline.origin[ 0 ]	= qCMessage::ReadCoord();
	pEntity->baseline.angles[ 0 ]	= qCMessage::ReadAngle();
	pEntity->baseline.origin[ 1 ]	= qCMessage::ReadCoord();
	pEntity->baseline.angles[ 1 ]	= qCMessage::ReadAngle();
	pEntity->baseline.origin[ 2 ]	= qCMessage::ReadCoord();
	pEntity->baseline.angles[ 2 ]	= qCMessage::ReadAngle();

}	// ParseBaseline

/*
===============
ParseClientData
===============
*/
static void ParseClientData( const qUInt32 Bits )
{
	if( Bits & SU_VIEWHEIGHT )	cl.viewheight	= ( qFloat )qCMessage::ReadChar();
	else						cl.viewheight	= DEFAULT_VIEWHEIGHT;

	if( Bits & SU_IDEALPITCH )
		qCMessage::ReadChar();	// I dont use this for anything

	VectorCopy( cl.mvelocity[ 0 ], cl.mvelocity[ 1 ] );

	for( qUInt8 i=0; i<3; ++i )
	{
		if( Bits & ( SU_PUNCH1 << i ) )		cl.punchangle[ i ]		= ( qFloat )qCMessage::ReadChar();
		else								cl.punchangle[ i ]		= 0.0f;
		if( Bits & ( SU_VELOCITY1 << i ) )	cl.mvelocity[ 0 ][ i ]	= ( qFloat )( qCMessage::ReadChar() * 16.0 );
		else								cl.mvelocity[ 0 ][ i ]	= 0.0f;
	}

	// [always sent]	if (bits & SU_ITEMS)
	qUInt32	Items	= qCMessage::ReadLong();

	if( cl.items != Items )
	{	// set flash times
		for( qUInt8 j=0; j<32; ++j )
			if( ( Items & ( 1 << j ) ) && !( cl.items & ( 1 << j ) ) )
				cl.item_gettime[ j ]	= ( qFloat )cl.time;

		cl.items	= Items;
	}

	cl.onground	= ( Bits & SU_ONGROUND )	!= 0;
	cl.inwater	= ( Bits & SU_INWATER )		!= 0;

	if( Bits & SU_WEAPONFRAME )	cl.stats[ STAT_WEAPONFRAME ]	= qCMessage::ReadByte();
	else						cl.stats[ STAT_WEAPONFRAME ]	= 0;

	if( Bits & SU_ARMOR )		cl.stats[ STAT_ARMOR ]			= qCMessage::ReadByte();
	else						cl.stats[ STAT_ARMOR ]			= 0;

	if( Bits & SU_WEAPON )		cl.stats[ STAT_WEAPON ]			= qCMessage::ReadByte();
	else						cl.stats[ STAT_WEAPON ]			= 0;

	cl.stats[ STAT_HEALTH ]			= qCMessage::ReadShort();
	cl.stats[ STAT_AMMO ]			= qCMessage::ReadByte();
	cl.stats[ STAT_SHELLS ]			= qCMessage::ReadByte();
	cl.stats[ STAT_NAILS ]			= qCMessage::ReadByte();
	cl.stats[ STAT_ROCKETS ]		= qCMessage::ReadByte();
	cl.stats[ STAT_CELLS ]			= qCMessage::ReadByte();
	cl.stats[ STAT_ACTIVEWEAPON ]	= qCMessage::ReadByte();

}	// ParseClientData

/*
===========
ParseStatic
===========
*/
static void ParseStatic( void )
{
	entity_t* pEntity;

	if( cl.num_statics >= MAX_STATIC_ENTITIES )
		Host_Error( "Too many static entities" );

	pEntity	= &cl_static_entities[ cl.num_statics ];
	++cl.num_statics;
	ParseBaseline( pEntity );

	// copy it to the current state
	pEntity->model		= cl.model_precache[ pEntity->baseline.modelindex ];
	pEntity->frame		= pEntity->baseline.frame;
	pEntity->colormap	= vid.colormap;
	pEntity->skinnum	= pEntity->baseline.skin;
	pEntity->effects	= pEntity->baseline.effects;

	VectorCopy( pEntity->baseline.origin, pEntity->origin );
	VectorCopy( pEntity->baseline.angles, pEntity->angles );
	qCEntityFragments::Add( pEntity );

}	// ParseStatic

/*
================
ParseStaticSound
================
*/
static void ParseStaticSound( void )
{
	qVector3	Origin;
	qUInt32		SoundNum;
	qFloat		Volume;
	qFloat		Attenuation;

	Origin[ 0 ]	= qCMessage::ReadCoord();
	Origin[ 1 ]	= qCMessage::ReadCoord();
	Origin[ 2 ]	= qCMessage::ReadCoord();
	SoundNum	= qCMessage::ReadByte();
	Volume		= ( qFloat )qCMessage::ReadByte();
	Attenuation	= ( qFloat )qCMessage::ReadByte();

	S_StaticSound( cl.sound_precache[ SoundNum ], Origin, Volume, Attenuation );

}	// ParseStaticSound

/*
==================
ParseServerMessage
==================
*/
static void ParseServerMessage( void )
{
	cl.onground	= FALSE;	// unless the server says otherwise

	// parse the message
	qCMessage::BeginReading();

	while( 1 )
	{
		if( qCMessage::BadRead )
			Host_Error( "ParseServerMessage: Bad server message" );

		qSInt32	Command	= qCMessage::ReadByte();

		if( Command == -1 )
			return;		// end of message

		// if the high bit of the command qUInt8 is set, it is a fast update
		if( Command & 128 )
		{
			ParseUpdate( Command & 127 );
			continue;
		}

		// other commands
		switch( Command )
		{
			case svc_nop:
			{
			}
			break;

			case svc_time:
			{
				cl.mtime[ 1 ]	= cl.mtime[ 0 ];
				cl.mtime[ 0 ]	= qCMessage::ReadFloat();
			}
			break;

			case svc_clientdata:
			{
				ParseClientData( qCMessage::ReadShort() );
			}
			break;

			case svc_version:
			{
				qUInt32	Protocol	= qCMessage::ReadLong();

				if( Protocol != PROTOCOL_VERSION )
					Host_Error( "ParseServerMessage: Server is protocol %d instead of %d\n", Protocol, PROTOCOL_VERSION );
			}
			break;

			case svc_disconnect:
			{
				Host_EndGame( "Server disconnected\n" );
			}
			break;

			case svc_print:
			{
				Con_Printf( "%s", qCMessage::ReadString() );
			}
			break;

			case svc_centerprint:
			{
				SCR_CenterPrint( qCMessage::ReadString() );
			}
			break;

			case svc_stufftext:
			{
				Cbuf_AddText( qCMessage::ReadString() );
			}
			break;

			case svc_damage:
			{
				qCView::ParseDamage();
			}
			break;

			case svc_serverinfo:
			{
				ParseServerInfo();
				vid.recalc_refdef	= TRUE;	// leave intermission full screen
			}
			break;

			case svc_setangle:
			{
				cl.viewangles[ 0 ]	= qCMessage::ReadAngle();
				cl.viewangles[ 1 ]	= qCMessage::ReadAngle();
				cl.viewangles[ 2 ]	= qCMessage::ReadAngle();
			}
			break;

			case svc_setview:
			{
				cl.viewentity	= qCMessage::ReadShort();
			}
			break;

			case svc_lightstyle:
			{
				qUInt8	LightStyle	= qCMessage::ReadByte();

				if( LightStyle >= MAX_LIGHTSTYLES )
					Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");

				strcpy( g_LightStyles[ LightStyle ].Map, qCMessage::ReadString() );
				g_LightStyles[ LightStyle ].Length	= strlen( g_LightStyles[ LightStyle ].Map );
			}
			break;

			case svc_sound:
			{
				ParseStartSoundPacket();
			}
			break;

			case svc_stopsound:
			{
				qUInt16	Value	= qCMessage::ReadShort();
				S_StopSound( Value >> 3, Value & 7 );
			}
			break;

			case svc_updatename:
			{
				qUInt8	ClientNum	= qCMessage::ReadByte();

				if( ClientNum >= cl.maxclients )
					Host_Error( "ParseServerMessage: svc_updatename > MAX_SCOREBOARD" );

				strcpy( cl.scores[ ClientNum ].name, qCMessage::ReadString() );
			}
			break;

			case svc_updatefrags:
			{
				qUInt8	ClientNum	= qCMessage::ReadByte();

				if( ClientNum >= cl.maxclients )
					Host_Error( "CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD" );

				cl.scores[ ClientNum ].frags	= qCMessage::ReadShort();
			}

			break;

			case svc_updatecolors:
			{
				qUInt8	ClientNum	= qCMessage::ReadByte();

				if( ClientNum >= cl.maxclients )
					Host_Error( "CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD" );

				cl.scores[ ClientNum ].colors	= qCMessage::ReadByte();
				NewTranslation( ClientNum );
			}
			break;

			case svc_particle:
			{
				R_ParseParticleEffect();
			}
			break;

			case svc_spawnbaseline:
			{
				// must use GetEntity() to force cl.num_entities up
				ParseBaseline( GetEntity( qCMessage::ReadShort() ) );
			}
			break;

			case svc_spawnstatic:
			{
				ParseStatic();
			}
			break;

			case svc_temp_entity:
			{
				qCTempEntities::Parse();
			}
			break;

			case svc_setpause:
			{
				cl.paused	= qCMessage::ReadByte();
			}
			break;

			case svc_signonnum:
			{
				qUInt8	Signon	= qCMessage::ReadByte();
				if( Signon <= cls.signon )
					Host_Error( "Received signon %d when at %d", Signon, cls.signon );

				cls.signon	= Signon;
				SignonReply();
			}
			break;

			case svc_killedmonster:
			{
				++cl.stats[ STAT_MONSTERS ];
			}
			break;

			case svc_foundsecret:
			{
				++cl.stats[ STAT_SECRETS ];
			}
			break;

			case svc_updatestat:
			{
				qUInt8	Stat	= qCMessage::ReadByte();

				if( Stat >= MAX_CL_STATS )
					Sys_Error( "svc_updatestat: %d is invalid", Stat );

				cl.stats[ Stat ]	= qCMessage::ReadLong();
			}
			break;

			case svc_spawnstaticsound:
			{
				ParseStaticSound();
			}
			break;

			case svc_cdtrack:
			{
				qCMessage::ReadByte();
				qCMessage::ReadByte();
			}
			break;

			case svc_intermission:
			{
				cl.intermission		= 1;
				cl.completed_time	= ( qUInt32 )cl.time;
				vid.recalc_refdef	= TRUE;	// go to full screen
			}
			break;

			case svc_finale:
			{
				cl.intermission		= 2;
				cl.completed_time	= ( qUInt32 )cl.time;
				vid.recalc_refdef	= TRUE;	// go to full screen
				SCR_CenterPrint( qCMessage::ReadString() );
			}
			break;

			case svc_cutscene:
			{
				cl.intermission		= 3;
				cl.completed_time	= ( qUInt32 )cl.time;
				vid.recalc_refdef	= TRUE;	// go to full screen
				SCR_CenterPrint( qCMessage::ReadString() );
			}
			break;

			case svc_sellscreen:
			{
				Cmd_ExecuteString( "help", src_command );
			}
			break;

			default:
			{
				Host_Error( "ParseServerMessage: Illegible server message\n" );
			}
			break;
		}
	}

}	// ParseServerMessage

//////////////////////////////////////////////////////////////////////////

/*
============
DisconnectCB
============
*/
void qCClient::DisconnectCB( void )
{
	// stop sounds (especially looping!)
	S_StopAllSounds( TRUE );

	// if running a local server, shut it down
	if( cls.demoplayback )
	{
		qCDemo::StopPlayback();
	}
	else if( cls.state == ca_connected )
	{
		if( cls.demorecording )
			qCDemo::StopRecordingCB();

		Con_DPrintf( "Sending clc_disconnect\n" );
		SZ_Clear( &cls.message );
		qCMessage::WriteByte( &cls.message, clc_disconnect );
		NET_SendUnreliableMessage( cls.netcon, &cls.message );
		SZ_Clear( &cls.message );
		NET_Close( cls.netcon );

		if( sv.active )
			Host_ShutdownServer( FALSE );
	}

	cls.state			= ca_disconnected;
	cls.demoplayback	= cls.timedemo = FALSE;
	cls.signon			= 0;

}	// DisconnectCB

/*
====
Init
====
*/
void qCClient::Init( void )
{
	SZ_Alloc( &cls.message, 1024 );

	qCTempEntities::Init();

	// register our commands
	qCCVar::Register( &g_CVarClientName );
	qCCVar::Register( &g_CVarClientColor );
	qCCVar::Register( &g_CVarClientSpeedForward );
	qCCVar::Register( &g_CVarClientSpeedBack );
	qCCVar::Register( &g_CVarClientSpeedSide );

	Cmd_AddCommand( "disconnect", DisconnectCB );

}	// Init

/*
===================
EstablishConnection
===================
*/
void qCClient::EstablishConnection( const qChar* pHost )
{
	if( cls.demoplayback )
		return;

	DisconnectCB();

	if( ( cls.netcon = NET_Connect( pHost ) ) == NULL )
		Host_Error( "qCClient::EstablishConnection: connect failed\n" );

	Con_DPrintf( "qCClient::EstablishConnection: connected to %s\n", pHost );

	cls.demonum	= -1;			// not in the demo loop now
	cls.state	= ca_connected;
	cls.signon	= 0;			// need all the signon messages before playing
	qCMessage::WriteByte( &cls.message, clc_nop );

}	// EstablishConnection

/*
==================
CreateDynamicLight
==================
*/
dlight_t* qCClient::CreateDynamicLight( const qUInt32 Key )
{
	dlight_t*	pDynamicLight;

	// first look for an exact key match
	if( Key )
	{
		pDynamicLight	= cl_dlights;

		for( qUInt32 i=0; i<MAX_DLIGHTS; ++i, ++pDynamicLight )
		{
			if( pDynamicLight->key == Key )
			{
				memset( pDynamicLight, 0, sizeof( dlight_t ) );
				pDynamicLight->key			= Key;
				pDynamicLight->color[ 0 ]	= 1;
				pDynamicLight->color[ 1 ]	= 0.5;
				pDynamicLight->color[ 2 ]	= 0.25;

				return pDynamicLight;
			}
		}
	}

	// then look for anything else
	pDynamicLight	= cl_dlights;

	for( qUInt32 i=0; i<MAX_DLIGHTS; ++i, ++pDynamicLight )
	{
		if( pDynamicLight->die < cl.time )
		{
			memset( pDynamicLight, 0, sizeof( dlight_t ) );
			pDynamicLight->key			= Key;
			pDynamicLight->color[ 0 ]	= 1;
			pDynamicLight->color[ 1 ]	= 0.5;
			pDynamicLight->color[ 2 ]	= 0.25;

			return pDynamicLight;
		}
	}

	pDynamicLight	= cl_dlights;

	memset( pDynamicLight, 0, sizeof( dlight_t ) );
	pDynamicLight->key			= Key;
	pDynamicLight->color[ 0 ]	= 1;
	pDynamicLight->color[ 1 ]	= 0.5;
	pDynamicLight->color[ 2 ]	= 0.25;

	return pDynamicLight;

}	// CreateDynamicLight

/*
==================
DecayDynamicLights
==================
*/
void qCClient::DecayDynamicLights( void )
{
	dlight_t*	pDynamicLight	= cl_dlights;
	qFloat		Time			= ( qFloat )( cl.time - cl.oldtime );

	for( qUInt32 i=0; i<MAX_DLIGHTS; ++i, ++pDynamicLight )
	{
		if( pDynamicLight->die < cl.time || !pDynamicLight->radius )
			continue;

		pDynamicLight->radius -= Time * pDynamicLight->decay;

		if( pDynamicLight->radius < 0 )
			pDynamicLight->radius = 0;
	}

}	// DecayDynamicLights

/*
==============
ReadFromServer
==============
*/
qSInt32 qCClient::ReadFromServer( void )
{
	qSInt32	Value;

	cl.oldtime	 = cl.time;
	cl.time		+= host_frametime;

	do
	{
		Value	= GetNewMessage();

		if( Value == -1 )	Host_Error( "qCClient::ReadFromServer: lost server connection" );
		if( !Value )		break;

		cl.last_received_message	= ( qFloat )realtime;
		ParseServerMessage();

	} while( Value && cls.state == ca_connected );

	RelinkEntities();
	qCTempEntities::Update();

	return 0;

}	// ReadFromServer

/*
===========
SendCommand
===========
*/
void qCClient::SendCommand( void )
{
	if( cls.state != ca_connected )
		return;

	if( cls.signon == SIGNONS )
		qCInput::SendMoveToServer();

	if( cls.demoplayback )
	{
		SZ_Clear( &cls.message );
		return;
	}

	// send the reliable message
	if( !cls.message.cursize )
		return;

	if( !NET_CanSendMessage( cls.netcon ) )
	{
		Con_DPrintf(" qCClient::SendCommand: can't send\n" );
		return;
	}

	if( NET_SendMessage( cls.netcon, &cls.message ) == -1 )
		Host_Error( "qCClient::SendCommand: lost server connection" );

	SZ_Clear( &cls.message );

}	// SendCommand
