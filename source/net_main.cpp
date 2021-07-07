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
// net_main.c

#include	"quakedef.h"
#include	"qCNetworkLoopback.h"
#include	"qCNetworkDatagram.h"
#include	"qCNetworkWinSock.h"

#include	"sys.h"
#include	"qCHunk.h"
#include	"qCCVar.h"
#include	"cmd.h"
#include	"server.h"
#include	"console.h"

qSNetworkDriver g_Drivers[ 2 ] =
{
	// Loopback
	{
		FALSE,
		qCNetworkLoopback::Init,
		qCNetworkLoopback::Listen,
		qCNetworkLoopback::SearchForHosts,
		qCNetworkLoopback::Connect,
		qCNetworkLoopback::CheckNewConnections,
		qCNetworkLoopback::GetMessage,
		qCNetworkLoopback::SendMessage,
		qCNetworkLoopback::SendUnreliableMessage,
		qCNetworkLoopback::CanSendMessage,
		qCNetworkLoopback::Close,
		qCNetworkLoopback::Shutdown
	},
	// Datagram
	{
		FALSE,
		qCNetworkDatagram::Init,
		qCNetworkDatagram::Listen,
		qCNetworkDatagram::SearchForHosts,
		qCNetworkDatagram::Connect,
		qCNetworkDatagram::CheckNewConnections,
		qCNetworkDatagram::GetMessage,
		qCNetworkDatagram::SendMessage,
		qCNetworkDatagram::SendUnreliableMessage,
		qCNetworkDatagram::CanSendMessage,
		qCNetworkDatagram::Close,
		qCNetworkDatagram::Shutdown
	}
};

qSNetworkLanDriver g_LanDriver =
{
	// WinSock TCPIP
	FALSE,
	0,
	qCNetworkWinSock::Init,
	qCNetworkWinSock::Shutdown,
	qCNetworkWinSock::Listen,
	qCNetworkWinSock::OpenSocket,
	qCNetworkWinSock::CloseSocket,
	qCNetworkWinSock::CheckNewConnections,
	qCNetworkWinSock::Read,
	qCNetworkWinSock::Write,
	qCNetworkWinSock::Broadcast,
	qCNetworkWinSock::AddressToString,
	qCNetworkWinSock::GetSocketAddress,
	qCNetworkWinSock::GetNameFromAddress,
	qCNetworkWinSock::GetAddressFromName,
	qCNetworkWinSock::AddressCompare,
	qCNetworkWinSock::GetSocketPort,
	qCNetworkWinSock::SetSocketPort
};

static qChar		g_MyTCPIPAddress[ NET_NAMELEN ];

static qSSocket*	g_pActiveSockets	= NULL;
static qSSocket*	g_pFreeSockets		= NULL;

static qDouble		g_Time				= 0.0;

static qUInt32		g_NumSockets		= 0;
static qUInt32		g_NumDrivers		= 2;
static qUInt32		g_DriverLevel		= 0;
static qUInt32		g_HostPort			= 0;
static qUInt32		g_DefaultHostPort	= 26000;

static qBool		g_TCPIPAvailable	= FALSE;

static qBool	listening = FALSE;

qBool	slistInProgress = FALSE;
qBool	slistSilent = FALSE;
qBool	slistLocal = TRUE;
static double	slistStartTime;
static int		slistLastShown;

static void Slist_Send(void);
static void Slist_Poll(void);
qsNetworkPollProcedure	slistSendProcedure = {NULL, 0.0, Slist_Send};
qsNetworkPollProcedure	slistPollProcedure = {NULL, 0.0, Slist_Poll};


sizebuf_t		net_message;
qUInt32				net_activeconnections = 0;

int messagesSent = 0;
int messagesReceived = 0;
int unreliableMessagesSent = 0;
int unreliableMessagesReceived = 0;

qSCVar	net_messagetimeout = {"net_messagetimeout","300"};
qSCVar	hostname = {"hostname", "UNNAMED"};

qBool	configRestored = FALSE;

// these two macros are to make the code more readable
#define sfunc	g_Drivers[sock->Driver]
#define dfunc	g_Drivers[g_DriverLevel]

double NET_UpdateTime(void)
{
	g_Time = Sys_FloatTime();
	return g_Time;
}


