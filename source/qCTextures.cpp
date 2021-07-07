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
#include	"qCTextures.h"
#include	"qCHunk.h"
#include	"glquake.h"
#include	"client.h"
#include	"console.h"
#include	"sys.h"

typedef struct qSGLTexture
{
	qChar	Identifier[ 64 ];
	qUInt32	CheckSum;
	qUInt32	TexNum;
	qUInt32	Width;
	qUInt32	Height;
	qUInt8	BytesPerPixel;
	qBool	Mipmap;
} qSGLTexture;

#define	MAX_GLTEXTURES	1024

static qSCVar		g_CVarMaxTextureSize		= { "gl_max_size", "1024" };
static qSGLTexture	g_GLTextures[ MAX_GLTEXTURES ];
static qSTexture*	g_pCheckerTextureData		= NULL;
static qUInt32		g_PlayerTextureIndex		= 0;
static qUInt32		g_ParticleTextureIndex		= 0;
static qUInt32		g_NumGLTextures				= 0;
static qUInt8		g_DotTextureData[ 8 ][ 8 ]	=
{
	{   0, 255, 255,   0,   0,   0,   0,   0 },
	{ 255, 255, 255, 255,   0,   0,   0,   0 },
	{ 255, 255, 255, 255,   0,   0,   0,   0 },
	{   0, 255, 255,   0,   0,   0,   0,   0 },
	{   0,   0,   0,   0,   0,   0,   0,   0 },
	{   0,   0,   0,   0,   0,   0,   0,   0 },
	{   0,   0,   0,   0,   0,   0,   0,   0 },
	{   0,   0,   0,   0,   0,   0,   0,   0 },
};

//////////////////////////////////////////////////////////////////////////

/*
================
ResampleLerpLine
================
*/
static void ResampleLerpLine( const qUInt8* pDataIn, qUInt8* pDataOut, const qUInt32 WidthIn, qUInt32 WidthOut )
{
	const qUInt32	Step	= ( qUInt32 )( WidthIn * 65536.0f / WidthOut );
	const qUInt32	EndX	= WidthIn - 1;
	qUInt32			OldX	= 0;
	qUInt32			F		= 0;

	for( qUInt32 i=0; i<WidthOut; ++i )
	{
		const qUInt32	X	= F >> 16;

		if( X != OldX )
		{
			pDataIn	+= ( X - OldX ) * 4;
			OldX	 = X;
		}

		if( X < EndX )
		{
			const qUInt32	Lerp	= F & 0xFFFF;

			*pDataOut++	= ( qUInt8 )( ( ( ( pDataIn[ 4 ] - pDataIn[ 0 ] ) * Lerp ) >> 16 ) + pDataIn[ 0 ] );
			*pDataOut++	= ( qUInt8 )( ( ( ( pDataIn[ 5 ] - pDataIn[ 1 ] ) * Lerp ) >> 16 ) + pDataIn[ 1 ] );
			*pDataOut++	= ( qUInt8 )( ( ( ( pDataIn[ 6 ] - pDataIn[ 2 ] ) * Lerp ) >> 16 ) + pDataIn[ 2 ] );
			*pDataOut++	= ( qUInt8 )( ( ( ( pDataIn[ 7 ] - pDataIn[ 3 ] ) * Lerp ) >> 16 ) + pDataIn[ 3 ] );
		}
		else
		{
			*pDataOut++	= pDataIn[ 0 ];
			*pDataOut++	= pDataIn[ 1 ];
			*pDataOut++	= pDataIn[ 2 ];
			*pDataOut++	= pDataIn[ 3 ];
		}

		F	+= Step;
	}

}	// ResampleLerpLine

