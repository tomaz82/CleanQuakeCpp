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
#include	"qCNetworkDatagram.h"

#include	"qCMessage.h"
#include	"qCEndian.h"
#include	"qCCVar.h"
#include	"screen.h"
#include	"cmd.h"
#include	"qCInput.h"
#include	"server.h"
#include	"console.h"

#define NET_FLAG_LENGTH_MASK	0x0000ffff
#define NET_FLAG_DATA			0x00010000
#define NET_FLAG_ACK			0x00020000
#define NET_FLAG_NAK			0x00040000
#define NET_FLAG_EOM			0x00080000
#define NET_FLAG_UNRELIABLE		0x00100000
#define NET_FLAG_CTL			0x80000000

#define NET_PROTOCOL_VERSION	3

#define NET_HEADERSIZE			( 2 * sizeof( qUInt32 ) )
#define NET_DATAGRAMSIZE		( MAX_DATAGRAM + NET_HEADERSIZE )

#define CCREQ_CONNECT			0x01
#define CCREQ_SERVER_INFO		0x02
#define CCREQ_PLAYER_INFO		0x03
#define CCREQ_RULE_INFO			0x04

#define CCREP_ACCEPT			0x81
#define CCREP_REJECT			0x82
#define CCREP_SERVER_INFO		0x83
#define CCREP_PLAYER_INFO		0x84
#define CCREP_RULE_INFO			0x85

typedef struct qSPacketBuffer
{
	qUInt32	Length;
	qUInt32	Sequence;
	qUInt8	Data[ MAX_DATAGRAM ];
} qSPacketBuffer;

static qSPacketBuffer	g_PacketBuffer;
static qUInt32			g_NumPacketsSent			= 0;
static qUInt32			g_NumPacketsReSent			= 0;
static qUInt32			g_NumPacketsReceived		= 0;
static qUInt32			g_NumReceivedDuplicateCount	= 0;
static qUInt32			g_NumShortPacketCount		= 0;
static qUInt32			g_NumDroppedDatagrams		= 0;

static qChar			g_ReturnReason[ 64 ];
static qBool			g_ReturnOnError;

/*
==========
PrintStats
==========
*/
static void PrintStats( const qSSocket* pSocket )
{
	Con_Printf( "canSend = %4d\n", pSocket->CanSend );
	Con_Printf( "sendSeq = %4d\n", pSocket->SendSequence );
	Con_Printf( "recvSeq = %4d\n", pSocket->ReceiveSequence );
	Con_Printf( "\n" );

}	// PrintStats

/*
=======
StatsCB
=======
*/
static void StatsCB( void )
{
	if( Cmd_Argc () == 1 )
	{
		Con_Printf( "unreliable messages sent      = %d\n", unreliableMessagesSent );
		Con_Printf( "unreliable messages recveived = %d\n", unreliableMessagesReceived );
		Con_Printf( "reliable messages sent        = %d\n", messagesSent );
		Con_Printf( "reliable messages received    = %d\n", messagesReceived );
		Con_Printf( "packets sent                  = %d\n", g_NumPacketsSent );
		Con_Printf( "packets resent                = %d\n", g_NumPacketsReSent );
		Con_Printf( "packets received              = %d\n", g_NumPacketsReceived );
		Con_Printf( "received duplicate count      = %d\n", g_NumReceivedDuplicateCount );
		Con_Printf( "short packet count            = %d\n", g_NumShortPacketCount );
		Con_Printf( "dropped datagrams             = %d\n", g_NumDroppedDatagrams );
	}
	else if( strcmp( Cmd_Argv( 1 ), "*" ) == 0 )
	{
		qSSocket*	pSocket;

		for( pSocket=NET_GetActiveSockets(); pSocket; pSocket=pSocket->pNext )
			PrintStats( pSocket );

		for( pSocket=NET_GetFreeSockets(); pSocket; pSocket=pSocket->pNext )
			PrintStats( pSocket );
	}
	else
	{
		qSSocket*	pSocket;

		for( pSocket=NET_GetActiveSockets(); pSocket; pSocket=pSocket->pNext )
			if( stricmp( Cmd_Argv( 1 ), pSocket->Address ) == 0 )
				break;

		if( pSocket == NULL )
			for( pSocket=NET_GetFreeSockets(); pSocket; pSocket=pSocket->pNext )
				if( stricmp( Cmd_Argv( 1 ), pSocket->Address ) == 0 )
					break;

		if( pSocket == NULL )
			return;

		PrintStats( pSocket );
	}

}	// StatsCB

