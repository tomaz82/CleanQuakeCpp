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
// sv_main.c -- server main program

#include	"quakedef.h"

#include	"qCMessage.h"
#include	"bspfile.h"
#include	"sys.h"
#include	"qCHunk.h"
#include	"qCMath.h"
#include	"qCCVar.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"sound.h"
#include	"server.h"
#include	"gl_model.h"
#include	"world.h"
#include	"console.h"

server_t		sv;
server_static_t	svs;

char	localmodels[MAX_MODELS][5];			// inline model names for precache

//============================================================================

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	int		i;
	extern	qSCVar	sv_maxvelocity;
	extern	qSCVar	sv_gravity;
	extern	qSCVar	sv_nostep;
	extern	qSCVar	sv_friction;
	extern	qSCVar	sv_edgefriction;
	extern	qSCVar	sv_stopspeed;
	extern	qSCVar	sv_maxspeed;
	extern	qSCVar	sv_accelerate;

	qCCVar::Register (&sv_maxvelocity);
	qCCVar::Register (&sv_gravity);
	qCCVar::Register (&sv_friction);
	qCCVar::Register (&sv_edgefriction);
	qCCVar::Register (&sv_stopspeed);
	qCCVar::Register (&sv_maxspeed);
	qCCVar::Register (&sv_accelerate);
	qCCVar::Register (&sv_nostep);

	for (i=0 ; i<MAX_MODELS ; i++)
		snprintf (localmodels[i], sizeof(localmodels[i]),"*%d", i);
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (qVector3 org, qVector3 dir, int color, int count)
{
	int		i, v;

	if (sv.datagram.cursize > MAX_DATAGRAM-16)
		return;
	qCMessage::WriteByte (&sv.datagram, svc_particle);
	qCMessage::WriteCoord (&sv.datagram, org[0]);
	qCMessage::WriteCoord (&sv.datagram, org[1]);
	qCMessage::WriteCoord (&sv.datagram, org[2]);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		qCMessage::WriteChar (&sv.datagram, v);
	}
	qCMessage::WriteByte (&sv.datagram, count);
	qCMessage::WriteByte (&sv.datagram, color);
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume,
    float attenuation)
{
    int         sound_num;
    int field_mask;
    int			i;
	int			ent;

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %d", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %d", channel);

	if (sv.datagram.cursize > MAX_DATAGRAM-16)
		return;

// find precache number for sound
    for (sound_num=1 ; sound_num<MAX_SOUNDS
        && sv.sound_precache[sound_num] ; sound_num++)
        if (!strcmp(sample, sv.sound_precache[sound_num]))
            break;

    if ( sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num] )
    {
        Con_Printf ("SV_StartSound: %s not precacheed\n", sample);
        return;
    }

	ent = NUM_FOR_EDICT(entity);

	channel = (ent<<3) | channel;

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;

// directed messages go only to the entity the are targeted on
	qCMessage::WriteByte (&sv.datagram, svc_sound);
	qCMessage::WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		qCMessage::WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		qCMessage::WriteByte (&sv.datagram, attenuation*64);
	qCMessage::WriteShort (&sv.datagram, channel);
	qCMessage::WriteByte (&sv.datagram, sound_num);
	for (i=0 ; i<3 ; i++)
		qCMessage::WriteCoord (&sv.datagram, entity->v.origin[i]+0.5*(entity->v.mins[i]+entity->v.maxs[i]));
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
	char			**s;
	char			message[2048];

	qCMessage::WriteByte (&client->message, svc_print);
	snprintf (message, sizeof( message ), "%c\nVERSION %4.2f SERVER (%d CRC)", 2, VERSION, pr_crc);
	qCMessage::WriteString (&client->message,message);

	qCMessage::WriteByte (&client->message, svc_serverinfo);
	qCMessage::WriteLong (&client->message, PROTOCOL_VERSION);
	qCMessage::WriteByte (&client->message, svs.maxclients);

	if (!coop.Value && deathmatch.Value)
		qCMessage::WriteByte (&client->message, GAME_DEATHMATCH);
	else
		qCMessage::WriteByte (&client->message, GAME_COOP);

	snprintf (message, sizeof( message ), pr_strings+sv.edicts->v.message);

	qCMessage::WriteString (&client->message,message);

	for (s = sv.model_precache+1 ; *s ; s++)
		qCMessage::WriteString (&client->message, *s);
	qCMessage::WriteByte (&client->message, 0);

	for (s = sv.sound_precache+1 ; *s ; s++)
		qCMessage::WriteString (&client->message, *s);
	qCMessage::WriteByte (&client->message, 0);