/*
========
Resample
========
*/
static void Resample( const qUInt8* pDataIn, const qUInt32 WidthIn, const qUInt32 HeightIn, qUInt8* pDataOut, const qUInt32 WidthOut, const qUInt32 HeightOut )
{
	qUInt8*	pRowIn	= ( qUInt8* )pDataIn;
	qUInt8*	pRow1	= ( qUInt8* )malloc( WidthOut * 4 );
	qUInt8*	pRow2	= ( qUInt8* )malloc( WidthOut * 4 );

	const qUInt32	Step	= ( qUInt32 )( HeightIn * 65536.0f / HeightOut );
	const qUInt32	EndY	= HeightIn - 1;
	qUInt32			OldY	= 0;
	qUInt32			F		= 0;

	ResampleLerpLine( pRowIn,				pRow1, WidthIn, WidthOut );
	ResampleLerpLine( pRowIn + WidthIn * 4,	pRow2, WidthIn, WidthOut );

	for( qUInt32 i=0; i<HeightOut; ++i )
	{
		const qUInt32	Y	= F >> 16;

		if( Y < EndY )
		{
			const qUInt32	Lerp	= F & 0xFFFF;

			if( Y != OldY )
			{
				pRowIn	= ( qUInt8* )pDataIn + WidthIn * 4 * Y;

				if( Y == OldY + 1 )	memcpy( pRow1, pRow2, WidthOut * 4 );
				else				ResampleLerpLine( pRowIn, pRow1, WidthIn, WidthOut );

				ResampleLerpLine( pRowIn + WidthIn * 4, pRow2, WidthIn, WidthOut );
				OldY	= Y;
			}

//////////////////////////////////////////////////////////////////////////

			qSInt32	j	= WidthOut - 4;

			while( j >= 0 )
			{
				pDataOut[  0 ]	= ( qUInt8 )( ( ( ( pRow2[  0 ] - pRow1[  0 ] ) * Lerp ) >> 16 ) + pRow1[  0 ] );
				pDataOut[  1 ]	= ( qUInt8 )( ( ( ( pRow2[  1 ] - pRow1[  1 ] ) * Lerp ) >> 16 ) + pRow1[  1 ] );
				pDataOut[  2 ]	= ( qUInt8 )( ( ( ( pRow2[  2 ] - pRow1[  2 ] ) * Lerp ) >> 16 ) + pRow1[  2 ] );
				pDataOut[  3 ]	= ( qUInt8 )( ( ( ( pRow2[  3 ] - pRow1[  3 ] ) * Lerp ) >> 16 ) + pRow1[  3 ] );
				pDataOut[  4 ]	= ( qUInt8 )( ( ( ( pRow2[  4 ] - pRow1[  4 ] ) * Lerp ) >> 16 ) + pRow1[  4 ] );
				pDataOut[  5 ]	= ( qUInt8 )( ( ( ( pRow2[  5 ] - pRow1[  5 ] ) * Lerp ) >> 16 ) + pRow1[  5 ] );
				pDataOut[  6 ]	= ( qUInt8 )( ( ( ( pRow2[  6 ] - pRow1[  6 ] ) * Lerp ) >> 16 ) + pRow1[  6 ] );
				pDataOut[  7 ]	= ( qUInt8 )( ( ( ( pRow2[  7 ] - pRow1[  7 ] ) * Lerp ) >> 16 ) + pRow1[  7 ] );
				pDataOut[  8 ]	= ( qUInt8 )( ( ( ( pRow2[  8 ] - pRow1[  8 ] ) * Lerp ) >> 16 ) + pRow1[  8 ] );
				pDataOut[  9 ]	= ( qUInt8 )( ( ( ( pRow2[  9 ] - pRow1[  9 ] ) * Lerp ) >> 16 ) + pRow1[  9 ] );
				pDataOut[ 10 ]	= ( qUInt8 )( ( ( ( pRow2[ 10 ] - pRow1[ 10 ] ) * Lerp ) >> 16 ) + pRow1[ 10 ] );
				pDataOut[ 11 ]	= ( qUInt8 )( ( ( ( pRow2[ 11 ] - pRow1[ 11 ] ) * Lerp ) >> 16 ) + pRow1[ 11 ] );
				pDataOut[ 12 ]	= ( qUInt8 )( ( ( ( pRow2[ 12 ] - pRow1[ 12 ] ) * Lerp ) >> 16 ) + pRow1[ 12 ] );
				pDataOut[ 13 ]	= ( qUInt8 )( ( ( ( pRow2[ 13 ] - pRow1[ 13 ] ) * Lerp ) >> 16 ) + pRow1[ 13 ] );
				pDataOut[ 14 ]	= ( qUInt8 )( ( ( ( pRow2[ 14 ] - pRow1[ 14 ] ) * Lerp ) >> 16 ) + pRow1[ 14 ] );
				pDataOut[ 15 ]	= ( qUInt8 )( ( ( ( pRow2[ 15 ] - pRow1[ 15 ] ) * Lerp ) >> 16 ) + pRow1[ 15 ] );

				pDataOut	+= 16;
				pRow1		+= 16;
				pRow2		+= 16;
				j			-= 4;
			}

			if( j & 2 )
			{
				pDataOut[  0 ]	= ( qUInt8 )( ( ( ( pRow2[  0 ] - pRow1[  0 ] ) * Lerp ) >> 16 ) + pRow1[  0 ] );
				pDataOut[  1 ]	= ( qUInt8 )( ( ( ( pRow2[  1 ] - pRow1[  1 ] ) * Lerp ) >> 16 ) + pRow1[  1 ] );
				pDataOut[  2 ]	= ( qUInt8 )( ( ( ( pRow2[  2 ] - pRow1[  2 ] ) * Lerp ) >> 16 ) + pRow1[  2 ] );
				pDataOut[  3 ]	= ( qUInt8 )( ( ( ( pRow2[  3 ] - pRow1[  3 ] ) * Lerp ) >> 16 ) + pRow1[  3 ] );
				pDataOut[  4 ]	= ( qUInt8 )( ( ( ( pRow2[  4 ] - pRow1[  4 ] ) * Lerp ) >> 16 ) + pRow1[  4 ] );
				pDataOut[  5 ]	= ( qUInt8 )( ( ( ( pRow2[  5 ] - pRow1[  5 ] ) * Lerp ) >> 16 ) + pRow1[  5 ] );
				pDataOut[  6 ]	= ( qUInt8 )( ( ( ( pRow2[  6 ] - pRow1[  6 ] ) * Lerp ) >> 16 ) + pRow1[  6 ] );
				pDataOut[  7 ]	= ( qUInt8 )( ( ( ( pRow2[  7 ] - pRow1[  7 ] ) * Lerp ) >> 16 ) + pRow1[  7 ] );

				pDataOut	+= 8;
				pRow1		+= 8;
				pRow2		+= 8;
			}

			if( j & 1 )
			{
				pDataOut[  0 ]	= ( qUInt8 )( ( ( ( pRow2[  0 ] - pRow1[  0 ] ) * Lerp ) >> 16 ) + pRow1[  0 ] );
				pDataOut[  1 ]	= ( qUInt8 )( ( ( ( pRow2[  1 ] - pRow1[  1 ] ) * Lerp ) >> 16 ) + pRow1[  1 ] );
				pDataOut[  2 ]	= ( qUInt8 )( ( ( ( pRow2[  2 ] - pRow1[  2 ] ) * Lerp ) >> 16 ) + pRow1[  2 ] );
				pDataOut[  3 ]	= ( qUInt8 )( ( ( ( pRow2[  3 ] - pRow1[  3 ] ) * Lerp ) >> 16 ) + pRow1[  3 ] );

				pDataOut	+= 4;
				pRow1		+= 4;
				pRow2		+= 4;
			}

			pRow1	-= WidthOut * 4;
			pRow2	-= WidthOut * 4;
		}
		else
		{
			if( Y != OldY )
			{
				pRowIn	= ( qUInt8* )pDataIn + WidthIn * 4 * Y;

				if( Y == OldY + 1 )	memcpy( pRow1, pRow2, WidthOut * 4 );
				else				ResampleLerpLine( pRowIn, pRow1, WidthIn, WidthOut );

				OldY	= Y;
			}

			memcpy( pDataOut, pRow1, WidthOut * 4 );
		}

		F	+= Step;
	}

	free( pRow1 );
	free( pRow2 );

}	// Resample

