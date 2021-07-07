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

#ifndef __QCCACHE_H__
#define __QCCACHE_H__

#include	"qTypes.h"

typedef struct qSCache
{
	void*	pData;
} qSCache;

class qCCache
{
public:

	// Constructor / Destructor
	explicit qCCache	( void )	{ }	/**< Constructor */
			~qCCache	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void		Init		( void );
	static void*	Alloc		( qSCache* pCache, qUInt32 Size, const qChar* pName );
	static void		FreeLow		( const qUInt32 NewLowHunk );
	static void		FreeHigh	( const qUInt32 NewHighHunk );
	static void*	Check		( const qSCache* pCache );
	static void		Report		( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCCache

#endif // __QCCACHE_H__
