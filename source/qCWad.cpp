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
#include	"qCWad.h"

#include	"sys.h"
#include	"qCEndian.h"

typedef struct qSLumpInfo
{
	qUInt32		FilePosition;
	qUInt32		DiskSize;
	qUInt32		Size;
	qChar		Type;
	qChar		Compression;
	qChar		Pad1;
	qChar		Pad2;
	qChar		Name[ 16 ];
} qSLumpInfo;

typedef struct qSWadInfo
{
	qChar	Id[ 4 ];
	qUInt32	NumLumps;
	qUInt32	InfoTableOffset;
} qSWadInfo;

static qSLumpInfo*	g_pLumps;
static qUInt8*		g_pLumpBase;
static qUInt32		g_NumLumps;

//////////////////////////////////////////////////////////////////////////

/*
===========
CleanupName
===========
*/
static void CleanupName( const qChar* pIn, qChar* pOut )
{
	for( qUInt8 i=0; i<16; ++i )
	{
		qChar	Char	= pIn[ i ];

		if( Char )
		{
			if( Char >= 'A' && Char <= 'Z' )
				Char += ( 'a' - 'A' );
		}

		pOut[ i ]	= Char;
	}

}	// CleanupName

/*
===========
GetLumpInfo
===========
*/
static qSLumpInfo* GetLumpInfo( const qChar* pName )
{
	qSLumpInfo*	pLump	= g_pLumps;
	char		Clean[ 16 ];

	CleanupName( pName, Clean );

	for( qUInt32 i=0; i<g_NumLumps; ++i )
	{
		if(! strcmp( Clean, pLump->Name ) )
			return pLump;

		++pLump;
	}

	Sys_Error( "GetLumpInfo: %s not found", pName );

	return NULL;

}	// GetLumpInfo

//////////////////////////////////////////////////////////////////////////

/*
========
LoadFile
========
*/
void qCWad::LoadFile( const qChar* pFileName )
{
	qSLumpInfo*	pLump;
	qSWadInfo*	pHeader;
	qUInt32		InfoTableOffset;

	if( ( g_pLumpBase = COM_LoadHunkFile( pFileName ) ) == NULL )
		Sys_Error( "qCWad::LoadFile: Couldn't load %s", pFileName );

	pHeader	= ( qSWadInfo* )g_pLumpBase;

	if( pHeader->Id[ 0 ] != 'W'	||
		pHeader->Id[ 1 ] != 'A'	||
		pHeader->Id[ 2 ] != 'D'	||
		pHeader->Id[ 3 ] != '2')
		Sys_Error( "qCWad::LoadFile: Wad file %s doesn't have WAD2 id\n", pFileName );

	InfoTableOffset	= qCEndian::LittleInt32( pHeader->InfoTableOffset );
	g_NumLumps		= qCEndian::LittleInt32( pHeader->NumLumps );
	g_pLumps		= ( qSLumpInfo* )( g_pLumpBase + InfoTableOffset );
	pLump			= g_pLumps;

	for( qUInt32 i=0; i<g_NumLumps; ++i )
	{
		pLump->FilePosition	= qCEndian::LittleInt32( pLump->FilePosition );
		pLump->Size			= qCEndian::LittleInt32( pLump->Size );
		CleanupName( pLump->Name, pLump->Name );

		if( pLump->Type == 66 )	// TYP_QPIC
			SwapPic( ( qpic_t* )( g_pLumpBase + pLump->FilePosition ) );

		++pLump;
	}

}	// LoadFile

/*
=======
GetLump
=======
*/
void* qCWad::GetLump( const qChar* pName )
{
	qSLumpInfo*	pLump	=  GetLumpInfo( pName );

	return ( void* )( g_pLumpBase + pLump->FilePosition );

}	// GetLump

/*
=======
SwapPic
=======
*/
void qCWad::SwapPic( qpic_t* pPic )
{
	pPic->width		= qCEndian::LittleInt32( pPic->width );
	pPic->height	= qCEndian::LittleInt32( pPic->height );

}	// SwapPic
