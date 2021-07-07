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
#include	"winquake.h"
#include	"qCNetworkWinSock.h"

#include	"sys.h"
#include	"qCCVar.h"
#include	"console.h"

#define	MAXHOSTNAMELEN	256

static qDouble			g_BlockTime;
static qUInt32			g_MyAddress;
static qSInt32			g_Initialized		=  0;
static qSInt32			g_AcceptSocket		= -1;
static qSInt32			g_ControlSocket;
static qSInt32			g_BroadcastSocket	= 0;
static qSSocketAddress	g_BroadcastAddress;

/*
============
BlockingHook
============
*/
static BOOL PASCAL FAR BlockingHook( void )
{
	MSG	Message;

	if( (Sys_FloatTime() - g_BlockTime ) > 2.0 )
	{
		WSACancelBlockingCall();
		return FALSE;
	}

	if( PeekMessage( &Message, NULL, 0, 0, PM_REMOVE ) )
	{
		TranslateMessage( &Message);
		DispatchMessage( &Message );
		return TRUE;
	}

	return FALSE;

}	// BlockingHook

/*
===============
GetLocalAddress
===============
*/
static void GetLocalAddress( void )
{
	struct hostent*	pLocal	= NULL;
	qChar			HostName[ MAXHOSTNAMELEN ];
	qUInt32			Address;

	if( g_MyAddress != INADDR_ANY )
		return;

	if( gethostname( HostName, MAXHOSTNAMELEN ) == SOCKET_ERROR )
		return;

	g_BlockTime	= Sys_FloatTime();

	WSASetBlockingHook( BlockingHook );
	pLocal	= gethostbyname( HostName );
	WSAUnhookBlockingHook();

	if( pLocal == NULL )
		return;

	g_MyAddress	= *( qSInt32* )pLocal->h_addr_list[ 0 ];

	Address	= ntohl( g_MyAddress );
	snprintf( NET_GetMyTCPIPAddress(), NET_NAMELEN, "%d.%d.%d.%d", ( Address >> 24 ) & 0xff, ( Address >> 16 ) & 0xff, ( Address >> 8 ) & 0xff, Address & 0xff );

}	// GetLocalAddress

/*
==========================
MakeSocketBroadcastCapable
==========================
*/
static qSInt32 MakeSocketBroadcastCapable( const qSInt32 Socket )
{
	qSInt32	Temp	= 1;

	// make this socket broadcast capable
	if( setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, ( qChar* )&Temp, sizeof( Temp ) ) < 0 )
		return -1;

	g_BroadcastSocket	= Socket;

	return 0;

}	// MakeSocketBroadcastCapable

