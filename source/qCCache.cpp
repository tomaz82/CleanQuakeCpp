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
#include	"qCCache.h"
#include	"qCHunk.h"
#include	"sys.h"
#include	"cmd.h"
#include	"console.h"

struct qSCacheSystem
{
	qUInt32			Size;
	qSCache*		pCache;
	qChar			Name[ 16 ];
	qSCacheSystem*	pPrev;
	qSCacheSystem*	pNext;
	qSCacheSystem*	pLRUPrev;	// Least Recently Used
	qSCacheSystem*	pLRUNext;	// Least Recently Used
};

qSCacheSystem	g_CacheHead;

/*
=======
MakeLRU
=======
*/
static void MakeLRU( qSCacheSystem* pCacheSystem )
{
	if( pCacheSystem->pLRUNext || pCacheSystem->pLRUPrev )
		Sys_Error( "qCCache::MakeLRU: active link" );

	g_CacheHead.pLRUNext->pLRUPrev	= pCacheSystem;
	pCacheSystem->pLRUNext			= g_CacheHead.pLRUNext;
	pCacheSystem->pLRUPrev			= &g_CacheHead;
	g_CacheHead.pLRUNext			= pCacheSystem;

}	// MakeLRU

/*
=========
UnlinkLRU
=========
*/
static void UnlinkLRU( qSCacheSystem* pCacheSystem )
{
	if( !pCacheSystem->pLRUNext || !pCacheSystem->pLRUPrev )
		Sys_Error( "qCCache::UnlinkLRU: NULL link" );

	pCacheSystem->pLRUNext->pLRUPrev	= pCacheSystem->pLRUPrev;
	pCacheSystem->pLRUPrev->pLRUNext	= pCacheSystem->pLRUNext;
	pCacheSystem->pLRUPrev				=
	pCacheSystem->pLRUNext				= NULL;

}	// UnlinkLRU

/*
========
TryAlloc
========
*/
static qSCacheSystem* TryAlloc( const qUInt32 Size, const qBool NoBottom )
{
	qSCacheSystem*	pCacheSystem;
	qSCacheSystem*	pCacheSystemNew;

	// is the cache completely empty?
	if( !NoBottom && g_CacheHead.pPrev == &g_CacheHead )
	{
		if( qCHunk::GetSize() - qCHunk::GetHighMark() - qCHunk::GetLowMark() < ( qSInt32 )Size )
			Sys_Error( "qCCache::TryAlloc: %d is greater then free hunk", Size );

		pCacheSystemNew	= ( qSCacheSystem* )( qCHunk::GetBase() + qCHunk::GetLowMark() );
		memset( pCacheSystemNew, 0, sizeof( qSCacheSystem ) );
		pCacheSystemNew->Size	= Size;

		g_CacheHead.pPrev		=
		g_CacheHead.pNext		= pCacheSystemNew;
		pCacheSystemNew->pPrev	=
		pCacheSystemNew->pNext	= &g_CacheHead;

		MakeLRU( pCacheSystemNew );
		return pCacheSystemNew;
	}

	// search from the bottom up for space
	pCacheSystemNew	= ( qSCacheSystem* )( qCHunk::GetBase() + qCHunk::GetLowMark() );
	pCacheSystem	= g_CacheHead.pNext;

	do
	{
		if( !NoBottom || pCacheSystem != g_CacheHead.pNext )
		{
			if( ( ( qUInt8* )pCacheSystem - ( qUInt8* )pCacheSystemNew ) >= ( qSInt32 )Size )
			{
				// found space
				memset( pCacheSystemNew, 0, sizeof( qSCacheSystem ) );

				pCacheSystemNew->Size		= Size;
				pCacheSystemNew->pNext		= pCacheSystem;
				pCacheSystemNew->pPrev		= pCacheSystem->pPrev;
				pCacheSystem->pPrev->pNext	= pCacheSystemNew;
				pCacheSystem->pPrev			= pCacheSystemNew;

				MakeLRU( pCacheSystemNew );

				return pCacheSystemNew;
			}
		}

		// continue looking
		pCacheSystemNew	= ( qSCacheSystem* )( ( qUInt8* )pCacheSystem + pCacheSystem->Size );
		pCacheSystem	= pCacheSystem->pNext;

	} while( pCacheSystem != &g_CacheHead );

	// try to allocate one at the very end
	if( qCHunk::GetBase() + qCHunk::GetSize() - qCHunk::GetHighMark() - ( qUInt8* )pCacheSystemNew >= ( qSInt32 )Size )
	{
		memset( pCacheSystemNew, 0, sizeof( qSCacheSystem ) );

		pCacheSystemNew->Size		= Size;
		pCacheSystemNew->pNext		= &g_CacheHead;
		pCacheSystemNew->pPrev		= g_CacheHead.pPrev;
		g_CacheHead.pPrev->pNext	= pCacheSystemNew;
		g_CacheHead.pPrev			= pCacheSystemNew;

		MakeLRU( pCacheSystemNew );

		return pCacheSystemNew;
	}

	// couldn't allocate
	return NULL;

}	// TryAlloc

