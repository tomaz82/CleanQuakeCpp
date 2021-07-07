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
#include	"qCMessage.h"
#include	"qCEndian.h"
#include	"qCNetwork.h"
#include	"net.h"

//////////////////////////////////////////////////////////////////////////

qSInt32	qCMessage::ReadCount	= 0;
qBool	qCMessage::BadRead		= FALSE;

//////////////////////////////////////////////////////////////////////////

/*
=========
WriteChar
=========
*/
void qCMessage::WriteChar( sizebuf_t* pSizeBuf, const qSInt32 Char )
{
	SZ_Write( pSizeBuf, ( void* )&Char, 1 );

}	// WriteChar

/*
=========
WriteByte
=========
*/
void qCMessage::WriteByte( sizebuf_t* pSizeBuf, const qSInt32 Byte )
{
	SZ_Write( pSizeBuf, ( void* )&Byte, 1 );

}	// WriteByte

/*
==========
WriteShort
==========
*/
void qCMessage::WriteShort( sizebuf_t* pSizeBuf, const qSInt32 Short )
{
	qSInt32	LEShort	= qCEndian::LittleInt16( Short );

	SZ_Write( pSizeBuf, ( void* )&LEShort, 2 );

}	// WriteShort

/*
=========
WriteLong
=========
*/
void qCMessage::WriteLong( sizebuf_t* pSizeBuf, const qSInt32 Long )
{
	qSInt32	LELong	= qCEndian::LittleInt32( Long );

	SZ_Write( pSizeBuf, ( void* )&LELong, 4 );

}	// WriteLong

/*
==========
WriteFloat
==========
*/
void qCMessage::WriteFloat( sizebuf_t* pSizeBuf, const qFloat Float )
{
	qFloat	LEFloat	= qCEndian::LittleFloat( Float );

	SZ_Write( pSizeBuf, ( void* )&LEFloat, 4 );

}	// WriteFloat

/*
==========
WriteAngle
==========
*/
void qCMessage::WriteAngle( sizebuf_t* pSizeBuf, const qFloat Float )
{
	WriteChar( pSizeBuf, ( qSInt32 )( Float * ( 256.0 / 360.0 ) ) & 255 );

}	// WriteAngle

/*
==================
WriteAngleProQuake
==================
*/
void qCMessage::WriteAngleProQuake( sizebuf_t* pSizeBuf, const qFloat Float )
{
	WriteShort( pSizeBuf, ( qSInt32 )( Float * ( 65535.0 / 360.0 ) ) & 65535 );

}	// WriteAngleProQuake

/*
==========
WriteCoord
==========
*/
void qCMessage::WriteCoord( sizebuf_t* pSizeBuf, const qFloat Float )
{
	WriteShort( pSizeBuf, ( qSInt32 )( Float * 8.0 ) );

}	// WriteCoord

/*
===========
WriteString
===========
*/
void qCMessage::WriteString( sizebuf_t* pSizeBuf, const qChar* pString )
{
	if( !pString )	SZ_Write( pSizeBuf, ( void* )"",		1 );
	else			SZ_Write( pSizeBuf, ( void* )pString,	strlen( pString ) + 1 );

}	// WriteString

//////////////////////////////////////////////////////////////////////////

/*
============
BeginReading
============
*/
void qCMessage::BeginReading( void )
{
	ReadCount	= 0;
	BadRead		= FALSE;

}	// BeginReading

/*
========
ReadChar
========
*/
qSInt32 qCMessage::ReadChar( void )
{
	if( ( ReadCount + 1 ) > net_message.cursize )
	{
		BadRead	= TRUE;
		return -1;
	}

	qSInt32	Char	 = *( qChar* )&net_message.data[ ReadCount ];
	ReadCount		+= 1;

	return Char;

}	// ReadChar

/*
========
ReadByte
========
*/
qSInt32 qCMessage::ReadByte( void )
{
	if( ( ReadCount + 1 ) > net_message.cursize )
	{
		BadRead	= TRUE;
		return -1;
	}

	qSInt32	Byte	 = *( qUInt8* )&net_message.data[ ReadCount ];
	ReadCount		+= 1;

	return Byte;

}	// ReadByte

/*
=========
ReadShort
=========
*/
qSInt32 qCMessage::ReadShort( void )
{
	if( ( ReadCount + 2 ) > net_message.cursize )
	{
		BadRead	= TRUE;
		return -1;
	}

	qSInt32	LEShort	 = qCEndian::LittleInt16( *( qUInt16* )&net_message.data[ ReadCount ] );
	ReadCount		+= 2;

	return LEShort;

}	// ReadShort

/*
========
ReadLong
========
*/
qSInt32 qCMessage::ReadLong( void )
{
	if( ( ReadCount + 4 ) > net_message.cursize )
	{
		BadRead	= TRUE;
		return -1;
	}

	qSInt32	LELong	 = qCEndian::LittleInt32( *( qUInt32* )&net_message.data[ ReadCount ] );
	ReadCount		+= 4;

	return LELong;

}	// ReadLong

/*
=========
ReadFloat
=========
*/
qFloat qCMessage::ReadFloat( void )
{
	if( ( ReadCount + 4 ) > net_message.cursize )
	{
		BadRead	= TRUE;
		return -1;
	}

	qFloat	LEFloat	 = qCEndian::LittleFloat( *( qFloat* )&net_message.data[ ReadCount ] );
	ReadCount		+= 4;

	return LEFloat;

}	// ReadFloat

/*
=========
ReadAngle
=========
*/
qFloat qCMessage::ReadAngle( void )
{
	return qCMessage::ReadChar() * ( 360.0 / 256.0 );

}	// ReadAngle

/*
=================
ReadAngleProQuake
=================
*/
qFloat qCMessage::ReadAngleProQuake( void )
{
	return qCMessage::ReadShort() * ( 360.0 / 65536.0 );

}	// ReadAngleProQuake

/*
=========
ReadCoord
=========
*/
qFloat qCMessage::ReadCoord( void )
{
	return qCMessage::ReadShort() * ( 1.0 / 8.0 );

}	// ReadCoord

/*
==========
ReadString
==========
*/
qChar* qCMessage::ReadString( void )
{
	static qChar	String[ 2048 ];
	qSInt32			Length	= 0;
	qSInt32			Char;

	do
	{
		Char	= ReadChar();

		if( Char == -1 || Char == 0 )
			break;

		String[ Length++ ]	 = Char;

	} while( Length < sizeof( String ) - 1 );

	String[ Length ]	= 0;

	return String;

}	// ReadString
