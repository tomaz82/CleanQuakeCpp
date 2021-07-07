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

#include	"vid.h"
#include	"draw.h"
#include	"screen.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"qCSBar.h"
#include	"glquake.h"

#define	STAT_MINUS	10

static qpic_t*	g_pPicNum[ 2 ][ 11 ];
static qpic_t*	g_pPicColon;
static qpic_t*	g_pPicSlash;
static qpic_t*	g_pPicIBar;
static qpic_t*	g_pPicSBar;
static qpic_t*	g_pPicScoreBar;

static qpic_t*	g_pPicWeapon[ 7 ][ 8 ];
static qpic_t*	g_pPicAmmo[ 4 ];
static qpic_t*	g_pPicSigil[ 4 ];
static qpic_t*	g_pPicArmor[ 3 ];
static qpic_t*	g_pPicItem[ 32 ];

static qpic_t*	g_pPicDisc;

static qpic_t*	g_pPicFace[ 7 ][ 2 ];
static qpic_t*	g_pPicFaceQuad;
static qpic_t*	g_pPicFaceInvis;
static qpic_t*	g_pPicFaceInvul;
static qpic_t*	g_pPicFaceInvisInvul;

static qpic_t*	g_pPicComplete;
static qpic_t*	g_pPicIntermission;
static qpic_t*	g_pPicFinale;
static qpic_t*	g_pPicRanking;

static qBool	g_ShowScores;

static qUInt32	g_NumLines;

static qUInt32	g_FragSort[ MAX_SCOREBOARD ];
static qChar	g_ScoreBoardText[ MAX_SCOREBOARD ][ 20 ];
static qUInt32	g_ScoreBoardTop[ MAX_SCOREBOARD ];
static qUInt32	g_ScoreBoardBottom[ MAX_SCOREBOARD ];
static qUInt32	g_ScoreBoardCount[ MAX_SCOREBOARD ];
static qUInt32	g_ScoreBoardLines;

/*
============
ScoresShowCB
============
*/
static void ScoresShowCB( void )
{
	g_ShowScores	= TRUE;

}	// ScoresShowCB

/*
============
ScoresHideCB
============
*/
static void ScoresHideCB( void )
{
	g_ShowScores	= FALSE;

}	// ScoresHideCB

/*
=========
SortFrags
=========
*/
static void SortFrags( void )
{
	g_ScoreBoardLines	= 0;

	for( qUInt32 i=0; i<cl.maxclients; ++i )
	{
		if( cl.scores[i].name[ 0 ] )
		{
			g_FragSort[ g_ScoreBoardLines ]	= i;
			++g_ScoreBoardLines;
		}
	}

	for( qUInt32 i=0; i<g_ScoreBoardLines; ++i )
	{
		for( qUInt32 j=0; j<g_ScoreBoardLines-1-i; ++j )
		{
			if( cl.scores[ g_FragSort[ j ] ].frags < cl.scores[g_FragSort[ j + 1 ] ].frags )
			{
				qUInt32	Temp		= g_FragSort[ j ];
				g_FragSort[ j ]		= g_FragSort[ j + 1 ];
				g_FragSort[ j + 1 ]	= Temp;
			}
		}
	}

}	// SortFrags

/*
=============
RenderNumbers
=============
*/
static void RenderNumbers( const qUInt32 _x, const qUInt32 _y, const qUInt32 _numbers, const qUInt32 _color )
{
	char string[ 4 ];

	snprintf( string, sizeof( string ), "%3d", _numbers );

	if( string[ 0 ] != ' ' )	Draw_Pic( _x +  0, _y, g_pPicNum[ _color ][ ( string[ 0 ] == '-' ) ? STAT_MINUS : ( string[ 0 ] - '0' ) ] );
	if( string[ 1 ] != ' ' )	Draw_Pic( _x + 24, _y, g_pPicNum[ _color ][ ( string[ 1 ] == '-' ) ? STAT_MINUS : ( string[ 1 ] - '0' ) ] );
	if( string[ 2 ] != ' ' )	Draw_Pic( _x + 48, _y, g_pPicNum[ _color ][ ( string[ 2 ] == '-' ) ? STAT_MINUS : ( string[ 2 ] - '0' ) ] );

}	// RenderNumbers

