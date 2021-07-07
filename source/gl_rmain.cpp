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
// r_main.c

#include	"quakedef.h"
#include	"client.h"
#include	"qCClient.h"
#include	"qCLight.h"
#include	"qCMath.h"
#include	"qCView.h"

#include	"bspfile.h"
#include	"vid.h"
#include	"sys.h"
#include	"sound.h"
#include	"render.h"
#include	"console.h"
#include	"glquake.h"

entity_t	r_worldentity;

qVector3		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

//
// view origin
//
qVector3	vup;
qVector3	vpn;
qVector3	vright;
qVector3	r_origin;

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

/*
=================
R_CullBox

Returns TRUE if the box is completely outside the frustom
=================
*/
qBool R_CullBox (qVector3 mins, qVector3 maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (qCMath::BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return TRUE;
	return FALSE;
}


void R_RotateForEntity (entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);
}


/*
=============
R_BlendedRotateForEntity
=============
*/
void R_BlendedRotateForEntity (entity_t *e)
{
	float timepassed;
	float blend;
	qVector3 d;
	int i;

	// positional interpolation

	timepassed = realtime - e->translate_start_time;

	if (e->translate_start_time == 0 || timepassed > 1)
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin, e->origin1);
		VectorCopy (e->origin, e->origin2);
	}

	if (!VectorCompare (e->origin, e->origin2))
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin2, e->origin1);
		VectorCopy (e->origin,  e->origin2);
		blend = 0;
	}
	else
	{
		blend =  (timepassed * 10);

		if (cl.paused || blend > 1) blend = 1;
	}

	VectorSubtract (e->origin2, e->origin1, d);

	glTranslatef (
		e->origin1[0] + (blend * d[0]),
		e->origin1[1] + (blend * d[1]),
		e->origin1[2] + (blend * d[2]));

	// orientation interpolation (Euler angles, yuck!)

	timepassed = realtime - e->rotate_start_time;

	if (e->rotate_start_time == 0 || timepassed > 1)
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles, e->angles1);
		VectorCopy (e->angles, e->angles2);
	}

	if (!VectorCompare (e->angles, e->angles2))
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles2, e->angles1);
		VectorCopy (e->angles,  e->angles2);
		blend = 0;
	}
	else
	{
		blend = (timepassed * 10);

		if (cl.paused || blend > 1) blend = 1;
	}

	VectorSubtract (e->angles2, e->angles1, d);

	// always interpolate along the shortest path
	for (i = 0; i < 3; i++)
	{
		while( d[ i ] >  180.0f )	d[ i ] -= 360.0f;
		while( d[ i ] < -180.0f )	d[ i ] += 360.0f;
	}

	glRotatef ( e->angles1[1] + ( blend * d[1]),  0, 0, 1);
	glRotatef (-e->angles1[0] + (-blend * d[0]),  0, 1, 0);
	glRotatef ( e->angles1[2] + ( blend * d[2]),  1, 0, 0);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = (msprite_t*)currententity->model->cache.pData;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	qVector3	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	qVector3		v_forward, v_right, v_up;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = (msprite_t*)currententity->model->cache.pData;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		qCMath::AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	glColor3f (1,1,1);

	GL_DisableMultitexture();

    GL_Bind(frame->gl_texturenum);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMultiplyAdd (e->origin, frame->down, up, point);
	VectorMultiplyAdd (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMultiplyAdd (e->origin, frame->up, up, point);
	VectorMultiplyAdd (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMultiplyAdd (e->origin, frame->up, up, point);
	VectorMultiplyAdd (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMultiplyAdd (e->origin, frame->down, up, point);
	VectorMultiplyAdd (point, frame->right, right, point);
	glVertex3fv (point);

	glEnd ();
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

int	lastposenum;
int	lastposenum0;

/*
=============
GL_DrawAliasFrame
=============
*/
extern qVector3 lightcolor;
void GL_DrawAliasFrame (int frame, aliashdr_t *paliashdr, entity_t* e)
{
	float		blend;
	trivertx_t	*verts1, *verts2;
	int			count, pose, numposes, *order;
	qVector3		d;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("GL_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose		= paliashdr->frames[frame].firstpose;
	numposes	= paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / e->frame_interval) % numposes;
	}
	else
	{
		e->frame_interval = 0.1f;
	}

	if (e->pose2 != pose)
	{
		e->frame_start_time = realtime;
		e->pose1 = e->pose2;
		e->pose2 = pose;
		blend = 0;
	}
	else
	{
		blend = (realtime - e->frame_start_time) / e->frame_interval;

		if (cl.paused || blend > 1)
			blend = 1;
	}

	lastposenum0 = e->pose1;
	lastposenum  = e->pose2;

	verts1  = (trivertx_t *)((qUInt8 *)paliashdr + paliashdr->posedata);
	verts2  = verts1;

	verts1 += e->pose1 * paliashdr->poseverts;
	verts2 += e->pose2 * paliashdr->poseverts;

	order = (int *)((qUInt8 *)paliashdr + paliashdr->commands);

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			glColor3fv (lightcolor);

			// texture coordinates come from the draw list
			glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list
			// normals and vertexes come from the frame list
			VectorSubtract(verts2->v, verts1->v, d);

			// blend the vertex positions from each frame together
			glVertex3f (
				verts1->v[0] + (blend * d[0]),
				verts1->v[1] + (blend * d[1]),
				verts1->v[2] + (blend * d[2]));

			verts1++;
			verts2++;
		} while (--count);

		glEnd ();
	}
	glColor4f (1,1,1,1);
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e, const qBool Cull, const qBool Blend )
{
	qUInt32			i;
	int			lnum;
	qVector3		dist;
	float		add;
	model_t		*clmodel;
	qVector3		mins, maxs;
	aliashdr_t	*paliashdr;
	int			anim;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (Cull && R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	R_LightPoint(currententity->origin);

	// always give the gun some light
	if (e == &cl.viewent)
	{
		if (lightcolor[0] < 24)
			lightcolor[0] = 24;
		if (lightcolor[1] < 24)
			lightcolor[1] = 24;
		if (lightcolor[2] < 24)
			lightcolor[2] = 24;
	}

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
			{
				lightcolor[0] += add * cl_dlights[lnum].color[0];
				lightcolor[1] += add * cl_dlights[lnum].color[1];
				lightcolor[2] += add * cl_dlights[lnum].color[2];
			}
		}
	}

	// ZOID: never allow players to go totally black
	i = currententity - cl_entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
	{
		if (lightcolor[0] < 8)
			lightcolor[0] = 8;
		if (lightcolor[1] < 8)
			lightcolor[1] = 8;
		if (lightcolor[2] < 8)
			lightcolor[2] = 8;
	}

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!strcmp (clmodel->name, "progs/flame2.mdl")
		|| !strcmp (clmodel->name, "progs/flame.mdl") )
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;

	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);

	VectorAdd   (lightcolor, currententity->last_light, lightcolor);
	VectorScale (lightcolor, 0.5f, lightcolor);
	VectorCopy  (lightcolor, currententity->last_light);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	GL_DisableMultitexture();

    glPushMatrix ();


	if( Blend )	R_BlendedRotateForEntity (e);
	else		R_RotateForEntity (e);

	glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
			GL_Bind( qCTextures::GetPlayerTextureIndex() - 1 + i);
	}

	GL_DrawAliasFrame (currententity->frame, paliashdr, currententity);

	glPopMatrix ();
}

//==================================================================================

void R_DrawBrushModel (entity_t *e);

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity, TRUE, TRUE );
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel (currententity);
			break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange( 0.0f, 0.3 );
	R_DrawAliasModel( currententity, FALSE, FALSE );
	glDepthRange( 0.0f, 1.0f );
}

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

void RotatePointAroundVector( qVector3 dst, const qVector3 dir, const qVector3 point, float degrees );

void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90)
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = VectorDot (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	qCLight::Animate();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	qCMath::AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	qCView::SetContentsColor( r_viewleaf->contents );
	qCView::CalcBlend();

	c_brush_polys = 0;
	c_alias_polys = 0;
}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * Q_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glViewport (0, 0, glwidth, glheight);
    screenaspect = (float)glwidth/glheight;
    MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	//
	// set drawing parms
	//
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

void R_DrawWorld (void);

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	GL_DisableMultitexture();

	R_DrawParticles ();
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glDepthFunc( GL_LESS );
	glDepthRange( 0.0f, 1.0f );
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	R_Clear ();

	// render normal view

	R_RenderScene ();
	R_DrawViewModel ();
}
