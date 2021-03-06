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
// sv_edict.c -- entity dictionary

#include	"quakedef.h"
#include	"qCCRC.h"

#include	"sys.h"
#include	"qCHunk.h"
#include	"qCMath.h"
#include	"qCEndian.h"
#include	"qCCVar.h"
#include	"cmd.h"
#include	"server.h"
#include	"world.h"
#include	"console.h"

dprograms_t		*progs;
dfunction_t		*pr_functions;
char			*pr_strings;
ddef_t			*pr_fielddefs;
ddef_t			*pr_globaldefs;
dstatement_t	*pr_statements;
globalvars_t	*pr_global_struct;
float			*pr_globals;			// same as pr_global_struct
int				pr_edict_size;	// in bytes

unsigned short		pr_crc;

int		type_size[8] = {1,sizeof(string_t)/4,1,3,1,1,sizeof(func_t)/4,sizeof(void *)/4};

ddef_t *ED_FieldAtOfs (int ofs);
qBool	ED_ParseEpair (void *base, ddef_t *key, char *s);

qSCVar	nomonsters = {"nomonsters", "0"};
qSCVar	gamecfg = {"gamecfg", "0"};
qSCVar	scratch1 = {"scratch1", "0"};
qSCVar	scratch2 = {"scratch2", "0"};
qSCVar	scratch3 = {"scratch3", "0"};
qSCVar	scratch4 = {"scratch4", "0"};
qSCVar	savedgamecfg = {"savedgamecfg", "0", TRUE};
qSCVar	saved1 = {"saved1", "0", TRUE};
qSCVar	saved2 = {"saved2", "0", TRUE};
qSCVar	saved3 = {"saved3", "0", TRUE};
qSCVar	saved4 = {"saved4", "0", TRUE};

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct {
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache	gefvCache[GEFV_CACHESIZE] = {{NULL, ""}, {NULL, ""}};

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
void ED_ClearEdict (edict_t *e)
{
	memset (&e->v, 0, progs->entityfields * 4);
	e->free = FALSE;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED_Alloc (void)
{
	qUInt32			i;
	edict_t		*e;

	for ( i=svs.maxclients+1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) )
		{
			ED_ClearEdict (e);
			return e;
		}
	}

	if (i == MAX_EDICTS)
		Sys_Error ("ED_Alloc: no free edicts");

	sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict (e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (edict_t *ed)
{
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->free = TRUE;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorClear(ed->v.origin);
	VectorClear(ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef_t *ED_GlobalAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t *ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FindField
============
*/
ddef_t *ED_FindField (char *name)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (!strcmp(pr_strings + def->s_name,name) )
			return def;
	}
	return NULL;
}


/*
============
ED_FindGlobal
============
*/
ddef_t *ED_FindGlobal (char *name)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (!strcmp(pr_strings + def->s_name,name) )
			return def;
	}
	return NULL;
}


/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction (char *name)
{
	dfunction_t		*func;
	int				i;

	for (i=0 ; i<progs->numfunctions ; i++)
	{
		func = &pr_functions[i];
		if (!strcmp(pr_strings + func->s_name,name) )
			return func;
	}
	return NULL;
}