/*
=================
RenderColoredQuad
=================
*/
static void RenderColoredQuad( const qUInt32 X, const qUInt32 Y, const qUInt32 Width, const qUInt32 Height, const qUInt8 ColorID )
{
	qUInt8*	pColor	= &host_basepal[ ColorID * 3 ];

	glDisable( GL_TEXTURE_2D );
	glColor3ubv( pColor );
	glBegin( GL_QUADS );

	glVertex2f( ( qFloat )( X ),			( qFloat )( Y ) );
	glVertex2f( ( qFloat )( X + Width ),	( qFloat )( Y ) );
	glVertex2f( ( qFloat )( X + Width ),	( qFloat )( Y + Height ) );
	glVertex2f( ( qFloat )( X ),			( qFloat )( Y + Height ) );

	glEnd();
	glColor3ub( 255, 255, 255 );
	glEnable( GL_TEXTURE_2D );

}	// RenderColoredQuad

/*
===========
RenderFrags
===========
*/
static void RenderFrags( const qUInt32 X, const qUInt32 Y )
{
	SortFrags();

//////////////////////////////////////////////////////////////////////////

	qUInt32	Length	= g_ScoreBoardLines <= 4 ? g_ScoreBoardLines : 4;

	for( qUInt32 i=0; i<Length; ++i )
	{
		qChar			Number[ 4 ];
		qUInt32			Place	= g_FragSort[ i ];
		scoreboard_t*	pScore	= &cl.scores[ Place ];
		qUInt32			Top		= 8 + ( ( pScore->colors & 0xf0 ) );
		qUInt32			Bottom	= 8 + ( ( pScore->colors & 15 ) << 4 );

//		if( !pScore->name[ 0 ] )
//			continue;

		RenderColoredQuad( X + 194 + ( i * 32 ), Y - 23, 28, 4, Top );
		RenderColoredQuad( X + 194 + ( i * 32 ), Y - 19, 28, 4, Bottom );

		snprintf( Number, sizeof( Number ), "%3d", pScore->frags );
		Draw_String( X + 194 + ( i * 32 ), Y - 24, Number );

		if( Place == cl.viewentity - 1 )
		{
			Draw_Character( X + 190 + ( i * 32 ), Y - 24, 16 );
			Draw_Character( X + 216 + ( i * 32 ), Y - 24, 17 );
		}
	}

}	// RenderFrags

/*
===============
RenderInventory
===============
*/
static void RenderInventory( const qUInt32 X, const qUInt32 Y )
{
	for( qUInt32 i=0; i<7; ++i )
	{
		if( cl.items & ( IT_SHOTGUN << i ) )
		{
			qFloat	Time	= cl.item_gettime[ i ];
			qUInt32	FlashOn	= ( qUInt32 )( ( cl.time - Time ) * 10 );

			if( FlashOn >= 10 )
			{
				if( cl.stats[ STAT_ACTIVEWEAPON ] == ( IT_SHOTGUN << i ) )	FlashOn	= 1;
				else														FlashOn	= 0;
			}
			else
				FlashOn	= ( FlashOn % 5 ) + 2;

			Draw_Pic( X + i * 24, Y - 16, g_pPicWeapon[ FlashOn ][ i ] );
		}
	}

	for( qUInt32 i=0; i<4; ++i )
	{
		char	Number[ 4 ];

		snprintf( Number, sizeof( Number ), "%3d", cl.stats[ STAT_SHELLS + i ] );

		if( Number[ 0 ] != ' ' )	Draw_Character( X + ( 6 * i + 1 ) * 8 + 2, Y - 24, 18 + Number[ 0 ] - '0' );
		if (Number[ 1 ] != ' ' )	Draw_Character( X + ( 6 * i + 2 ) * 8 + 2, Y - 24, 18 + Number[ 1 ] - '0' );
		if (Number[ 2 ] != ' ' )	Draw_Character( X + ( 6 * i + 3 ) * 8 + 2, Y - 24, 18 + Number[ 2 ] - '0' );
	}

	for( qUInt32 i=0; i<6; ++i )
	{
		if( cl.items & ( 1 << ( 17 + i ) ) )
			Draw_Pic( X + 192 + i * 16, Y - 16, g_pPicItem[ i ] );
	}

	for( qUInt32 i=0; i<4; ++i )
	{
		if( cl.items & ( 1 << ( 28 + i ) ) )
			Draw_Pic( X + 320 - 32 + i * 8, Y - 16, g_pPicSigil[ i ] );
	}

}	// RenderInventory

