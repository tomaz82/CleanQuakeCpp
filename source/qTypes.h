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

#ifndef	__QTYPES_H__
#define	__QTYPES_H__

typedef		int					qBool;
typedef		char				qChar;
typedef		unsigned char		qUInt8;
typedef		signed char			qSInt8;
typedef		wchar_t				qWChar;
typedef		unsigned short		qUInt16;
typedef		signed short		qSInt16;
typedef		unsigned int		qUInt32;
typedef		signed int			qSInt32;
typedef		float				qFloat;
typedef		double				qDouble;

typedef		float				qVector2[ 2 ];
typedef		float				qVector3[ 3 ];
typedef		float				qVector4[ 4 ];
typedef		float				qMatrix3x3[ 3 ][ 3 ];
typedef		float				qMatrix4x4[ 4 ][ 4 ];

#endif // __QTYPES_H__
