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

#include	<GL/gl.h>
#include	"gl_model.h"

void GL_BeginRendering (int *width, int *height);
void GL_EndRendering (void);

extern	int texture_extension_number;
void GL_Set2D (void);
void GL_BuildLightmaps (void);

extern	int glwidth, glheight;

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01

//====================================================


extern	entity_t	r_worldentity;
extern	qVector3		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys;


//
// view origin
//
extern	qVector3	vup;
extern	qVector3	vpn;
extern	qVector3	vright;
extern	qVector3	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int	currenttexture;
extern	int	cnttextures[2];

extern  qSCVar  fpsshow;
extern  qSCVar  fpsmax;

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void GL_Bind (int texnum);

// Multitexture
#define		TEXTURE0_SGIS_		0x835E
#define		TEXTURE1_SGIS_		0x835F

#define		TEXTURE0_ARB_		0x84C0
#define		TEXTURE1_ARB_		0x84C1

extern GLenum TEXTURE0_SGIS_ARB;
extern GLenum TEXTURE1_SGIS_ARB;

typedef void (APIENTRY *lpMTexFUNC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum);
extern lpMTexFUNC qglMultiTexCoord2f;
extern lpSelTexFUNC qglActiveTexture;

void GL_DisableMultitexture(void);
void GL_EnableMultitexture(void);
