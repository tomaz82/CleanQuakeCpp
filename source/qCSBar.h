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

#ifndef __QCSBAR_H__
#define __QCSBAR_H__

#include	"qTypes.h"

class qCSBar
{
public:

	// Constructor / Destructor
	explicit qCSBar	( void )	{ }	/**< Constructor */
			~qCSBar	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void		Init				( void );
	static void		Render				( void );
	static void		OverlayIntermission	( void );
	static void		OverlayFinale		( void );
	static void		SetNumLines			( const qUInt32 NumLines );
	static qUInt32	GetNumLines			( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCSBar

#endif // __QCSBAR_H__