/*
======
Mipmap
======
*/
static void Mipmap( const qUInt8* pDataIn, qUInt32 Width, qUInt32 Height )
{
	qUInt8*	pDataOut	= ( qUInt8* )pDataIn;

	Width	<<= 2;
	Height	>>= 1;

	for( qUInt32 i=0; i<Height; ++i, pDataIn+=Width )
	{
		for( qUInt32 j=0; j<Width; j+=8, pDataOut+=4, pDataIn+=8 )
		{
			pDataOut[ 0 ]	= ( pDataIn[ 0 ] + pDataIn[ 4 ] + pDataIn[ Width + 0 ] + pDataIn[ Width + 4 ] ) >> 2;
			pDataOut[ 1 ]	= ( pDataIn[ 1 ] + pDataIn[ 5 ] + pDataIn[ Width + 1 ] + pDataIn[ Width + 5 ] ) >> 2;
			pDataOut[ 2 ]	= ( pDataIn[ 2 ] + pDataIn[ 6 ] + pDataIn[ Width + 2 ] + pDataIn[ Width + 6 ] ) >> 2;
			pDataOut[ 3 ]	= ( pDataIn[ 3 ] + pDataIn[ 7 ] + pDataIn[ Width + 3 ] + pDataIn[ Width + 7 ] ) >> 2;
		}
	}

}	// Mipmap

