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
#include	"qCEntityFragments.h"
#include	"client.h"
#include	"qCClient.h"

#include	"bspfile.h"
#include	"sys.h"
#include	"qCMath.h"
#include	"render.h"
#include	"console.h"
#include	"glquake.h"

static qSEntityFragment**	g_ppLastLink;
static qVector3				g_Mins;
static qVector3				g_Maxs;

/*
=================
SplitEntityOnNode
=================
*/
static void SplitEntityOnNode( mnode_t* pNode, entity_t* pEntity )
{
	if( pNode->contents == CONTENTS_SOLID )
		return;

//////////////////////////////////////////////////////////////////////////

	qSEntityFragment*	pEntityFragment;
	mplane_t*			pSplitPlane;
	mleaf_t*			pLeaf;
	qUInt32				Sides;

	// add an entity fragment if the node is a leaf
	if( pNode->contents < 0 )
	{
		pLeaf			= ( mleaf_t* )pNode;
		pEntityFragment	= cl.free_efrags;

		if( !pEntityFragment )
		{
			Con_Printf( "Too many entity fragments!\n" );
			return;
		}

		cl.free_efrags					= cl.free_efrags->pEntityNext;
		pEntityFragment->pEntity		= pEntity;

		// add the entity link
		*g_ppLastLink					= pEntityFragment;
		g_ppLastLink					= &pEntityFragment->pEntityNext;
		pEntityFragment->pEntityNext	= NULL;

		// set the leaf links
		pEntityFragment->pLeaf			= pLeaf;
		pEntityFragment->pLeafNext		= pLeaf->efrags;
		pLeaf->efrags					= pEntityFragment;

		return;
	}

	// NODE_MIXED
	pSplitPlane	= pNode->plane;
	Sides		= BOX_ON_PLANE_SIDE( g_Mins, g_Maxs, pSplitPlane );

	// recurse down the contacted sides
	if( Sides & 1 )	SplitEntityOnNode( pNode->children[ 0 ], pEntity );
	if( Sides & 2 )	SplitEntityOnNode( pNode->children[ 1 ], pEntity );

}	// SplitEntityOnNode

//////////////////////////////////////////////////////////////////////////

/*
===
Add
===
*/
void qCEntityFragments::Add( entity_t* pEntity )
{
	model_t*	pModel	= pEntity->model;

	if( !pModel )
		return;

	g_ppLastLink	= &pEntity->efrag;

	for( qUInt32 i=0; i<3; ++i )
	{
		g_Mins[ i ]	= pEntity->origin[ i ] + pModel->mins[ i ];
		g_Maxs[ i ]	= pEntity->origin[ i ] + pModel->maxs[ i ];
	}

	SplitEntityOnNode( cl.worldmodel->nodes, pEntity );

}	// Add


/*
======
Remove
======
*/
void qCEntityFragments::Remove( entity_t* pEntity )
{
	qSEntityFragment*	pEntityFragment	= pEntity->efrag;
	qSEntityFragment*	pOld;
	qSEntityFragment*	pWalk;
	qSEntityFragment**	ppPrev;

	while( pEntityFragment )
	{
		ppPrev	= &pEntityFragment->pLeaf->efrags;

		while( 1 )
		{
			pWalk	= *ppPrev;

			if( !pWalk )
				break;

			if( pWalk == pEntityFragment )
			{
				*ppPrev	= pEntityFragment->pLeafNext;
				break;
			}
			else
				ppPrev	= &pWalk->pLeafNext;
		}

		pOld			= pEntityFragment;
		pEntityFragment	= pEntityFragment->pEntityNext;

		// put it on the free list
		pOld->pEntityNext	= cl.free_efrags;
		cl.free_efrags		= pOld;
	}

	pEntity->efrag	= NULL;

}	// Remove

/*
=====
Store
=====
*/
void qCEntityFragments::Store( qSEntityFragment** ppEntityFragment )
{
	qSEntityFragment*	pEntityFragment;
	entity_t*			pEntity;
	model_t*			pModel;

	while( ( pEntityFragment = *ppEntityFragment ) != NULL )
	{
		pEntity	= pEntityFragment->pEntity;
		pModel	= pEntity->model;

		switch( pModel->type )
		{
			case mod_alias:
			case mod_brush:
			case mod_sprite:
			{
				if( ( pEntity->visframe != r_framecount )	&&
					( cl_numvisedicts < MAX_VISEDICTS ) )
				{
					cl_visedicts[ cl_numvisedicts++ ]	= pEntity;
					pEntity->visframe					= r_framecount;
				}

				ppEntityFragment	= &pEntityFragment->pLeafNext;
			}
			break;

			default:
			{
				Sys_Error("qCEntityFragment::Store: Bad entity type %d\n", pModel->type );
			}
			break;
		}
	}

}	// Store
