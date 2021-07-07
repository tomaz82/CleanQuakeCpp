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
#include	"qCCVar.h"

#include	"vid.h"
#include	"qCZone.h"
#include	"cmd.h"
#include	"server.h"
#include	"console.h"

qSCVar*	g_pVariables;
qChar*	g_pNullString	= "";

/*
========
Register
========
*/
void qCCVar::Register( qSCVar* pVariable )
{
	if( FindVariable( pVariable->pName ) )
	{
		Con_Printf( "Can't register variable %s, already defined\n", pVariable->pName );
		return;
	}

	if( Cmd_Exists( pVariable->pName ) )
	{
		Con_Printf( "Register: %s is a command\n", pVariable->pName );
		return;
	}

	const qChar*	pOldString	= pVariable->pString;
	pVariable->pString			= ( qChar* )qCZone::Alloc( strlen( pVariable->pString ) + 1 );
	strcpy( pVariable->pString, pOldString );
	pVariable->Value			= ( qFloat )atof( pVariable->pString );
	pVariable->pNext			= g_pVariables;
	g_pVariables				= pVariable;

}	// Register

/*
===
Set
===
*/
void qCCVar::Set( const qChar* pName, const qChar* pValue )
{
	qSCVar*	pVariable	= FindVariable( pName );

	if( pVariable == NULL )
	{
		Con_Printf( "Set: variable %s not found\n", pName );
		return;
	}

	qBool	Changed	= strcmp( pVariable->pString, pValue );

	qCZone::Free( pVariable->pString );

	pVariable->pString	= ( qChar* )qCZone::Alloc( strlen( pValue ) + 1 );
	strcpy( pVariable->pString, pValue );
	pVariable->Value	= ( qFloat )atof( pVariable->pString );

	if( pVariable->Server && Changed )
	{
		if( sv.active )
			SV_BroadcastPrintf( "\"%s\" changed to \"%s\"\n", pVariable->pName, pVariable->pString );
	}

	if( pVariable->Value != 0 )
	{
		if ( pVariable == &deathmatch )	Set( "coop",		"0" );
		if ( pVariable == &coop )		Set( "deathmatch",	"0" );
	}

}	// Set

/*
===
Set
===
*/
void qCCVar::Set( const qChar* pName, const qFloat Value )
{
	Set( pName, va( "%f", Value ) );

}	// Set

/*
========
GetValue
========
*/
qFloat qCCVar::GetValue( const qChar* pName )
{
	qSCVar*	pVariable	= FindVariable( pName );

	if( pVariable == NULL )
		return 0;

	return ( qFloat )atof( pVariable->pString );

}	// GetValue

/*
=========
GetString
=========
*/
qChar* qCCVar::GetString( const qChar* pName )
{
	qSCVar*	pVariable	= FindVariable( pName );

	if( pVariable == NULL )
		return g_pNullString;

	return pVariable->pString;

}	// GetString

/*
=============
FirstVariable
=============
*/
qSCVar* qCCVar::FirstVariable( void )
{
	return g_pVariables;

}	// FirstVariable

/*
============
FindVariable
============
*/
qSCVar* qCCVar::FindVariable( const qChar* pName )
{
	qSCVar*	pVariable;

	for( pVariable=g_pVariables; pVariable; pVariable=pVariable->pNext )
		if( !strcmp( pName, pVariable->pName ) )
			return pVariable;

	return NULL;

}	// FindVariable

/*
================
CompleteVariable
================
*/
qChar* qCCVar::CompleteVariable( const qChar* pPartial )
{
	const qUInt32	StringLength	= strlen( pPartial );

	if( StringLength == 0 )
		return NULL;

//////////////////////////////////////////////////////////////////////////

	qSCVar*	pVariable;

	for( pVariable=g_pVariables; pVariable; pVariable=pVariable->pNext )
		if( !strncmp( pPartial, pVariable->pName, StringLength ) )
			return pVariable->pName;

	return NULL;

}	// CompleteVariable

/*
============
CountMatches
============
*/
qUInt32 qCCVar::CountMatches( const qChar* pPartial )
{
	const qUInt32	StringLength	= strlen( pPartial );

	if( StringLength == 0 )
		return 0;

//////////////////////////////////////////////////////////////////////////

	qSCVar*	pVariable;
	qUInt32	Mathces	= 0;

	// Loop through the cvars and count all partial matches
	for( pVariable=g_pVariables; pVariable; pVariable=pVariable->pNext )
		if( !strncmp( pPartial, pVariable->pName, StringLength ) )
			++Mathces;

	return Mathces;

}	// CountMatches

/*
============
PrintMatches
============
*/
void qCCVar::PrintMatches( const qChar* pPartial )
{
	const qUInt32	StringLength	= strlen( pPartial );
	const qUInt32	ConsoleWidth	= ( vid.width >> 3 ) - 3;
	qSCVar*			pVariable;
	qChar			String[ 2048 ]	= { 0 };
	qChar			CVarName[ 32 ]	= { 0 };
	qUInt32			Position		= 0;
	qUInt32			CVarLength;

	// Loop through the cvars and print all matches
	for( pVariable=g_pVariables; pVariable; pVariable=pVariable->pNext )
	{
		if( !strncmp( pPartial, pVariable->pName, StringLength ) )
		{
			strcpy( CVarName, pVariable->pName );
			CVarLength	= strlen( CVarName );
			Position	+= CVarLength;

			// Pad with spaces
			for( ; CVarLength<24; ++CVarLength )
			{
				if( Position < ConsoleWidth )
					strcat( CVarName, " " );

				++Position;
			}

			strcat( String, CVarName );

			if( Position > ConsoleWidth - 24 )
				for( ; Position<ConsoleWidth; ++Position )
					strcat( String, " " );

			if( Position >= ConsoleWidth )
				Position	= 0;
		}
	}

	Con_Printf( "%s\n\n", String );

}	// PrintMatches

/*
=======
Command
=======
*/
qBool qCCVar::Command( void )
{
	qSCVar*	pVariable	= FindVariable( Cmd_Argv( 0 ) );

	if( pVariable == NULL )
		return FALSE;

	if( Cmd_Argc() == 1 )
	{
		Con_Printf( "\"%s\" is \"%s\"\n", pVariable->pName, pVariable->pString );
		return TRUE;
	}

	Set( pVariable->pName, Cmd_Argv( 1 ) );

	return TRUE;

}	// COmmand

/*
==============
WriteVariables
==============
*/
void qCCVar::WriteVariables( const FILE* pFile )
{
	qSCVar*	pVariable;

	for( pVariable=g_pVariables; pVariable; pVariable=pVariable->pNext )
		if( pVariable->Archive )
			fprintf( ( FILE* )pFile, "%s \"%s\"\n", pVariable->pName, pVariable->pString );

}	// WriteVariables