/*
===============
SendMessageNext
===============
*/
static qSInt32 SendMessageNext( qSSocket* pSocket )
{
	qUInt32	PacketLength;
	qUInt32	DataLength;
	qUInt32	EOM;

	if( pSocket->SendMessageLength <= MAX_DATAGRAM )
	{
		DataLength	= pSocket->SendMessageLength;
		EOM			= NET_FLAG_EOM;
	}
	else
	{
		DataLength	= MAX_DATAGRAM;
		EOM			= 0;
	}

	PacketLength	= NET_HEADERSIZE + DataLength;

	g_PacketBuffer.Length	= qCEndian::BigInt32( PacketLength | ( NET_FLAG_DATA | EOM ) );
	g_PacketBuffer.Sequence	= qCEndian::BigInt32( pSocket->SendSequence++ );
	memcpy( g_PacketBuffer.Data, pSocket->SendMessageBuffer, DataLength );

	pSocket->SendNext	= FALSE;

	if( NET_GetLanDriver()->Write( pSocket->Socket, ( qUInt8* )&g_PacketBuffer, PacketLength, &pSocket->SocketAddress ) == -1 )
		return -1;

	pSocket->LastSendTime	= NET_GetTime();
	g_NumPacketsSent++;

	return 1;

}	// SendMessageNext

/*
=============
ReSendMessage
=============
*/
static qSInt32 ReSendMessage( qSSocket* pSocket )
{
	qUInt32	PacketLength;
	qUInt32	DataLength;
	qUInt32	EOM;

	if( pSocket->SendMessageLength <= MAX_DATAGRAM )
	{
		DataLength	= pSocket->SendMessageLength;
		EOM			= NET_FLAG_EOM;
	}
	else
	{
		DataLength	= MAX_DATAGRAM;
		EOM			= 0;
	}

	PacketLength	= NET_HEADERSIZE + DataLength;

	g_PacketBuffer.Length	= qCEndian::BigInt32( PacketLength | ( NET_FLAG_DATA | EOM ) );
	g_PacketBuffer.Sequence	= qCEndian::BigInt32( pSocket->SendSequence - 1 );
	memcpy( g_PacketBuffer.Data, pSocket->SendMessageBuffer, DataLength );

	pSocket->SendNext	= FALSE;

	if( NET_GetLanDriver()->Write( pSocket->Socket, ( qUInt8* )&g_PacketBuffer, PacketLength, &pSocket->SocketAddress ) == -1 )
		return -1;

	pSocket->LastSendTime	= NET_GetTime();
	g_NumPacketsReSent++;

	return 1;

}	// ReSendMessage

//////////////////////////////////////////////////////////////////////////

/*
====
Init
====
*/
qSInt32 qCNetworkDatagram::Init( void )
{
	Cmd_AddCommand( "net_stats", StatsCB );

	if( COM_CheckParm( "-nolan" ) )
		return -1;

	qSInt32	ControlSocket	= NET_GetLanDriver()->Init();

	if( ControlSocket != -1 )
	{
		NET_GetLanDriver()->Initialized		= TRUE;
		NET_GetLanDriver()->ControlSocket	= ControlSocket;
	}

	return 0;

}	// Init

/*
======
Listen
======
*/
void qCNetworkDatagram::Listen( const qBool State )
{
	if( NET_GetLanDriver()->Initialized )
		NET_GetLanDriver()->Listen( State );

}	// Listen

