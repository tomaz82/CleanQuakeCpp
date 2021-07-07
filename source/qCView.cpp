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
#include	"glquake.h"
#include	"qCView.h"
#include	"client.h"
#include	"qCClient.h"
#include	"bspfile.h"
#include	"qCMath.h"
#include	"qCCVar.h"
#include	"cmd.h"
#include	"console.h"
#include	"qCMessage.h"

qSCVar	cl_rollspeed = {"cl_rollspeed", "200"};
qSCVar	cl_rollangle = {"cl_rollangle", "2.0"};

qSCVar	cl_bob = {"cl_bob","0.02", FALSE};
qSCVar	cl_bobcycle = {"cl_bobcycle","0.6", FALSE};
qSCVar	cl_bobup = {"cl_bobup","0.5", FALSE};

qSCVar	v_kicktime = {"v_kicktime", "0.5", FALSE};
qSCVar	v_kickroll = {"v_kickroll", "0.6", FALSE};
qSCVar	v_kickpitch = {"v_kickpitch", "0.6", FALSE};

qSCVar	v_iyaw_cycle = {"v_iyaw_cycle", "2", FALSE};
qSCVar	v_iroll_cycle = {"v_iroll_cycle", "0.5", FALSE};
qSCVar	v_ipitch_cycle = {"v_ipitch_cycle", "1", FALSE};
qSCVar	v_iyaw_level = {"v_iyaw_level", "0.3", FALSE};
qSCVar	v_iroll_level = {"v_iroll_level", "0.1", FALSE};
qSCVar	v_ipitch_level = {"v_ipitch_level", "0.3", FALSE};

qSCVar	v_idlescale = {"v_idlescale", "0", FALSE};

static qFloat	g_DamageTime;
static qFloat	g_DamageRoll;
static qFloat	g_DamagePitch;

static cshift_t	g_ColorShiftEmpty	= { { 130, 80, 50 },   0 };
static cshift_t	g_ColorShiftWater	= { { 130, 80, 50 }, 128 };
static cshift_t	g_ColorShiftSlime	= { {   0, 25,  5 }, 150 };
static cshift_t	g_ColorShiftLava	= { { 255, 80,  0 }, 150 };

static qVector4	g_BlendColor		= { 0.0f, 0.0f, 0.0f, 0.0f };

/*
============
ColorShiftCB
============
*/
static void ColorShiftCB( void )
{
	g_ColorShiftEmpty.destcolor[ 0 ]	= atoi( Cmd_Argv( 1 ) );
	g_ColorShiftEmpty.destcolor[ 1 ]	= atoi( Cmd_Argv( 2 ) );
	g_ColorShiftEmpty.destcolor[ 2 ]	= atoi( Cmd_Argv( 3 ) );
	g_ColorShiftEmpty.percent			= atoi( Cmd_Argv( 4 ) );

}	// ColorShiftCB

/*
============
BonusFlashCB
============
*/
static void BonusFlashCB( void )
{
	cl.cshifts[ CSHIFT_BONUS ].destcolor[ 0 ]	= 215;
	cl.cshifts[ CSHIFT_BONUS ].destcolor[ 1 ]	= 186;
	cl.cshifts[ CSHIFT_BONUS ].destcolor[ 2 ]	= 69;
	cl.cshifts[ CSHIFT_BONUS ].percent			= 50;

}	// BonusFlashCB

/*
============
BoundOffsets
============
*/
static void BoundOffsets( void )
{
	entity_t*	pEntity	= &cl_entities[ cl.viewentity ];

	// Absolutely bound refresh relative to entity clipping hull so the view can never be inside a solid wall
	r_refdef.vieworg[ 0 ]	= qCMath::Clamp( pEntity->origin[ 0 ] - 14 ,r_refdef.vieworg[ 0 ], pEntity->origin[ 0 ] + 14 );
	r_refdef.vieworg[ 1 ]	= qCMath::Clamp( pEntity->origin[ 1 ] - 14 ,r_refdef.vieworg[ 1 ], pEntity->origin[ 1 ] + 14 );
	r_refdef.vieworg[ 2 ]	= qCMath::Clamp( pEntity->origin[ 2 ] - 22 ,r_refdef.vieworg[ 2 ], pEntity->origin[ 2 ] + 30 );

}	// BoundOffsets

