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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include	"quakedef.h"

#include	"bspfile.h"
#include	"sys.h"
#include	"qCHunk.h"
#include	"qCMath.h"
#include	"qCEndian.h"
#include	"console.h"
#include	"glquake.h"
#include	"qCSky.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qBool crash);

qUInt8	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	r = qCCache::Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, TRUE);

	if (!mod->cache.pData)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.pData;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (qVector3 p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;

	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = VectorDot (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
qUInt8 *Mod_DecompressVis (qUInt8 *in, model_t *model)
{
	static qUInt8	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	qUInt8	*out;
	int		row;

	row = (model->numleafs+7)>>3;
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

qUInt8 *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = TRUE;
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (const char *name)
{
	int		i;
	model_t	*mod;

	if (!name[0])
		Sys_Error ("Mod_ForName: NULL name");

//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		strcpy (mod->name, name);
		mod->needload = TRUE;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (const char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			qCCache::Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qBool crash)
{
	void	*d;
	unsigned *buf;
	qUInt8	stackbuf[1024];		// avoid dirtying the cache heap

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			d = qCCache::Check (&mod->cache);
			if (d)
				return mod;
		}
		else
			return mod;		// not cached at all
	}

//
// load the file
//
	buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
	{
		if (crash)
			Sys_Error ("Mod_NumForName: %s not found", mod->name);
		return NULL;
	}

//
// allocate a new model
//
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = FALSE;

	switch (qCEndian::LittleInt32(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qBool crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

qUInt8	*mod_base;


/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int		i, j, pixels, num, max, altmax;
	miptex_t	*mt;
	qSTexture	*tx, *tx2;
	qSTexture	*anims[10];
	qSTexture	*altanims[10];
	dmiptexlump_t *m;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *)(mod_base + l->fileofs);

	m->nummiptex = qCEndian::LittleInt32(m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = (qSTexture**)qCHunk::AllocName (m->nummiptex * sizeof(*loadmodel->textures) , loadname);

	for (i=0 ; i<m->nummiptex ; i++)
	{
		m->dataofs[i] = qCEndian::LittleInt32(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *)((qUInt8 *)m + m->dataofs[i]);
		mt->width = qCEndian::LittleInt32 (mt->width);
		mt->height = qCEndian::LittleInt32 (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = qCEndian::LittleInt32 (mt->offsets[j]);

		if ( (mt->width & 15) || (mt->height & 15) )
			Sys_Error ("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width*mt->height/64*85;
		tx = (qSTexture*)qCHunk::AllocName (sizeof(qSTexture) +pixels, loadname );
		loadmodel->textures[i] = tx;

		memcpy (tx->Name, mt->name, sizeof(tx->Name));
		tx->Width = mt->width;
		tx->Height = mt->height;
		for (j=0 ; j<MIPLEVELS ; j++)
			tx->Offsets[j] = mt->offsets[j] + sizeof(qSTexture) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		memcpy ( tx+1, mt+1, pixels);


		if (!strncmp(mt->name,"sky",3))
		{
			qCSky::LoadTextures( tx );
		}
		else
		{
			tx->TextureIndex = qCTextures::Load(mt->name, tx->Width, tx->Height, (qUInt8 *)(tx+1), TRUE, 1);
		}
	}

//
// sequence the animations
//
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->Name[0] != '+')
			continue;
		if (tx->pAnimNext)
			continue;	// already sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->Name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error ("Bad animating texture %s", tx->Name);

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->Name[0] != '+')
				continue;
			if (strcmp (tx2->Name+2, tx->Name+2))
				continue;

			num = tx2->Name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Sys_Error ("Bad animating texture %s", tx->Name);
		}

#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %d of %s",j, tx->Name);
			tx2->AnimTotal = max * ANIM_CYCLE;
			tx2->AnimMin = j * ANIM_CYCLE;
			tx2->AnimMax = (j+1) * ANIM_CYCLE;
			tx2->pAnimNext = anims[ (j+1)%max ];
			if (altmax)
				tx2->pAlternateAnims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %d of %s",j, tx->Name);
			tx2->AnimTotal = altmax * ANIM_CYCLE;
			tx2->AnimMin = j * ANIM_CYCLE;
			tx2->AnimMax = (j+1) * ANIM_CYCLE;
			tx2->pAnimNext = altanims[ (j+1)%altmax ];
			if (max)
				tx2->pAlternateAnims = anims[0];
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	int i;
	qUInt8 *in, *out, *data;
	qUInt8 d;
	char litfilename[1024];
	loadmodel->lightdata = NULL;

	strcpy(litfilename, loadmodel->name);
	COM_StripExtension(litfilename, litfilename);
	strcat(litfilename, ".lit");
	data = (qUInt8*) COM_LoadHunkFile (litfilename);
	if (data)
	{
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
		{
			i = qCEndian::LittleInt32(((int *)data)[1]);
			if (i == 1)
			{
				Con_DPrintf("%s loaded", litfilename);
				loadmodel->lightdata = data + 8;
				return;
			}
			else
				Con_Printf("Unknown .lit file version (%d)\n", i);
		}
		else
			Con_Printf("Corrupt .lit file (old version?), ignoring\n");
	}

	if (!l->filelen)
		return;

	loadmodel->lightdata = (qUInt8*)qCHunk::AllocName ( l->filelen*3, litfilename);
	in = loadmodel->lightdata + l->filelen*2; // place the file at the end, so it will not be overwritten until the very last write
	out = loadmodel->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0;i < l->filelen;i++)
	{
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = (qUInt8*)qCHunk::AllocName ( l->filelen, loadname);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = (char*)qCHunk::AllocName ( l->filelen, loadname);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (dvertex_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mvertex_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = qCEndian::LittleFloat(in->point[0]);
		out->position[1] = qCEndian::LittleFloat(in->point[1]);
		out->position[2] = qCEndian::LittleFloat(in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (dmodel_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (dmodel_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = qCEndian::LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = qCEndian::LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = qCEndian::LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = qCEndian::LittleInt32(in->headnode[j]);
		out->visleafs = qCEndian::LittleInt32 (in->visleafs);
		out->firstface = qCEndian::LittleInt32 (in->firstface);
		out->numfaces = qCEndian::LittleInt32 (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (dedge_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (medge_t*)qCHunk::AllocName ( (count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)qCEndian::LittleInt16(in->v[0]);
		out->v[1] = (unsigned short)qCEndian::LittleInt16(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (texinfo_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = qCEndian::LittleFloat(in->vecs[0][j]);
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = qCEndian::LittleInt32 (in->miptex);
		out->flags = qCEndian::LittleInt32 (in->flags);

		if (!loadmodel->textures)
		{
			out->texture = qCTextures::GetCheckerTextureData();	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = qCTextures::GetCheckerTextureData(); // texture not found
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
			Sys_Error ("Bad surface extents");
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (dface_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = qCEndian::LittleInt32(in->firstedge);
		out->numedges = qCEndian::LittleInt16(in->numedges);
		out->flags = 0;

		planenum = qCEndian::LittleInt16(in->planenum);
		side = qCEndian::LittleInt16(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + qCEndian::LittleInt16 (in->texinfo);

		CalcSurfaceExtents (out);

	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = qCEndian::LittleInt32(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + ( i * 3 );

	// set the drawing flags flag

		if (!strncmp(out->texinfo->texture->Name,"sky",3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			continue;
		}

		if (!strncmp(out->texinfo->texture->Name,"*",1))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			continue;
		}
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (dnode_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = qCEndian::LittleInt16 (in->mins[j]);
			out->minmaxs[3+j] = qCEndian::LittleInt16 (in->maxs[j]);
		}

		p = qCEndian::LittleInt32(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = qCEndian::LittleInt16 (in->firstface);
		out->numsurfaces = qCEndian::LittleInt16 (in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			p = qCEndian::LittleInt16 (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (dleaf_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = qCEndian::LittleInt16 (in->mins[j]);
			out->minmaxs[3+j] = qCEndian::LittleInt16 (in->maxs[j]);
		}

		p = qCEndian::LittleInt32(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			qCEndian::LittleInt16(in->firstmarksurface);
		out->nummarksurfaces = qCEndian::LittleInt16(in->nummarksurfaces);

		p = qCEndian::LittleInt32(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (dclipnode_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (dclipnode_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = qCEndian::LittleInt32(in->planenum);
		out->children[0] = qCEndian::LittleInt16(in->children[0]);
		out->children[1] = qCEndian::LittleInt16(in->children[1]);
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	dclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = (dclipnode_t*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{
	int		i, j, count;
	short		*in;
	msurface_t **out;

	in = (short*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t**)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = qCEndian::LittleInt16(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{
	int		i, count;
	int		*in, *out;

	in = (int*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int*)qCHunk::AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = qCEndian::LittleInt32 (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;

	in = (dplane_t*)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mplane_t*)qCHunk::AllocName ( count*2*sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = qCEndian::LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = qCEndian::LittleFloat (in->dist);
		out->type = qCEndian::LittleInt32 (in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (qVector3 mins, qVector3 maxs)
{
	int		i;
	qVector3	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	i = qCEndian::LittleInt32 (header->version);
	if (i != BSPVERSION)
		Sys_Error ("Mod_LoadBrushModel: %s has wrong version number (%d should be %d)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (qUInt8 *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = qCEndian::LittleInt32 ( ((int *)header)[i]);

// load into heap

	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation

//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			snprintf( name, sizeof( name ), "*%d", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (model_t *mod, void * pin, maliasframedesc_t *frame)
{
	trivertx_t		*pinframe;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (int i=0 ; i<3 ; i++)
	{
		if (mod->mins[i] > pdaliasframe->bboxmin.v[i]) mod->mins[i] = pdaliasframe->bboxmin.v[i];
		if (mod->maxs[i] < pdaliasframe->bboxmax.v[i]) mod->maxs[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (model_t *mod, void * pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;

	pingroup = (daliasgroup_t *)pin;

	numframes = qCEndian::LittleInt32 (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
		if (mod->mins[i] > pingroup->bboxmin.v[i]) mod->mins[i] = pingroup->bboxmin.v[i];
		if (mod->maxs[i] < pingroup->bboxmax.v[i]) mod->maxs[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = qCEndian::LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void Mod_FloodFillSkin( qUInt8 *skin, int skinwidth, int skinheight )
{
	qUInt8				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		qUInt8		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int		i, j, k;
	char	name[32];
	int		s;
	qUInt8	*skin;
	qUInt8	*texels;
	daliasskingroup_t		*pinskingroup;
	int		groupskins;
	daliasskininterval_t	*pinskinintervals;

	skin = (qUInt8 *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	s = pheader->skinwidth * pheader->skinheight;

	for (i=0 ; i<numskins ; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			// save 8 bit texels for the player model to remap
//			if (!strcmp(loadmodel->name,"progs/player.mdl")) {
				texels = (qUInt8*)qCHunk::AllocName(s, loadname);
				pheader->texels[i] = texels - (qUInt8 *)pheader;
				memcpy (texels, (qUInt8 *)(pskintype + 1), s);
//			}
			snprintf( name, sizeof( name ), "%s_%d", loadmodel->name, i);
			pheader->gl_texturenum[i][0] =
			pheader->gl_texturenum[i][1] =
			pheader->gl_texturenum[i][2] =
			pheader->gl_texturenum[i][3] =
				qCTextures::Load (name, pheader->skinwidth,
				pheader->skinheight, (qUInt8 *)(pskintype + 1), TRUE, 1);
			pskintype = (daliasskintype_t *)((qUInt8 *)(pskintype+1) + s);
		} else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = qCEndian::LittleInt32 (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (daliasskintype_t *)(pinskinintervals + groupskins);

			for (j=0 ; j<groupskins ; j++)
			{
					Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
					if (j == 0) {
						texels = (qUInt8*)qCHunk::AllocName(s, loadname);
						pheader->texels[i] = texels - (qUInt8 *)pheader;
						memcpy (texels, (qUInt8 *)(pskintype), s);
					}
					snprintf( name, sizeof( name ), "%s_%d_%d", loadmodel->name, i,j);
					pheader->gl_texturenum[i][j&3] =
						qCTextures::Load (name, pheader->skinwidth,
						pheader->skinheight, (qUInt8 *)(pskintype), TRUE, 1);
					pskintype = (daliasskintype_t *)((qUInt8 *)(pskintype) + s);
			}
			k = j;
			for (/* */; j < 4; j++)
				pheader->gl_texturenum[i][j&3] =
				pheader->gl_texturenum[i][j - k];
		}
	}

	return (void *)pskintype;
}

//=========================================================================

void GL_MakeAliasModelDisplayLists (aliashdr_t *hdr);

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version, numframes;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int					start, end, total;

	start = qCHunk::GetLowMark ();

	pinmodel = (mdl_t *)buffer;

	version = qCEndian::LittleInt32 (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%d should be %d)",
				 mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t)
			+ (qCEndian::LittleInt32 (pinmodel->numframes) - 1) *
			sizeof (pheader->frames[0]);
	pheader = (aliashdr_t*)qCHunk::AllocName (size, loadname);

	mod->flags = qCEndian::LittleInt32 (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pheader->boundingradius = qCEndian::LittleFloat (pinmodel->boundingradius);
	pheader->numskins = qCEndian::LittleInt32 (pinmodel->numskins);
	pheader->skinwidth = qCEndian::LittleInt32 (pinmodel->skinwidth);
	pheader->skinheight = qCEndian::LittleInt32 (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	pheader->numverts = qCEndian::LittleInt32 (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Sys_Error ("model %s has too many vertices: %d", mod->name, pheader->numverts);

	pheader->numtris = qCEndian::LittleInt32 (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	pheader->numframes = qCEndian::LittleInt32 (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = qCEndian::LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = (synctype_t)qCEndian::LittleInt32 (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = qCEndian::LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = qCEndian::LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = qCEndian::LittleFloat (pinmodel->eyeposition[i]);
	}


//
// load the skins
//
	pskintype = (daliasskintype_t*)&pinmodel[1];
	pskintype = (daliasskintype_t*)Mod_LoadAllSkins (pheader->numskins, pskintype);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *)pskintype;

	for (i=0 ; i<pheader->numverts ; i++)
	{
		stverts[i].onseam = qCEndian::LittleInt32 (pinstverts[i].onseam);
		stverts[i].s = qCEndian::LittleInt32 (pinstverts[i].s);
		stverts[i].t = qCEndian::LittleInt32 (pinstverts[i].t);
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i=0 ; i<pheader->numtris ; i++)
	{
		triangles[i].facesfront = qCEndian::LittleInt32 (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			triangles[i].vertindex[j] = qCEndian::LittleInt32 (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	mod->mins[0] = mod->mins[1] = mod->mins[2] =  9999999;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = -9999999;

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype = (aliasframetype_t)qCEndian::LittleInt32 (pframetype->type);

		if (frametype == ALIAS_SINGLE)	pframetype = (daliasframetype_t *)Mod_LoadAliasFrame (mod, pframetype + 1, &pheader->frames[i]);
		else							pframetype = (daliasframetype_t *)Mod_LoadAliasGroup (mod, pframetype + 1, &pheader->frames[i]);
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	for (i = 0; i < 3; i++)
	{
		mod->mins[i] = mod->mins[i] * pheader->scale[i] + pheader->scale_origin[i];
		mod->maxs[i] = mod->maxs[i] * pheader->scale[i] + pheader->scale_origin[i];
	}

	//
	// build the draw lists
	//
	GL_MakeAliasModelDisplayLists (pheader);

//
// move the complete, relocatable alias model to the cache
//
	end = qCHunk::GetLowMark ();
	total = end - start;

	qCCache::Alloc (&mod->cache, total, loadname);
	if (!mod->cache.pData)
		return;
	memcpy (mod->cache.pData, pheader, total);

	qCHunk::FreeToLowMark (start);
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					width, height, size, origin[2];
	char				name[64];

	pinframe = (dspriteframe_t *)pin;

	width = qCEndian::LittleInt32 (pinframe->width);
	height = qCEndian::LittleInt32 (pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t*)qCHunk::AllocName (sizeof (mspriteframe_t),loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = qCEndian::LittleInt32 (pinframe->origin[0]);
	origin[1] = qCEndian::LittleInt32 (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf( name, sizeof( name ), "%s_%d", loadmodel->name, framenum);
	pspriteframe->gl_texturenum = qCTextures::Load (name, width, height, (qUInt8 *)(pinframe + 1), TRUE, 1);

	return (void *)((qUInt8 *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = qCEndian::LittleInt32 (pingroup->numframes);

	pspritegroup = (mspritegroup_t*)qCHunk::AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = (float*)qCHunk::AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = qCEndian::LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;

	pin = (dsprite_t *)buffer;

	version = qCEndian::LittleInt32 (pin->version);
	if (version != SPRITE_VERSION)
		Sys_Error ("%s has wrong version number "
				 "(%d should be %d)", mod->name, version, SPRITE_VERSION);

	numframes = qCEndian::LittleInt32 (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = (msprite_t*)qCHunk::AllocName (size, loadname);

	mod->cache.pData = psprite;

	psprite->type = qCEndian::LittleInt32 (pin->type);
	psprite->maxwidth = qCEndian::LittleInt32 (pin->width);
	psprite->maxheight = qCEndian::LittleInt32 (pin->height);
	mod->synctype = (synctype_t)qCEndian::LittleInt32 (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

//
// load the frames
//
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = (spriteframetype_t)qCEndian::LittleInt32 (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteFrame (pframetype + 1,
										 &psprite->frames[i].frameptr, i);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (pframetype + 1,
										 &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}