/*
==============
SearchForHosts
==============
*/
void qCNetworkDatagram::SearchForHosts( const qBool Transmit )
{
	if( hostCacheCount == HOSTCACHESIZE )
		return;

	if( !NET_GetLanDriver()->Initialized )
		return;

	qSSocketAddress	ReadAddress;
	qSSocketAddress	MyAddress;
	qSInt32			Control;
	qSInt32			Length;

	NET_GetLanDriver()->GetSocketAddress( NET_GetLanDriver()->ControlSocket, &MyAddress );

	if( Transmit )
	{
		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREQ_SERVER_INFO );
		qCMessage::WriteString( &net_message, "QUAKE" );
		qCMessage::WriteByte( &net_message, NET_PROTOCOL_VERSION );
		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Broadcast( NET_GetLanDriver()->ControlSocket, net_message.data, net_message.cursize );
		SZ_Clear( &net_message );
	}

	while( ( Length = NET_GetLanDriver()->Read( NET_GetLanDriver()->ControlSocket, net_message.data, net_message.maxsize, &ReadAddress ) ) > 0 )
	{
		if( Length < sizeof( qSInt32 ) )
			continue;

		net_message.cursize	= Length;

		// don't answer our own query
		if( NET_GetLanDriver()->AddressCompare( &ReadAddress, &MyAddress ) >= 0 )
			continue;

		// is the cache full?
		if( hostCacheCount == HOSTCACHESIZE )
			continue;

		qCMessage::BeginReading();
		Control	= qCEndian::BigInt32( *( ( qSInt32* )net_message.data ) );
		qCMessage::ReadLong();

		if( Control == -1 )												continue;
		if( ( Control & ( ~NET_FLAG_LENGTH_MASK ) ) !=  NET_FLAG_CTL )	continue;
		if( ( Control & NET_FLAG_LENGTH_MASK ) != Length )				continue;

		if( qCMessage::ReadByte() != CCREP_SERVER_INFO )
			continue;

		NET_GetLanDriver()->GetAddressFromName( qCMessage::ReadString(), &ReadAddress );
		// search the cache for this server
		qUInt32	Count;
		for( Count=0; Count<hostCacheCount; ++Count )
			if( NET_GetLanDriver()->AddressCompare( &ReadAddress, &hostcache[ Count ].Address ) == 0 )
				break;

		// is it already there?
		if( Count < hostCacheCount )
			continue;

		// add it
		++hostCacheCount;
		strcpy( hostcache[ Count ].Name,	qCMessage::ReadString() );
		strcpy( hostcache[ Count ].Map,		qCMessage::ReadString() );
		hostcache[ Count ].Users	= qCMessage::ReadByte();
		hostcache[ Count ].MaxUsers	= qCMessage::ReadByte();

		if( qCMessage::ReadByte() != NET_PROTOCOL_VERSION )
		{
			strcpy( hostcache[ Count ].CName, hostcache[ Count ].Name );
			hostcache[ Count ].CName[ 14 ]	= 0;
			strcpy( hostcache[ Count ].Name, "*" );
			strcat( hostcache[ Count ].Name, hostcache[ Count ].CName );
		}

		memcpy( &hostcache[ Count ].Address, &ReadAddress, sizeof( qSSocketAddress ) );
		hostcache[ Count ].Driver	= NET_GetDriverLevel();
		strcpy( hostcache[ Count ].CName, NET_GetLanDriver()->AddressToString( &ReadAddress ) );

		// check for a name conflict
		for( qUInt32 i=0; i<hostCacheCount; ++i )
		{
			if( i == Count )
				continue;

			if( stricmp( hostcache[ Count ].Name, hostcache[ i ].Name ) == 0 )
			{
				i	= strlen( hostcache[ Count ].Name );

				if( i < 15 && hostcache[ Count ].Name[ i - 1 ] > '8' )
				{
					hostcache[ Count ].Name[ i ]		= '0';
					hostcache[ Count ].Name[ i + 1 ]	= 0;
				}
				else
					hostcache[ Count ].Name[ i - 1 ]++;

				i	= -1;
			}
		}
	}

}	// SearchForHosts

