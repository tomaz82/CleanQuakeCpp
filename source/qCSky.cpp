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
#include	"qCSky.h"
#include	"qCMath.h"
#include	"client.h"

#define		GRIDSIZE	32
#define		GRIDSIZE1	( GRIDSIZE + 1 )
#define		GRIDRECIP	( 1.0f / GRIDSIZE )
#define		NUMVERTS	( GRIDSIZE1 * GRIDSIZE1 )
#define		NUMTRIS		( GRIDSIZE * GRIDSIZE * 2 )

static qUInt32	g_TextureSkySolid;
static qUInt32	g_TextureSkyAlpha;

static qFloat	g_Vertices[ NUMVERTS * 3 ];
static qFloat	g_TexCoords[ NUMVERTS * 2 ];
static qUInt32	g_Elements[ NUMTRIS * 3 ];

extern qUInt32	d_8to24table[];

/*
====
Init
====
*/
void qCSky::Init( void )
{
	qFloat*		pVertex		= g_Vertices;
	qFloat*		pTexCoord	= g_TexCoords;
	qUInt32*	pElement	= g_Elements;
	qFloat		DX			= 16.0f;
	qFloat		DY			= 16.0f;
	qFloat		DZ			= 16.0f / 3.0f;

	for( qUInt32 j=0; j<GRIDSIZE1; ++j )
	{
		qFloat	A		= j * GRIDRECIP;
		qFloat	ACos	=  cos( A * Q_PI * 2 );
		qFloat	ASin	= -sin( A * Q_PI * 2 );

		for( qUInt32 i=0; i<GRIDSIZE1; ++i )
		{
			qFloat	B		= i * GRIDRECIP;
			qFloat	BCos	= cos( ( B + 0.5 ) * Q_PI );
			qFloat	Vertex[ 3 ];

			Vertex[ 0 ]	= DX * ACos * BCos;
			Vertex[ 1 ]	= DY * ASin * BCos;
			Vertex[ 2 ]	= DZ * -sin( ( B + 0.5 ) * Q_PI );

			*pVertex++		= Vertex[ 0 ];
			*pVertex++		= Vertex[ 1 ];
			*pVertex++		= Vertex[ 2 ];

//////////////////////////////////////////////////////////////////////////

			qFloat	Length	= 3.0f / sqrt( Vertex[ 0 ] * Vertex[ 0 ] + Vertex[ 1 ] * Vertex[ 1 ] + ( Vertex[ 2 ] * Vertex[ 2 ]*9 ) );

			*pTexCoord++	= Vertex[ 0 ]	* Length;
			*pTexCoord++	= Vertex[ 1 ]	* Length;
		}
	}

	for( qUInt32 j=0; j<GRIDSIZE; ++j )
	{
		for( qUInt32 i=0; i<GRIDSIZE; ++i )
		{
			*pElement++	=   j		* GRIDSIZE1 + i;
			*pElement++	=   j		* GRIDSIZE1 + i + 1;
			*pElement++	= ( j + 1 )	* GRIDSIZE1 + i;

			*pElement++	=   j		* GRIDSIZE1 + i + 1;
			*pElement++	= ( j + 1 )	* GRIDSIZE1 + i + 1;
			*pElement++	= ( j + 1 )	* GRIDSIZE1 + i;
		}
	}

}	// Init

/*
============
LoadTextures
============
*/
void qCSky::LoadTextures( const qSTexture* pTexture )
{
	qUInt8*	pSource	= ( qUInt8* )pTexture + pTexture->Offsets[ 0 ];
	qUInt32	Pixels[ 128 * 128 ];
	qUInt32	Red		= 0;
	qUInt32	Green	= 0;
	qUInt32	Blue	= 0;

	// make an average value for the back to avoid a fringe on the top level

	for( qUInt32 i=0; i<128 ; ++i )
	{
		for( qUInt32 j=0; j<128; ++j )
		{
			qUInt32		Pixel	= pSource[ i * 256 + j + 128 ];
			qUInt32*	pPixel	= &d_8to24table[ Pixel ];

			Pixels[ ( i * 128 ) + j ]	= *pPixel;

			Red		+= ( ( qUInt8* )pPixel )[ 0 ];
			Green	+= ( ( qUInt8* )pPixel )[ 1 ];
			Blue	+= ( ( qUInt8* )pPixel )[ 2 ];
		}
	}

	if( !g_TextureSkySolid )
		g_TextureSkySolid	= texture_extension_number++;

	GL_Bind( g_TextureSkySolid );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

//////////////////////////////////////////////////////////////////////////

	qUInt32	TransPixel;

	( ( qUInt8* )&TransPixel )[ 0 ]	= Red	/ ( 128 * 128 );
	( ( qUInt8* )&TransPixel )[ 1 ]	= Green	/ ( 128 * 128 );
	( ( qUInt8* )&TransPixel )[ 2 ]	= Blue	/ ( 128 * 128 );
	( ( qUInt8* )&TransPixel )[ 3 ]	= 0;

	for( qUInt32 i=0; i<128; ++i )
	{
		for( qUInt32 j=0; j<128; ++j )
		{
			qUInt32		Pixel	= pSource[ i * 256 + j ];

			if( Pixel == 0 )	Pixels[ ( i * 128 ) + j ]	= TransPixel;
			else				Pixels[ ( i * 128 ) + j ]	= d_8to24table[ Pixel ];
		}
	}

	if( !g_TextureSkyAlpha )
		g_TextureSkyAlpha	= texture_extension_number++;

	GL_Bind( g_TextureSkyAlpha );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

}	// LoadTextures

/*
======
Render
======
*/
void qCSky::Render( void )
{
	qDouble	SpeedScale	= cl.time / 128.0 *  8.0;
	qFloat	Matrix[ 16 ];

	SpeedScale	-= floor( SpeedScale );

	glGetFloatv( GL_MODELVIEW_MATRIX, Matrix );

	Matrix[ 12 ]	=
	Matrix[ 13 ]	=
	Matrix[ 14 ]	= 0.0f;

	glPushMatrix();
	glLoadMatrixf( Matrix );

//////////////////////////////////////////////////////////////////////////

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer(	3, GL_FLOAT, sizeof( qFloat ) * 3, g_Vertices );
	glTexCoordPointer(	2, GL_FLOAT, sizeof( qFloat ) * 2, g_TexCoords );

	glDepthMask( FALSE );

	glMatrixMode( GL_TEXTURE );
	glLoadIdentity();

//////////////////////////////////////////////////////////////////////////

	glTranslatef( SpeedScale, SpeedScale, 0.0f );

	GL_Bind( g_TextureSkySolid );

	glDrawElements( GL_TRIANGLES, NUMTRIS * 3, GL_UNSIGNED_INT, g_Elements );

//////////////////////////////////////////////////////////////////////////

	glTranslatef( SpeedScale, SpeedScale, 0.0f );

	GL_Bind( g_TextureSkyAlpha );

	glEnable( GL_BLEND );
	glDrawElements( GL_TRIANGLES, NUMTRIS * 3, GL_UNSIGNED_INT, g_Elements );
	glDisable( GL_BLEND );

//////////////////////////////////////////////////////////////////////////

	glLoadIdentity();

	glDepthMask( TRUE );

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glDisableClientState( GL_VERTEX_ARRAY );

}	// Render
