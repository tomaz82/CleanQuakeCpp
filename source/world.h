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

#ifndef __WORLD_H__
#define __WORLD_H__

typedef struct
{
	qVector3	normal;
	float	dist;
} plane_t;

typedef struct
{
	qBool	allsolid;	// if TRUE, plane is not valid
	qBool	startsolid;	// if TRUE, the initial point was in a solid area
	qBool	inopen, inwater;
	float	fraction;		// time completed, 1.0 = didn't hit anything
	qVector3	endpos;			// final position
	plane_t	plane;			// surface normal at impact
	edict_t	*ent;			// entity the surface is on
} trace_t;


#define	MOVE_NORMAL		0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2


void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_UnlinkEdict (edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
// flags ent->v.modified

void SV_LinkEdict (edict_t *ent, qBool touch_triggers);
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v.modified
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers

int SV_PointContents (qVector3 p);
int SV_TruePointContents (qVector3 p);
// returns the CONTENTS_* value from the world at the given point.
// does not check any entities at all
// the non-TRUE version remaps the water current contents to content_water

edict_t	*SV_TestEntityPosition (edict_t *ent);

trace_t SV_Move (qVector3 start, qVector3 mins, qVector3 maxs, qVector3 end, int type, edict_t *passedict);
// mins and maxs are reletive

// if the entire move stays in a solid volume, trace.allsolid will be set

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// nomonsters is used for line of sight or edge testing, where mosnters
// shouldn't be considered solid objects

// passedict is explicitly excluded from clipping checks (normally NULL)

#endif // __WORLD_H__
