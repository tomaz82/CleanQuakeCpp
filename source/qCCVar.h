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

#ifndef __QCCVAR_H__
#define __QCCVAR_H__

#include	"qTypes.h"

typedef struct qSCVar
{
	qChar*			pName;
	qChar*			pString;
	qBool			Archive;
	qBool			Server;
	qFloat			Value;
	struct qSCVar*	pNext;
} qSCVar;

class qCCVar
{
public:

	// Constructor / Destructor
	explicit qCCVar	( void )	{ }	/**< Constructor */
			~qCCVar	( void )	{ }	/**< Destructor */

//////////////////////////////////////////////////////////////////////////

	static void 	Register			( qSCVar* pVariable );
	static void 	Set					( const qChar* pName, const qChar* pValue );
	static void		Set					( const qChar* pName, const qFloat Value );
	static qFloat	GetValue			( const qChar* pName );
	static qChar*	GetString			( const qChar* pName );
	static qSCVar*	FirstVariable		( void );
	static qSCVar*	FindVariable		( const qChar* pName );
	static qChar*	CompleteVariable	( const qChar* pPartial );
	static qUInt32	CountMatches		( const qChar* pPartial );
	static void		PrintMatches		( const qChar* pPartial );
	static qBool	Command				( void );
	static void 	WriteVariables		( const FILE* pFile );

//////////////////////////////////////////////////////////////////////////

private:

};	// qCCVar

#endif // __QCCVAR_H__
