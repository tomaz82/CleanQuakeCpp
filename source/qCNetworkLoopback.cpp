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
#include	"qCNetworkLoopback.h"

#include	"sys.h"
#include	"qCCVar.h"
#include	"server.h"
#include	"console.h"

static qSSocket*	g_pClient			= NULL;
static qSSocket*	g_pServer			= NULL;
static qBool		g_ConnectPending	= FALSE;

/*
========
IntAlign
========
*/
static qUInt32 IntAlign( qUInt32 Value )
{
	return ( Value + ( sizeof( qUInt32 ) - 1 ) ) & ( ~( sizeof( qUInt32 ) - 1 ) );

}	// IntAlign

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
qSInt32 qCNetworkLoopback::Init( void )
{
	return 0;

}	// Init

/*
======
Listen
======
*/
void qCNetworkLoopback::Listen( const qBool State )
{

}	// Listen

/*
==============
SearchForHosts
==============
*/
void qCNetworkLoopback::SearchForHosts( const qBool Transmit )
{
	if( !sv.active )
		return;

	hostCacheCount	= 1;

	if( strcmp( hostname.pString, "UNNAMED" ) == 0 )	strcpy( hostcache[ 0 ].Name, "local" );
	else												strcpy( hostcache[ 0 ].Name, hostname.pString );

	strcpy( hostcache[ 0 ].CName,	"local" );
	strcpy( hostcache[ 0 ].Map,		sv.name );

	hostcache[ 0 ].Users	= net_activeconnections;
	hostcache[ 0 ].MaxUsers	= svs.maxclients;
	hostcache[ 0 ].Driver	= NET_GetDriverLevel();

}	// SearchForHosts

/*
=======
Connect
=======
*/
qSSocket* qCNetworkLoopback::Connect( const qChar* pHost )
{
	if( strcmp( pHost,"local" ) != 0 )
		return NULL;

	g_ConnectPending	= TRUE;

	if(! g_pClient )
	{
		if( ( g_pClient = NET_NewQSocket() ) == NULL )
		{
			Con_Printf( "qCNetLoop::Connect: no qsocket available\n" );
			return NULL;
		}

		strcpy( g_pClient->Address, "localhost" );
	}

	g_pClient->ReceiveMessageLength	= 0;
	g_pClient->SendMessageLength	= 0;
	g_pClient->CanSend				= TRUE;

	if( !g_pServer )
	{
		if( ( g_pServer = NET_NewQSocket() ) == NULL )
		{
			Con_Printf( "qCNetLoop::Connect: no qsocket available\n" );
			return NULL;
		}

		strcpy( g_pServer->Address, "LOCAL" );
	}

	g_pServer->ReceiveMessageLength	= 0;
	g_pServer->SendMessageLength	= 0;
	g_pServer->CanSend				= TRUE;

	g_pClient->pDriverData	= ( void* )g_pServer;
	g_pServer->pDriverData	= ( void* )g_pClient;

	return g_pClient;

}	// Connect

/*
===================
CheckNewConnections
===================
*/
qSSocket* qCNetworkLoopback::CheckNewConnections( void )
{
	if( !g_ConnectPending )
		return NULL;

	g_ConnectPending				= FALSE;

	g_pServer->SendMessageLength	= 0;
	g_pServer->ReceiveMessageLength	= 0;
	g_pServer->CanSend				= TRUE;

	g_pClient->SendMessageLength	= 0;
	g_pClient->ReceiveMessageLength	= 0;
	g_pClient->CanSend				= TRUE;

	return g_pServer;

}	// CheckNewConnections

