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

#ifndef __QCENDIAN_H__
#define __QCENDIAN_H__

#include	"qTypes.h"

class qCEndian
{
public:

	// Constructor / Destructor
	explicit qCEndian	( void )	{ }	/**< Constructor */
			~qCEndian	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static qFloat	BigFloat	( const qFloat In );
	static qFloat	LittleFloat	( const qFloat In );
	static qSInt32	BigInt32	( const qSInt32 In );
	static qSInt32	LittleInt32	( const qSInt32 In );
	static qSInt16	BigInt16	( const qSInt16 In );
	static qSInt16	LittleInt16	( const qSInt16 In );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCEndian

#endif // __QCENDIAN_H__
