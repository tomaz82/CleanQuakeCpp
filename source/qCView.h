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

#ifndef __QCVIEW_H__
#define __QCVIEW_H__

#include	"qTypes.h"

class qCView
{
public:

	// Constructor / Destructor
	explicit qCView	( void )	{ }	/**< Constructor */
			~qCView	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void		Init				( void );
	static qFloat	CalcRoll			( qVector3 Angles, qVector3 Velocity );
	static void		CalcBlend			( void );
	static void		ParseDamage			( void );
	static void		SetContentsColor	( qSInt32 Contents );
	static void		UpdatePalette		( void );
	static void		Render				( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCCRC

#endif // __QCCRC_H__