/*
===================
NET_NewQSocket

Called by drivers when a new communications endpoint is required
The sequence and buffer fields will be filled in properly
===================
*/
qSSocket *NET_NewQSocket (void)
{
	qSSocket	*sock;

	if (g_pFreeSockets == NULL)
		return NULL;

	if (net_activeconnections >= svs.maxclients)
		return NULL;

	// get one from free list
	sock = g_pFreeSockets;
	g_pFreeSockets = sock->pNext;

	// add it to active list
	sock->pNext = g_pActiveSockets;
	g_pActiveSockets = sock;

	sock->Disconnected = FALSE;
	sock->ConnectTime = g_Time;
	strcpy (sock->Address,"UNSET ADDRESS");
	sock->Driver = g_DriverLevel;
	sock->Socket = 0;
	sock->pDriverData = NULL;
	sock->CanSend = TRUE;
	sock->SendNext = FALSE;
	sock->LastMessageTime = g_Time;
	sock->ACKSequence = 0;
	sock->SendSequence = 0;
	sock->UnreliableSendSequence = 0;
	sock->SendMessageLength = 0;
	sock->ReceiveSequence = 0;
	sock->UnreliableReceiveSequence = 0;
	sock->ReceiveMessageLength = 0;

	return sock;
}


void NET_FreeQSocket(qSSocket *sock)
{
	qSSocket	*s;

	// remove it from active list
	if (sock == g_pActiveSockets)
		g_pActiveSockets = g_pActiveSockets->pNext;
	else
	{
		for (s = g_pActiveSockets; s; s = s->pNext)
			if (s->pNext == sock)
			{
				s->pNext = sock->pNext;
				break;
			}
		if (!s)
			Sys_Error ("NET_FreeQSocket: not active\n");
	}

	// add it to free list
	sock->pNext = g_pFreeSockets;
	g_pFreeSockets = sock;
	sock->Disconnected = TRUE;
}


static void NET_Listen_f (void)
{
	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"listen\" is \"%u\"\n", listening ? 1 : 0);
		return;
	}

	listening = atoi(Cmd_Argv(1)) ? TRUE : FALSE;

	for (g_DriverLevel=0 ; g_DriverLevel<g_NumDrivers; g_DriverLevel++)
	{
		if (g_Drivers[g_DriverLevel].Initialized == FALSE)
			continue;
		dfunc.Listen (listening);
	}
}


static void MaxPlayers_f (void)
{
	qUInt32 	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"maxplayers\" is \"%u\"\n", svs.maxclients);
		return;
	}

	if (sv.active)
	{
		Con_Printf ("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = atoi(Cmd_Argv(1));
	if (n < 1)
		n = 1;
	if (n > svs.maxclientslimit)
	{
		n = svs.maxclientslimit;
		Con_Printf ("\"maxplayers\" set to \"%u\"\n", n);
	}

	if ((n == 1) && listening)
		Cbuf_AddText ("listen 0\n");

	if ((n > 1) && (!listening))
		Cbuf_AddText ("listen 1\n");

	svs.maxclients = n;
	if (n == 1)
		qCCVar::Set ("deathmatch", "0");
	else
		qCCVar::Set ("deathmatch", "1");
}


static void NET_Port_f (void)
{
	int 	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"port\" is \"%u\"\n", g_HostPort );
		return;
	}

	n = atoi(Cmd_Argv(1));
	if (n < 1 || n > 65534)
	{
		Con_Printf ("Bad value, must be between 1 and 65534\n");
		return;
	}

	g_DefaultHostPort = n;
	g_HostPort = n;

	if (listening)
	{
		// force a change to the new port
		Cbuf_AddText ("listen 0\n");
		Cbuf_AddText ("listen 1\n");
	}
}


static void PrintSlistHeader(void)
{
	Con_Printf("Server          Map             Users\n");
	Con_Printf("--------------- --------------- -----\n");
	slistLastShown = 0;
}


static void PrintSlist(void)
{
	qUInt32 n;

	for (n = slistLastShown; n < hostCacheCount; n++)
	{
		if (hostcache[n].MaxUsers)
			Con_Printf("%-15.15s %-15.15s %2u/%2u\n", hostcache[n].Name, hostcache[n].Map, hostcache[n].Users, hostcache[n].MaxUsers);
		else
			Con_Printf("%-15.15s %-15.15s\n", hostcache[n].Name, hostcache[n].Map);
	}
	slistLastShown = n;
}


