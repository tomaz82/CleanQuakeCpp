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

#ifndef __QCMATH_H__
#define __QCMATH_H__

#include	"qTypes.h"

#define Q_PI					3.14159265358979323846
#define	Q_IS_NAN( X )			( ( ( *(qSInt32* ) & X ) & ( 255 << 23 ) ) == 255 << 23 )
#define	Q_RAD_TO_DEG( Angle )	( ( Angle ) * ( 180.0 / Q_PI ) )
#define	Q_DEG_TO_RAD( Angle )	( ( Angle ) * ( Q_PI / 180.0 ) )

class qCMath
{
public:

	// Constructor / Destructor
	explicit qCMath	( void )	{ }	/**< Constructor */
			~qCMath	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	#define VectorClear( Out )	\
	(							\
		Out[ 0 ]	= 0,		\
		Out[ 1 ]	= 0,		\
		Out[ 2 ]	= 0			\
	)

	#define VectorSet( In1, In2, In3, Out )	\
	(										\
		Out[ 0 ]	= In1,					\
		Out[ 1 ]	= In2,					\
		Out[ 2 ]	= In3					\
	)

	#define VectorCopy( In, Out )	\
	(								\
		Out[ 0 ]	= In[ 0 ],		\
		Out[ 1 ]	= In[ 1 ],		\
		Out[ 2 ]	= In[ 2 ]		\
	)

	#define VectorAdd( In1, In2, Out )		\
	(										\
		Out[ 0 ]	= In1[ 0 ] + In2[ 0 ],	\
		Out[ 1 ]	= In1[ 1 ] + In2[ 1 ],	\
		Out[ 2 ]	= In1[ 2 ] + In2[ 2 ]	\
	)

	#define VectorSubtract( In1, In2, Out )	\
	(										\
		Out[ 0 ]	= In1[ 0 ] - In2[ 0 ],	\
		Out[ 1 ]	= In1[ 1 ] - In2[ 1 ],	\
		Out[ 2 ]	= In1[ 2 ] - In2[ 2 ]	\
	)

	#define VectorScale( In, Scalar, Out )	\
	(										\
		Out[ 0 ]	= In[ 0 ] * Scalar,		\
		Out[ 1 ]	= In[ 1 ] * Scalar,		\
		Out[ 2 ]	= In[ 2 ] * Scalar		\
	)

	#define VectorNegate( Out )		\
	(								\
		Out[ 0 ]	= -Out[ 0 ],	\
		Out[ 1 ]	= -Out[ 1 ],	\
		Out[ 2 ]	= -Out[ 2 ]		\
	)

	#define VectorDot( In1, In2 )	\
	(								\
		In1[ 0 ] * In2[ 0 ] +		\
		In1[ 1 ] * In2[ 1 ] +		\
		In1[ 2 ] * In2[ 2 ]			\
	)

	#define VectorCross( In1, In2, Out )							\
	(																\
		Out[ 0 ]	= In1[ 1 ] * In2[ 2 ] - In1[ 2 ] * In2[ 1 ],	\
		Out[ 1 ]	= In1[ 2 ] * In2[ 0 ] - In1[ 0 ] * In2[ 2 ],	\
		Out[ 2 ]	= In1[ 0 ] * In2[ 1 ] - In1[ 1 ] * In2[ 0 ]		\
	)

	#define VectorMultiplyAdd( In1, Scalar, In2, Out )	\
	(													\
		Out[ 0 ]	= In1[ 0 ] + ( Scalar * In2[ 0 ] ),	\
		Out[ 1 ]	= In1[ 1 ] + ( Scalar * In2[ 1 ] ),	\
		Out[ 2 ]	= In1[ 2 ] + ( Scalar * In2[ 2 ] )	\
	)

	#define VectorCompare( In1, In2 )	\
	(									\
		( In1[ 0 ] == In2[ 0 ] ) &		\
		( In1[ 1 ] == In2[ 1 ] ) &		\
		( In1[ 2 ] == In2[ 2 ] )		\
	)

	#define VectorLength( In )		\
	(								\
		sqrt( VectorDot( In, In ) )	\
	)

	#define BOX_ON_PLANE_SIDE( Min, Max, Plane )				\
	( ( ( Plane )->type < 3 ) ?									\
	(															\
		( ( Plane )->dist <= ( Min )[ ( Plane )->type ] ) ?		\
			1													\
		:														\
		(														\
			( ( Plane )->dist >= ( Max )[ ( Plane )->type ] ) ?	\
				2												\
			:													\
				3												\
		)														\
	)															\
	: qCMath::BoxOnPlaneSide( ( Min ), ( Max ), ( Plane ) ) )

	static qFloat	Clamp			( const qFloat Min, const qFloat Num, const qFloat Max );
	static qFloat	AngleMod		( const qFloat Angle );
	static qFloat	AngleDelta		( const qFloat Angle );
	static void		AngleVectors	( const qVector3 Angles, qVector3 Forward, qVector3 Right, qVector3 Up );
	static qFloat	VectorNormalize	( qVector3 In );
	static void		ConcatRotations ( qMatrix3x3 In1, qMatrix3x3 In2, qMatrix3x3 Out );
	static qUInt32	BoxOnPlaneSide	( const qVector3 Min, const qVector3 Max, struct mplane_s* pPlane );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCMath

#endif // __QCMATH_H__