/*
=======
Connect
=======
*/
qSSocket* qCNetworkDatagram::Connect( const qChar* pHost )
{
	if( !NET_GetLanDriver()->Initialized )
		return NULL;

	qSSocket*		pSocket;
	qSSocketAddress	SendAddress;
	qSSocketAddress	ReadAddress;
	qDouble			StartTime;
	qSInt32			ClientSocket;
	qSInt32			NewSocket;
	qSInt32			Control;
	qSInt32			Length;

	// see if we can resolve the host name
	memset( &SendAddress, 0, sizeof( qSSocketAddress ) );
	if( NET_GetLanDriver()->GetAddressFromName( pHost, &SendAddress ) == -1 )
	{
		Con_Printf( "Could not resolve %s\n", pHost );
		return NULL;
	}

	if( ( NewSocket = NET_GetLanDriver()->OpenSocket( 0 ) ) == -1 )
		return NULL;

	if( ( pSocket = NET_NewQSocket() ) == NULL )
	{
		NET_GetLanDriver()->CloseSocket( NewSocket );

		if( g_ReturnOnError )
		{
			qCInput::SetKeyDestination( qMENU );
			g_ReturnOnError	= FALSE;
		}

		return NULL;
	}

	pSocket->Socket	= NewSocket;

	// send the connection request
	Con_Printf( "trying...\n" );
	SCR_UpdateScreen();

	StartTime	= NET_GetTime();

	for( qUInt32 Repetitions=0; Repetitions<3; ++Repetitions )
	{
		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREQ_CONNECT );
		qCMessage::WriteString( &net_message, "QUAKE" );
		qCMessage::WriteByte( &net_message, NET_PROTOCOL_VERSION );
		qCMessage::WriteByte( &net_message, MOD_PROQUAKE );
		qCMessage::WriteByte( &net_message, MOD_PROQUAKE_VERSION * 10 );
		qCMessage::WriteByte( &net_message, 0 );
		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Write( NewSocket, net_message.data, net_message.cursize, &SendAddress );
		SZ_Clear( &net_message );

		do
		{
			memset( &ReadAddress, 0, sizeof( qSSocketAddress ) );
			Length	= NET_GetLanDriver()->Read( NewSocket, net_message.data, net_message.maxsize, &ReadAddress );
			// if we got something, validate it
			if( Length > 0 )
			{
				// is it from the right place?
				if( NET_GetLanDriver()->AddressCompare( &ReadAddress, &SendAddress ) != 0 )
				{
					Length	= 0;
					continue;
				}

				if( Length < sizeof( qSInt32 ) )
				{
					Length	= 0;
					continue;
				}

				net_message.cursize	= Length;

				qCMessage::BeginReading();
				Control	= qCEndian::BigInt32( *( ( qSInt32* )net_message.data ) );
				qCMessage::ReadLong();

				if( Control == -1 )												{ Length = 0; continue; }
				if( ( Control & ( ~NET_FLAG_LENGTH_MASK ) ) !=  NET_FLAG_CTL )	{ Length = 0; continue; }
				if( ( Control & NET_FLAG_LENGTH_MASK ) != Length )				{ Length = 0; continue; }
			}
		}
		while( Length == 0 && ( NET_UpdateTime() - StartTime ) < 2.5 );

		if( Length )
			break;

		Con_Printf( "still trying...\n");
		SCR_UpdateScreen ();
		StartTime	= NET_UpdateTime();
	}

	if( Length == 0 )
	{
		strcpy( g_ReturnReason, "No Response" );
		Con_Printf( "%s\n", g_ReturnReason );

		NET_FreeQSocket( pSocket );
		NET_GetLanDriver()->CloseSocket( NewSocket );

		if( g_ReturnOnError )
		{
			qCInput::SetKeyDestination( qMENU );
			g_ReturnOnError	= FALSE;
		}

		return NULL;
	}

	if( Length == -1 )
	{
		strcpy( g_ReturnReason, "Network Error" );
		Con_Printf( "%s\n", g_ReturnReason );

		NET_FreeQSocket( pSocket );
		NET_GetLanDriver()->CloseSocket( NewSocket );

		if( g_ReturnOnError )
		{
			qCInput::SetKeyDestination( qMENU );
			g_ReturnOnError	= FALSE;
		}

		return NULL;
	}

	qUInt32	Message	= qCMessage::ReadByte();

	if( Message == CCREP_REJECT )
	{
		snprintf( g_ReturnReason, sizeof( g_ReturnReason ), "%s", qCMessage::ReadString() );
		Con_Printf( "%s\n", g_ReturnReason );

		NET_FreeQSocket( pSocket );
		NET_GetLanDriver()->CloseSocket( NewSocket );

		if( g_ReturnOnError )
		{
			qCInput::SetKeyDestination( qMENU );
			g_ReturnOnError	= FALSE;
		}

		return NULL;
	}

	if( Message == CCREP_ACCEPT )
	{
		memcpy( &pSocket->SocketAddress, &SendAddress, sizeof( qSSocketAddress ) );
		NET_GetLanDriver()->SetSocketPort( &pSocket->SocketAddress, qCMessage::ReadLong() );

		if( Length >  9 )	pSocket->Mod		= qCMessage::ReadByte();
		else				pSocket->Mod		= MOD_NONE;
		if( Length > 10 )	pSocket->ModVersion	= qCMessage::ReadByte();
		else				pSocket->ModVersion	= 0;
		if( Length > 11 )	pSocket->ModFlags	= qCMessage::ReadByte();
		else				pSocket->ModFlags	= 0;
	}
	else
	{
		strcpy( g_ReturnReason, "Bad Response" );
		Con_Printf( "%s\n", g_ReturnReason );

		NET_FreeQSocket( pSocket );
		NET_GetLanDriver()->CloseSocket( NewSocket );

		if( g_ReturnOnError )
		{
			qCInput::SetKeyDestination( qMENU );
			g_ReturnOnError	= FALSE;
		}

		return NULL;
	}

	NET_GetLanDriver()->GetNameFromAddress( &SendAddress, pSocket->Address );

	Con_Printf( "Connection accepted\n" );
	pSocket->LastMessageTime	= NET_UpdateTime();

	if( pSocket->Mod == MOD_PROQUAKE && pSocket->ModVersion >= 34 )
	{
		ClientSocket	= NET_GetLanDriver()->OpenSocket( 0 );

		if( ClientSocket == -1 )
		{
			NET_FreeQSocket( pSocket );
			NET_GetLanDriver()->CloseSocket( NewSocket );

			if( g_ReturnOnError )
			{
				qCInput::SetKeyDestination( qMENU );
				g_ReturnOnError	= FALSE;
			}
		}

		NET_GetLanDriver()->CloseSocket( NewSocket );

		NewSocket		= ClientSocket;
		pSocket->Socket	= NewSocket;
	}

	g_ReturnOnError	= FALSE;

	return pSocket;

}	// Connect