static void PrintSlistTrailer(void)
{
	if (hostCacheCount)
		Con_Printf("== end list ==\n\n");
	else
		Con_Printf("No Quake servers found.\n\n");
}


void NET_Slist_f (void)
{
	if (slistInProgress)
		return;

	if (! slistSilent)
	{
		Con_Printf("Looking for Quake servers...\n");
		PrintSlistHeader();
	}

	slistInProgress = TRUE;
	slistStartTime = Sys_FloatTime();

	NET_SchedulePollProcedure(&slistSendProcedure, 0.0);
	NET_SchedulePollProcedure(&slistPollProcedure, 0.1);

	hostCacheCount = 0;
}


static void Slist_Send(void)
{
	for (g_DriverLevel=0; g_DriverLevel < g_NumDrivers; g_DriverLevel++)
	{
		if (!slistLocal && g_DriverLevel == 0)
			continue;
		if (g_Drivers[g_DriverLevel].Initialized == FALSE)
			continue;
		dfunc.SearchForHosts (TRUE);
	}

	if ((Sys_FloatTime() - slistStartTime) < 0.5)
		NET_SchedulePollProcedure(&slistSendProcedure, 0.75);
}


static void Slist_Poll(void)
{
	for (g_DriverLevel=0; g_DriverLevel < g_NumDrivers; g_DriverLevel++)
	{
		if (!slistLocal && g_DriverLevel == 0)
			continue;
		if (g_Drivers[g_DriverLevel].Initialized == FALSE)
			continue;
		dfunc.SearchForHosts (FALSE);
	}

	if (! slistSilent)
		PrintSlist();

	if ((Sys_FloatTime() - slistStartTime) < 1.5)
	{
		NET_SchedulePollProcedure(&slistPollProcedure, 0.1);
		return;
	}

	if (! slistSilent)
		PrintSlistTrailer();
	slistInProgress = FALSE;
	slistSilent = FALSE;
	slistLocal = TRUE;
}


/*
===================
NET_Connect
===================
*/

qUInt32 hostCacheCount = 0;
qSNetworkHostCache hostcache[HOSTCACHESIZE];

qSSocket *NET_Connect( const qChar *host )
{
	qSSocket		*ret;
	qUInt32				n;
	qUInt32				numdrivers = g_NumDrivers;

	NET_UpdateTime();

	if (host && *host == 0)
		host = NULL;

	if (host)
	{
		if (stricmp (host, "local") == 0)
		{
			numdrivers = 1;
			goto JustDoIt;
		}

		if (hostCacheCount)
		{
			for (n = 0; n < hostCacheCount; n++)
				if (stricmp (host, hostcache[n].Name) == 0)
				{
					host = hostcache[n].CName;
					break;
				}
			if (n < hostCacheCount)
				goto JustDoIt;
		}
	}

	slistSilent = host ? TRUE : FALSE;
	NET_Slist_f ();

	while(slistInProgress)
		NET_Poll();

	if (host == NULL)
	{
		if (hostCacheCount != 1)
			return NULL;
		host = hostcache[0].CName;
		Con_Printf("Connecting to...\n%s @ %s\n\n", hostcache[0].Name, host);
	}

	if (hostCacheCount)
		for (n = 0; n < hostCacheCount; n++)
			if (stricmp (host, hostcache[n].Name) == 0)
			{
				host = hostcache[n].CName;
				break;
			}

JustDoIt:
	for (g_DriverLevel=0 ; g_DriverLevel<numdrivers; g_DriverLevel++)
	{
		if (g_Drivers[g_DriverLevel].Initialized == FALSE)
			continue;
		ret = dfunc.Connect (host);
		if (ret)
			return ret;
	}

	if (host)
	{
		Con_Printf("\n");
		PrintSlistHeader();
		PrintSlist();
		PrintSlistTrailer();
	}

	return NULL;
}


/*
===================
NET_CheckNewConnections
===================
*/
qSSocket *NET_CheckNewConnections (void)
{
	qSSocket	*ret;

	NET_UpdateTime();

	for (g_DriverLevel=0 ; g_DriverLevel<g_NumDrivers; g_DriverLevel++)
	{
		if (g_Drivers[g_DriverLevel].Initialized == FALSE)
			continue;
		if (g_DriverLevel && listening == FALSE)
			continue;
		ret = dfunc.CheckNewConnections ();
		if (ret)
		{
			return ret;
		}
	}

	return NULL;
}

