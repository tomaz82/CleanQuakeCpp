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

#ifndef __QCENTITYFRAGMENTS_H__
#define __QCENTITYFRAGMENTS_H__

#include	"qTypes.h"

#include	"render.h"

typedef struct qSEntityFragment
{
	struct mleaf_s*		pLeaf;
	struct entity_s*	pEntity;
	qSEntityFragment*	pLeafNext;
	qSEntityFragment*	pEntityNext;
} qSEntityFragment;

class qCEntityFragments
{
public:

	// Constructor / Destructor
	explicit qCEntityFragments	( void )	{ }	/**< Constructor */
			~qCEntityFragments	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void	Add		( entity_t* pEntity );
	static void	Remove	( entity_t* pEntity );
	static void	Store	( qSEntityFragment** ppEntityFragment );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCEntityFragments

#endif // __QCENTITYFRAGMENTS_H__
