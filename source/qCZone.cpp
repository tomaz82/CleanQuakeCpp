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
#include	"qCZone.h"
#include	"qCHunk.h"
#include	"sys.h"

#define		DYNAMIC_SIZE	0x00010000
#define		ZONEID			0x001D4A11
#define		MINFRAGMENT		64

typedef struct qSZoneBlock
{
	qUInt32				Size;	// including the header and possibly tiny fragments
	qUInt32				Tag;	// a tag of 0 is a free block
	qUInt32				ID;		// should be ZONEID
	struct qSZoneBlock*	pNext;
	struct qSZoneBlock*	pPrev;
	qUInt32				Pad;	// pad to 64 bit boundary
} qSZoneBlock;

typedef struct qSZone
{
	qUInt32			Size;		// total bytes malloced, including header
	qSZoneBlock		BlockList;	// start / end cap for linked list
	qSZoneBlock*	pRover;
} qSZone;

qSZone*	g_pMainZone;

/*
=====
Clear
=====
*/
static void Clear( qSZone* pZone, const qUInt32 Size )
{
	qSZoneBlock*	pBlock;

	// set the entire zone to one free block
	pZone->BlockList.pNext	=
	pZone->BlockList.pPrev	=
	pBlock					= ( qSZoneBlock* )( ( qUInt8* )pZone + sizeof( qSZone ) );
	pZone->BlockList.Tag	= 1;	// in use block
	pZone->BlockList.ID		= 0;
	pZone->BlockList.Size	= 0;
	pZone->pRover			= pBlock;

	pBlock->pNext			=
	pBlock->pPrev			= &pZone->BlockList;
	pBlock->Tag				= 0;	// free block
	pBlock->ID				= ZONEID;
	pBlock->Size			= Size - sizeof( qSZone );

}	// Clear

/*
========
TagAlloc
========
*/
static void* TagAlloc( qUInt32 Size, const qUInt32 Tag )
{
	qSZoneBlock*	pStart;
	qSZoneBlock*	pRover;
	qSZoneBlock*	pNew;
	qSZoneBlock*	pBase;
	qUInt32			Extra;

	if( !Tag )
		Sys_Error( "TagAlloc: tried to use a 0 tag" );

	// scan through the block list looking for the first free block
	// of sufficient size
	Size	+= sizeof( qSZoneBlock );	// account for size of block header
	Size	+= 4;						// space for memory trash tester
	Size	 = ( Size + 7 ) & ~7;		// align to 8-byte boundary

	pBase	=
	pRover	= g_pMainZone->pRover;
	pStart	= pBase->pPrev;

	do
	{
		if( pRover == pStart )	return NULL;	// scaned all the way around the list
		if( pRover->Tag )		pBase	= pRover	= pRover->pNext;
		else					pRover	= pRover->pNext;
	} while( pBase->Tag || pBase->Size < Size );

	// found a block big enough
	Extra	= pBase->Size - Size;

	if( Extra >  MINFRAGMENT )
	{	// there will be a free fragment after the allocated block
		pNew				= ( qSZoneBlock* )( ( qUInt8* )pBase + Size );
		pNew->Size			= Extra;
		pNew->Tag			= 0;			// free block
		pNew->pPrev			= pBase;
		pNew->ID			= ZONEID;
		pNew->pNext			= pBase->pNext;
		pNew->pNext->pPrev	= pNew;

		pBase->pNext		= pNew;
		pBase->Size			= Size;
	}

	g_pMainZone->pRover	= pBase->pNext;	// next allocation will start looking here
	pBase->Tag			= Tag;			// no longer a free block
	pBase->ID			= ZONEID;

	// marker for memory trash testing
	*( qUInt32* )( ( qUInt8* )pBase + pBase->Size - 4 )	= ZONEID;

	return ( void* )( ( qUInt8* )pBase + sizeof( qSZoneBlock ) );

}	// TagAlloc

/*
=========
CheckHeap
=========
*/
static void CheckHeap( void )
{
	qSZoneBlock*	pBlock;

	for( pBlock = g_pMainZone->BlockList.pNext; pBlock->pNext != &g_pMainZone->BlockList; pBlock = pBlock->pNext)
	{
		if( ( qUInt8* )pBlock + pBlock->Size != ( qUInt8* )pBlock->pNext )	Sys_Error( "CheckHeap: block size does not touch the next block\n" );
		if( pBlock->pNext->pPrev != pBlock )								Sys_Error( "CheckHeap: next block doesn't have proper back link\n" );
		if( !pBlock->Tag && !pBlock->pNext->Tag )							Sys_Error( "CheckHeap: two consecutive free blocks\n" );
	}

}	// CheckHeap

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
void qCZone::Init( void )
{
	qUInt32	ZoneSize	= DYNAMIC_SIZE;
	qSInt32	ParamID		= COM_CheckParm( "-zone" );

	if( ParamID )
	{
		if( ParamID < com_argc - 1 )	ZoneSize	= atoi ( com_argv[ ParamID + 1 ] ) * 1024;
		else							Sys_Error( "Memory_Init: you must specify a size in KB after -zone" );
	}

	g_pMainZone	= ( qSZone* )qCHunk::AllocName( ZoneSize, "zone" );
	Clear( g_pMainZone, ZoneSize );

}	// Init

/*
=====
Alloc
=====
*/
void* qCZone::Alloc( const qUInt32 Size )
{
	void*	pBuffer;

	CheckHeap();	// DEBUG

	if( !( pBuffer = TagAlloc( Size, 1 ) ) )
		Sys_Error( "qCZone::Alloc: failed on allocation of %d bytes", Size );

	memset( pBuffer, 0, Size );

	return pBuffer;

}	// Alloc

/*
====
Free
====
*/
void qCZone::Free( const void* pMemory )
{
	qSZoneBlock*	pBlock;
	qSZoneBlock*	pOther;

	if( !pMemory )
		Sys_Error( "qCZone::Free: NULL pointer" );

	pBlock	= ( qSZoneBlock* )( ( qUInt8* )pMemory - sizeof( qSZoneBlock ) );

	if( pBlock->ID != ZONEID )	Sys_Error( "qCZone::Free: freed a pointer without ZONEID" );
	if( pBlock->Tag == 0 )		Sys_Error( "qCZone::Free: freed a freed pointer" );

	pBlock->Tag	= 0;		// mark as free

	pOther	= pBlock->pPrev;

	if( !pOther->Tag )
	{
		// merge with previous free block
		pOther->Size			+= pBlock->Size;
		pOther->pNext			 = pBlock->pNext;
		pOther->pNext->pPrev	 = pOther;

		if( pBlock == g_pMainZone->pRover )
			g_pMainZone->pRover = pOther;

		pBlock = pOther;
	}

	pOther	= pBlock->pNext;

	if( !pOther->Tag )
	{
		// merge the next free block onto the end
		pBlock->Size			+= pOther->Size;
		pBlock->pNext			 = pOther->pNext;
		pBlock->pNext->pPrev	 = pBlock;

		if( pOther == g_pMainZone->pRover )
			g_pMainZone->pRover = pBlock;
	}

}	// Free