/*
===================
CheckNewConnections
===================
*/
qSSocket* qCNetworkDatagram::CheckNewConnections( void )
{
	if( !NET_GetLanDriver()->Initialized )
		return NULL;

	qSSocket*		pSocket;
	qSSocketAddress	ClientAddress;
	qSSocketAddress	NewAddress;
	qSInt32			AcceptSocket;
	qSInt32			NewSocket;
	qSInt32			Control;
	qSInt32			ReturnValue;
	qUInt32			Length;
	qUInt32			Command;

	if( ( AcceptSocket = NET_GetLanDriver()->CheckNewConnections() ) == -1 )
		return NULL;

	SZ_Clear( &net_message );

	if( ( Length = NET_GetLanDriver()->Read( AcceptSocket, net_message.data, net_message.maxsize, &ClientAddress ) ) < sizeof( qSInt32 ) )
		return NULL;

	net_message.cursize	= Length;

	qCMessage::BeginReading();
	Control	= qCEndian::BigInt32( *( ( qSInt32* )net_message.data ) );
	qCMessage::ReadLong();

	if( Control == -1 )												return NULL;
	if( ( Control & ( ~NET_FLAG_LENGTH_MASK ) ) !=  NET_FLAG_CTL )	return NULL;
	if( ( Control & NET_FLAG_LENGTH_MASK ) != Length )				return NULL;

	Command	= qCMessage::ReadByte();

	if( Command == CCREQ_SERVER_INFO )
	{
		if( strcmp( qCMessage::ReadString(), "QUAKE" ) != 0 )
			return NULL;

		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREP_SERVER_INFO );
		NET_GetLanDriver()->GetSocketAddress( AcceptSocket, &NewAddress );
		qCMessage::WriteString( &net_message, NET_GetLanDriver()->AddressToString( &NewAddress ) );
		qCMessage::WriteString( &net_message, hostname.pString );
		qCMessage::WriteString( &net_message, sv.name );
		qCMessage::WriteByte( &net_message, net_activeconnections );
		qCMessage::WriteByte( &net_message, svs.maxclients );
		qCMessage::WriteByte( &net_message, NET_PROTOCOL_VERSION );
		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
		SZ_Clear( &net_message );
		return NULL;
	}

	if( Command == CCREQ_PLAYER_INFO )
	{
		client_t*	pClient;
		qSInt32		ActiveNumber	= -1;
		qUInt32		PlayerNumber	= qCMessage::ReadByte();
		qUInt32		ClientNumber;

		for( ClientNumber=0, pClient=svs.clients; ClientNumber<svs.maxclients; ClientNumber++, pClient++ )
		{
			if( pClient->active )
			{
				ActiveNumber++;

				if( ActiveNumber == PlayerNumber )
					break;
			}
		}

		if( ClientNumber == svs.maxclients )
			return NULL;

		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREP_PLAYER_INFO );
		qCMessage::WriteByte( &net_message, PlayerNumber );
		qCMessage::WriteString( &net_message, pClient->name );
		qCMessage::WriteLong( &net_message, pClient->colors );
		qCMessage::WriteLong( &net_message, ( qSInt32 )pClient->edict->v.frags );
		qCMessage::WriteLong( &net_message, ( qSInt32 )( NET_GetTime() - pClient->netconnection->ConnectTime ) );
		qCMessage::WriteString( &net_message, pClient->netconnection->Address );
		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
		SZ_Clear( &net_message );

		return NULL;
	}

	if( Command == CCREQ_RULE_INFO )
	{
		qChar*	pPreviousCVarName	= qCMessage::ReadString();
		qSCVar*	pCVar;

		if( *pPreviousCVarName )
		{
			pCVar	= qCCVar::FindVariable( pPreviousCVarName );

			if( !pCVar )
				return NULL;

			pCVar	= pCVar->pNext;
		}
		else
			pCVar	= qCCVar::FirstVariable();

		// search for the next server cvar
		while( pCVar )
		{
			if( pCVar->Server )
				break;

			pCVar	= pCVar->pNext;
		}

		// send the response

		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREP_RULE_INFO );

		if( pCVar )
		{
			qCMessage::WriteString( &net_message, pCVar->pName );
			qCMessage::WriteString( &net_message, pCVar->pString );
		}

		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
		SZ_Clear( &net_message );

		return NULL;
	}

	if( Command != CCREQ_CONNECT )
		return NULL;

	if( strcmp( qCMessage::ReadString(), "QUAKE" ) != 0 )
		return NULL;

	if( qCMessage::ReadByte() != NET_PROTOCOL_VERSION )
	{
		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREP_REJECT );
		qCMessage::WriteString( &net_message, "Incompatible version.\n" );
		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
		SZ_Clear( &net_message );

		return NULL;
	}

	// see if this guy is already connected
	for( pSocket=NET_GetActiveSockets(); pSocket; pSocket=pSocket->pNext )
	{
		if( pSocket->Driver != NET_GetDriverLevel() )
			continue;

		if( ( ReturnValue = NET_GetLanDriver()->AddressCompare( &ClientAddress, &pSocket->SocketAddress ) ) >= 0 )
		{
			// is this a duplicate connection reqeust?
			if( ReturnValue == 0 && NET_GetTime() - pSocket->ConnectTime < 2.0 )
			{
				// yes, so send a duplicate reply
				SZ_Clear( &net_message );
				qCMessage::WriteLong( &net_message, 0 );
				qCMessage::WriteByte( &net_message, CCREP_ACCEPT );
				NET_GetLanDriver()->GetSocketAddress( pSocket->Socket, &NewAddress );
				qCMessage::WriteLong( &net_message, NET_GetLanDriver()->GetSocketPort( &NewAddress ) );
				*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
				NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
				SZ_Clear( &net_message );

				return NULL;
			}

			// it's somebody coming back in from a crash/disconnect so close the old qsocket and let their retry get them back in
			NET_Close( pSocket );
			return NULL;
		}
	}

	// allocate a QSocket
	if( ( pSocket = NET_NewQSocket() ) == NULL )
	{
		// no room; try to let him know
		SZ_Clear( &net_message );
		qCMessage::WriteLong( &net_message, 0 );
		qCMessage::WriteByte( &net_message, CCREP_REJECT );
		qCMessage::WriteString( &net_message, "Server is full.\n" );
		*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
		NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
		SZ_Clear( &net_message );

		return NULL;
	}

	// allocate a network socket
	if( ( NewSocket = NET_GetLanDriver()->OpenSocket( 0 ) ) == -1 )
	{
		NET_FreeQSocket( pSocket );
		return NULL;
	}

	// everything is allocated, just fill in the details
	if( Length > 12 )	pSocket->Mod		= qCMessage::ReadByte();
	else				pSocket->Mod		= MOD_NONE;
	if( Length > 13 )	pSocket->ModVersion	= qCMessage::ReadByte();
	else				pSocket->ModVersion	= 0;
	if( Length > 14 )	pSocket->ModFlags	= qCMessage::ReadByte();
	else				pSocket->ModFlags	= 0;	

	if( pSocket->Mod == MOD_PROQUAKE && pSocket->ModVersion >= 34 )
		pSocket->NetWait	= TRUE;

	pSocket->Socket			= NewSocket;
	pSocket->SocketAddress	= ClientAddress;
	strcpy( pSocket->Address, NET_GetLanDriver()->AddressToString( &ClientAddress ) );

	// send him back the info about the server connection he has been allocated
	SZ_Clear( &net_message );
	qCMessage::WriteLong( &net_message, 0 );
	qCMessage::WriteByte( &net_message, CCREP_ACCEPT );
	NET_GetLanDriver()->GetSocketAddress( NewSocket, &NewAddress );
	qCMessage::WriteLong( &net_message, NET_GetLanDriver()->GetSocketPort( &NewAddress ) );
	qCMessage::WriteByte( &net_message, MOD_PROQUAKE );
	qCMessage::WriteByte( &net_message, 10 * MOD_PROQUAKE_VERSION );
	qCMessage::WriteByte( &net_message, 0 );
	*( ( qSInt32* )net_message.data )	= qCEndian::BigInt32( NET_FLAG_CTL | ( net_message.cursize & NET_FLAG_LENGTH_MASK ) );
	NET_GetLanDriver()->Write( AcceptSocket, net_message.data, net_message.cursize, &ClientAddress );
	SZ_Clear( &net_message );

	return pSocket;

}	// CheckNewConnections

