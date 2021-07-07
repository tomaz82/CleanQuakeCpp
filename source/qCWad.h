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

#include "draw.h"

#ifndef __QCWAD_H__
#define __QCWAD_H__

#include	"qTypes.h"

class qCWad
{
public:

	// Constructor / Destructor
	explicit qCWad	( void )	{ }	/**< Constructor */
			~qCWad	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void		LoadFile	( const qChar*	pFileName );
	static void*	GetLump		( const qChar* pName);
	static void		SwapPic		( qpic_t* pPic );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCWad

#endif // __QCWAD_H__