/*
=======
AddIdle
=======
*/
static void AddIdle( void )
{
	r_refdef.viewangles[ PITCH ]	+= v_idlescale.Value * sin( cl.time * v_ipitch_cycle.Value ) * v_ipitch_level.Value;
	r_refdef.viewangles[ YAW ]		+= v_idlescale.Value * sin( cl.time * v_iyaw_cycle.Value )   * v_iyaw_level.Value;
	r_refdef.viewangles[ ROLL ]		+= v_idlescale.Value * sin( cl.time * v_iroll_cycle.Value )  * v_iroll_level.Value;

}	// AddIdle

/*
=======
CalcBob
=======
*/
static qFloat CalcBob( void )
{
	qFloat	Bob;
	qFloat	Cycle;

	Cycle	 = cl.time - ( qUInt32 )( cl.time / cl_bobcycle.Value ) * cl_bobcycle.Value;
	Cycle	/= cl_bobcycle.Value;

	if( Cycle < cl_bobup.Value )	Cycle	= Q_PI * Cycle / cl_bobup.Value;
	else							Cycle	= Q_PI + Q_PI * ( Cycle - cl_bobup.Value ) / ( 1.0f - cl_bobup.Value );

	// Bob is proportional to velocity in the xy plane, ( don't count Z, or jumping messes it up )
	Bob	= sqrt( cl.velocity[ 0 ] * cl.velocity[ 0 ] + cl.velocity[ 1 ] * cl.velocity[ 1 ] ) * cl_bob.Value;
	Bob	= Bob * 0.3f + Bob * 0.7f * sin( Cycle );

	if( Bob > 4.0f )		Bob	=  4.0f;
	else if( Bob < -7.0f )	Bob	= -7.0f;

	return Bob;

}	// CalcBob

/*
=====================
CalcPowerupColorShift
=====================
*/
static void CalcPowerupColorShift (void)
{
	if( cl.items & IT_QUAD )
	{
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 0 ]	= 0;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 1 ]	= 0;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 2 ]	= 255;
		cl.cshifts[ CSHIFT_POWERUP ].percent		= 30;
	}
	else if( cl.items & IT_SUIT )
	{
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 0 ]	= 0;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 1 ]	= 255;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 2 ]	= 0;
		cl.cshifts[ CSHIFT_POWERUP ].percent		= 20;
	}
	else if( cl.items & IT_INVISIBILITY )
	{
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 0 ]	= 100;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 1 ]	= 100;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 2 ]	= 100;
		cl.cshifts[ CSHIFT_POWERUP ].percent		= 100;
	}
	else if( cl.items & IT_INVULNERABILITY )
	{
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 0 ]	= 255;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 1 ]	= 255;
		cl.cshifts[ CSHIFT_POWERUP ].destcolor[ 2 ]	= 0;
		cl.cshifts[ CSHIFT_POWERUP ].percent		= 30;
	}
	else
		cl.cshifts[ CSHIFT_POWERUP ].percent		= 0;

}	// CalcPowerupColorShift

/*
============
CalcGunAngle
============
*/
static void CalcGunAngle( void )
{
	cl.viewent.angles[ PITCH ]	= -r_refdef.viewangles[ PITCH ];
	cl.viewent.angles[ YAW ]	=  r_refdef.viewangles[ YAW ];

	cl.viewent.angles[ PITCH ]	-= v_idlescale.Value * sin( cl.time * v_ipitch_cycle.Value ) * v_ipitch_level.Value;
	cl.viewent.angles[ YAW ]	-= v_idlescale.Value * sin( cl.time * v_iyaw_cycle.Value )   * v_iyaw_level.Value;
	cl.viewent.angles[ ROLL ]	-= v_idlescale.Value * sin( cl.time * v_iroll_cycle.Value )  * v_iroll_level.Value;

}	// CalcGunAngle

/*
============
CalcViewRoll
============
*/
static void CalcViewRoll( void )
{
	qFloat	Side	= qCView::CalcRoll( cl_entities[ cl.viewentity ].angles, cl.velocity );

	r_refdef.viewangles[ ROLL ] += Side;

	if( g_DamageTime > 0.0f )
	{
		r_refdef.viewangles[ PITCH ]	+= g_DamageTime / v_kicktime.Value * g_DamagePitch;
		r_refdef.viewangles[ ROLL ]		+= g_DamageTime / v_kicktime.Value * g_DamageRoll;

		g_DamageTime					-= host_frametime;
	}

	if( cl.stats[ STAT_HEALTH ] <= 0 )
	{
		// Dead view angle
		r_refdef.viewangles[ ROLL ]	= 80.0f;
		return;
	}

}	// CalcViewRoll