/*
==========
GetMessage
==========
*/
qSInt32 qCNetworkLoopback::GetMessage( qSSocket* pSocket )
{
	if( pSocket->ReceiveMessageLength == 0 )
		return 0;

	qSInt32	Return	= pSocket->ReceiveMessageBuffer[ 0 ];
	qUInt32	Length	= pSocket->ReceiveMessageBuffer[ 1 ] + ( pSocket->ReceiveMessageBuffer[ 2 ] << 8 );

	SZ_Clear( &net_message );
	SZ_Write( &net_message, &pSocket->ReceiveMessageBuffer[ 4 ], Length );

	Length	= IntAlign( Length + 4 );

	pSocket->ReceiveMessageLength	-= Length;

	if( pSocket->ReceiveMessageLength )
		memcpy( pSocket->ReceiveMessageBuffer, &pSocket->ReceiveMessageBuffer[ Length ], pSocket->ReceiveMessageLength );

	if( pSocket->pDriverData && Return == 1 )
		( ( qSSocket* )pSocket->pDriverData )->CanSend	= TRUE;

	return Return;

}	// GetMessage

/*
===========
SendMessage
===========
*/
qSInt32 qCNetworkLoopback::SendMessage( qSSocket* pSocket, sizebuf_t* pData )
{
	if( !pSocket->pDriverData )
		return -1;

	qUInt32*	pBufferLength	= ( qUInt32* )&( ( qSSocket* )pSocket->pDriverData )->ReceiveMessageLength;
	qUInt8*		pBuffer;

	if( ( *pBufferLength + pData->cursize + 4 ) > NET_MAXMESSAGE )
		Sys_Error( "qCNetLoop::SendMessage: overflow\n" );

	pBuffer	= ( ( qSSocket* )pSocket->pDriverData )->ReceiveMessageBuffer + *pBufferLength;

	// message type
	*pBuffer++	= 1;

	// length
	*pBuffer++	= pData->cursize & 0xff;
	*pBuffer++	= pData->cursize >> 8;

	// align
	pBuffer++;

	// message
	memcpy( pBuffer, pData->data, pData->cursize );
	*pBufferLength = IntAlign( *pBufferLength + pData->cursize + 4 );

	pSocket->CanSend	= FALSE;

	return 1;

}	// SendMessage

/*
=====================
SendUnreliableMessage
=====================
*/
qSInt32 qCNetworkLoopback::SendUnreliableMessage( qSSocket* pSocket, sizebuf_t* pData )
{
	if( !pSocket->pDriverData )
		return -1;

	qUInt32*	pBufferLength	= ( qUInt32* )&( ( qSSocket* )pSocket->pDriverData )->ReceiveMessageLength;
	qUInt8*		pBuffer;

	if ( ( *pBufferLength + pData->cursize + sizeof( qUInt8 ) + sizeof( qUInt16 ) ) > NET_MAXMESSAGE )
		return 0;

	pBuffer	= ( ( qSSocket* )pSocket->pDriverData )->ReceiveMessageBuffer + *pBufferLength;

	// message type
	*pBuffer++	= 2;

	// length
	*pBuffer++	= pData->cursize & 0xff;
	*pBuffer++	= pData->cursize >> 8;

	// align
	pBuffer++;

	// message
	memcpy( pBuffer, pData->data, pData->cursize );
	*pBufferLength = IntAlign( *pBufferLength + pData->cursize + 4 );

	return 1;

}	// SendUnreliableMessage

/*
==============
CanSendMessage
==============
*/
qBool qCNetworkLoopback::CanSendMessage( qSSocket* pSocket )
{
	if( !pSocket->pDriverData )
		return FALSE;

	return pSocket->CanSend;

}	// CanSendMessage

/*
=====
Close
=====
*/
void qCNetworkLoopback::Close( qSSocket* pSocket )
{
	if( pSocket->pDriverData )
		( ( qSSocket* )pSocket->pDriverData )->pDriverData	= NULL;

	pSocket->ReceiveMessageLength	= 0;
	pSocket->SendMessageLength		= 0;
	pSocket->CanSend				= TRUE;

	if( pSocket == g_pClient )	g_pClient	= NULL;
	else						g_pServer	= NULL;

}	// Close

/*
========
Shutdown
========
*/
void qCNetworkLoopback::Shutdown( void )
{

}	// Shutdown
