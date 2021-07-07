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

#ifndef __QCCLIENT_H__
#define __QCCLIENT_H__

#include	"qTypes.h"

extern qSCVar	g_CVarClientName;
extern qSCVar	g_CVarClientColor;

extern qSCVar	g_CVarClientSpeedForward;
extern qSCVar	g_CVarClientSpeedBack;
extern qSCVar	g_CVarClientSpeedSide;

typedef struct qSLightStyle
{
	qChar	Map[ 64 ];
	qUInt32	Length;
} qSLightStyle;

extern qSLightStyle	g_LightStyles[ MAX_LIGHTSTYLES ];

class qCClient
{
public:

	// Constructor / Destructor
	explicit qCClient	( void )	{ }	/**< Constructor */
			~qCClient	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void			DisconnectCB		( void );
	static void			Init				( void );
	static void			EstablishConnection	( const qChar* pHost );
	static dlight_t*	CreateDynamicLight	( const qUInt32 Key );
	static void			DecayDynamicLights	( void );
	static qSInt32		ReadFromServer		( void );
	static void			SendCommand			( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCClient

#endif // __QCCLIENT_H__