/*
========
Upload32
========
*/
static void Upload32( const qUInt8* pData, const qUInt32 Width, const qUInt32 Height, const qBool CreateMipmaps )
{
	qUInt8*	pScaled;
	qUInt32	ScaledWidth;
	qUInt32	ScaledHeight;

	for( ScaledWidth  = 2; ScaledWidth  < Width;  ScaledWidth  <<= 1 );
	for( ScaledHeight = 2; ScaledHeight < Height; ScaledHeight <<= 1 );

	if( ScaledWidth  > g_CVarMaxTextureSize.Value )	ScaledWidth  = g_CVarMaxTextureSize.Value;
	if( ScaledHeight > g_CVarMaxTextureSize.Value )	ScaledHeight = g_CVarMaxTextureSize.Value;

	pScaled	= ( qUInt8* )malloc( ScaledWidth * ScaledHeight * 4 );

//////////////////////////////////////////////////////////////////////////

	if( ScaledWidth != Width || ScaledHeight != Height )	Resample( pData, Width, Height, pScaled, ScaledWidth, ScaledHeight );
	else													memcpy( pScaled, pData, Width * Height * 4 );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, ScaledWidth, ScaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pScaled );

	if( CreateMipmaps )
	{
		qUInt32	Miplevel	= 0;

		while( ScaledWidth > 1 || ScaledHeight > 1 )
		{
			Mipmap( pScaled, ScaledWidth, ScaledHeight );

			ScaledWidth  >>= 1;
			ScaledHeight >>= 1;

			if( ScaledWidth  < 1 )	ScaledWidth		= 1;
			if( ScaledHeight < 1 )	ScaledHeight	= 1;

			Miplevel++;

			glTexImage2D( GL_TEXTURE_2D, Miplevel, GL_RGBA, ScaledWidth, ScaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pScaled );
		}

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	free( pScaled );

}	// Upload32

/*
=======
Upload8
=======
*/
static void Upload8( const qUInt8* pData, const qUInt32 Width, const qUInt32 Height, const qBool CreateMipmaps )
{
	qUInt32*	pTrans	= ( qUInt32* )malloc( Width * Height * 4 );

	for( qUInt32 i=0; i<Width*Height; ++i )
		pTrans[ i ]	= d_8to24table[ pData[ i ] ];

	Upload32( ( qUInt8* )pTrans, Width, Height, CreateMipmaps );

	free( pTrans );

}	// Upload8

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCTextures::Init( void )
{
	// create a simple checkerboard texture for the default
	g_pCheckerTextureData				= ( qSTexture* )qCHunk::AllocName( sizeof( qSTexture ) + ( 16 * 16 ) + ( 8 * 8 ) + ( 4 * 4 ) + ( 2 * 2 ), "notexture" );
	g_pCheckerTextureData->Width		=
	g_pCheckerTextureData->Height		= 16;
	g_pCheckerTextureData->Offsets[ 0 ]	= sizeof( qSTexture );
	g_pCheckerTextureData->Offsets[ 1 ]	= g_pCheckerTextureData->Offsets[ 0 ] + ( 16 * 16 );
	g_pCheckerTextureData->Offsets[ 2 ]	= g_pCheckerTextureData->Offsets[ 1 ] + (  8 *  8 );
	g_pCheckerTextureData->Offsets[ 3 ]	= g_pCheckerTextureData->Offsets[ 2 ] + (  4 *  4 );

	for( qUInt8 MipIndex=0; MipIndex<4; ++MipIndex )
	{
		qUInt8*	pDest	= ( qUInt8* )g_pCheckerTextureData + g_pCheckerTextureData->Offsets[ MipIndex ];

		for( qSInt32 y=0; y<( 16 >> MipIndex ); ++y )
		{
			for( qSInt32 x=0; x<( 16 >> MipIndex ); ++x )
			{
				if( ( y<( 8 >> MipIndex ) ) ^ ( x<( 8 >> MipIndex ) ) )	*pDest++	= 0;
				else													*pDest++	= 0xff;
			}
		}
	}

//////////////////////////////////////////////////////////////////////////

	// particle texture
	qUInt8	TextureData[ 8 ][ 8 ][ 4 ];

	for( qUInt32 x=0; x<8; ++x )
	{
		for( qUInt32 y=0; y<8; ++y )
		{
			TextureData[ y ][ x ][ 0 ]	= 255;
			TextureData[ y ][ x ][ 1 ]	= 255;
			TextureData[ y ][ x ][ 2 ]	= 255;
			TextureData[ y ][ x ][ 3 ]	= g_DotTextureData[ x ][ y ];
		}
	}

	g_ParticleTextureIndex	= texture_extension_number++;
	GL_Bind( g_ParticleTextureIndex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, TextureData );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

//////////////////////////////////////////////////////////////////////////

	g_PlayerTextureIndex		 = texture_extension_number;
	texture_extension_number	+= 16;

//////////////////////////////////////////////////////////////////////////

	qCCVar::Register( &g_CVarMaxTextureSize );

}	// Init

