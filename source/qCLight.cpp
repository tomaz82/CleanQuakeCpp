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
#include	"qCLight.h"
#include	"client.h"
#include	"qCClient.h"
#include	"glquake.h"

/*
=======
Animate
=======
*/
void qCLight::Animate( void )
{
	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	const qUInt32	Time	= ( qUInt32 )( cl.time * 10.0 );

	for( qUInt32 LightStyleIndex=0; LightStyleIndex<MAX_LIGHTSTYLES; ++LightStyleIndex )
	{
		if( !g_LightStyles[ LightStyleIndex ].Length )
		{
			d_lightstylevalue[ LightStyleIndex ] = 256;
			continue;
		}

		d_lightstylevalue[ LightStyleIndex ] = ( g_LightStyles[ LightStyleIndex ].Map[ Time % g_LightStyles[ LightStyleIndex ].Length ] - 'a' ) * 22;
	}

}	// Animate
