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
#include	"qCHunk.h"
#include	"qCCache.h"
#include	"sys.h"
#include	"console.h"

#define		HUNK_SENTINAL	0x1DF001ED

typedef struct qSHunk
{
	qUInt32	Sentinal;
	qSInt32	Size;	// including sizeof( qSHunk ), -1 = not allocated
	qChar	Name[ 8 ];
} qSHunk;

qUInt8*	g_pBase;

qUInt32	g_Size;
qUInt32	g_LowMark;
qUInt32	g_HighMark;
qUInt32	g_TempMark;

qBool	g_TempActive;

/*
====
Init
====
*/
void qCHunk::Init( const void* pBuffer, const qUInt32 Size )
{
	g_pBase		= ( qUInt8* )pBuffer;
	g_Size		= Size;
	g_LowMark	= 0;
	g_HighMark	= 0;

}	// Init

/*
=========
AllocName
=========
*/
void* qCHunk::AllocName( qUInt32 Size, const qChar* pName )
{
	qSHunk*	pHunk;

	Size	= sizeof( qSHunk ) + ( ( Size + 15 ) &~ 15 );

	if( g_Size - g_LowMark - g_HighMark < Size )
		Sys_Error( "qCHunk::AllocName: failed on %d bytes", Size );

	pHunk		 = ( qSHunk* )( g_pBase + g_LowMark );
	g_LowMark	+= Size;

	qCCache::FreeLow( g_LowMark );

	memset( pHunk, 0, Size );

	pHunk->Size		= Size;
	pHunk->Sentinal	= HUNK_SENTINAL;

	strncpy( pHunk->Name, pName, 8 );

	return ( void* )( pHunk + 1 );

}	// AllocName

/*
=====
Alloc
=====
*/
void* qCHunk::Alloc( const qUInt32 Size )
{
	return AllocName( Size, "unknown" );

}	// Alloc

/*
=============
HighAllocName
=============
*/
void* qCHunk::HighAllocName( qUInt32 Size, const qChar* pName )
{
	qSHunk*	pHunk;

	if( g_TempActive )
	{
		FreeToHighMark( g_TempMark );
		g_TempActive	= FALSE;
	}

	Size	= sizeof( qSHunk ) + ( ( Size + 15 ) &~ 15 );

	if( g_Size - g_LowMark - g_HighMark < Size )
	{
		Con_Printf( "qCHunk::HighAllocName: failed on %d bytes\n", Size );
		return NULL;
	}

	g_HighMark	+= Size;

	qCCache::FreeHigh( g_HighMark );

	pHunk	= ( qSHunk* )( g_pBase + g_Size - g_HighMark );

	memset( pHunk, 0, Size );

	pHunk->Size		= Size;
	pHunk->Sentinal	= HUNK_SENTINAL;

	strncpy( pHunk->Name, pName, 8 );

	return ( void* )( pHunk + 1 );

}	// HighAllocName

/*
=========
TempAlloc
=========
*/
void* qCHunk::TempAlloc( qUInt32 Size )
{
	void*	pMemory;

	Size	= ( Size + 15 ) &~ 15;

	if( g_TempActive )
	{
		FreeToHighMark( g_TempMark );
		g_TempActive	= FALSE;
	}

	g_TempMark		= g_HighMark;
	pMemory			= HighAllocName( Size, "temp" );
	g_TempActive	= TRUE;

	return pMemory;

}	// TempAlloc

/*
=============
FreeToLowMark
=============
*/
void qCHunk::FreeToLowMark( const qSInt32 Mark )
{
	if( Mark < 0 || Mark > ( signed )g_LowMark )
		Sys_Error( "qCHunk::FreeToLowMark: bad mark %d", Mark );

	memset( g_pBase + Mark, 0, g_LowMark - Mark );
	g_LowMark	= Mark;

}	// FreeToLowMark

/*
==============
FreeToHighMark
==============
*/
void qCHunk::FreeToHighMark( const qSInt32 Mark )
{
	if( g_TempActive )
	{
		g_TempActive	= FALSE;
		FreeToHighMark( g_TempMark );
	}

	if( Mark < 0 || Mark > ( signed )g_HighMark )
		Sys_Error( "qCHunk::FreeToHighMark: bad mark %d", Mark );

	memset( g_pBase + g_Size - g_HighMark, 0, g_HighMark - Mark );
	g_HighMark = Mark;

}	// FreeToHighMark

/*
=====
Check
=====
*/
void qCHunk::Check( void )
{
	qSHunk*	pHunk;

	for( pHunk = ( qSHunk* )g_pBase; ( qUInt8* )pHunk != g_pBase + g_LowMark; )
	{
		if( pHunk->Sentinal != HUNK_SENTINAL )													Sys_Error( "qCHunk::Check: trashed sentinal" );
		if( pHunk->Size < 16 || pHunk->Size + ( qUInt8* )pHunk - g_pBase > ( signed )g_Size )	Sys_Error( "qCHunk::Check: bad size" );

		pHunk	= ( qSHunk* )( ( qUInt8* )pHunk + pHunk->Size );
	}

}	// Check

/*
=======
GetBase
=======
*/
qUInt8* qCHunk::GetBase( void )
{
	return g_pBase;

}	// GetBase

/*
=======
GetSize
=======
*/
qUInt32 qCHunk::GetSize( void )
{
	return g_Size;

}	// GetSize

/*
==========
GetLowMark
==========
*/
qUInt32 qCHunk::GetLowMark( void )
{
	return g_LowMark;

}	// GetLowMark

/*
===========
GetHighMark
===========
*/
qUInt32 qCHunk::GetHighMark( void )
{
	return g_HighMark;

}	// GetHighMark