/*
===================
TranslatePlayerSkin
===================
*/
void qCTextures::TranslatePlayerSkin( const qUInt32 PlayerNum )
{
	model_t*	pModel;
	aliashdr_t*	pAliasHeader;
	qUInt32*	pPixel;
	qUInt8*		pInRow;
	qUInt8*		pOriginal;
	qUInt32		Pixels[ 512 * 256 ];
	qUInt32		Translate32[ 256 ];
	qUInt8		Translate[ 256 ];
	qUInt32		Top		= ( cl.scores[ PlayerNum ].colors & 0xF0 );
	qUInt32		Bottom	= ( cl.scores[ PlayerNum ].colors & 15 ) << 4;
	qUInt32		Size;
	qUInt32		ScaledWidth;
	qUInt32		ScaledHeight;
	qUInt32		Fraction;
	qUInt32		FractionStep;

	for( qUInt32 i=0; i<256; ++i )
		Translate[ i ]	= i;

	for( qUInt8 i=0; i<16; ++i )
	{
			// the artists made some backwards ranges.  sigh.
		if( Top < 128 )		Translate[ TOP_RANGE + i ]		= Top + i;
		else				Translate[ TOP_RANGE + i ]		= Top + 15 - i;

		if( Bottom < 128 )	Translate[ BOTTOM_RANGE + i]	= Bottom + i;
		else				Translate[ BOTTOM_RANGE + i]	= Bottom + 15 - i;
	}

	// locate the original skin pixels
	currententity	= &cl_entities[ PlayerNum + 1 ];

	// player doesn't have a model yet
	if( !( pModel = currententity->model ) )
		return;

	// only translate skins on alias models
	if( pModel->type != mod_alias )
		return;

	pAliasHeader	= ( aliashdr_t* )Mod_Extradata( pModel );
	Size			= pAliasHeader->skinwidth * pAliasHeader->skinheight;

	if( Size & 3 )
		Sys_Error( "qCTextures::TranslatePlayerSkin: Size & 3" );

	if( currententity->skinnum < 0 || currententity->skinnum >= pAliasHeader->numskins )
	{
		Con_Printf( "( %d ): Invalid player skin #%d\n", PlayerNum, currententity->skinnum );
		pOriginal	= ( qUInt8* )pAliasHeader + pAliasHeader->texels[ 0 ];
	}
	else
	{
		pOriginal	= ( qUInt8* )pAliasHeader + pAliasHeader->texels[ currententity->skinnum ];
	}

	ScaledWidth		= g_CVarMaxTextureSize.Value < 512 ? g_CVarMaxTextureSize.Value : 512;
	ScaledHeight	= g_CVarMaxTextureSize.Value < 256 ? g_CVarMaxTextureSize.Value : 256;

	for( qUInt32 i=0; i<256; ++i )
		Translate32[ i ]	= d_8to24table[ Translate[ i ] ];

	pPixel	= Pixels;

	FractionStep	= pAliasHeader->skinwidth * 0x10000 / ScaledWidth;

	for( qUInt32 i=0 ;i<ScaledHeight; ++i, pPixel += ScaledWidth )
	{
		pInRow		= pOriginal + pAliasHeader->skinwidth * ( i * pAliasHeader->skinheight / ScaledHeight );
		Fraction	= FractionStep >> 1;

		for( qUInt32 j=0; j<ScaledWidth; j+=4 )
		{
			pPixel[ j + 0 ]	= Translate32[ pInRow[ Fraction >> 16 ] ];	Fraction	+= FractionStep;
			pPixel[ j + 1 ]	= Translate32[ pInRow[ Fraction >> 16 ] ];	Fraction	+= FractionStep;
			pPixel[ j + 2 ]	= Translate32[ pInRow[ Fraction >> 16 ] ];	Fraction	+= FractionStep;
			pPixel[ j + 3 ]	= Translate32[ pInRow[ Fraction >> 16 ] ];	Fraction	+= FractionStep;
		}
	}

	GL_DisableMultitexture();

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8

	GL_Bind( g_PlayerTextureIndex + PlayerNum );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, ScaledWidth, ScaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

}	// TranslatePlayerSkin