/*
====================
RenderSoloScoreBoard
====================
*/
static void RenderSoloScoreBoard( const qUInt32 X, const qUInt32 Y )
{
	qChar	String[ 80 ];
	qUInt32	Minutes	= ( qUInt32 )( cl.time / 60 );
	qUInt32	Seconds	= ( qUInt32 )( cl.time - ( Minutes * 60 ) );
	qUInt32	Tens	= Seconds / 10;
	qUInt32	Units	= Seconds - ( Tens * 10 );

	snprintf( String, sizeof( String ), "Monsters:%3d /%3d", cl.stats[ STAT_MONSTERS ], cl.stats[ STAT_TOTALMONSTERS ] );
	Draw_String( X + 8, Y + 4, String );

	snprintf( String, sizeof( String ), "Secrets: %3d /%3d", cl.stats[ STAT_SECRETS ], cl.stats[ STAT_TOTALSECRETS ] );
	Draw_String( X + 8, Y + 12, String );

	snprintf( String, sizeof( String ), "Time: %3d:%d%d", Minutes, Tens, Units );
	Draw_String( X + 184, Y + 4, String );

	Draw_String( X + 232 - strlen( cl.levelname ) * 4, Y + 12, cl.levelname );

}	// RenderSoloScoreBoard

/*
=================
OverlayDeathmatch
=================
*/
static void OverlayDeathmatch( void )
{
	Draw_Pic( ( 320 - g_pPicRanking->width ) / 2 + ( ( vid.width - 320 ) >> 1 ), 8, g_pPicRanking );

	SortFrags();

//////////////////////////////////////////////////////////////////////////

	qUInt32	Length	= g_ScoreBoardLines <= 4 ? g_ScoreBoardLines : 4;
	qUInt32	X		= 80 + ( ( vid.width - 320 ) >> 1 );
	qUInt32	Y		= 40;

	for( qUInt32 i=0; i<Length; ++i )
	{
		qChar			Number[ 4 ];
		qUInt32			Place	= g_FragSort[ i ];
		scoreboard_t*	pScore	= &cl.scores[ Place ];
		qUInt32			Top		= 8 + ( ( pScore->colors & 0xf0 ) );
		qUInt32			Bottom	= 8 + ( ( pScore->colors & 15 ) << 4 );

		if( !pScore->name[ 0 ] )
			continue;

		RenderColoredQuad( X, Y + 0, 40, 4, Top );
		RenderColoredQuad( X, Y + 4, 40, 4, Bottom );

		snprintf( Number, sizeof( Number ), "%3d", pScore->frags );
		Draw_String( X + 12, Y, Number );

		if( Place == cl.viewentity - 1 )
			Draw_Character( X - 4, Y, 12 );

		Draw_String( X + 64, Y, pScore->name );

		Y	+= 10;
	}

}	// OverlayDeathmatch

