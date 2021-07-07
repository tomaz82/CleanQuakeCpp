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

#ifndef __QCNETWORK_H__
#define __QCNETWORK_H__

#include	"qTypes.h"

#define	NET_NAMELEN				64
#define NET_MAXMESSAGE			8192

#define	MOD_NONE				0x00
#define	MOD_PROQUAKE			0x01
#define MOD_PROQUAKE_VERSION	3.50

typedef struct qSSocketAddress
{
	qSInt16	Family;
	qUInt8	Data[ 14 ];
} qSSocketAddress;

typedef struct qSSocket
{
	qSSocket*		pNext;
	qDouble			ConnectTime;
	qDouble			LastMessageTime;
	qDouble			LastSendTime;

	qBool			Disconnected;
	qBool			CanSend;
	qBool			SendNext;

	qSInt32			Driver;
	qSInt32			Socket;
	void*			pDriverData;

	qUInt32			ACKSequence;
	qUInt32			SendSequence;
	qUInt32			UnreliableSendSequence;
	qSInt32			SendMessageLength;
	qUInt8			SendMessageBuffer[ NET_MAXMESSAGE ];

	qUInt32			ReceiveSequence;
	qUInt32			UnreliableReceiveSequence;
	qSInt32			ReceiveMessageLength;
	qUInt8			ReceiveMessageBuffer[ NET_MAXMESSAGE ];

	qSSocketAddress	SocketAddress;
	qChar			Address[ NET_NAMELEN ];

	qUInt8			Mod;
	qUInt8			ModVersion;
	qUInt8			ModFlags;
	qBool			NetWait;
} qSSocket;

typedef struct qSNetworkDriver
{
	qBool		Initialized;
	qSInt32		( *Init )					( void );
	void		( *Listen )					( const qBool State );
	void		( *SearchForHosts )			( const qBool Transmit );
	qSSocket*	( *Connect )				( const qChar* pHost );
	qSSocket*	( *CheckNewConnections )	( void );
	qSInt32		( *GetMessage )				( qSSocket* pSocket );
	qSInt32		( *SendMessage )			( qSSocket* pSocket, sizebuf_t* pData );
	qSInt32		( *SendUnreliableMessage )	( qSSocket* pSocket, sizebuf_t* pData );
	qBool		( *CanSendMessage )			( qSSocket* pSocket );
	void		( *Close )					( qSSocket* pSocket );
	void		( *Shutdown )				( void );
} qSNetworkDriver;

typedef struct qSNetworkLanDriver
{
	qBool	Initialized;
	qSInt32	ControlSocket;
	qSInt32	( *Init )					( void );
	void	( *Shutdown )				( void );
	void	( *Listen )					( const qBool State );
	qSInt32 ( *OpenSocket )				( const qSInt32 Port );
	qSInt32 ( *CloseSocket )			( const qSInt32 Socket );
	qSInt32 ( *CheckNewConnections )	( void );
	qSInt32 ( *Read )					( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length, const qSSocketAddress* pAddress );
	qSInt32 ( *Write )					( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length, const qSSocketAddress* pAddress );
	qSInt32 ( *Broadcast )				( const qSInt32 Socket, qUInt8* pBuffer, const qUInt32 Length );
	qChar*	( *AddressToString )		( const qSSocketAddress* pAddress );
	void 	( *GetSocketAddress )		( const qSInt32 Socket, qSSocketAddress* pAddress );
	void 	( *GetNameFromAddress )		( const qSSocketAddress* pAddress, qChar* pName );
	qSInt32 ( *GetAddressFromName )		( const qChar* pName, qSSocketAddress* pAddress );
	qSInt32	( *AddressCompare )			( const qSSocketAddress* pAddress1, const qSSocketAddress* pAddress2 );
	qSInt32	( *GetSocketPort )			( const qSSocketAddress* pAddress );
	void	( *SetSocketPort )			( qSSocketAddress* pAddress, const qSInt32 Port );
} qSNetworkLanDriver;

typedef struct qSNetworkHostCache
{
	qChar			Name[ 16 ];
	qChar			Map[ 16 ];
	qChar			CName[ 32 ];
	qSInt32			Users;
	qSInt32			MaxUsers;
	qSInt32			Driver;
	qSSocketAddress	Address;
} qSNetworkHostCache;

typedef struct qSNetworkPollProcedure
{
	qSNetworkPollProcedure* pNext;
	qDouble					NextTime;
	void					( *pProcedure )( void );
} qsNetworkPollProcedure;

#include	"net.h"

class qCNetwork
{
public:

	// Constructor / Destructor
	explicit qCNetwork	( void )	{ }	/**< Constructor */
			~qCNetwork	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////

private:

};	// qCNetwork

qSSocket*			NET_NewQSocket				( void );
void				NET_FreeQSocket				( qSSocket* pSocket );
qDouble				NET_UpdateTime				( void );
void				NET_Init					( void );
void				NET_Shutdown				( void );
qSSocket*			NET_CheckNewConnections		( void );
qSSocket*			NET_Connect					( const qChar* pHost );
qBool				NET_CanSendMessage			( qSSocket* pSocket );
qSInt32				NET_GetMessage				( qSSocket* pSocket );
qSInt32				NET_SendMessage				( qSSocket* pSocket, sizebuf_t* pData );
qSInt32				NET_SendUnreliableMessage	( qSSocket* pSocket, sizebuf_t* pData );
qSInt32				NET_SendToAll				( sizebuf_t* pData, qSInt32 BlockTime );
void				NET_Close					( qSSocket* pSocket );
qSSocket*			NET_GetActiveSockets		( void );
qSSocket*			NET_GetFreeSockets			( void );
qSNetworkLanDriver*	NET_GetLanDriver			( void );
qUInt32				NET_GetDriverLevel			( void );
void				NET_SetHostPort				( const qUInt32 HostPort );
qUInt32				NET_GetHostPort				( void );
qUInt32				NET_GetDefaultHostPort		( void );
void				NET_SetTCPIPAvailable		( const qBool TCPIPAvailable );
qBool				NET_GetTCPIPAvailable		( void );
qChar*				NET_GetMyTCPIPAddress		( void );
qDouble				NET_GetTime					( void );
void				NET_Poll					( void );
void				NET_SchedulePollProcedure	( qsNetworkPollProcedure* pPollProcedure, qDouble TimeOffset );
void				NET_Slist_f					( void );

#endif // __QCNETWORK_H__
