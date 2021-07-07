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

#ifndef __QCHUNK_H__
#define __QCHUNK_H__

#include	"qTypes.h"

class qCHunk
{
public:

	// Constructor / Destructor
	explicit qCHunk	( void )	{ }	/**< Constructor */
			~qCHunk	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void		Init			( const void* pBuffer, const qUInt32 Size );
	static void*	AllocName		( qUInt32 Size, const qChar* pName );
	static void*	Alloc			( const qUInt32 Size );
	static void*	HighAllocName	( qUInt32 Size, const qChar* pName );
	static void*	TempAlloc		( qUInt32 Size );
	static void		FreeToLowMark	( const qSInt32 Mark );
	static void		FreeToHighMark	( const qSInt32 Mark );
	static void		Check			( void );
	static qUInt8*	GetBase			( void );
	static qUInt32	GetSize			( void );
	static qUInt32	GetLowMark		( void );
	static qUInt32	GetHighMark		( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCHunk

#endif // __QCHUNK_H__