/*
=====================
OverlayDeathmatchMini
=====================
*/
static void OverlayDeathmatchMini( void )
{
	if( vid.width < 512 || !g_NumLines )
		return;

	SortFrags();

//////////////////////////////////////////////////////////////////////////

	qUInt32	NumLines	= g_NumLines / 8;
	qUInt32	X			= 324;
	qUInt32	Y			= vid.height - g_NumLines;
	qSInt32	i			= 0;

	if( NumLines < 3 )
		return;

	for( i=0; i<( qSInt32 )g_ScoreBoardLines; ++i )
		if( g_FragSort[ i ] == cl.viewentity - 1 )
			break;

	if( i == g_ScoreBoardLines )	i	= 0;
	else							i	= i - NumLines / 2;

	if( i > ( qSInt32 )( g_ScoreBoardLines - NumLines ) )	i	= g_ScoreBoardLines - NumLines;
	if( i < 0 )												i	= 0;

	for( ; i<( qSInt32 )g_ScoreBoardLines && Y<vid.height-8; ++i )
	{
		qChar			Number[ 4 ];
		qUInt32			Place	= g_FragSort[ i ];
		scoreboard_t*	pScore	= &cl.scores[ Place ];
		qUInt32			Top		= 8 + ( ( pScore->colors & 0xf0 ) );
		qUInt32			Bottom	= 8 + ( ( pScore->colors & 15 ) << 4 );

		if( !pScore->name[ 0 ] )
			continue;

		RenderColoredQuad( X, Y + 0, 40, 4, Top );
		RenderColoredQuad( X, Y + 4, 40, 4, Bottom );

		snprintf( Number, sizeof( Number ), "%3d", pScore->frags );
		Draw_String( X + 8, Y, Number );

		if( Place == cl.viewentity - 1 )
		{
			Draw_Character( X +  0, Y, 16 );
			Draw_Character( X + 32, Y, 17 );
		}

		Draw_String( X + 48, Y, pScore->name );

		Y	+= 8;
	}

}	// OverlayDeathmatchMini

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCSBar::Init( void )
{
	for( qUInt32 i=0; i<10; ++i )
	{
		g_pPicNum[ 0 ][ i ]	= Draw_PicFromWad( va( "num_%d",  i ) );
		g_pPicNum[ 1 ][ i ]	= Draw_PicFromWad( va( "anum_%d", i ) );
	}

	g_pPicNum[ 0 ][ 10 ]	= Draw_PicFromWad( "num_minus" );
	g_pPicNum[ 1 ][ 10 ]	= Draw_PicFromWad( "anum_minus" );
	g_pPicColon				= Draw_PicFromWad( "num_colon" );
	g_pPicSlash				= Draw_PicFromWad( "num_slash" );
	g_pPicIBar				= Draw_PicFromWad( "ibar" );
	g_pPicSBar				= Draw_PicFromWad( "sbar" );
	g_pPicScoreBar			= Draw_PicFromWad( "scorebar" );

	g_pPicWeapon[ 0 ][ 0 ]	= Draw_PicFromWad( "inv_shotgun" );
	g_pPicWeapon[ 0 ][ 1 ]	= Draw_PicFromWad( "inv_sshotgun" );
	g_pPicWeapon[ 0 ][ 2 ]	= Draw_PicFromWad( "inv_nailgun" );
	g_pPicWeapon[ 0 ][ 3 ]	= Draw_PicFromWad( "inv_snailgun" );
	g_pPicWeapon[ 0 ][ 4 ]	= Draw_PicFromWad( "inv_rlaunch" );
	g_pPicWeapon[ 0 ][ 5 ]	= Draw_PicFromWad( "inv_srlaunch" );
	g_pPicWeapon[ 0 ][ 6 ]	= Draw_PicFromWad( "inv_lightng" );

	g_pPicWeapon[ 1 ][ 0 ]	= Draw_PicFromWad( "inv2_shotgun" );
	g_pPicWeapon[ 1 ][ 1 ]	= Draw_PicFromWad( "inv2_sshotgun" );
	g_pPicWeapon[ 1 ][ 2 ]	= Draw_PicFromWad( "inv2_nailgun" );
	g_pPicWeapon[ 1 ][ 3 ]	= Draw_PicFromWad( "inv2_snailgun" );
	g_pPicWeapon[ 1 ][ 4 ]	= Draw_PicFromWad( "inv2_rlaunch" );
	g_pPicWeapon[ 1 ][ 5 ]	= Draw_PicFromWad( "inv2_srlaunch" );
	g_pPicWeapon[ 1 ][ 6 ]	= Draw_PicFromWad( "inv2_lightng" );

	for( qUInt32 i=0; i<5; ++i )
	{
		g_pPicWeapon[ 2 + i ][ 0 ]	= Draw_PicFromWad( va( "inva%d_shotgun",  i + 1 ) );
		g_pPicWeapon[ 2 + i ][ 1 ]	= Draw_PicFromWad( va( "inva%d_sshotgun", i + 1 ) );
		g_pPicWeapon[ 2 + i ][ 2 ]	= Draw_PicFromWad( va( "inva%d_nailgun",  i + 1 ) );
		g_pPicWeapon[ 2 + i ][ 3 ]	= Draw_PicFromWad( va( "inva%d_snailgun", i + 1 ) );
		g_pPicWeapon[ 2 + i ][ 4 ]	= Draw_PicFromWad( va( "inva%d_rlaunch",  i + 1 ) );
		g_pPicWeapon[ 2 + i ][ 5 ]	= Draw_PicFromWad( va( "inva%d_srlaunch", i + 1 ) );
		g_pPicWeapon[ 2 + i ][ 6 ]	= Draw_PicFromWad( va( "inva%d_lightng",  i + 1 ) );
	}

	g_pPicAmmo[ 0 ]			= Draw_PicFromWad( "sb_shells" );
	g_pPicAmmo[ 1 ]			= Draw_PicFromWad( "sb_nails" );
	g_pPicAmmo[ 2 ]			= Draw_PicFromWad( "sb_rocket" );
	g_pPicAmmo[ 3 ]			= Draw_PicFromWad( "sb_cells" );

	g_pPicSigil[ 0 ]		= Draw_PicFromWad( "sb_sigil1" );
	g_pPicSigil[ 1 ]		= Draw_PicFromWad( "sb_sigil2" );
	g_pPicSigil[ 2 ]		= Draw_PicFromWad( "sb_sigil3" );
	g_pPicSigil[ 3 ]		= Draw_PicFromWad( "sb_sigil4" );

	g_pPicArmor[ 0 ]		= Draw_PicFromWad( "sb_armor1" );
	g_pPicArmor[ 1 ]		= Draw_PicFromWad( "sb_armor2" );
	g_pPicArmor[ 2 ]		= Draw_PicFromWad( "sb_armor3" );

	g_pPicItem[ 0 ]			= Draw_PicFromWad( "sb_key1" );
	g_pPicItem[ 1 ]			= Draw_PicFromWad( "sb_key2" );
	g_pPicItem[ 2 ]			= Draw_PicFromWad( "sb_invis" );
	g_pPicItem[ 3 ]			= Draw_PicFromWad( "sb_invuln" );
	g_pPicItem[ 4 ]			= Draw_PicFromWad( "sb_suit" );
	g_pPicItem[ 5 ]			= Draw_PicFromWad( "sb_quad" );

	g_pPicDisc				= Draw_PicFromWad( "disc" );

	g_pPicFace[ 0 ][ 0 ]	= Draw_PicFromWad( "face5" );
	g_pPicFace[ 0 ][ 1 ]	= Draw_PicFromWad( "face_p5" );
	g_pPicFace[ 1 ][ 0 ]	= Draw_PicFromWad( "face4" );
	g_pPicFace[ 1 ][ 1 ]	= Draw_PicFromWad( "face_p4" );
	g_pPicFace[ 2 ][ 0 ]	= Draw_PicFromWad( "face3" );
	g_pPicFace[ 2 ][ 1 ]	= Draw_PicFromWad( "face_p3" );
	g_pPicFace[ 3 ][ 0 ]	= Draw_PicFromWad( "face2" );
	g_pPicFace[ 3 ][ 1 ]	= Draw_PicFromWad( "face_p2" );
	g_pPicFace[ 4 ][ 0 ]	= Draw_PicFromWad( "face1" );
	g_pPicFace[ 4 ][ 1 ]	= Draw_PicFromWad( "face_p1" );

	g_pPicFaceQuad			= Draw_PicFromWad( "face_quad" );
	g_pPicFaceInvis			= Draw_PicFromWad( "face_invis" );
	g_pPicFaceInvul			= Draw_PicFromWad( "face_invul2" );
	g_pPicFaceInvisInvul	= Draw_PicFromWad( "face_inv2" );

	g_pPicComplete			= Draw_CachePic( "gfx/complete.lmp" );
	g_pPicIntermission		= Draw_CachePic( "gfx/inter.lmp" );
	g_pPicFinale			= Draw_CachePic( "gfx/finale.lmp" );
	g_pPicRanking			= Draw_CachePic( "gfx/ranking.lmp" );

	Cmd_AddCommand( "+showscores", ScoresShowCB );
	Cmd_AddCommand( "-showscores", ScoresHideCB );

}	// Init

