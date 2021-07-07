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
#include	"qCTempEntities.h"
#include	"client.h"
#include	"qCClient.h"

#include	"qCMessage.h"
#include	"vid.h"
#include	"sys.h"
#include	"qCMath.h"
#include	"sound.h"
#include	"render.h"
#include	"gl_model.h"
#include	"console.h"

#define		SPIKE				0
#define		SUPERSPIKE			1
#define		GUNSHOT				2
#define		EXPLOSION			3
#define		TAREXPLOSION		4
#define		LIGHTNING1			5
#define		LIGHTNING2			6
#define		WIZARDSPIKE			7
#define		KNIGHTSPIKE			8
#define		LIGHTNING3			9
#define		LAVASPLASH			10
#define		TELEPORT			11
#define		EXPLOSION2			12

#define		MAX_TEMP_ENTITIES	64
#define		MAX_BEAMS			24

typedef struct qSBeam
{
	struct model_s*	pModel;
	qVector3		Start;
	qVector3		End;
	qFloat			EndTime;
	qUInt32			EntityNum;
} qSBeam;

static qUInt32	g_NumTempEntities;
static entity_t	g_TempEntities[ MAX_TEMP_ENTITIES ];
static qSBeam	g_Beams[ MAX_BEAMS ];

static sfx_t*	g_pSoundWizardHit;
static sfx_t*	g_pSoundKnightHit;
static sfx_t*	g_pSoundTink;
static sfx_t*	g_pSoundRicochet1;
static sfx_t*	g_pSoundRicochet2;
static sfx_t*	g_pSoundRicochet3;
static sfx_t*	g_pSoundExplosion;

static model_t*	g_pModelBolt1;
static model_t*	g_pModelBolt2;
static model_t*	g_pModelBolt3;

/*
=========
ParseBeam
=========
*/
static void ParseBeam( model_t* pModel )
{
	qSBeam*		pBeam	= g_Beams;
	qVector3	Start;
	qVector3	End;
	qUInt16		EntityNum;

	EntityNum	= qCMessage::ReadShort();

	Start[ 0 ]	= qCMessage::ReadCoord();
	Start[ 1 ]	= qCMessage::ReadCoord();
	Start[ 2 ]	= qCMessage::ReadCoord();

	End[ 0 ]	= qCMessage::ReadCoord();
	End[ 1 ]	= qCMessage::ReadCoord();
	End[ 2 ]	= qCMessage::ReadCoord();

	// override any beam with the same entity
	for( qUInt32 i=0; i<MAX_BEAMS; ++i, ++pBeam )
	{
		if( pBeam->EntityNum == EntityNum )
		{
//			pBeam->EntityNum	= EntityNum;
			pBeam->pModel		= pModel;
			pBeam->EndTime		= ( qFloat )( cl.time + 0.2 );
			VectorCopy( Start,	pBeam->Start );
			VectorCopy( End,	pBeam->End );

			return;
		}
	}

	// find a free beam
	pBeam	= g_Beams;
	for( qUInt32 i=0; i<MAX_BEAMS; ++i, ++pBeam )
	{
		if( !pBeam->pModel || pBeam->EndTime < cl.time )
		{
			pBeam->EntityNum	= EntityNum;
			pBeam->pModel		= pModel;
			pBeam->EndTime		= ( qFloat )( cl.time + 0.2 );
			VectorCopy( Start,	pBeam->Start );
			VectorCopy( End,	pBeam->End );

			return;
		}
	}

	Con_Printf( "Beam list overflow!\n" );

}	// ParseBeam

/*
=====
Clear
=====
*/
void qCTempEntities::Clear( void )
{
	memset( g_TempEntities,	0, sizeof( g_TempEntities ) );
	memset( g_Beams,		0, sizeof( g_Beams ) );

}	// Clear

