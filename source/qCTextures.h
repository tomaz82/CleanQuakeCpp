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

#ifndef __QCTEXTURES_H__
#define __QCTEXTURES_H__

#include	"qTypes.h"
#include	"bspfile.h"

typedef struct qSTexture
{
	qChar		Name[ 16 ];
	qUInt32		Width;
	qUInt32		Height;
	qUInt32		TextureIndex;
	qUInt32		AnimTotal;				// total tenths in sequence ( 0 = no)
	qUInt32		AnimMin;
	qUInt32		AnimMax;				// time for this frame min <= time < max
	qSTexture*	pAnimNext;				// in the animation sequence
	qSTexture*	pAlternateAnims;		// bmodels in frame 1 use these
	qUInt32		Offsets[ MIPLEVELS ];	// four mip maps stored
} qSTexture;

class qCTextures
{
public:

	// Constructor / Destructor
	explicit qCTextures	( void )	{ }	/**< Constructor */
			~qCTextures	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void			Init					( void );
	static void			TranslatePlayerSkin		( const qUInt32 PlayerNum );
	static qSTexture*	Animation				( qSTexture* pBaseTexture );
	static qUInt32		Load					( const qChar* pIdentifier, const qUInt32 Width, const qUInt32 Height, const qUInt8* pData, const qBool CreateMipmaps, const qUInt8 BytesPerPixel );
	static qSTexture*	GetCheckerTextureData	( void );
	static qUInt32		GetParticleTextureIndex	( void );
	static qUInt32		GetPlayerTextureIndex	( void );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCTextures

#endif // __QCTEXTURES_H__