/*
======
Render
======
*/
void qCSBar::Render( void )
{
	qUInt32	X	= ( qUInt32 )( vid.width * 0.5 - 160 );
	qUInt32	Y	= vid.height - 24;

	if( cl.gametype == GAME_DEATHMATCH )
		X	-= ( ( vid.width - 320 ) >> 1 );

	if( scr_con_current == vid.height )
		return;

	if( g_NumLines > 24 )
	{
		Draw_Pic( X, Y - 24, g_pPicIBar );

		RenderInventory( X, Y );

		if( cl.maxclients != 1 )
			RenderFrags( X, Y );
	}

	if( g_ShowScores || cl.stats[ STAT_HEALTH ] <= 0 )
	{
		Draw_Pic( X, Y, g_pPicScoreBar );

		RenderSoloScoreBoard( X, Y );

		if( cl.gametype == GAME_DEATHMATCH )
			OverlayDeathmatch();
	}
	else if( g_NumLines )
	{
		Draw_Pic( X, Y, g_pPicSBar );

//////////////////////////////////////////////////////////////////////////

		if( cl.items & IT_INVULNERABILITY )
		{
			Draw_Pic( X, Y, g_pPicDisc );

			RenderNumbers( X + 24, Y, 666, 1 );
		}
		else
		{
			if(		 cl.items & IT_ARMOR3 )	Draw_Pic( X, Y, g_pPicArmor[ 2 ] );
			else if( cl.items & IT_ARMOR2 )	Draw_Pic( X, Y, g_pPicArmor[ 1 ] );
			else if (cl.items & IT_ARMOR1 )	Draw_Pic( X, Y, g_pPicArmor[ 0 ] );

			RenderNumbers( X + 24, Y, cl.stats[ STAT_ARMOR ], cl.stats[ STAT_ARMOR ] <= 25 );
		}

//////////////////////////////////////////////////////////////////////////

		if( ( cl.items &
			( IT_INVISIBILITY | IT_INVULNERABILITY) ) ==
			( IT_INVISIBILITY | IT_INVULNERABILITY) )	Draw_Pic( X + 112, Y, g_pPicFaceInvisInvul );
		else if( cl.items & IT_QUAD )					Draw_Pic( X + 112, Y, g_pPicFaceQuad );
		else if( cl.items & IT_INVISIBILITY )			Draw_Pic( X + 112, Y, g_pPicFaceInvis );
		else if( cl.items & IT_INVULNERABILITY )		Draw_Pic( X + 112, Y, g_pPicFaceInvul );
		else
		{
			qUInt32	Face	= 4;
			qUInt32	Anim	= 0;

			if(		 cl.stats[ STAT_HEALTH ] <=  0 )	Face	= 0;
			else if( cl.stats[ STAT_HEALTH ] < 100 )	Face	= cl.stats[ STAT_HEALTH ] / 20;

			if( cl.time <= cl.faceanimtime )
				Anim	= 1;

			Draw_Pic( X + 112, Y, g_pPicFace[ Face ][ Anim ] );
		}

		RenderNumbers( X + 136, Y, cl.stats[ STAT_HEALTH ], cl.stats[ STAT_HEALTH ] <= 25 );

//////////////////////////////////////////////////////////////////////////

		if(		 cl.items & IT_SHELLS )		Draw_Pic( X + 224, Y, g_pPicAmmo[ 0 ] );
		else if( cl.items & IT_NAILS )		Draw_Pic( X + 224, Y, g_pPicAmmo[ 1 ] );
		else if( cl.items & IT_ROCKETS )	Draw_Pic( X + 224, Y, g_pPicAmmo[ 2 ] );
		else if( cl.items & IT_CELLS )		Draw_Pic( X + 224, Y, g_pPicAmmo[ 3 ] );

		RenderNumbers( X + 248, Y, cl.stats[ STAT_AMMO ], cl.stats[ STAT_AMMO ] <= 10 );
	}

	if( vid.width > 320 && cl.gametype == GAME_DEATHMATCH )
		OverlayDeathmatchMini();

}	// Render