/*
====
Init
====
*/
void qCTempEntities::Init( void )
{
	g_pSoundWizardHit	= S_PrecacheSound( "wizard/hit.wav" );
	g_pSoundKnightHit	= S_PrecacheSound( "hknight/hit.wav" );
	g_pSoundTink		= S_PrecacheSound( "weapons/tink1.wav" );
	g_pSoundRicochet1	= S_PrecacheSound( "weapons/ric1.wav" );
	g_pSoundRicochet2	= S_PrecacheSound( "weapons/ric2.wav" );
	g_pSoundRicochet3	= S_PrecacheSound( "weapons/ric3.wav" );
	g_pSoundExplosion	= S_PrecacheSound( "weapons/r_exp3.wav" );

	g_pModelBolt1		= Mod_ForName( "progs/bolt.mdl",	TRUE );
	g_pModelBolt2		= Mod_ForName( "progs/bolt2.mdl",	TRUE );
	g_pModelBolt3		= Mod_ForName( "progs/bolt3.mdl",	TRUE );

}	// Init


/*
=====
Parse
=====
*/
void qCTempEntities::Parse( void )
{
	dlight_t*	pDynamicLight;
	qVector3	Position;
	qUInt32		Random;
	qUInt8		Type	= qCMessage::ReadByte();
	qUInt8		ColorStart;
	qUInt8		ColorLength;

	switch( Type )
	{
		case SPIKE:			// spike hitting wall
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_RunParticleEffect( Position, vec3_origin, 0, 10 );

			if ( rand() % 5 )
			{
				S_StartSound( -1, 0, g_pSoundTink, Position, 1, 1 );
			}
			else
			{
				Random	= rand() & 3;
				if( Random == 1 )		S_StartSound( -1, 0, g_pSoundRicochet1, Position, 1, 1 );
				else if( Random == 2 )	S_StartSound( -1, 0, g_pSoundRicochet2, Position, 1, 1 );
				else					S_StartSound( -1, 0, g_pSoundRicochet3, Position, 1, 1 );
			}
		}
		break;	// SPIKE

		case SUPERSPIKE:			// super spike hitting wall
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_RunParticleEffect( Position, vec3_origin, 0, 20 );

			if ( rand() % 5 )
			{
				S_StartSound( -1, 0, g_pSoundTink, Position, 1, 1 );
			}
			else
			{
				Random	= rand() & 3;
				if( Random == 1 )		S_StartSound( -1, 0, g_pSoundRicochet1, Position, 1, 1 );
				else if( Random == 2 )	S_StartSound( -1, 0, g_pSoundRicochet2, Position, 1, 1 );
				else					S_StartSound( -1, 0, g_pSoundRicochet3, Position, 1, 1 );
			}
		}
		break;	// SUPERSPIKE

		case GUNSHOT:			// bullet hitting wall
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_RunParticleEffect( Position, vec3_origin, 0, 20 );
		}
		break;	// GUNSHOT

		case EXPLOSION:			// rocket explosion
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_ParticleExplosion( Position );
			pDynamicLight			= qCClient::CreateDynamicLight( 0 );
			pDynamicLight->radius	= 350;
			pDynamicLight->die		= ( qFloat )( cl.time + 0.5 );
			pDynamicLight->decay	= 300;
			VectorCopy( Position, pDynamicLight->origin );
			S_StartSound( -1, 0, g_pSoundExplosion, Position, 1, 1 );
		}
		break;	// EXPLOSION

		case TAREXPLOSION:			// tarbaby explosion
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_BlobExplosion( Position );
			S_StartSound( -1, 0, g_pSoundExplosion, Position, 1, 1 );
		}
		break;	// TAREXPLOSION

		case WIZARDSPIKE:			// spike hitting wall
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_RunParticleEffect( Position, vec3_origin, 20, 30 );
			S_StartSound( -1, 0, g_pSoundWizardHit, Position, 1, 1 );
		}
		break;	// WIZARDSPIKE

		case KNIGHTSPIKE:			// spike hitting wall
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_RunParticleEffect( Position, vec3_origin, 226, 30 );
			S_StartSound( -1, 0, g_pSoundKnightHit, Position, 1, 1 );
		}
		break;	// KNIGHTSPIKE

		case EXPLOSION2:				// color mapped explosion
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			ColorStart		= qCMessage::ReadByte();
			ColorLength		= qCMessage::ReadByte();
			R_ParticleExplosion2( Position, ColorStart, ColorLength );
			pDynamicLight			= qCClient::CreateDynamicLight( 0 );
			pDynamicLight->radius	= 350;
			pDynamicLight->die		= ( qFloat )( cl.time + 0.5 );
			pDynamicLight->decay	= 300;
			VectorCopy( Position, pDynamicLight->origin );
			S_StartSound( -1, 0, g_pSoundExplosion, Position, 1, 1 );
		}
		break;	// EXPLOSION2

		case LAVASPLASH:
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_LavaSplash( Position );
		}
		break;	// LAVASPLASH

		case TELEPORT:
		{
			Position[ 0 ]	= qCMessage::ReadCoord();
			Position[ 1 ]	= qCMessage::ReadCoord();
			Position[ 2 ]	= qCMessage::ReadCoord();
			R_TeleportSplash( Position );
		}
		break;	// TELEPORT

		// lightning bolts
		case LIGHTNING1:	{ ParseBeam( g_pModelBolt1 );						} break;
		case LIGHTNING2:	{ ParseBeam( g_pModelBolt2 );						} break;
		case LIGHTNING3:	{ ParseBeam( g_pModelBolt3 );						} break;

		default:			{ Sys_Error(" qCTempEntities::Parse: Bad type");	} break;
	}

}	// Parse