/*
================
PartialIPAddress
================
*/
static qSInt32 PartialIPAddress(  const qChar* pIn, qSSocketAddress* pHostAddress )
{
	qChar	Buffer[ 256 ]	= { '.', 0 };
	qChar*	pChar			= Buffer;
	qSInt32	Address			= 0;
	qSInt32	Mask			= -1;
	qSInt32	Port;

	strcpy( Buffer + 1, pIn );

	if( Buffer[ 1 ] == '.' )
		pChar++;

	while( *pChar == '.' )
	{
		qSInt32	Num	= 0;
		qSInt32	Run	= 0;

		pChar++;

		while( !( *pChar < '0' || *pChar > '9' ) )
		{
			Num	= Num * 10 + *pChar++ - '0';

			if( ++Run > 3 )
				return -1;
		}

		if( ( *pChar < '0' || *pChar > '9' ) && *pChar != '.' && *pChar != ':' && *pChar != 0 )
			return -1;

		if( Num < 0 || Num > 255 )
			return -1;

		Mask	<<= 8;
		Address	  = ( Address << 8 ) + Num;
	}

	if( *pChar++ == ':' )	Port	= atoi( pChar );
	else					Port	= NET_GetHostPort();

	( ( struct sockaddr_in* )pHostAddress )->sin_family			= AF_INET;
	( ( struct sockaddr_in* )pHostAddress )->sin_port			= htons( ( qUInt16 )Port );
	( ( struct sockaddr_in* )pHostAddress )->sin_addr.s_addr	= ( g_MyAddress & htonl( Mask ) ) | htonl( Address );

	return 0;

}	// PartialIPAddress

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
qSInt32 qCNetworkWinSock::Init( void )
{
	qChar	HostName[ MAXHOSTNAMELEN ];
	qSInt32	IPParm	= COM_CheckParm( "-ip" );

	if( COM_CheckParm( "-noudp" ) )
		return -1;

	if( g_Initialized == 0 )
	{
		WSADATA	Data;

		if( WSAStartup( MAKEWORD( 1, 1 ), &Data ) )
		{
			Con_SafePrintf( "Winsock initialization failed.\n" );
			return -1;
		}
	}

	g_Initialized++;

	// determine my name
	if( gethostname( HostName, MAXHOSTNAMELEN ) == SOCKET_ERROR )
	{
		Con_DPrintf( "Winsock TCP/IP Initialization failed.\n" );
		if( --g_Initialized == 0 )
			WSACleanup();
		return -1;
	}

	// if the quake hostname isn't set, set it to the machine name
	if( strcmp( hostname.pString, "UNNAMED" ) == 0 )
	{
		qChar*	pChar;
		// see if it's a text IP address (well, close enough)
		for( pChar=HostName; *pChar; ++pChar )
			if( ( *pChar < '0' || *pChar > '9' ) && *pChar != '.' )
				break;

		// if it is a real name, strip off the domain, we only want the host
		if(* pChar )
		{
			qUInt8	i;

			for( i=0; i<15; ++i )
				if( HostName[ i ] == '.' )
					break;

			HostName[ i ]	= 0;
		}

		qCCVar::Set( "hostname", HostName );
	}



	if( IPParm )
	{
		if( IPParm < com_argc - 1 )
		{
			if( ( g_MyAddress = inet_addr( com_argv[ IPParm + 1 ] ) ) == INADDR_NONE )
				Sys_Error( "%s is not a valid IP address", com_argv[ IPParm + 1 ] );

			strcpy( NET_GetMyTCPIPAddress(), com_argv[ IPParm + 1 ] );
		}
		else
		{
			Sys_Error( "qCNetworkWinSock::Init: you must specify an IP address after -ip" );
		}
	}
	else
	{
		g_MyAddress	= INADDR_ANY;
		strcpy( NET_GetMyTCPIPAddress(), "INADDR_ANY" );
	}

	if( ( g_ControlSocket = OpenSocket( 0 ) ) == -1 )
	{
		Con_Printf( "qCNetworkWinSock::Init: Unable to open control socket\n" );
		if( --g_Initialized == 0 )
			WSACleanup();
		return -1;
	}

	( ( struct sockaddr_in* )&g_BroadcastAddress )->sin_family		= AF_INET;
	( ( struct sockaddr_in* )&g_BroadcastAddress )->sin_port		= htons( ( qUInt16 )NET_GetHostPort() );
	( ( struct sockaddr_in* )&g_BroadcastAddress )->sin_addr.s_addr	= INADDR_BROADCAST;

	Con_Printf( "Winsock TCP/IP Initialized\n" );
	NET_SetTCPIPAvailable( TRUE );

	return g_ControlSocket;

}	// Init

/*
========
Shutdown
========
*/
void qCNetworkWinSock::Shutdown( void )
{
	Listen( FALSE );
	CloseSocket( g_ControlSocket );

	if( --g_Initialized == 0 )
		WSACleanup();

}	// Shutdown

/*
======
Listen
======
*/
void qCNetworkWinSock::Listen( const qBool State )
{
	// enable listening
	if( State )
	{
		if( g_AcceptSocket != -1 )
			return;

		GetLocalAddress();

		if( ( g_AcceptSocket = OpenSocket( NET_GetHostPort() ) ) == -1 )
			Sys_Error( "qCNetworkWinSock::Listen: Unable to open accept socket\n" );

		return;
	}

	// disable listening
	if( g_AcceptSocket == -1 )
		return;

	CloseSocket( g_AcceptSocket );
	g_AcceptSocket	= -1;

}	// Listen