/*
==========
GetMessage
==========
*/
qSInt32 qCNetworkDatagram::GetMessage( qSSocket* pSocket )
{
	qSSocketAddress	ReadAddress;
	qSInt32			ReturnValue	= 0;
	qUInt32			Length;
	qUInt32			Flags;
	qUInt32			Sequence;

	if( !pSocket->CanSend )
		if( ( NET_GetTime() - pSocket->LastSendTime ) > 1.0 )
			ReSendMessage( pSocket );

	while( 1 )
	{
		Length	= NET_GetLanDriver()->Read( pSocket->Socket, ( qUInt8* )&g_PacketBuffer, NET_DATAGRAMSIZE, &ReadAddress );

		if( Length == 0 )
			break;

		if( Length == -1 )
		{
			Con_Printf( "Read error\n" );
			return -1;
		}

		if( !pSocket->NetWait && NET_GetLanDriver()->AddressCompare( &ReadAddress, &pSocket->SocketAddress ) != 0 )
			continue;

		if( Length < NET_HEADERSIZE )
		{
			++g_NumShortPacketCount;
			continue;
		}

		Length	 = qCEndian::BigInt32( g_PacketBuffer.Length );
		Flags	 = Length & ( ~NET_FLAG_LENGTH_MASK );
		Length	&= NET_FLAG_LENGTH_MASK;

		if( Length > NET_DATAGRAMSIZE )
		{
			Con_Printf( "Invalid length\n" );
			return -1;
		}

		if( Flags & NET_FLAG_CTL )
			continue;

		Sequence	= qCEndian::BigInt32( g_PacketBuffer.Sequence );
		++g_NumPacketsReceived;

		if( pSocket->NetWait )
		{
			pSocket->SocketAddress	= ReadAddress;
			strcpy( pSocket->Address, NET_GetLanDriver()->AddressToString( &ReadAddress ) );
			pSocket->NetWait		= FALSE;
		}

		if( Flags & NET_FLAG_UNRELIABLE )
		{
			if( Sequence < pSocket->UnreliableReceiveSequence )
			{
				Con_DPrintf( "Got a stale datagram\n" );
				ReturnValue	= 0;
				break;
			}

			if( Sequence != pSocket->UnreliableReceiveSequence )
			{
				g_NumDroppedDatagrams	+= Sequence - pSocket->UnreliableReceiveSequence;
				Con_DPrintf( "Dropped %u datagram(s)\n", Sequence - pSocket->UnreliableReceiveSequence );
			}

			pSocket->UnreliableReceiveSequence	 = Sequence + 1;
			Length								-= NET_HEADERSIZE;

			SZ_Clear( &net_message );
			SZ_Write( &net_message, g_PacketBuffer.Data, Length );

			ReturnValue	= 2;

			break;
		}

		if( Flags & NET_FLAG_ACK )
		{
			if( Sequence != ( pSocket->SendSequence - 1 ) )
			{
				Con_DPrintf( "Stale ACK received\n" );
				continue;
			}

			if( Sequence == pSocket->ACKSequence )
			{
				pSocket->ACKSequence++;

				if( pSocket->ACKSequence != pSocket->SendSequence )
					Con_DPrintf( "ACK sequencing error\n" );
			}
			else
			{
				Con_DPrintf( "Duplicate ACK received\n" );
				continue;
			}

			pSocket->SendMessageLength	-= MAX_DATAGRAM;

			if( pSocket->SendMessageLength > 0 )
			{
				memcpy( pSocket->SendMessageBuffer, pSocket->SendMessageBuffer + MAX_DATAGRAM, pSocket->SendMessageLength );
				pSocket->SendNext	= TRUE;
			}
			else
			{
				pSocket->SendMessageLength	= 0;
				pSocket->CanSend			= TRUE;
			}

			continue;
		}

		if( Flags & NET_FLAG_DATA )
		{
			g_PacketBuffer.Length	= qCEndian::BigInt32( NET_HEADERSIZE | NET_FLAG_ACK );
			g_PacketBuffer.Sequence	= qCEndian::BigInt32( Sequence );

			NET_GetLanDriver()->Write( pSocket->Socket, ( qUInt8* )&g_PacketBuffer, NET_HEADERSIZE, &ReadAddress );

			if( Sequence != pSocket->ReceiveSequence )
			{
				++g_NumReceivedDuplicateCount;
				continue;
			}

			++pSocket->ReceiveSequence;

			Length	-= NET_HEADERSIZE;

			if( Flags & NET_FLAG_EOM )
			{
				SZ_Clear( &net_message);
				SZ_Write( &net_message, pSocket->ReceiveMessageBuffer, pSocket->ReceiveMessageLength );
				SZ_Write( &net_message, g_PacketBuffer.Data, Length );

				pSocket->ReceiveMessageLength	= 0;
				ReturnValue						= 1;

				break;
			}

			memcpy( pSocket->ReceiveMessageBuffer + pSocket->ReceiveMessageLength, g_PacketBuffer.Data, Length );
			pSocket->ReceiveMessageLength	+= Length;

			continue;
		}
	}

	if( pSocket->SendNext )
		SendMessageNext( pSocket );

	return ReturnValue;

}	// GetMessage