/*
======================
CalcIntermissionRefdef
======================
*/
static void CalcIntermissionRefdef( void )
{
	// pEntity is the player model ( visible when out of body )
	// pViewEntity is the weapon model ( only visible from inside body )
	entity_t*	pEntity		= &cl_entities[ cl.viewentity ];
	entity_t*	pViewEntity	= &cl.viewent;
	qFloat		Old			= v_idlescale.Value;;

	VectorCopy( pEntity->origin, r_refdef.vieworg );
	VectorCopy( pEntity->angles, r_refdef.viewangles );
	pViewEntity->model	= NULL;

	// Always idle in intermission
	v_idlescale.Value	= 1.0f;
	AddIdle();
	v_idlescale.Value	= Old;

}	// CalcIntermissionRefdef


/*
==========
CalcRefdef
==========
*/
static void CalcRefdef( void )
{
	// pEntity is the player model ( visible when out of body )
	// pViewEntity is the weapon model ( only visible from inside body )
	static qFloat	OldZ		= 0.0f;
	entity_t*		pEntity		= &cl_entities[ cl.viewentity ];
	entity_t*		pViewEntity	= &cl.viewent;
	qVector3		Forward;
	qVector3		Angles;
	qFloat			Bob;

	// Transform the view offset by the model's matrix to get the offset from
	pEntity->angles[ YAW ]		=  cl.viewangles[ YAW ];	// The model should face the view dir
	pEntity->angles[ PITCH ]	= -cl.viewangles[ PITCH ];	// The model should face the view dir

	Bob	= CalcBob();

	// Refresh position
	VectorCopy( pEntity->origin, r_refdef.vieworg );
	r_refdef.vieworg[ 2 ]	+= cl.viewheight + Bob;

	// Never let it sit exactly on a node line, because a water plane can dissapear when viewed with the eye exactly
	// on it, the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef.vieworg[ 0 ] += 1.0f / 32.0f;
	r_refdef.vieworg[ 1 ] += 1.0f / 32.0f;
	r_refdef.vieworg[ 2 ] += 1.0f / 32.0f;

	VectorCopy( cl.viewangles, r_refdef.viewangles );
	CalcViewRoll();
	AddIdle();

	// Offsets
	Angles[ PITCH ]	= -pEntity->angles[ PITCH ];	// Because entity pitches are actually backward
	Angles[ YAW ]	=  pEntity->angles[ YAW ];
	Angles[ ROLL ]	=  pEntity->angles[ ROLL ];

	qCMath::AngleVectors( Angles, Forward, NULL, NULL );

	BoundOffsets();

	// Set up gun position
	VectorCopy( cl.viewangles, pViewEntity->angles );

	CalcGunAngle();

	VectorCopy( pEntity->origin, pViewEntity->origin );

	pViewEntity->origin[ 0 ]	+= Forward[ 0 ] * Bob * 0.4f;
	pViewEntity->origin[ 1 ]	+= Forward[ 1 ] * Bob * 0.4f;
	pViewEntity->origin[ 2 ]	+= Forward[ 2 ] * Bob * 0.4f + cl.viewheight + Bob;
	pViewEntity->model			 = cl.model_precache[ cl.stats[ STAT_WEAPON ] ];
	pViewEntity->frame			 = cl.stats[ STAT_WEAPONFRAME ];
	pViewEntity->colormap		 = vid.colormap;

	// Set up the refresh position
	VectorAdd( r_refdef.viewangles, cl.punchangle, r_refdef.viewangles );

	// Smooth out stair step ups
	if( cl.onground && pEntity->origin[ 2 ] - OldZ > 0.0f )
	{
		qFloat	StepTime	= cl.time - cl.oldtime;;

		if( StepTime < 0.0f )
			StepTime = 0.0f;

		OldZ	+= StepTime * 80.0f;

		if( OldZ > pEntity->origin[ 2 ] )			OldZ	= pEntity->origin[ 2 ];
		if( OldZ < pEntity->origin[ 2 ] - 12.0f )	OldZ	= pEntity->origin[ 2 ] - 12;

		r_refdef.vieworg[ 2 ]		+= OldZ - pEntity->origin[ 2 ];
		pViewEntity->origin[ 2 ]	+= OldZ - pEntity->origin[ 2 ];
	}
	else
		OldZ	= pEntity->origin[ 2 ];

}	// CalcRefdef