/*
==========
OpenSocket
==========
*/
qSInt32 qCNetworkWinSock::OpenSocket( const qSInt32 Port )
{
	struct sockaddr_in	Address;
	qSInt32				NewSocket;
	u_long				True	= 1;

	if( ( NewSocket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == -1 )
		return -1;

	if( ioctlsocket( NewSocket, FIONBIO, &True ) == -1 )
	{
		closesocket( NewSocket );
		return -1;
	}

	Address.sin_family		= AF_INET;
	Address.sin_port		= htons( ( qUInt16 )Port );
	Address.sin_addr.s_addr	= g_MyAddress;

	if( Port != 0 )
	{
		if( bind( NewSocket, ( const sockaddr* )&Address, sizeof( Address ) ) == 0 )
			return NewSocket;
	}
	else
	{
		for( qUInt16 NewPort = NET_GetHostPort() + 1; NewPort < NET_GetHostPort() + 64; ++NewPort )
		{
			Address.sin_family		= AF_INET;
			Address.sin_port		= htons( ( qUInt16 )NewPort );
			Address.sin_addr.s_addr	= g_MyAddress;

			if( bind( NewSocket, ( const sockaddr* )&Address, sizeof( Address ) ) == 0 )
				return NewSocket;
		}
	}

	Sys_Error( "Unable to bind to %s", AddressToString( ( qSSocketAddress* )&Address ) );
	closesocket( NewSocket );
	return -1;

}	// OpenSocket

/*
===========
CloseSocket
===========
*/
qSInt32 qCNetworkWinSock::CloseSocket( const qSInt32 Socket )
{
	if( Socket == g_BroadcastSocket )
		g_BroadcastSocket	= 0;

	return closesocket( Socket );

}	// CloseSocket

/*
===================
CheckNewConnections
===================
*/
qSInt32 qCNetworkWinSock::CheckNewConnections( void )
{
	qChar	Buffer[ 4096 ];

	if( g_AcceptSocket == -1 )
		return -1;

	if( recvfrom( g_AcceptSocket, Buffer, sizeof( Buffer ), MSG_PEEK, NULL, NULL ) > 0 )
		return g_AcceptSocket;

	return -1;

}	// CheckNewConnections

/*
====
Read
====
*/
qSInt32 qCNetworkWinSock::Read( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length, const qSSocketAddress* pAddress )
{
	qSInt32	AddressLength	= sizeof( qSSocketAddress );
	qSInt32	ReturnValue		= recvfrom( Socket, ( qChar* )pBuffer, Length, 0, ( sockaddr* )pAddress, &AddressLength );

	if( ReturnValue == -1 )
	{
		qSInt32	LastError	= WSAGetLastError();

		if( LastError == WSAEWOULDBLOCK || LastError == WSAECONNREFUSED )
			return 0;
	}

	return ReturnValue;

}	// Read

/*
=====
Write
=====
*/
qSInt32 qCNetworkWinSock::Write( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length, const qSSocketAddress* pAddress )
{
	qSInt32	ReturnValue	= sendto( Socket, ( qChar* )pBuffer, Length, 0, ( sockaddr* )pAddress, sizeof( qSSocketAddress ) );

	if( ReturnValue == -1 )
		if( WSAGetLastError() == WSAEWOULDBLOCK )
			return 0;

	return ReturnValue;

}	// Write

/*
=========
Broadcast
=========
*/
qSInt32 qCNetworkWinSock::Broadcast( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length )
{
	if( Socket != g_BroadcastSocket )
	{
		if( g_BroadcastSocket != 0 )
			Sys_Error( "Attempted to use multiple broadcasts sockets\n" );

		GetLocalAddress();

		if( MakeSocketBroadcastCapable( Socket ) == -1 )
		{
			Con_Printf( "Unable to make socket broadcast capable\n" );
			return -1;
		}
	}

	return Write( Socket, pBuffer, Length, &g_BroadcastAddress );

}	// Broadcast

/*
===============
AddressToString
===============
*/
qChar* qCNetworkWinSock::AddressToString( const qSSocketAddress* pAddress )
{
	static qChar	Buffer[ 22 ];
	qSInt32			Address	= ntohl( ( ( struct sockaddr_in* )pAddress )->sin_addr.s_addr );

	snprintf( Buffer, sizeof( Buffer ), "%d.%d.%d.%d:%d", ( Address >> 24 ) & 0xff, ( Address >> 16 ) & 0xff, ( Address >> 8 ) & 0xff, Address & 0xff, ntohs( ( ( struct sockaddr_in* )pAddress )->sin_port ) );

	return Buffer;

}	// AddressToString

/*
================
GetSocketAddress
================
*/
void qCNetworkWinSock::GetSocketAddress( const qSInt32 Socket, qSSocketAddress* pAddress )
{
	qSInt32	AddressLength	= sizeof( qSSocketAddress );
	qSInt32	Address;

	memset( pAddress, 0, AddressLength );
	getsockname( Socket, ( struct sockaddr* )pAddress, &AddressLength );

	Address	= ( ( struct sockaddr_in* )pAddress )->sin_addr.s_addr;

	if( Address == 0 || Address == inet_addr( "127.0.0.1" ) )
		( ( struct sockaddr_in* )pAddress )->sin_addr.s_addr	= g_MyAddress;

}	// GetSocketAddress

/*
==================
GetNameFromAddress
==================
*/
void qCNetworkWinSock::GetNameFromAddress( const qSSocketAddress* pAddress, qChar* pName )
{
	struct hostent*	pHostEntry	= gethostbyaddr( ( qChar* )&( ( struct sockaddr_in* )pAddress )->sin_addr, sizeof( struct in_addr ), AF_INET );

	if( pHostEntry )
	{
		strncpy( pName, ( qChar* )pHostEntry->h_name, NET_NAMELEN - 1 );
		return;
	}

	strcpy( pName, AddressToString( pAddress ) );

}	// GetNameFromAddress

/*
==================
GetAddressFromName
==================
*/
qSInt32 qCNetworkWinSock::GetAddressFromName( const qChar* pName, qSSocketAddress* pAddress )
{
	if( pName[ 0 ] >= '0' && pName[ 0 ] <= '9' )
		return PartialIPAddress( pName, pAddress );

	struct hostent*	pHostEntry	= gethostbyname( pName );

	if( !pHostEntry )
		return -1;

	( ( struct sockaddr_in* )pAddress )->sin_family			= AF_INET;
	( ( struct sockaddr_in* )pAddress )->sin_port			= htons( ( qUInt16 )NET_GetHostPort() );
	( ( struct sockaddr_in* )pAddress )->sin_addr.s_addr	= *( qSInt32* )pHostEntry->h_addr_list[ 0 ];

	return 0;

}	// GetAddressFromName

/*
==============
AddressCompare
==============
*/
qSInt32 qCNetworkWinSock::AddressCompare( const qSSocketAddress* pAddress1, const qSSocketAddress* pAddress2 )
{
	if( ( ( ( struct sockaddr_in* )pAddress1 )->sin_family			!=
		  ( ( struct sockaddr_in* )pAddress2 )->sin_family )		||
		( ( ( struct sockaddr_in* )pAddress1 )->sin_addr.s_addr		!=
		  ( ( struct sockaddr_in* )pAddress2 )->sin_addr.s_addr ) )
		return -1;

	if(	( ( struct sockaddr_in* )pAddress1 )->sin_port	!=
		( ( struct sockaddr_in* )pAddress2 )->sin_port )
		return 1;

	return 0;

}	// AddressCompare

/*
=============
GetSocketPort
=============
*/
qSInt32 qCNetworkWinSock::GetSocketPort( const qSSocketAddress* pAddress )
{
	return ntohs( ( ( struct sockaddr_in* )pAddress )->sin_port );

}	// GetSocketPort

/*
=============
SetSocketPort
=============
*/
void qCNetworkWinSock::SetSocketPort( qSSocketAddress* pAddress, const qSInt32 Port )
{
	( ( struct sockaddr_in* )pAddress )->sin_port	= htons( ( qUInt16 )Port );

}	// SetSocketPort