eval_t *GetEdictFieldValue(edict_t *ed, char *field)
{
	ddef_t			*def = NULL;
	int				i;
	static int		rep = 0;

	for (i=0 ; i<GEFV_CACHESIZE ; i++)
	{
		if (!strcmp(field, gefvCache[i].field))
		{
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	def = ED_FindField (field);

	if (strlen(field) < MAX_FIELD_LEN)
	{
		gefvCache[rep].pcache = def;
		strcpy (gefvCache[rep].field, field);
		rep ^= 1;
	}

Done:
	if (!def)
		return NULL;

	return (eval_t *)((char *)&ed->v + def->ofs*4);
}


/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;
	int			temp	= type;

	temp	&= ~DEF_SAVEGLOBAL;
	type	 = (etype_t)temp;

	switch (type)
	{
	case ev_string:
		snprintf (line, sizeof( line ), "%s", pr_strings + val->string);
		break;
	case ev_entity:
		snprintf (line, sizeof( line ), "entity %d", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)) );
		break;
	case ev_function:
		f = pr_functions + val->function;
		snprintf (line, sizeof( line ), "%s()", pr_strings + f->s_name);
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		snprintf (line, sizeof( line ), ".%s", pr_strings + def->s_name);
		break;
	case ev_void:
		snprintf (line, sizeof( line ), "void");
		break;
	case ev_float:
		snprintf (line, sizeof( line ), "%5.1f", val->_float);
		break;
	case ev_vector:
		snprintf (line, sizeof( line ), "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		snprintf (line, sizeof( line ), "pointer");
		break;
	default:
		snprintf (line, sizeof( line ), "bad type %d", type);
		break;
	}

	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PR_UglyValueString (etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;
	int			temp	= type;

	temp	&= ~DEF_SAVEGLOBAL;
	type	 = (etype_t)temp;

	switch (type)
	{
	case ev_string:
		snprintf (line, sizeof( line ), "%s", pr_strings + val->string);
		break;
	case ev_entity:
		snprintf (line, sizeof( line ), "%d", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = pr_functions + val->function;
		snprintf (line, sizeof( line ), "%s", pr_strings + f->s_name);
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		snprintf (line, sizeof( line ), "%s", pr_strings + def->s_name);
		break;
	case ev_void:
		snprintf (line, sizeof( line ), "void");
		break;
	case ev_float:
		snprintf (line, sizeof( line ), "%f", val->_float);
		break;
	case ev_vector:
		snprintf (line, sizeof( line ), "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		snprintf (line, sizeof( line ), "bad type %d", type);
		break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PR_GlobalString (int ofs)
{
	char	*s;
	int		i;
	ddef_t	*def;
	void	*val;
	static char	line[128];

	val = (void *)&pr_globals[ofs];
	def = ED_GlobalAtOfs(ofs);
	if (!def)
		snprintf (line,sizeof(line),"%d(???)", ofs);
	else
	{
		s = PR_ValueString ((etype_t)def->type, (eval_t*)val);
		snprintf (line,sizeof(line),"%d(%s)%s", ofs, pr_strings + def->s_name, s);
	}

	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");

	return line;
}

char *PR_GlobalStringNoContents (int ofs)
{
	int		i;
	ddef_t	*def;
	static char	line[128];

	def = ED_GlobalAtOfs(ofs);
	if (!def)
		snprintf (line, sizeof( line ), "%d(???)", ofs);
	else
		snprintf (line, sizeof( line ), "%d(%s)", ofs, pr_strings + def->s_name);

	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");

	return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print (edict_t *ed)
{
	int		l;
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;

	if (ed->free)
	{
		Con_Printf ("FREE\n");
		return;
	}

	Con_Printf("\nEDICT %d:\n", NUM_FOR_EDICT(ed));
	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = pr_strings + d->s_name;
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Con_Printf ("%s",name);
		l = strlen (name);
		while (l++ < 15)
			Con_Printf (" ");

		Con_Printf ("%s\n", PR_ValueString((etype_t)d->type, (eval_t *)v));
	}
}

/*
=============
ED_Write

For savegames
=============
*/
void ED_Write (FILE *f, edict_t *ed)
{
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;

	fprintf (f, "{\n");

	if (ed->free)
	{
		fprintf (f, "}\n");
		return;
	}

	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = pr_strings + d->s_name;
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		fprintf (f,"\"%s\" ",name);
		fprintf (f,"\"%s\"\n", PR_UglyValueString((etype_t)d->type, (eval_t *)v));
	}

	fprintf (f, "}\n");
}

void ED_PrintNum (int ent)
{
	ED_Print (EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts (void)
{
	qUInt32		i;

	Con_Printf ("%d entities\n", sv.num_edicts);
	for (i=0 ; i<sv.num_edicts ; i++)
		ED_PrintNum (i);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edicy
=============
*/
void ED_PrintEdict_f (void)
{
	qUInt32		i;

	i = atoi (Cmd_Argv(1));
	if (i >= sv.num_edicts)
	{
		Con_Printf("Bad edict number\n");
		return;
	}
	ED_PrintNum (i);
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count (void)
{
	qUInt32		i;
	edict_t	*ent;
	int		active, models, solid, step;

	active = models = solid = step = 0;
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(i);
		if (ent->free)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Con_Printf ("num_edicts:%3i\n", sv.num_edicts);
	Con_Printf ("active    :%3i\n", active);
	Con_Printf ("view      :%3i\n", models);
	Con_Printf ("touch     :%3i\n", solid);
	Con_Printf ("step      :%3i\n", step);

}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void ED_WriteGlobals (FILE *f)
{
	ddef_t		*def;
	int			i;
	char		*name;
	int			type;

	fprintf (f,"{\n");
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		type = def->type;
		if ( !(def->type & DEF_SAVEGLOBAL) )
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string
		&& type != ev_float
		&& type != ev_entity)
			continue;

		name = pr_strings + def->s_name;
		fprintf (f,"\"%s\" ", name);
		fprintf (f,"\"%s\"\n", PR_UglyValueString((etype_t)type, (eval_t *)&pr_globals[def->ofs]));
	}
	fprintf (f,"}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals (char *data)
{
	char	keyname[64];
	ddef_t	*key;

	while (1)
	{
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		strcpy (keyname, com_token);

	// parse value
		data = COM_Parse (data);
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error ("ED_ParseEntity: closing brace without data");

		key = ED_FindGlobal (keyname);
		if (!key)
		{
			Con_Printf ("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair ((void *)pr_globals, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
char *ED_NewString (char *string)
{
	char	*qnew, *qnew_p;
	int		i,l;

	l = strlen(string) + 1;
	qnew = (char*)qCHunk::Alloc (l);
	qnew_p = qnew;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*qnew_p++ = '\n';
			else
				*qnew_p++ = '\\';
		}
		else
			*qnew_p++ = string[i];
	}

	return qnew;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns FALSE if error
=============
*/
qBool	ED_ParseEpair (void *base, ddef_t *key, char *s)
{
	int		i;
	char	string[128];
	ddef_t	*def;
	char	*v, *w;
	void	*d;
	dfunction_t	*func;

	d = (void *)((int *)base + key->ofs);

	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		*(string_t *)d = ED_NewString (s) - pr_strings;
		break;

	case ev_float:
		*(float *)d = atof (s);
		break;

	case ev_vector:
		strcpy (string, s);
		v = string;
		w = string;
		for (i=0 ; i<3 ; i++)
		{
			while (*v && *v != ' ')
				v++;
			*v = 0;
			((float *)d)[i] = atof (w);
			w = v = v+1;
		}
		break;

	case ev_entity:
		*(int *)d = EDICT_TO_PROG(EDICT_NUM(atoi (s)));
		break;

	case ev_field:
		def = ED_FindField (s);
		if (!def)
		{
			Con_Printf ("Can't find field %s\n", s);
			return FALSE;
		}
		*(int *)d = G_INT(def->ofs);
		break;

	case ev_function:
		func = ED_FindFunction (s);
		if (!func)
		{
			Con_Printf ("Can't find function %s\n", s);
			return FALSE;
		}
		*(func_t *)d = func - pr_functions;
		break;

	default:
		break;
	}
	return TRUE;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	ddef_t		*key;
	qBool	anglehack;
	qBool	init;
	char		keyname[256];
	int			n;

	init = FALSE;

// clear it
	if (ent != sv.edicts)	// hack
		memset (&ent->v, 0, progs->entityfields * 4);

// go through all the dictionary pairs
	while (1)
	{
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

// anglehack is to allow QuakeEd to write single scalar angles
// and allow them to be turned into vectors. (FIXME...)
if (!strcmp(com_token, "angle"))
{
	strcpy (com_token, "angles");
	anglehack = TRUE;
}
else
	anglehack = FALSE;

// FIXME: change light to _light to get rid of this hack
if (!strcmp(com_token, "light"))
	strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix heynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

	// parse value
		data = COM_Parse (data);
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error ("ED_ParseEntity: closing brace without data");

		init = TRUE;

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		key = ED_FindField (keyname);
		if (!key)
		{
			Con_Printf ("'%s' is not a field\n", keyname);
			continue;
		}

if (anglehack)
{
char	temp[32];
strcpy (temp, com_token);
snprintf (com_token, sizeof(com_token),"0 %s 0", temp);
}

		if (!ED_ParseEpair ((void *)&ent->v, key, com_token))
			Host_Error ("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = TRUE;

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile (char *data)
{
	edict_t		*ent;
	int			inhibit;
	dfunction_t	*func;

	ent = NULL;
	inhibit = 0;
	pr_global_struct->time = sv.time;

// parse ents
	while (1)
	{
// parse the opening brace
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Sys_Error ("ED_LoadFromFile: found %s when expecting {",com_token);

		if (!ent)
			ent = EDICT_NUM(0);
		else
			ent = ED_Alloc ();
		data = ED_ParseEdict (data, ent);

// remove things from different skill levels or deathmatch
		if (deathmatch.Value)
		{
			if (((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
			{
				ED_Free (ent);
				inhibit++;
				continue;
			}
		}
		else if ((current_skill == 0 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD)) )
		{
			ED_Free (ent);
			inhibit++;
			continue;
		}

//
// immediately call spawn function
//
		if (!ent->v.classname)
		{
			Con_Printf ("No classname for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

	// look for the spawn function
		func = ED_FindFunction ( pr_strings + ent->v.classname );

		if (!func)
		{
			Con_Printf ("No spawn function for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram (func - pr_functions);
	}

	Con_DPrintf ("%d entities inhibited\n", inhibit);
}


/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs (void)
{
	int		i;

// flush the non-C variable lookup cache
	for (i=0 ; i<GEFV_CACHESIZE ; i++)
		gefvCache[i].field[0] = 0;

	progs = (dprograms_t *)COM_LoadHunkFile ("progs.dat");
	if (!progs)
		Sys_Error ("PR_LoadProgs: couldn't load progs.dat");
	Con_DPrintf ("Programs occupy %dK.\n", com_filesize/1024);

	pr_crc	= qCCRC::ProcessBlock( ( const qUInt8* )progs, com_filesize );

// qUInt8 swap the header
	for (i=0 ; i<sizeof(*progs)/4 ; i++)
		((int *)progs)[i] = qCEndian::LittleInt32 ( ((int *)progs)[i] );

	if (progs->version != PROG_VERSION)
		Sys_Error ("progs.dat has wrong version number (%d should be %d)", progs->version, PROG_VERSION);
	if (progs->crc != PROGHEADER_CRC)
		Sys_Error ("progs.dat system vars have been modified, progdefs.h is out of date");

	pr_functions = (dfunction_t *)((qUInt8 *)progs + progs->ofs_functions);
	pr_strings = (char *)progs + progs->ofs_strings;
	pr_globaldefs = (ddef_t *)((qUInt8 *)progs + progs->ofs_globaldefs);
	pr_fielddefs = (ddef_t *)((qUInt8 *)progs + progs->ofs_fielddefs);
	pr_statements = (dstatement_t *)((qUInt8 *)progs + progs->ofs_statements);

	pr_global_struct = (globalvars_t *)((qUInt8 *)progs + progs->ofs_globals);
	pr_globals = (float *)pr_global_struct;

	pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof(entvars_t);

// qUInt8 swap the lumps
	for (i=0 ; i<progs->numstatements ; i++)
	{
		pr_statements[i].op = qCEndian::LittleInt16(pr_statements[i].op);
		pr_statements[i].a = qCEndian::LittleInt16(pr_statements[i].a);
		pr_statements[i].b = qCEndian::LittleInt16(pr_statements[i].b);
		pr_statements[i].c = qCEndian::LittleInt16(pr_statements[i].c);
	}

	for (i=0 ; i<progs->numfunctions; i++)
	{
	pr_functions[i].first_statement = qCEndian::LittleInt32 (pr_functions[i].first_statement);
	pr_functions[i].parm_start = qCEndian::LittleInt32 (pr_functions[i].parm_start);
	pr_functions[i].s_name = qCEndian::LittleInt32 (pr_functions[i].s_name);
	pr_functions[i].s_file = qCEndian::LittleInt32 (pr_functions[i].s_file);
	pr_functions[i].numparms = qCEndian::LittleInt32 (pr_functions[i].numparms);
	pr_functions[i].locals = qCEndian::LittleInt32 (pr_functions[i].locals);
	}

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		pr_globaldefs[i].type = qCEndian::LittleInt16 (pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = qCEndian::LittleInt16 (pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = qCEndian::LittleInt32 (pr_globaldefs[i].s_name);
	}

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		pr_fielddefs[i].type = qCEndian::LittleInt16 (pr_fielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Sys_Error ("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = qCEndian::LittleInt16 (pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = qCEndian::LittleInt32 (pr_fielddefs[i].s_name);
	}

	for (i=0 ; i<progs->numglobals ; i++)
		((int *)pr_globals)[i] = qCEndian::LittleInt32 (((int *)pr_globals)[i]);
}


/*
===============
PR_Init
===============
*/
void PR_Init (void)
{
	Cmd_AddCommand ("edict", ED_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED_PrintEdicts);
	Cmd_AddCommand ("edictcount", ED_Count);
	Cmd_AddCommand ("profile", PR_Profile_f);
	qCCVar::Register (&nomonsters);
	qCCVar::Register (&gamecfg);
	qCCVar::Register (&scratch1);
	qCCVar::Register (&scratch2);
	qCCVar::Register (&scratch3);
	qCCVar::Register (&scratch4);
	qCCVar::Register (&savedgamecfg);
	qCCVar::Register (&saved1);
	qCCVar::Register (&saved2);
	qCCVar::Register (&saved3);
	qCCVar::Register (&saved4);
}



edict_t *EDICT_NUM(int n)
{
	if (n < 0 || n >= (qSInt32)sv.max_edicts)
		Sys_Error ("EDICT_NUM: bad number %d", n);
	return (edict_t *)((qUInt8 *)sv.edicts+ (n)*pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e)
{
	qUInt32		b;

	b = (qUInt8 *)e - (qUInt8 *)sv.edicts;
	b = b / pr_edict_size;

	if (b >= sv.num_edicts)
		Sys_Error ("NUM_FOR_EDICT: bad pointer");
	return b;
}
