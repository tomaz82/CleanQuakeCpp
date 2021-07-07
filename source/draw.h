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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

#ifndef __DRAW_H__
#define __DRAW_H__

typedef struct qpic_s
{
	int			width, height;
	qUInt8		data[4];			// variably sized
} qpic_t;

void Draw_Init (void);
void Draw_Character( const qSInt32 X, const qSInt32 Y, const qSInt8 Num );
void Draw_Pic (int x, int y, const qpic_t *pic);
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, qUInt8 *translation);
void Draw_ConsoleBackground (int lines);
void Draw_FadeScreen (void);
void Draw_String (int x, int y, const char *str );
qpic_t *Draw_PicFromWad (char *name);
qpic_t *Draw_CachePic (char *path);

#endif	// __DRAW_H__