/*
===================
OverlayIntermission
===================
*/
void qCSBar::OverlayIntermission( void )
{
	if( cl.gametype == GAME_DEATHMATCH )
	{
		OverlayDeathmatch();
		return;
	}

//////////////////////////////////////////////////////////////////////////

	qUInt32	HalfWidth	= ( qUInt32 )( vid.width  * 0.5 );
	qUInt32	HalfHeight	= ( qUInt32 )( vid.height * 0.5 );
	qUInt32	Minutes		= cl.completed_time / 60;
	qUInt32	Seconds		= cl.completed_time - ( Minutes * 60 );

	Draw_Pic(		HalfWidth -  94, HalfHeight - 96, g_pPicComplete );
	Draw_Pic(		HalfWidth - 160, HalfHeight - 64, g_pPicIntermission );
	RenderNumbers(	HalfWidth +   0, HalfHeight - 56, Minutes, 0 );
	Draw_Pic(		HalfWidth +  74, HalfHeight - 56, g_pPicColon );
	Draw_Pic(		HalfWidth +  86, HalfHeight - 56, g_pPicNum[ 0 ][ Seconds / 10 ] );
	Draw_Pic(		HalfWidth + 106, HalfHeight - 56, g_pPicNum[ 0 ][ Seconds % 10 ] );

	RenderNumbers(	HalfWidth +   0, HalfHeight - 16, cl.stats[ STAT_SECRETS ], 0 );
	Draw_Pic(		HalfWidth +  72, HalfHeight - 16, g_pPicSlash );
	RenderNumbers(	HalfWidth +  80, HalfHeight - 16, cl.stats[ STAT_TOTALSECRETS ], 0 );

	RenderNumbers(	HalfWidth +   0, HalfHeight + 24, cl.stats[ STAT_MONSTERS ], 0 );
	Draw_Pic(		HalfWidth +  72, HalfHeight + 24, g_pPicSlash );
	RenderNumbers(	HalfWidth +  80, HalfHeight + 24, cl.stats[ STAT_TOTALMONSTERS ], 0 );

}	// OverlayIntermission

/*
=============
OverlayFinale
=============
*/
void qCSBar::OverlayFinale (void)
{
	Draw_Pic( ( vid.width - g_pPicFinale->width ) / 2, 16, g_pPicFinale );

}	// OverlayFinale

/*
===========
SetNumLines
===========
*/
void qCSBar::SetNumLines( const qUInt32 NumLines )
{
	g_NumLines	= NumLines;

}	// SetNumLines

/*
===========
GetNumLines
===========
*/
qUInt32 qCSBar::GetNumLines( void )
{
	return g_NumLines;

}	// GetNumLines