/*
====
Free
====
*/
static void Free( qSCache* pCache )
{
	if( !pCache->pData )
		Sys_Error( "qCCache::Free: not allocated" );

	qSCacheSystem*	pCacheSystem	= ( ( qSCacheSystem*)pCache->pData ) - 1;
	pCacheSystem->pPrev->pNext		= pCacheSystem->pNext;
	pCacheSystem->pNext->pPrev		= pCacheSystem->pPrev;
	pCacheSystem->pNext				=
	pCacheSystem->pPrev				= NULL;

	pCache->pData					= NULL;

	UnlinkLRU( pCacheSystem );

}	// Free

/*
====
Move
====
*/
static void Move( const qSCacheSystem* pCacheSystem )
{
	// we are clearing up space at the bottom, so only allocate it late
	qSCacheSystem*	pCacheSystemNew	= TryAlloc( pCacheSystem->Size, TRUE );

	if( pCacheSystemNew )
	{
		memcpy( pCacheSystemNew + 1, pCacheSystem + 1, pCacheSystem->Size - sizeof( qSCacheSystem ) );
		pCacheSystemNew->pCache			= pCacheSystem->pCache;
		memcpy( pCacheSystemNew->Name, pCacheSystem->Name, sizeof( pCacheSystemNew->Name ) );
		Free( pCacheSystem->pCache );
		pCacheSystemNew->pCache->pData	= ( void*)( pCacheSystemNew + 1 );
	}
	else
	{
		Free( pCacheSystem->pCache );
	}

}	// Move

/*
=======
FlushCB
=======
*/
static void FlushCB( void )
{
	while( g_CacheHead.pNext != &g_CacheHead )
		Free( g_CacheHead.pNext->pCache );

}	// FlushCB

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCCache::Init( void )
{
	g_CacheHead.pNext		=
	g_CacheHead.pPrev		=
	g_CacheHead.pLRUNext	=
	g_CacheHead.pLRUPrev	= &g_CacheHead;

	Cmd_AddCommand( "flush", FlushCB );

}	// Init

/*
=====
Alloc
=====
*/
void* qCCache::Alloc( qSCache* pCache, qUInt32 Size, const qChar* pName )
{
	if( pCache->pData )	Sys_Error( "qCCache::Alloc: already allocated" );
	if( Size <= 0 )		Sys_Error( "qCCache::Alloc: size %d", Size );

	Size	= ( Size + sizeof( qSCacheSystem ) + 15 ) & ~15;

	while( 1 )
	{
		qSCacheSystem*	pCacheSystem;

		if( ( pCacheSystem = TryAlloc( Size, FALSE ) ) != NULL )
		{
			strncpy( pCacheSystem->Name, pName, sizeof( pCacheSystem->Name ) - 1 );
			pCache->pData			= ( void* )( pCacheSystem + 1 );
			pCacheSystem->pCache	= pCache;
			break;
		}

		if( g_CacheHead.pLRUPrev == &g_CacheHead )
			Sys_Error( "qCCache::Alloc: out of memory" );

		Free( g_CacheHead.pLRUPrev->pCache );
	}

	return Check( pCache );

}	// Alloc

/*
=======
FreeLow
=======
*/
void qCCache::FreeLow( const qUInt32 NewLowHunk )
{
	qSCacheSystem*	pCacheSystem;

	while( 1 )
	{
		pCacheSystem	= g_CacheHead.pNext;

		if( pCacheSystem == &g_CacheHead )
			return;	// nothing in cache at all

		if( ( qUInt8* )pCacheSystem >= qCHunk::GetBase() + NewLowHunk )
			return;	// there is space to grow the hunk

		// reclaim the space
		Move( pCacheSystem );
	}

}	// FreeLow

/*
========
FreeHigh
========
*/
void qCCache::FreeHigh( const qUInt32 NewHighHunk )
{
	qSCacheSystem*	pCacheSystem;
	qSCacheSystem*	pCacheSystemPrev;

	pCacheSystemPrev	= NULL;

	while( 1 )
	{
		pCacheSystem = g_CacheHead.pPrev;

		if( pCacheSystem == &g_CacheHead )
			return;	// nothing in cache at all

		if( ( qUInt8* )pCacheSystem + pCacheSystem->Size <= qCHunk::GetBase() + qCHunk::GetSize() - NewHighHunk )
			return;	// there is space to grow the hunk

		if( pCacheSystem == pCacheSystemPrev )
			Free( pCacheSystem->pCache );	// didn't move out of the way
		else
		{
			Move( pCacheSystem );	// try to move it
			pCacheSystemPrev	= pCacheSystem;
		}
	}

}	// FreeHigh

/*
=====
Check
=====
*/
void* qCCache::Check( const qSCache* pCache )
{
	qSCacheSystem*	pCacheSystem;

	if( !pCache->pData )
		return NULL;

	pCacheSystem	= ( ( qSCacheSystem* )pCache->pData ) - 1;

	// move to head of LRU
	UnlinkLRU( pCacheSystem );
	MakeLRU( pCacheSystem );

	return pCache->pData;

}	// Check

/*
======
Report
======
*/
void qCCache::Report( void )
{
	Con_DPrintf( "%4.1f megabyte data cache\n", ( qCHunk::GetSize() - qCHunk::GetHighMark() - qCHunk::GetLowMark() ) / ( qFloat )( 1024 * 1024 ) );

}	// Report