/*
===========
RenderBlend
===========
*/
static void RenderBlend( void )
{
	if( !g_BlendColor[ 3 ] )
		return;

	GL_DisableMultitexture();

	glEnable( GL_BLEND );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_TEXTURE_2D );

	glLoadIdentity();

	glRotatef( -90.0f, 1.0f, 0.0f, 0.0f );
	glRotatef(  90.0f, 0.0f, 0.0f, 1.0f );

	glColor4fv( g_BlendColor );

	glBegin( GL_QUADS );
	glVertex3f( 10.0f,  100.0f,  100.0f );
	glVertex3f( 10.0f, -100.0f,  100.0f );
	glVertex3f( 10.0f, -100.0f, -100.0f );
	glVertex3f( 10.0f,  100.0f, -100.0f );
	glEnd();

	glDisable( GL_BLEND );
	glEnable( GL_TEXTURE_2D );

}	// RenderBlend


//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCView::Init( void )
{
	Cmd_AddCommand( "v_cshift", ColorShiftCB );
	Cmd_AddCommand( "bf",		BonusFlashCB );

	qCCVar::Register( &v_iyaw_cycle );
	qCCVar::Register( &v_iroll_cycle );
	qCCVar::Register( &v_ipitch_cycle );
	qCCVar::Register( &v_iyaw_level );
	qCCVar::Register( &v_iroll_level );
	qCCVar::Register( &v_ipitch_level );

	qCCVar::Register( &v_idlescale );

	qCCVar::Register( &cl_rollspeed );
	qCCVar::Register( &cl_rollangle );
	qCCVar::Register( &cl_bob );
	qCCVar::Register( &cl_bobcycle );
	qCCVar::Register( &cl_bobup );

	qCCVar::Register( &v_kicktime );
	qCCVar::Register( &v_kickroll );
	qCCVar::Register( &v_kickpitch );

}	// Init

/*
========
CalcRoll
========
*/
qFloat qCView::CalcRoll( qVector3 Angles, qVector3 Velocity )
{
	qVector3	Right;
	qFloat		Sign;
	qFloat		Side;
	qFloat		Value;

	qCMath::AngleVectors( Angles, NULL, Right, NULL );

	Side	= VectorDot( Velocity, Right );
	Sign	= Side < 0.0f ? -1.0f : 1.0f;
	Side	= fabs( Side );
	Value	= cl_rollangle.Value;

	if( Side < cl_rollspeed.Value )	Side	= Side * Value / cl_rollspeed.Value;
	else							Side	= Value;

	return Side * Sign;

}	// CalcRoll

/*
=========
CalcBlend
=========
*/
void qCView::CalcBlend( void )
{
	qFloat	R	= 0.0f;
	qFloat	G	= 0.0f;
	qFloat	B	= 0.0f;
	qFloat	A	= 0.0f;
	qFloat	A2;

	for( qUInt32 i=0; i<NUM_CSHIFTS; ++i )
	{
		if( !( A2 = cl.cshifts[ i ].percent / 255.0 ) )
			continue;

		A	= A + A2 * ( 1.0f - A );
		A2	= A2 / A;
		R	= R * ( 1.0f - A2 ) + cl.cshifts[ i ].destcolor[ 0 ] * A2;
		G	= G * ( 1.0f - A2 ) + cl.cshifts[ i ].destcolor[ 1 ] * A2;
		B	= B * ( 1.0f - A2 ) + cl.cshifts[ i ].destcolor[ 2 ] * A2;
	}

	g_BlendColor[ 0 ]	= R / 255.0;
	g_BlendColor[ 1 ]	= G / 255.0;
	g_BlendColor[ 2 ]	= B / 255.0;
	g_BlendColor[ 3 ]	= qCMath::Clamp( 0.0f, A, 1.0f );

}	// CalcBlend

