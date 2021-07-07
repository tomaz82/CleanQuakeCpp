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
#include	"qCMath.h"

#include	"sys.h"
#include	"gl_model.h"

/*
===================
ProjectPointOnPlane
===================
*/
void ProjectPointOnPlane( qVector3 Out, const qVector3 Point, const qVector3 Normal )
{
	qFloat		InvertedDenominator	= 1.0f / VectorDot( Normal, Normal );
	qFloat		Distance			= VectorDot( Normal, Point ) * InvertedDenominator;
	qVector3	NewNormal;

	VectorScale( Normal, InvertedDenominator, NewNormal );
	VectorMultiplyAdd( Point, -Distance, NewNormal, Out );

}	// ProjectPointOnPlane

/*
===================
PerpendicularVector
===================
*/
void PerpendicularVector( qVector3 Out, const qVector3 In )
{
	qFloat		MinElem		= 1.0f;
	qUInt32		Position	= 0;
	qVector3	Temp;

	for( qUInt32 i=0; i<3; ++i )
	{
		if( fabs( In[ i ] ) < MinElem )
		{
			Position	= i;
			MinElem		= fabs( In[ i ] );
		}
	}

	Temp[ 0 ]			=
	Temp[ 1 ]			=
	Temp[ 2 ]			= 0.0f;

	Temp[ Position ]	= 1.0f;

	ProjectPointOnPlane( Out, Temp, In );

	qCMath::VectorNormalize( Out );

}	// PerpendicularVector

/*
===============
ConcatRotations
===============
*/
void ConcatRotations( qMatrix3x3 In1, qMatrix3x3 In2, qMatrix3x3 Out )
{
	Out[ 0 ][ 0 ] = In1[ 0 ][ 0 ] * In2[ 0 ][ 0 ] + In1[ 0 ][ 1 ] * In2[ 1 ][ 0 ] +	In1[ 0 ][ 2 ] * In2[ 2 ][ 0 ];
	Out[ 0 ][ 1 ] = In1[ 0 ][ 0 ] * In2[ 0 ][ 1 ] + In1[ 0 ][ 1 ] * In2[ 1 ][ 1 ] +	In1[ 0 ][ 2 ] * In2[ 2 ][ 1 ];
	Out[ 0 ][ 2 ] = In1[ 0 ][ 0 ] * In2[ 0 ][ 2 ] + In1[ 0 ][ 1 ] * In2[ 1 ][ 2 ] +	In1[ 0 ][ 2 ] * In2[ 2 ][ 2 ];
	Out[ 1 ][ 0 ] = In1[ 1 ][ 0 ] * In2[ 0 ][ 0 ] + In1[ 1 ][ 1 ] * In2[ 1 ][ 0 ] +	In1[ 1 ][ 2 ] * In2[ 2 ][ 0 ];
	Out[ 1 ][ 1 ] = In1[ 1 ][ 0 ] * In2[ 0 ][ 1 ] + In1[ 1 ][ 1 ] * In2[ 1 ][ 1 ] +	In1[ 1 ][ 2 ] * In2[ 2 ][ 1 ];
	Out[ 1 ][ 2 ] = In1[ 1 ][ 0 ] * In2[ 0 ][ 2 ] + In1[ 1 ][ 1 ] * In2[ 1 ][ 2 ] +	In1[ 1 ][ 2 ] * In2[ 2 ][ 2 ];
	Out[ 2 ][ 0 ] = In1[ 2 ][ 0 ] * In2[ 0 ][ 0 ] + In1[ 2 ][ 1 ] * In2[ 1 ][ 0 ] +	In1[ 2 ][ 2 ] * In2[ 2 ][ 0 ];
	Out[ 2 ][ 1 ] = In1[ 2 ][ 0 ] * In2[ 0 ][ 1 ] + In1[ 2 ][ 1 ] * In2[ 1 ][ 1 ] +	In1[ 2 ][ 2 ] * In2[ 2 ][ 1 ];
	Out[ 2 ][ 2 ] = In1[ 2 ][ 0 ] * In2[ 0 ][ 2 ] + In1[ 2 ][ 1 ] * In2[ 1 ][ 2 ] +	In1[ 2 ][ 2 ] * In2[ 2 ][ 2 ];

}	// ConcatRotations

/*
=======================
RotatePointAroundVector
=======================
*/
void RotatePointAroundVector( qVector3 Out, const qVector3 Direction, const qVector3 Point, qFloat Angle )
{
	qMatrix3x3	Matrix;
	qMatrix3x3	Inverted;
	qMatrix3x3	ZRotation;
	qMatrix3x3	Temp;
	qMatrix3x3	Rotation;
	qVector3	Right;
	qVector3	Up;
	qVector3	Forward;

	VectorCopy( Direction, Forward );
	PerpendicularVector( Right, Direction );
	VectorCross( Right, Forward, Up );

	VectorSet( Right[ 0 ], Up[ 0 ], Forward[ 0 ], Matrix[ 0 ] );
	VectorSet( Right[ 1 ], Up[ 1 ], Forward[ 1 ], Matrix[ 1 ] );
	VectorSet( Right[ 2 ], Up[ 2 ], Forward[ 2 ], Matrix[ 2 ] );

	VectorCopy( Right,		Inverted[ 0 ] );
	VectorCopy( Up,			Inverted[ 1 ] );
	VectorCopy( Forward,	Inverted[ 2 ] );

	memset( ZRotation, 0, sizeof( ZRotation ) );

	ZRotation[ 0 ][ 0 ]	=
	ZRotation[ 1 ][ 1 ]	= ( qFloat ) cos( Q_DEG_TO_RAD( Angle ) );
	ZRotation[ 0 ][ 1 ]	= ( qFloat ) sin( Q_DEG_TO_RAD( Angle ) );
	ZRotation[ 1 ][ 0 ]	= ( qFloat )-sin( Q_DEG_TO_RAD( Angle ) );
	ZRotation[ 2 ][ 2 ]	= 1.0f;

	ConcatRotations( Matrix, ZRotation, Temp );
	ConcatRotations( Temp, Inverted, Rotation );

	for( qUInt32 i=0; i<3; ++i )
	{
		Out[ i ]	= Rotation[ i ][ 0 ] * Point[ 0 ] +
					  Rotation[ i ][ 1 ] * Point[ 1 ] +
					  Rotation[ i ][ 2 ] * Point[ 2 ];
	}

}	// RotatePointAroundVector