// send music
	qCMessage::WriteByte (&client->message, svc_cdtrack);
	qCMessage::WriteByte (&client->message, sv.edicts->v.sounds);
	qCMessage::WriteByte (&client->message, sv.edicts->v.sounds);

// set view
	qCMessage::WriteByte (&client->message, svc_setview);
	qCMessage::WriteShort (&client->message, NUM_FOR_EDICT(client->edict));

	qCMessage::WriteByte (&client->message, svc_signonnum);
	qCMessage::WriteByte (&client->message, 1);

	client->sendsignon = TRUE;
	client->spawned = FALSE;		// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum)
{
	edict_t			*ent;
	client_t		*client;
	int				edictnum;
	qSSocket*		netconnection;
	int				i;
	float			spawn_parms[NUM_SPAWN_PARMS];

	client = svs.clients + clientnum;

	Con_DPrintf ("Client %s connected\n", client->netconnection->Address);

	edictnum = clientnum+1;

	ent = EDICT_NUM(edictnum);

// set up the client_t
	netconnection = client->netconnection;

	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
	memset (client, 0, sizeof(*client));
	client->netconnection = netconnection;

	strcpy (client->name, "unconnected");
	client->active = TRUE;
	client->spawned = FALSE;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof(client->msgbuf);
	client->message.allowoverflow = TRUE;		// we can catch it

	if (sv.loadgame)
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	else
	{
	// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (pr_global_struct->SetNewParms);
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
	}

	SV_SendServerinfo (client);
}


