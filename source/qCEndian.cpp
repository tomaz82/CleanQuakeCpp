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
#include	"qCEndian.h"

#define		IS_LITTLE_ENDIAN

typedef union qUEndian
{
	qUInt8	UInt8[ 4 ];
	qFloat	Float;
	qSInt32	Int32;
	qSInt16	Int16;
} qUEndian;

/*
========
BigFloat
========
*/
qFloat qCEndian::BigFloat( const qFloat In )
{

#ifdef IS_LITTLE_ENDIAN

	qUEndian	Temp;
	qUEndian	Out;

	Temp.Float		= In;

	Out.UInt8[ 0 ]	= Temp.UInt8[ 3 ];
	Out.UInt8[ 1 ]	= Temp.UInt8[ 2 ];
	Out.UInt8[ 2 ]	= Temp.UInt8[ 1 ];
	Out.UInt8[ 3 ]	= Temp.UInt8[ 0 ];

	return Out.Float;

#else	// IS_LITTLE_ENDIAN

	return In;

#endif	// IS_LITTLE_ENDIAN

}	// BigFloat

/*
===========
LittleFloat
===========
*/
qFloat qCEndian::LittleFloat( const qFloat In )
{

#ifdef IS_LITTLE_ENDIAN

	return In;

#else	// IS_LITTLE_ENDIAN

	qUEndian	Temp;
	qUEndian	Out;

	Temp.Float		= In;

	Out.UInt8[ 0 ]	= Temp.UInt8[ 3 ];
	Out.UInt8[ 1 ]	= Temp.UInt8[ 2 ];
	Out.UInt8[ 2 ]	= Temp.UInt8[ 1 ];
	Out.UInt8[ 3 ]	= Temp.UInt8[ 0 ];

	return Out.Float;

#endif	// IS_LITTLE_ENDIAN

}	// LittleFloat

/*
========
BigInt32
========
*/
qSInt32 qCEndian::BigInt32( const qSInt32 In )
{

#ifdef IS_LITTLE_ENDIAN

	qUEndian	Temp;
	qUEndian	Out;

	Temp.Int32		= In;

	Out.UInt8[ 0 ]	= Temp.UInt8[ 3 ];
	Out.UInt8[ 1 ]	= Temp.UInt8[ 2 ];
	Out.UInt8[ 2 ]	= Temp.UInt8[ 1 ];
	Out.UInt8[ 3 ]	= Temp.UInt8[ 0 ];

	return Out.Int32;

#else	// IS_LITTLE_ENDIAN

	return In;

#endif	// IS_LITTLE_ENDIAN

}	// BigInt32

/*
===========
LittleInt32
===========
*/
qSInt32 qCEndian::LittleInt32( const qSInt32 In )
{

#ifdef IS_LITTLE_ENDIAN

	return In;

#else	// IS_LITTLE_ENDIAN

	qUEndian	Temp;
	qUEndian	Out;

	Temp.Int32		= In;

	Out.UInt8[ 0 ]	= Temp.UInt8[ 3 ];
	Out.UInt8[ 1 ]	= Temp.UInt8[ 2 ];
	Out.UInt8[ 2 ]	= Temp.UInt8[ 1 ];
	Out.UInt8[ 3 ]	= Temp.UInt8[ 0 ];

	return Out.Int32;

#endif	// IS_LITTLE_ENDIAN

}	// LittleInt32

/*
========
BigInt16
========
*/
qSInt16 qCEndian::BigInt16( const qSInt16 In )
{

#ifdef IS_LITTLE_ENDIAN

	qUEndian	Temp;
	qUEndian	Out;

	Temp.Int16		= In;

	Out.UInt8[ 0 ]	= Temp.UInt8[ 1 ];
	Out.UInt8[ 1 ]	= Temp.UInt8[ 0 ];

	return Out.Int16;

#else	// IS_LITTLE_ENDIAN

	return In;

#endif	// IS_LITTLE_ENDIAN

}	// BigInt16

/*
===========
LittleInt16
===========
*/
qSInt16 qCEndian::LittleInt16( const qSInt16 In )
{

#ifdef IS_LITTLE_ENDIAN

	return In;

#else	// IS_LITTLE_ENDIAN

	qUEndian	Temp;
	qUEndian	Out;

	Temp.Int16		= In;

	Out.UInt8[ 0 ]	= Temp.UInt8[ 1 ];
	Out.UInt8[ 1 ]	= Temp.UInt8[ 0 ];

	return Out.Int16;

#endif	// IS_LITTLE_ENDIAN

}	// LittleInt16
