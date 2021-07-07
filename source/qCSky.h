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

#ifndef __QCSKY_H__
#define __QCSKY_H__

#include	"qTypes.h"
#include	"gl_model.h"

class qCSky
{
public:

	// Constructor / Destructor
	explicit qCSky	( void )	{ }	/**< Constructor */
			~qCSky	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void	Init			( void );
	static void	LoadTextures	( const qSTexture* pTexture );
	static void	Render			( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCSky

#endif // __QCSKY_H__