/*
===================
SV_CheckForNewClients

===================
*/
void SV_CheckForNewClients (void)
{
	qSSocket	*ret;
	qUInt32				i;

//
// check for new connections
//
	while (1)
	{
		ret = NET_CheckNewConnections ();
		if (!ret)
			break;

	//
	// init a new client structure
	//
		for (i=0 ; i<svs.maxclients ; i++)
			if (!svs.clients[i].active)
				break;
		if (i == svs.maxclients)
			Sys_Error ("Host_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient (i);

		net_activeconnections++;
	}
}



/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram

==================
*/
void SV_ClearDatagram (void)
{
	SZ_Clear (&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int		fatbytes;
qUInt8	fatpvs[MAX_MAP_LEAFS/8];

void SV_AddToFatPVS (qVector3 org, mnode_t *node)
{
	int		i;
	qUInt8	*pvs;
	mplane_t	*plane;
	float	d;

	while (1)
	{
	// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = Mod_LeafPVS ( (mleaf_t *)node, sv.worldmodel);
				for (i=0 ; i<fatbytes ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = VectorDot (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPVS (org, node->children[0]);
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
qUInt8 *SV_FatPVS (qVector3 org)
{
	fatbytes = (sv.worldmodel->numleafs+31)>>3;
	memset (fatpvs, 0, fatbytes);
	SV_AddToFatPVS (org, sv.worldmodel->nodes);
	return fatpvs;
}

//=============================================================================


/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient (edict_t	*clent, sizebuf_t *msg)
{
	qUInt32		e, i;
	int		bits;
	qUInt8	*pvs;
	qVector3	org;
	float	miss;
	edict_t	*ent;

// find the client's PVS
	VectorAdd (clent->v.origin, clent->v.view_ofs, org);
	pvs = SV_FatPVS (org);

// send over all entities (excpet the client) that touch the pvs
	ent = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
	{
// ignore if not touching a PV leaf
		if (ent != clent)	// clent is ALLWAYS sent
		{
// ignore ents without visible models
			if (!ent->v.modelindex || !pr_strings[ent->v.model])
				continue;

			for (i=0 ; i < ent->num_leafs ; i++)
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
					break;

			if (i == ent->num_leafs)
				continue;		// not visible
		}

		if (msg->maxsize - msg->cursize < 16)
		{
			Con_Printf ("packet overflow\n");
			return;
		}

// send an update
		bits = 0;

		for (i=0 ; i<3 ; i++)
		{
			miss = ent->v.origin[i] - ent->baseline.origin[i];
			if ( miss < -0.1 || miss > 0.1 )
				bits |= U_ORIGIN1<<i;
		}

		if ( ent->v.angles[0] != ent->baseline.angles[0] )
			bits |= U_ANGLE1;

		if ( ent->v.angles[1] != ent->baseline.angles[1] )
			bits |= U_ANGLE2;

		if ( ent->v.angles[2] != ent->baseline.angles[2] )
			bits |= U_ANGLE3;

		if (ent->v.movetype == MOVETYPE_STEP)
			bits |= U_NOLERP;	// don't mess up the step animation

		if (ent->baseline.colormap != ent->v.colormap)
			bits |= U_COLORMAP;

		if (ent->baseline.skin != ent->v.skin)
			bits |= U_SKIN;

		if (ent->baseline.frame != ent->v.frame)
			bits |= U_FRAME;

		if (ent->baseline.effects != ent->v.effects)
			bits |= U_EFFECTS;

		if (ent->baseline.modelindex != ent->v.modelindex)
			bits |= U_MODEL;

		if (e >= 256)
			bits |= U_LONGENTITY;

		if (bits >= 256)
			bits |= U_MOREBITS;

	//
	// write the message
	//
		qCMessage::WriteByte (msg,bits | U_SIGNAL);

		if (bits & U_MOREBITS)
			qCMessage::WriteByte (msg, bits>>8);
		if (bits & U_LONGENTITY)
			qCMessage::WriteShort (msg,e);
		else
			qCMessage::WriteByte (msg,e);

		if (bits & U_MODEL)
			qCMessage::WriteByte (msg,	ent->v.modelindex);
		if (bits & U_FRAME)
			qCMessage::WriteByte (msg, ent->v.frame);
		if (bits & U_COLORMAP)
			qCMessage::WriteByte (msg, ent->v.colormap);
		if (bits & U_SKIN)
			qCMessage::WriteByte (msg, ent->v.skin);
		if (bits & U_EFFECTS)
			qCMessage::WriteByte (msg, ent->v.effects);
		if (bits & U_ORIGIN1)
			qCMessage::WriteCoord (msg, ent->v.origin[0]);
		if (bits & U_ANGLE1)
			qCMessage::WriteAngle(msg, ent->v.angles[0]);
		if (bits & U_ORIGIN2)
			qCMessage::WriteCoord (msg, ent->v.origin[1]);
		if (bits & U_ANGLE2)
			qCMessage::WriteAngle(msg, ent->v.angles[1]);
		if (bits & U_ORIGIN3)
			qCMessage::WriteCoord (msg, ent->v.origin[2]);
		if (bits & U_ANGLE3)
			qCMessage::WriteAngle(msg, ent->v.angles[2]);
	}
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts (void)
{
	qUInt32		e;
	edict_t	*ent;

	ent = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
	{
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}

}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
	int		bits;
	int		i;
	edict_t	*other;
	int		items;
	eval_t	*val;

//
// send a damage message
//
	if (ent->v.dmg_take || ent->v.dmg_save)
	{
		other = PROG_TO_EDICT(ent->v.dmg_inflictor);
		qCMessage::WriteByte (msg, svc_damage);
		qCMessage::WriteByte (msg, ent->v.dmg_save);
		qCMessage::WriteByte (msg, ent->v.dmg_take);
		for (i=0 ; i<3 ; i++)
			qCMessage::WriteCoord (msg, other->v.origin[i] + 0.5*(other->v.mins[i] + other->v.maxs[i]));

		ent->v.dmg_take = 0;
		ent->v.dmg_save = 0;
	}

//
// send the current viewpos offset from the view entity
//

// a fixangle might get lost in a dropped packet.  Oh well.
	if ( ent->v.fixangle )
	{
		qCMessage::WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			qCMessage::WriteAngle (msg, ent->v.angles[i] );
		ent->v.fixangle = 0;
	}

	bits = 0;

	if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;

// stuff the sigil bits into the high bits of items for sbar, or else
// mix in items2
	val = GetEdictFieldValue(ent, "items2");

	if (val)
		items = (int)ent->v.items | ((int)val->_float << 23);
	else
		items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);

	bits |= SU_ITEMS;

	if ( (int)ent->v.flags & FL_ONGROUND)
		bits |= SU_ONGROUND;

	if ( ent->v.waterlevel >= 2)
		bits |= SU_INWATER;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->v.punchangle[i])
			bits |= (SU_PUNCH1<<i);
		if (ent->v.velocity[i])
			bits |= (SU_VELOCITY1<<i);
	}

	if (ent->v.weaponframe)
		bits |= SU_WEAPONFRAME;

	if (ent->v.armorvalue)
		bits |= SU_ARMOR;

//	if (ent->v.weapon)
		bits |= SU_WEAPON;

// send the data

	qCMessage::WriteByte (msg, svc_clientdata);
	qCMessage::WriteShort (msg, bits);

	if (bits & SU_VIEWHEIGHT)
		qCMessage::WriteChar (msg, ent->v.view_ofs[2]);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
			qCMessage::WriteChar (msg, ent->v.punchangle[i]);
		if (bits & (SU_VELOCITY1<<i))
			qCMessage::WriteChar (msg, ent->v.velocity[i]/16);
	}

// [always sent]	if (bits & SU_ITEMS)
	qCMessage::WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		qCMessage::WriteByte (msg, ent->v.weaponframe);
	if (bits & SU_ARMOR)
		qCMessage::WriteByte (msg, ent->v.armorvalue);
	if (bits & SU_WEAPON)
		qCMessage::WriteByte (msg, SV_ModelIndex(pr_strings+ent->v.weaponmodel));

	qCMessage::WriteShort (msg, ent->v.health);
	qCMessage::WriteByte (msg, ent->v.currentammo);
	qCMessage::WriteByte (msg, ent->v.ammo_shells);
	qCMessage::WriteByte (msg, ent->v.ammo_nails);
	qCMessage::WriteByte (msg, ent->v.ammo_rockets);
	qCMessage::WriteByte (msg, ent->v.ammo_cells);
	qCMessage::WriteByte (msg, ent->v.weapon);
}

/*
=======================
SV_SendClientDatagram
=======================
*/
qBool SV_SendClientDatagram (client_t *client)
{
	qUInt8		buf[MAX_DATAGRAM];
	sizebuf_t	msg;

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	qCMessage::WriteByte (&msg, svc_time);
	qCMessage::WriteFloat (&msg, sv.time);

// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client->edict, &msg);

	SV_WriteEntitiesToClient (client->edict, &msg);

// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

// send the datagram
	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
	{
		SV_DropClient (TRUE);// if the message couldn't send, kick off
		return FALSE;
	}

	return TRUE;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
	qUInt32		i, j;
	client_t *client;

// check for changes to be sent over the reliable streams
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (host_client->old_frags != host_client->edict->v.frags)
		{
			for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
			{
				if (!client->active)
					continue;
				qCMessage::WriteByte (&client->message, svc_updatefrags);
				qCMessage::WriteByte (&client->message, i);
				qCMessage::WriteShort (&client->message, host_client->edict->v.frags);
			}

			host_client->old_frags = host_client->edict->v.frags;
		}
	}

	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
void SV_SendNop (client_t *client)
{
	sizebuf_t	msg;
	qUInt8		buf[4];

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	qCMessage::WriteChar (&msg, svc_nop);

	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
		SV_DropClient (TRUE);	// if the message couldn't send, kick off
	client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	qUInt32		i;

// update frags, names, etc
	SV_UpdateToReliableMessages ();

// build individual updates
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		if (host_client->spawned)
		{
			if (!SV_SendClientDatagram (host_client))
				continue;
		}
		else
		{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		// send a full message when the next signon stage has been requested
		// some other message data (name changes, etc) may accumulate
		// between signon stages
			if (!host_client->sendsignon)
			{
				if (realtime - host_client->last_message > 5)
					SV_SendNop (host_client);
				continue;	// don't send out non-signon messages
			}
		}

		if( host_client->netconnection->NetWait )
			continue;

		// check for an overflowed message.  Should only happen
		// on a very fucked up connection that backs up a lot, then
		// changes level
		if (host_client->message.overflowed)
		{
			SV_DropClient (TRUE);
			host_client->message.overflowed = FALSE;
			continue;
		}

		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NET_CanSendMessage (host_client->netconnection))
			{
//				I_Printf ("can't write\n");
				continue;
			}

			if (host_client->dropasap)
				SV_DropClient (FALSE);	// went to another level
			else
			{
				if (NET_SendMessage (host_client->netconnection
				, &host_client->message) == -1)
					SV_DropClient (TRUE);	// if the message couldn't send, kick off
				SZ_Clear (&host_client->message);
				host_client->last_message = realtime;
				host_client->sendsignon = FALSE;
			}
		}
	}


// clear muzzle flashes
	SV_CleanupEnts ();
}


