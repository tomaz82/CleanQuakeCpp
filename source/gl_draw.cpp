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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include	"quakedef.h"
#include	"qCWad.h"

#include	"vid.h"
#include	"sys.h"
#include	"qCHunk.h"
#include	"qCCVar.h"
#include	"console.h"
#include	"glquake.h"

qUInt8		*draw_chars;				// 8*8 graphic characters

int			translate_texture;
int			char_texture;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

qUInt8		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

void GL_Bind (int texnum)
{
	if (currenttexture == texnum)
		return;
	currenttexture = texnum;

	glBindTexture( GL_TEXTURE_2D, texnum );
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	qUInt8		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

qUInt8		menuplyr_pixels[4096];

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*pic;
	glpic_t	*gl;

	pic = (qpic_t*)qCWad::GetLump(name);
	gl = (glpic_t *)pic->data;

	gl->texnum = qCTextures::Load (name, pic->width, pic->height, pic->data, FALSE, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return pic;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	qCWad::SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = qCTextures::Load (path, dat->width, dat->height, dat->data, FALSE, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	qpic_t	*cb;
	glpic_t	*gl;
	int		start;
	qUInt8	*ncdata;

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = (qUInt8*)qCWad::GetLump("conchars");
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// now turn them into textures
	char_texture = qCTextures::Load ("charset", 128, 128, draw_chars, FALSE, 1);

	start = qCHunk::GetLowMark();

	cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");
	if (!cb)
		Sys_Error ("Couldn't load gfx/conback.lmp");
	qCWad::SwapPic (cb);

	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	gl->texnum = qCTextures::Load ("conback", conback->width, conback->height, ncdata, FALSE, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	qCHunk::FreeToLowMark(start);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;
}

#define	CHARACTER_SIZE	0.0625f

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character( const qSInt32 X, const qSInt32 Y, const qSInt8 Num )
{
	if( Num == 32 || X <= -8 || Y <= -8 )
		return;

//////////////////////////////////////////////////////////////////////////

	const qFloat	Col					= ( Num & 15 ) * CHARACTER_SIZE;
	const qFloat	Row					= ( Num >> 4 ) * CHARACTER_SIZE;
	const qFloat	TexCoords[ 4 ][ 2 ]	= { { Col,					Row },
											{ Col + CHARACTER_SIZE,	Row },
											{ Col + CHARACTER_SIZE,	Row + CHARACTER_SIZE },
											{ Col,					Row + CHARACTER_SIZE } };
	const qFloat	Vertices[ 4 ][ 2 ]	= { { X,					Y },
											{ X + 8,				Y },
											{ X + 8,				Y + 8 },
											{ X,					Y + 8 } };

	GL_Bind( char_texture );

	glBegin( GL_QUADS );
		glTexCoord2fv( TexCoords[ 0 ] );	glVertex2fv( Vertices[ 0 ] );
		glTexCoord2fv( TexCoords[ 1 ] );	glVertex2fv( Vertices[ 1 ] );
		glTexCoord2fv( TexCoords[ 2 ] );	glVertex2fv( Vertices[ 2 ] );
		glTexCoord2fv( TexCoords[ 3 ] );	glVertex2fv( Vertices[ 3 ] );
	glEnd();
}

/*
================
Draw_String
================
*/
void Draw_String( qSInt32 X, qSInt32 Y, const qChar* pString )
{
	if( Y <= -8 )
		return;

//////////////////////////////////////////////////////////////////////////

	GL_Bind( char_texture );

	glBegin( GL_QUADS );

	while( *pString )
	{
		const qChar	Char	= *pString;

		if( Char != 32 && X > -8 )
		{
			const qFloat	Col					= ( Char & 15 ) * CHARACTER_SIZE;
			const qFloat	Row					= ( Char >> 4 ) * CHARACTER_SIZE;
			const qFloat	TexCoords[ 4 ][ 2 ]	= { { Col,					Row },
													{ Col + CHARACTER_SIZE,	Row },
													{ Col + CHARACTER_SIZE,	Row + CHARACTER_SIZE },
													{ Col,					Row + CHARACTER_SIZE } };
			const qFloat	Vertices[ 4 ][ 2 ]	= { { X,					Y },
													{ X + 8,				Y },
													{ X + 8,				Y + 8 },
													{ X,					Y + 8 } };

			glTexCoord2fv( TexCoords[ 0 ] );	glVertex2fv( Vertices[ 0 ] );
			glTexCoord2fv( TexCoords[ 1 ] );	glVertex2fv( Vertices[ 1 ] );
			glTexCoord2fv( TexCoords[ 2 ] );	glVertex2fv( Vertices[ 2 ] );
			glTexCoord2fv( TexCoords[ 3 ] );	glVertex2fv( Vertices[ 3 ] );
		}

		++pString;
		X += 8;
	}

	glEnd();
}

/*
========
Draw_Pic
========
*/
void Draw_Pic( int x, int y, const qpic_t *pic )
{
	glpic_t*	gl = ( glpic_t* )pic->data;

	glEnable (GL_BLEND);
	glColor4f (1,1,1,1);
	GL_Bind (gl->texnum);

	glBegin (GL_QUADS);
		glTexCoord2f (gl->sl, gl->tl);	glVertex2f (x, y);
		glTexCoord2f (gl->sh, gl->tl);	glVertex2f (x+pic->width, y);
		glTexCoord2f (gl->sh, gl->th);	glVertex2f (x+pic->width, y+pic->height);
		glTexCoord2f (gl->sl, gl->th);	glVertex2f (x, y+pic->height);
	glEnd ();

	glDisable (GL_BLEND);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, qUInt8 *translation)
{
	int				v, u;
	unsigned		trans[64*64], *dest;
	qUInt8			*src;
	int				p;

	GL_Bind (translate_texture);

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glColor3f (1,1,1);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+pic->height);
	glEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	Draw_Pic (0, lines - vid.height, conback);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.8f);
	glBegin (GL_QUADS);

	glVertex2f (0,0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	glViewport (0, 0, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);

	glColor4f (1,1,1,1);
}

//====================================================================

/****************************************/

void GL_SelectTexture (GLenum target)
{
	static int oldtarget = TEXTURE0_SGIS_ARB;

	qglActiveTexture(target);
	if (target == oldtarget)
		return;
	cnttextures[oldtarget-TEXTURE0_SGIS_ARB] = currenttexture;
	currenttexture = cnttextures[target-TEXTURE0_SGIS_ARB];
	oldtarget = target;
}
