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

#ifndef __QCNETWORKWINSOCK_H__
#define __QCNETWORKWINSOCK_H__

#include	"qTypes.h"
#include	"qCNetwork.h"

class qCNetworkWinSock
{
public:

	// Constructor / Destructor
	explicit qCNetworkWinSock	( void )	{ }	/**< Constructor */
			~qCNetworkWinSock	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static qSInt32	Init				( void );
	static void		Shutdown			( void );
	static void		Listen				( const qBool State );
	static qSInt32	OpenSocket			( const qSInt32 Port );
	static qSInt32	CloseSocket			( const qSInt32 Socket );
	static qSInt32	CheckNewConnections	( void );
	static qSInt32	Read				( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length, const qSSocketAddress* pAddress );
	static qSInt32	Write				( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length, const qSSocketAddress* pAddress );
	static qSInt32	Broadcast			( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length );
	static qChar*	AddressToString		( const qSSocketAddress* pAddress );
	static void		GetSocketAddress	( const qSInt32 Socket, qSSocketAddress* pAddress );
	static void		GetNameFromAddress	( const qSSocketAddress* pAddress, qChar* pName );
	static qSInt32	GetAddressFromName	( const qChar* pName, qSSocketAddress* pAddress );
	static qSInt32	AddressCompare		( const qSSocketAddress* pAddress1, const qSSocketAddress* pAddress2 );
	static qSInt32	GetSocketPort		( const qSSocketAddress* pAddress );
	static void		SetSocketPort		( qSSocketAddress* pAddress, const qSInt32 Port );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCNetworkWinSock

#endif // __QCNETWORKWINSOCK_H__