/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex (char *name)
{
	int		i;

	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
		Sys_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
	qUInt32			i;
	edict_t			*svent;
	qUInt32				entnum;

	for (entnum = 0; entnum < (qUInt32)sv.num_edicts ; entnum++)
	{
	// get the current server version
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !svent->v.modelindex)
			continue;

	//
	// create entity baseline
	//
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skin = svent->v.skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex =
				SV_ModelIndex(pr_strings + svent->v.model);
		}

	//
	// add to the message
	//
		qCMessage::WriteByte (&sv.signon,svc_spawnbaseline);
		qCMessage::WriteShort (&sv.signon,entnum);

		qCMessage::WriteByte (&sv.signon, svent->baseline.modelindex);
		qCMessage::WriteByte (&sv.signon, svent->baseline.frame);
		qCMessage::WriteByte (&sv.signon, svent->baseline.colormap);
		qCMessage::WriteByte (&sv.signon, svent->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			qCMessage::WriteCoord(&sv.signon, svent->baseline.origin[i]);
			qCMessage::WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}
	}
}


/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect (void)
{
	char	data[128];
	sizebuf_t	msg;

	msg.data = (qUInt8*)data;
	msg.cursize = 0;
	msg.maxsize = sizeof(data);

	qCMessage::WriteChar (&msg, svc_stufftext);
	qCMessage::WriteString (&msg, "reconnect\n");
	NET_SendToAll (&msg, 5);

	Cmd_ExecuteString ("reconnect\n", src_command);
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	qUInt32		i, j;

	svs.serverflags = pr_global_struct->serverflags;

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

	// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram (pr_global_struct->SetChangeParms);
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}