/*
===========
SendMessage
===========
*/
qSInt32 qCNetworkDatagram::SendMessage( qSSocket* pSocket, sizebuf_t* pData )
{
	qUInt32	PacketLength;
	qUInt32	DataLength;
	qUInt32	EOM;

	memcpy( pSocket->SendMessageBuffer, pData->data, pData->cursize );
	pSocket->SendMessageLength	= pData->cursize;

	if( pData->cursize <= MAX_DATAGRAM )
	{
		DataLength	= pData->cursize;
		EOM			= NET_FLAG_EOM;
	}
	else
	{
		DataLength	= MAX_DATAGRAM;
		EOM			= 0;
	}

	PacketLength	= NET_HEADERSIZE + DataLength;

	g_PacketBuffer.Length	= qCEndian::BigInt32( PacketLength | ( NET_FLAG_DATA | EOM ) );
	g_PacketBuffer.Sequence	= qCEndian::BigInt32( pSocket->SendSequence++ );
	memcpy( g_PacketBuffer.Data, pSocket->SendMessageBuffer, DataLength );

	pSocket->CanSend	= FALSE;

	if( NET_GetLanDriver()->Write( pSocket->Socket, ( qUInt8* )&g_PacketBuffer, PacketLength, &pSocket->SocketAddress ) == -1 )
		return -1;

	pSocket->LastSendTime	= NET_GetTime();
	g_NumPacketsSent++;

	return 1;

}	// SendMessage

