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

#ifndef __QCMESSAGE_H__
#define __QCMESSAGE_H__

#include	"qTypes.h"

class qCMessage
{
public:

	// Constructor / Destructor
	explicit qCMessage	( void )	{ }	/**< Constructor */
			~qCMessage	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void		WriteChar			( sizebuf_t* pSizeBuf, const qSInt32 Char );
	static void		WriteByte			( sizebuf_t* pSizeBuf, const qSInt32 Byte );
	static void		WriteShort			( sizebuf_t* pSizeBuf, const qSInt32 Short );
	static void		WriteLong			( sizebuf_t* pSizeBuf, const qSInt32 Long );
	static void		WriteFloat			( sizebuf_t* pSizeBuf, const qFloat Float );
	static void		WriteAngle			( sizebuf_t* pSizeBuf, const qFloat Float );
	static void		WriteAngleProQuake	( sizebuf_t* pSizeBuf, const qFloat Float );
	static void		WriteCoord			( sizebuf_t* pSizeBuf, const qFloat Float );
	static void		WriteString			( sizebuf_t* pSizeBuf, const qChar* pString );

	static void		BeginReading		( void );
	static qSInt32	ReadChar			( void );
	static qSInt32	ReadByte			( void );
	static qSInt32	ReadShort			( void );
	static qSInt32	ReadLong			( void );
	static qFloat	ReadFloat			( void );
	static qFloat	ReadAngle			( void );
	static qFloat	ReadAngleProQuake	( void );
	static qFloat	ReadCoord			( void );
	static qChar*	ReadString			( void );

//////////////////////////////////////////////////////////////////////////

	static qSInt32	ReadCount;
	static qBool	BadRead;

};	// qCMessage

#endif // __QCMESSAGE_H__
