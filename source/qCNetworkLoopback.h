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

#ifndef __QCNETWORKLOOPBACK_H__
#define __QCNETWORKLOOPBACK_H__

#include	"qTypes.h"
#include	"qCNetwork.h"

class qCNetworkLoopback
{
public:

	// Constructor / Destructor
	explicit qCNetworkLoopback	( void )	{ }	/**< Constructor */
			~qCNetworkLoopback	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static qSInt32		Init					( void );
	static void			Listen					( const qBool State );
	static void			SearchForHosts			( const qBool Transmit );
	static qSSocket*	Connect					( const qChar* pHost );
	static qSSocket*	CheckNewConnections		( void );
	static qSInt32		GetMessage				( qSSocket* pSocket );
	static qSInt32		SendMessage				( qSSocket* pSocket, sizebuf_t* pData );
	static qSInt32		SendUnreliableMessage	( qSSocket* pSocket, sizebuf_t* pData );
	static qBool		CanSendMessage			( qSSocket* pSocket );
	static void			Close					( qSSocket* pSocket );
	static void			Shutdown				( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCNetworkLoopback

#endif // __QCNETWORKLOOPBACK_H__