/*
=========
Animation
=========
*/
qSTexture* qCTextures::Animation( qSTexture* pBaseTexture )
{
	if( currententity->frame )
	{
		if( pBaseTexture->pAlternateAnims )
			pBaseTexture	= pBaseTexture->pAlternateAnims;
	}

	if( !pBaseTexture->AnimTotal )
		return pBaseTexture;

	qUInt32	Reletive	= ( qUInt32 )( cl.time * 10 ) % pBaseTexture->AnimTotal;
	qUInt32	Count		= 0;

	while( pBaseTexture->AnimMin  > Reletive ||
		   pBaseTexture->AnimMax <= Reletive )
	{
		pBaseTexture	= pBaseTexture->pAnimNext;

		if( !pBaseTexture )	Sys_Error( "qCTextures::Animation: broken cycle" );
		if( ++Count > 100 )	Sys_Error( "qCTextures::Animation: infinite cycle" );
	}

	return pBaseTexture;

}	// Animation

/*
====
Load
====
*/
qUInt32 qCTextures::Load( const qChar* pIdentifier, const qUInt32 Width, const qUInt32 Height, const qUInt8* pData, const qBool CreateMipmaps, const qUInt8 BytesPerPixel )
{
	const qUInt32	Size		= Width * Height * BytesPerPixel;
	qSGLTexture*	pTexture	= g_GLTextures;
	qUInt32			CheckSumTable[ 256 ];
	qUInt32			CheckSum	= 0;

	for( qUInt32 i=0; i<256; ++i )	CheckSumTable[ i ]	 = i + 1;
	for( qUInt32 i=0; i<Size; ++i )	CheckSum			+= ( CheckSumTable[ pData[ i ] & 255 ]++ );

	// See if the texture is already present
	if( pIdentifier[ 0 ] )
	{
		for( qUInt32 i=0; i<g_NumGLTextures; ++i, ++pTexture )
		{
			if (!strcmp( pIdentifier, pTexture->Identifier ) )
			{
				if( CheckSum != pTexture->CheckSum || Width != pTexture->Width || Height != pTexture->Height )
				{
					Con_DPrintf( "GL_LoadTexture: cache mismatch\n" );
					goto Setup;
				}

				return pTexture->TexNum;
			}
		}
	}

	pTexture			= &g_GLTextures[ g_NumGLTextures ];
	pTexture->TexNum	= texture_extension_number;
	g_NumGLTextures++;
	texture_extension_number++;

Setup:

	snprintf( pTexture->Identifier, sizeof( pTexture->Identifier ), "%s", pIdentifier );

	pTexture->CheckSum		= CheckSum;
	pTexture->Width			= Width;
	pTexture->Height		= Height;
	pTexture->BytesPerPixel	= BytesPerPixel;
	pTexture->Mipmap		= CreateMipmaps;

	GL_Bind( pTexture->TexNum );

	if(		 BytesPerPixel == 1 )	Upload8(  pData, Width, Height, CreateMipmaps );
	else if( BytesPerPixel == 4 )	Upload32( pData, Width, Height, CreateMipmaps );
	else							Sys_Error( "GL_LoadTexture: unknown bytesperpixel\n" );

	return pTexture->TexNum;

}	// Load

/*
=====================
GetCheckerTextureData
=====================
*/
qSTexture* qCTextures::GetCheckerTextureData( void )
{
	return g_pCheckerTextureData;

}	// GetCheckerTextureData

/*
=======================
GetParticleTextureIndex
=======================
*/
qUInt32 qCTextures::GetParticleTextureIndex( void )
{
	return g_ParticleTextureIndex;

}	// GetParticleTextureIndex

/*
=====================
GetPlayerTextureIndex
=====================
*/
qUInt32 qCTextures::GetPlayerTextureIndex( void )
{
	return g_PlayerTextureIndex;

}	// GetPlayerTextureIndex