/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float		scr_centertime_off;

void SV_SpawnServer (char *server)
{
	edict_t		*ent;
	qUInt32			i;

	// let's not have any servers with no name
	if (hostname.pString[0] == 0)
		qCCVar::Set ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	Con_DPrintf ("SpawnServer: %s\n",server);
	svs.changelevel_issued = FALSE;		// now safe to issue another

//
// tell all connected clients that we are going to a new level
//
	if (sv.active)
	{
		SV_SendReconnect ();
	}

//
// make cvars consistant
//
	if (coop.Value)
		qCCVar::Set("deathmatch", 0.0f);
	current_skill = (int)(skill.Value + 0.5);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;

	qCCVar::Set("skill", (float)current_skill);

//
// set up the new server
//
	Host_ClearMemory ();

	memset (&sv, 0, sizeof(sv));

	strcpy (sv.name, server);

// load progs to get entity field count
	PR_LoadProgs ();

// allocate server memory
	sv.max_edicts = MAX_EDICTS;

	sv.edicts = (edict_t*)qCHunk::AllocName (sv.max_edicts*pr_edict_size, "edicts");

	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.signon.maxsize = sizeof(sv.signon_buf);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

// leave slots at start for clients only
	sv.num_edicts = svs.maxclients+1;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		ent = EDICT_NUM(i+1);
		svs.clients[i].edict = ent;
	}

	sv.state = ss_loading;
	sv.paused = FALSE;

	sv.time = 1.0;

	strcpy (sv.name, server);
	snprintf (sv.modelname,sizeof(sv.modelname),"maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName (sv.modelname, FALSE);
	if (!sv.worldmodel)
	{
		Con_Printf ("Couldn't spawn server %s\n", sv.modelname);
		sv.active = FALSE;
		return;
	}
	sv.models[1] = sv.worldmodel;

//
// clear world interaction links
//
	SV_ClearWorld ();

	sv.sound_precache[0] = pr_strings;

	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	for (i=1 ; i<(qUInt32)sv.worldmodel->numsubmodels ; i++)
	{
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], FALSE);
	}

//
// load the rest of the entities
//
	ent = EDICT_NUM(0);
	memset (&ent->v, 0, progs->entityfields * 4);
	ent->free = FALSE;
	ent->v.model = sv.worldmodel->name - pr_strings;
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	if (coop.Value)
		pr_global_struct->coop = coop.Value;
	else
		pr_global_struct->deathmatch = deathmatch.Value;

	pr_global_struct->mapname = sv.name - pr_strings;

// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	ED_LoadFromFile (sv.worldmodel->entities);

	sv.active = TRUE;

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

// run two frames to allow everything to settle
	host_frametime = 0.1;
	SV_Physics ();
	SV_Physics ();

// create a baseline for more efficient communications
	SV_CreateBaseline ();

// send serverinfo to all connected clients
	for (i=0,host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo (host_client);

	Con_DPrintf ("Server spawned.\n");
}