//////////////////////////////////////////////////////////////////////////

/*
=====
Clamp
=====
*/
qFloat qCMath::Clamp( const qFloat Min, const qFloat Num, const qFloat Max )
{
	return min( Max, max( Num, Min ) );

}	// Clamp

/*
========
AngleMod
========
*/
qFloat qCMath::AngleMod( const qFloat Angle )
{
	return ( qFloat )( ( 360.0 / 65536.0 ) * ( ( qSInt32 )( Angle * ( 65536.0 / 360.0 ) ) & 65535 ) );

}	// AngleMod

/*
==========
AngleDelta
==========
*/
qFloat qCMath::AngleDelta( const qFloat Angle )
{
	qFloat NewAngle	= AngleMod( Angle );

	while( NewAngle > 180.0f )
		NewAngle -= 360.0f;

	return NewAngle;

}	// AngleDelta

/*
============
AngleVectors
============
*/
void qCMath::AngleVectors( const qVector3 Angles, qVector3 Forward, qVector3 Right, qVector3 Up )
{
	const qVector3	RadAngles	= { Q_DEG_TO_RAD( Angles[ PITCH ] ), Q_DEG_TO_RAD( Angles[ YAW ] ), Q_DEG_TO_RAD( Angles[ ROLL ] ) };
	const qFloat	CP			= ( qFloat )cos( Q_DEG_TO_RAD( Angles[ PITCH ] ) );
	const qFloat	SP			= ( qFloat )sin( Q_DEG_TO_RAD( Angles[ PITCH ] ) );
	const qFloat	CY			= ( qFloat )cos( Q_DEG_TO_RAD( Angles[ YAW ] ) );
	const qFloat	SY			= ( qFloat )sin( Q_DEG_TO_RAD( Angles[ YAW ] ) );
	const qFloat	CR			= ( qFloat )cos( Q_DEG_TO_RAD( Angles[ ROLL ] ) );
	const qFloat	SR			= ( qFloat )sin( Q_DEG_TO_RAD( Angles[ ROLL ] ) );

	if( Forward )
	{
		Forward[ 0 ]	=  CP * CY;
		Forward[ 1 ]	=  CP * SY;
		Forward[ 2 ]	= -SP;
	}

	if( Right )
	{
		Right[ 0 ]		= ( -SR * SP * CY + -CR * -SY );
		Right[ 1 ]		= ( -SR * SP * SY + -CR *  CY );
		Right[ 2 ]		=   -SR * CP;
	}

	if( Up )
	{
		Up[ 0 ]			= ( CR * SP * CY + -SR * -SY );
		Up[ 1 ]			= ( CR * SP * SY + -SR *  CY );
		Up[ 2 ]			=   CR * CP;
	}

}	// AngleVectors

/*
===============
VectorNormalize
===============
*/
qFloat qCMath::VectorNormalize( qVector3 In )
{
	qFloat	Length	= VectorDot( In, In );
	qFloat	Invert;

	if( Length )
	{
		Length	= sqrt( Length );
		Invert	= 1.0f / Length;
		VectorScale( In, Invert, In );
	}

	return Length;

}	// VectorNormalize

/*
==============
BoxOnPlaneSide
==============
*/
qUInt32 qCMath::BoxOnPlaneSide( const qVector3 Min, const qVector3 Max, mplane_t* pPlane )
{
	qFloat	Dist1	= 0.0f;
	qFloat	Dist2	= 0.0f;
	qUInt32	Sides	= 0;

	switch( pPlane->signbits )
	{
		case 0:
			Dist1	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
		break;
		case 1:
			Dist1	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
		break;
		case 2:
			Dist1	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
		break;
		case 3:
			Dist1	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
		break;
		case 4:
			Dist1	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
		break;
		case 5:
			Dist1	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
		break;
		case 6:
			Dist1	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
		break;
		case 7:
			Dist1	= pPlane->normal[ 0 ] * Min[ 0 ] + pPlane->normal[ 1 ] * Min[ 1 ] + pPlane->normal[ 2 ] * Min[ 2 ];
			Dist2	= pPlane->normal[ 0 ] * Max[ 0 ] + pPlane->normal[ 1 ] * Max[ 1 ] + pPlane->normal[ 2 ] * Max[ 2 ];
		break;
		default:
			Sys_Error( "BoxOnPlaneSide:  Bad signbits" );
		break;
	}

	if( Dist1 >= pPlane->dist )	Sides  = 1;
	if( Dist2  < pPlane->dist )	Sides |= 2;

	return Sides;

}	// BoxOnPlaneSide