/*
===================
NET_Close
===================
*/
void NET_Close (qSSocket *sock)
{
	if (!sock)
		return;

	if (sock->Disconnected)
		return;

	NET_UpdateTime();

	// call the driver_Close function
	sfunc.Close (sock);

	NET_FreeQSocket(sock);
}


/*
=================
NET_GetMessage

If there is a complete message, return it in net_message

returns 0 if no data is waiting
returns 1 if a message was received
returns -1 if connection is invalid
=================
*/

int	NET_GetMessage (qSSocket *sock)
{
	int ret;

	if (!sock)
		return -1;

	if (sock->Disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	NET_UpdateTime();

	ret = sfunc.GetMessage(sock);

	// see if this connection has timed out
	if (ret == 0 && sock->Driver)
	{
		if (g_Time - sock->LastMessageTime > net_messagetimeout.Value)
		{
			NET_Close(sock);
			return -1;
		}
	}


	if (ret > 0)
	{
		if (sock->Driver)
		{
			sock->LastMessageTime = g_Time;
			if (ret == 1)
				messagesReceived++;
			else if (ret == 2)
				unreliableMessagesReceived++;
		}
	}

	return ret;
}


/*
==================
NET_SendMessage

Try to send a complete length+message unit over the reliable stream.
returns 0 if the message cannot be delivered reliably, but the connection
		is still considered valid
returns 1 if the message was sent properly
returns -1 if the connection died
==================
*/
int NET_SendMessage (qSSocket *sock, sizebuf_t *data)
{
	int		r;

	if (!sock)
		return -1;

	if (sock->Disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	NET_UpdateTime();
	r = sfunc.SendMessage(sock, data);
	if (r == 1 && sock->Driver)
		messagesSent++;

	return r;
}


int NET_SendUnreliableMessage (qSSocket *sock, sizebuf_t *data)
{
	int		r;

	if (!sock)
		return -1;

	if (sock->Disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	NET_UpdateTime();
	r = sfunc.SendUnreliableMessage(sock, data);
	if (r == 1 && sock->Driver)
		unreliableMessagesSent++;

	return r;
}


/*
==================
NET_CanSendMessage

Returns TRUE or FALSE if the given qsocket can currently accept a
message to be transmitted.
==================
*/
qBool NET_CanSendMessage (qSSocket *sock)
{
	int		r;

	if (!sock)
		return FALSE;

	if (sock->Disconnected)
		return FALSE;

	NET_UpdateTime();

	r = sfunc.CanSendMessage(sock);

	return r;
}


int NET_SendToAll(sizebuf_t *data, int blocktime)
{
	double		start;
	qUInt32			i;
	int			count = 0;
	qBool	state1 [MAX_SCOREBOARD];
	qBool	state2 [MAX_SCOREBOARD];

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->netconnection)
			continue;
		if (host_client->active)
		{
			if (host_client->netconnection->Driver == 0)
			{
				NET_SendMessage(host_client->netconnection, data);
				state1[i] = TRUE;
				state2[i] = TRUE;
				continue;
			}
			count++;
			state1[i] = FALSE;
			state2[i] = FALSE;
		}
		else
		{
			state1[i] = TRUE;
			state2[i] = TRUE;
		}
	}

	start = Sys_FloatTime();
	while (count)
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (! state1[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					state1[i] = TRUE;
					NET_SendMessage(host_client->netconnection, data);
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}

			if (! state2[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					state2[i] = TRUE;
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}
		}
		if ((Sys_FloatTime() - start) > blocktime)
			break;
	}
	return count;
}


//=============================================================================

/*
====================
NET_Init
====================
*/

void NET_Init (void)
{
	qSInt32		Parm;
	int			controlSocket;
	qSSocket	*s;

	Parm = COM_CheckParm ("-port");
	if (!Parm)
		Parm = COM_CheckParm ("-udpport");

	if (Parm)
	{
		if (Parm < com_argc-1)
			g_DefaultHostPort = atoi (com_argv[Parm+1]);
		else
			Sys_Error ("NET_Init: you must specify a number after -port");
	}
	g_HostPort = g_DefaultHostPort;

	if (COM_CheckParm("-listen"))
		listening = TRUE;
	g_NumSockets = svs.maxclientslimit;
	g_NumSockets++;

	NET_UpdateTime();

	for ( qUInt32 i = 0; i < g_NumSockets; i++)
	{
		s = (qSSocket *)qCHunk::AllocName(sizeof(qSSocket), "qsocket");
		s->pNext = g_pFreeSockets;
		g_pFreeSockets = s;
		s->Disconnected = TRUE;
	}

	// allocate space for network message buffer
	SZ_Alloc (&net_message, NET_MAXMESSAGE);

	qCCVar::Register (&net_messagetimeout);
	qCCVar::Register (&hostname);

	Cmd_AddCommand ("slist", NET_Slist_f);
	Cmd_AddCommand ("listen", NET_Listen_f);
	Cmd_AddCommand ("maxplayers", MaxPlayers_f);
	Cmd_AddCommand ("port", NET_Port_f);

	// initialize all the drivers
	for (g_DriverLevel=0 ; g_DriverLevel<g_NumDrivers ; g_DriverLevel++)
		{
		controlSocket = g_Drivers[g_DriverLevel].Init();
		if (controlSocket == -1)
			continue;
		g_Drivers[g_DriverLevel].Initialized = TRUE;
		if (listening)
			g_Drivers[g_DriverLevel].Listen (TRUE);
		}

	if (g_MyTCPIPAddress[ 0 ])
		Con_DPrintf("TCP/IP address %s\n", g_MyTCPIPAddress);
}

/*
====================
NET_Shutdown
====================
*/

void		NET_Shutdown (void)
{
	qSSocket	*sock;

	NET_UpdateTime();

	for (sock = g_pActiveSockets; sock; sock = sock->pNext)
		NET_Close(sock);

//
// shutdown the drivers
//
	for (g_DriverLevel = 0; g_DriverLevel < g_NumDrivers; g_DriverLevel++)
	{
		if (g_Drivers[g_DriverLevel].Initialized == TRUE)
		{
			g_Drivers[g_DriverLevel].Shutdown ();
			g_Drivers[g_DriverLevel].Initialized = FALSE;
		}
	}
}

qSSocket* NET_GetActiveSockets( void )
{
	return g_pActiveSockets;
}

qSSocket* NET_GetFreeSockets( void )
{
	return g_pFreeSockets;
}

qSNetworkLanDriver* NET_GetLanDriver( void )
{
	return &g_LanDriver;
}

qUInt32 NET_GetDriverLevel( void )
{
	return g_DriverLevel;
}

void NET_SetHostPort( const qUInt32 HostPort )
{
	g_HostPort	= HostPort;
}

qUInt32 NET_GetHostPort( void )
{
	return g_HostPort;
}

qUInt32 NET_GetDefaultHostPort( void )
{
	return g_DefaultHostPort;
}

void NET_SetTCPIPAvailable( const qBool TCPIPAvailable )
{
	g_TCPIPAvailable	= TCPIPAvailable;
}

qBool NET_GetTCPIPAvailable( void )
{
	return g_TCPIPAvailable;
}

qChar* NET_GetMyTCPIPAddress( void )
{
	return g_MyTCPIPAddress;
}

qDouble NET_GetTime( void )
{
	return g_Time;
}





static qsNetworkPollProcedure *pollProcedureList = NULL;

void NET_Poll(void)
{
	qsNetworkPollProcedure *pp;

	NET_UpdateTime();

	for (pp = pollProcedureList; pp; pp = pp->pNext)
	{
		if (pp->NextTime > g_Time)
			break;
		pollProcedureList = pp->pNext;
		pp->pProcedure();
	}
}


void NET_SchedulePollProcedure(qsNetworkPollProcedure *proc, double timeOffset)
{
	qsNetworkPollProcedure *pp, *prev;

	proc->NextTime = Sys_FloatTime() + timeOffset;
	for (pp = pollProcedureList, prev = NULL; pp; pp = pp->pNext)
	{
		if (pp->NextTime >= proc->NextTime)
			break;
		prev = pp;
	}

	if (prev == NULL)
	{
		proc->pNext = pollProcedureList;
		pollProcedureList = proc;
		return;
	}

	proc->pNext = pp;
	prev->pNext = proc;
}