/*
=====================
SendUnreliableMessage
=====================
*/
qSInt32 qCNetworkDatagram::SendUnreliableMessage( qSSocket* pSocket, sizebuf_t* pData )
{
	qUInt32	PacketLength	= NET_HEADERSIZE + pData->cursize;

	g_PacketBuffer.Length	= qCEndian::BigInt32( PacketLength | NET_FLAG_UNRELIABLE );
	g_PacketBuffer.Sequence	= qCEndian::BigInt32( pSocket->UnreliableSendSequence++ );
	memcpy( g_PacketBuffer.Data, pData->data, pData->cursize );

	if( NET_GetLanDriver()->Write( pSocket->Socket, ( qUInt8* )&g_PacketBuffer, PacketLength, &pSocket->SocketAddress ) == -1 )
		return -1;

	g_NumPacketsSent++;

	return 1;

}	// SendUnreliableMessage

/*
==============
CanSendMessage
==============
*/
qBool qCNetworkDatagram::CanSendMessage( qSSocket* pSocket )
{
	if( pSocket->SendNext )
		SendMessageNext( pSocket );

	return pSocket->CanSend;

}	// CanSendMessage

/*
=====
Close
=====
*/
void qCNetworkDatagram::Close( qSSocket* pSocket )
{
	NET_GetLanDriver()->CloseSocket( pSocket->Socket );

}	// Close

/*
========
Shutdown
========
*/
void qCNetworkDatagram::Shutdown( void )
{
	if( NET_GetLanDriver()->Initialized )
	{
		NET_GetLanDriver()->Shutdown();
		NET_GetLanDriver()->Initialized	= FALSE;
	}

}	// Shutdown

/*
================
SetReturnOnError
================
*/
void qCNetworkDatagram::SetReturnOnError( const qBool ReturnOnError )
{
	g_ReturnOnError	= ReturnOnError;

}	// SetReturnOnError

/*
===============
SetReturnReason
===============
*/
void qCNetworkDatagram::SetReturnReason( const qChar* pReturnReason )
{
	snprintf( g_ReturnReason, sizeof( g_ReturnReason ), "%s", pReturnReason );

}	// SetReturnReason

/*
===============
GetReturnReason
===============
*/
qChar* qCNetworkDatagram::GetReturnReason( void )
{
	return g_ReturnReason;

}	// GetReturnReason
