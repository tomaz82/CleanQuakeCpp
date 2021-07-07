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

// refresh.h -- public interface to refresh functions

#ifndef __RENDER_H__
#define	__RENDER_H__

#define	MAXCLIPPLANES	11

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct entity_s
{
	qBool				forcelink;		// model changed

	int						update_type;

	entity_state_t			baseline;		// to fill in defaults in updates

	double					msgtime;		// time of last update
	qVector3					msg_origins[2];	// last two updates (0 is newest)
	qVector3					origin;
	qVector3					msg_angles[2];	// last two updates (0 is newest)
	qVector3					angles;
	struct model_s			*model;			// NULL = no model
	struct qSEntityFragment	*efrag;			// linked list of efrags
	int						frame;
	float					syncbase;		// for client-side animations
	qUInt8					*colormap;
	int						effects;		// light, particals, etc
	int						skinnum;		// for Alias models
	int						visframe;		// last frame this entity was
											//  found in an active leaf

	float					frame_start_time;
	float					frame_interval;
	int						pose1;
	int						pose2;

	float					translate_start_time;
	qVector3					origin1;
	qVector3					origin2;
	float					rotate_start_time;
	qVector3					angles1;
	qVector3					angles2;

	float					last_light[3];
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	int	width,height;

	qVector3	vieworg;
	qVector3	viewangles;

	float		fov_x, fov_y;
} refdef_t;


//
// refresh
//

extern	refdef_t	r_refdef;
extern qVector3	r_origin, vpn, vright, vup;

void R_InitTextures (void);
void R_RenderView (void);		// must set r_refdef first

void R_ParseParticleEffect (void);
void R_RunParticleEffect (qVector3 org, qVector3 dir, int color, int count);
void R_RocketTrail (qVector3 start, qVector3 end, int type);

void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (qVector3 org);
void R_ParticleExplosion (qVector3 org);
void R_ParticleExplosion2 (qVector3 org, int colorStart, int colorLength);
void R_LavaSplash (qVector3 org);
void R_TeleportSplash (qVector3 org);

void R_PushDlights (void);

void R_DrawParticles (void);
void R_InitParticles (void);
void R_ClearParticles (void);

int R_LightPoint (qVector3 p);

#endif // __RENDER_H__