/*
===========
ParseDamage
===========
*/
void qCView::ParseDamage( void )
{
	entity_t*	pEntity;
	qVector3	From;
	qVector3	Forward;
	qVector3	Right;
	qFloat		Side;
	qFloat		Count;
	qSInt32		Armor;
	qSInt32		Blood;

	Armor	= qCMessage::ReadByte();
	Blood	= qCMessage::ReadByte();

	for( qUInt32 i=0; i<3; ++i )
		From[ i ]	= qCMessage::ReadCoord();

	Count	= Blood * 0.5f + Armor * 0.5f;

	if( Count < 10.0f )
		Count = 10.0f;

	cl.faceanimtime	= cl.time + 0.2f;	// put sbar face into pain frame

	cl.cshifts[ CSHIFT_DAMAGE ].percent	+= Count * 3.0f;

	if( cl.cshifts[ CSHIFT_DAMAGE ].percent <   0.0f )	cl.cshifts[ CSHIFT_DAMAGE ].percent	=   0.0f;
	if( cl.cshifts[ CSHIFT_DAMAGE ].percent > 150.0f )	cl.cshifts[ CSHIFT_DAMAGE ].percent = 150.0f;

	if( Armor > Blood )
	{
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 0 ]	= 200;
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 1 ]	= 100;
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 2 ]	= 100;
	}
	else if( Armor )
	{
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 0 ]	= 220;
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 1 ]	= 50;
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 2 ]	= 50;
	}
	else
	{
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 0 ]	= 255;
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 1 ]	= 0;
		cl.cshifts[ CSHIFT_DAMAGE ].destcolor[ 2 ]	= 0;
	}

	// calculate view angle kicks
	pEntity	= &cl_entities[ cl.viewentity ];

	VectorSubtract( From, pEntity->origin, From );
	qCMath::VectorNormalize( From );

	qCMath::AngleVectors( pEntity->angles, Forward, Right, NULL );

	Side			= VectorDot( From, Right );
	g_DamageRoll	= Count * Side * v_kickroll.Value;

	Side			= VectorDot( From, Forward );
	g_DamagePitch	= Count * Side * v_kickpitch.Value;

	g_DamageTime	= v_kicktime.Value;

}	// ParseDamage

/*
================
SetContentsColor
================
*/
void qCView::SetContentsColor( const qSInt32 Contents )
{
	switch( Contents )
	{
		case CONTENTS_EMPTY:
		case CONTENTS_SOLID:	{ cl.cshifts[ CSHIFT_CONTENTS ]	= g_ColorShiftEmpty;	} break;
		case CONTENTS_LAVA:		{ cl.cshifts[ CSHIFT_CONTENTS ]	= g_ColorShiftLava;		} break;
		case CONTENTS_SLIME:	{ cl.cshifts[ CSHIFT_CONTENTS ]	= g_ColorShiftSlime;	} break;
		default:				{ cl.cshifts[ CSHIFT_CONTENTS ]	= g_ColorShiftWater;	}
	}

}	// SetContentsColor

/*
=============
UpdatePalette
=============
*/
void qCView::UpdatePalette( void )
{
	qBool	NewPalette	= FALSE;

	CalcPowerupColorShift();

	for( qUInt32 i=0; i<NUM_CSHIFTS; ++i )
	{
		if( cl.cshifts[ i ].percent != cl.prev_cshifts[ i ].percent )
		{
			NewPalette						= TRUE;
			cl.prev_cshifts[ i ].percent	= cl.cshifts[ i ].percent;
		}

		for( qUInt32 j=0; j<3; ++j )
		{
			if( cl.cshifts[ i ].destcolor[ j ] != cl.prev_cshifts[ i ].destcolor[ j ] )
			{
				NewPalette							= TRUE;
				cl.prev_cshifts[ i ].destcolor[ j ]	= cl.cshifts[ i ].destcolor[ j ];
			}
		}
	}

	// Drop the damage and bonus values
	cl.cshifts[ CSHIFT_DAMAGE ].percent	-= host_frametime * 150.0;
	cl.cshifts[ CSHIFT_BONUS ].percent	-= host_frametime * 100.0;

	if( cl.cshifts[ CSHIFT_DAMAGE ].percent <= 0 )	cl.cshifts[ CSHIFT_DAMAGE ].percent	= 0;
	if( cl.cshifts[ CSHIFT_BONUS ].percent  <= 0 )	cl.cshifts[ CSHIFT_BONUS ].percent	= 0;

	if( NewPalette )
		CalcBlend();

}	// UpdatePalette

/*
======
Render
======
*/
void qCView::Render( void )
{
	if( con_forcedup )
		return;

	if( cl.intermission )	CalcIntermissionRefdef();
	else if( !cl.paused )	CalcRefdef();

	R_PushDlights();
	R_RenderView();

	RenderBlend();

}	// Render