/*
======
Update
======
*/
void qCTempEntities::Update( void )
{
	qSBeam*		pBeam	= g_Beams;
	entity_t*	pEntity;
	qVector3	Diff;
	qVector3	Position;
	qFloat		Distance;
	qFloat		Yaw;
	qFloat		Pitch;
	qFloat		Forward;

	g_NumTempEntities	= 0;

	// update lightning
	for( qUInt32 i=0; i<MAX_BEAMS; ++i, ++pBeam )
	{
		if(! pBeam->pModel || pBeam->EndTime < cl.time )
			continue;

		// if coming from the player, update the start position
		if( pBeam->EntityNum == cl.viewentity )
			VectorCopy( cl_entities[ cl.viewentity ].origin, pBeam->Start );

		// calculate pitch and yaw
		VectorSubtract( pBeam->End, pBeam->Start, Diff );

		if( Diff[ 0 ] == 0 && Diff[ 1 ] == 0 )
		{
			Yaw	= 0;
			if( Diff[ 2 ] > 0 )	Pitch	= 90;
			else				Pitch	= 270;
		}
		else
		{
			Forward	= sqrt( Diff[ 0 ] * Diff[ 0 ] + Diff[ 1 ] * Diff[ 1 ] );
			Yaw		= ( qFloat )Q_RAD_TO_DEG( atan2( Diff[ 1 ], Diff[ 0 ] ) );
			Pitch	= ( qFloat )Q_RAD_TO_DEG( atan2( Diff[ 2 ], Forward ) );

			if( Yaw < 0.0f )	Yaw		+= 360.0f;
			if( Pitch < 0.0f )	Pitch	+= 360.0f;
		}

		// add new entities for the lightning
		VectorCopy( pBeam->Start, Position );
		Distance	= qCMath::VectorNormalize( Diff );

		while( Distance > 0.0f )
		{
			if( cl_numvisedicts		== MAX_VISEDICTS	||
				g_NumTempEntities	== MAX_TEMP_ENTITIES )
				return;

			pEntity	= &g_TempEntities[ g_NumTempEntities ];
			++g_NumTempEntities;
			memset( pEntity, 0, sizeof( entity_t ) );
			cl_visedicts[ cl_numvisedicts ]	= pEntity;
			++cl_numvisedicts;

			pEntity->colormap		= vid.colormap;
			pEntity->model			= pBeam->pModel;
			pEntity->angles[ 0 ]	= Pitch;
			pEntity->angles[ 1 ]	= Yaw;
			pEntity->angles[ 2 ]	= ( qFloat )( rand() % 360 );

			VectorCopy( Position, pEntity->origin );

			Position[ 0 ]	+= Diff[ 0 ] * 30.0f;
			Position[ 1 ]	+= Diff[ 1 ] * 30.0f;
			Position[ 2 ]	+= Diff[ 2 ] * 30.0f;

			Distance	-= 30.0f;
		}
	}

}	// Update
