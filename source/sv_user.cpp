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
// sv_user.c -- server code for moving users

#include	"quakedef.h"

#include	"qCMessage.h"
#include	"qCMath.h"
#include	"qCCVar.h"
#include	"protocol.h"
#include	"cmd.h"
#include	"qCInput.h"
#include	"server.h"
#include	"world.h"
#include	"console.h"
#include	"qCView.h"

edict_t	*sv_player;

extern	qSCVar	sv_friction;
qSCVar	sv_edgefriction = {"edgefriction", "2"};
extern	qSCVar	sv_stopspeed;

qVector3	wishdir;
float	wishspeed;

// world
float	*angles;
float	*origin;
float	*velocity;

qBool	onground;

usercmd_t	cmd;

/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction (void)
{
	float	*vel;
	float	speed, newspeed, control;
	qVector3	start, stop;
	float	friction;
	trace_t	trace;

	vel = velocity;

	speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1]);
	if (!speed)
		return;

// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0]/speed*16;
	start[1] = stop[1] = origin[1] + vel[1]/speed*16;
	start[2] = origin[2] + sv_player->v.mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move (start, vec3_origin, vec3_origin, stop, TRUE, sv_player);

	if (trace.fraction == 1.0)
		friction = sv_friction.Value*sv_edgefriction.Value;
	else
		friction = sv_friction.Value;

// apply friction
	control = speed < sv_stopspeed.Value ? sv_stopspeed.Value : speed;
	newspeed = speed - host_frametime*control*friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

/*
==============
SV_Accelerate
==============
*/
qSCVar	sv_maxspeed = {"sv_maxspeed", "320", FALSE, TRUE};
qSCVar	sv_accelerate = {"sv_accelerate", "10"};
void SV_Accelerate (void)
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	currentspeed = VectorDot (velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.Value*host_frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishdir[i];
}

void SV_AirAccelerate (qVector3 wishveloc)
{
	int			i;
	float		addspeed, wishspd, accelspeed, currentspeed;

	wishspd = qCMath::VectorNormalize (wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = VectorDot (velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
//	accelspeed = sv_accelerate.Value * host_frametime;
	accelspeed = sv_accelerate.Value*wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishveloc[i];
}


void DropPunchAngle (void)
{
	float	len;

	len = qCMath::VectorNormalize (sv_player->v.punchangle);

	len -= 10*host_frametime;
	if (len < 0)
		len = 0;
	VectorScale (sv_player->v.punchangle, len, sv_player->v.punchangle);
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove (void)
{
	int		i;
	qVector3	wishvel;
	float	speed, newspeed, wishspeed, addspeed, accelspeed;

//
// user intentions
//
	qVector3		_forward, _right;
	qCMath::AngleVectors (sv_player->v.v_angle, _forward, _right, NULL);

	for (i=0 ; i<3 ; i++)
		wishvel[i] = _forward[i]*cmd.forwardmove + _right[i]*cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = VectorLength(wishvel);
	if (wishspeed > sv_maxspeed.Value)
	{
		VectorScale (wishvel, sv_maxspeed.Value/wishspeed, wishvel);
		wishspeed = sv_maxspeed.Value;
	}
	wishspeed *= 0.7f;

//
// water friction
//
	speed = VectorLength (velocity);
	if (speed)
	{
		newspeed = speed - host_frametime * speed * sv_friction.Value;
		if (newspeed < 0)
			newspeed = 0;
		VectorScale (velocity, newspeed/speed, velocity);
	}
	else
		newspeed = 0;

//
// water acceleration
//
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	qCMath::VectorNormalize (wishvel);
	accelspeed = sv_accelerate.Value * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > sv_player->v.teleport_time
	|| !sv_player->v.waterlevel)
	{
		sv_player->v.flags = (int)sv_player->v.flags & ~FL_WATERJUMP;
		sv_player->v.teleport_time = 0;
	}
	sv_player->v.velocity[0] = sv_player->v.movedir[0];
	sv_player->v.velocity[1] = sv_player->v.movedir[1];
}


/*
===================
SV_AirMove

===================
*/
void SV_AirMove (void)
{
	int			i;
	qVector3		wishvel;
	float		fmove, smove;
	qVector3		_forward, _right;
	qCMath::AngleVectors (sv_player->v.angles, _forward, _right, NULL);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

// hack to not let you back into teleporter
	if (sv.time < sv_player->v.teleport_time && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = _forward[i]*fmove + _right[i]*smove;

	if ( (int)sv_player->v.movetype != MOVETYPE_WALK)
		wishvel[2] = cmd.upmove;
	else
		wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = qCMath::VectorNormalize(wishdir);
	if (wishspeed > sv_maxspeed.Value)
	{
		VectorScale (wishvel, sv_maxspeed.Value/wishspeed, wishvel);
		wishspeed = sv_maxspeed.Value;
	}

	if ( sv_player->v.movetype == MOVETYPE_NOCLIP)
	{	// noclip
		VectorCopy (wishvel, velocity);
	}
	else if ( onground )
	{
		SV_UserFriction ();
		SV_Accelerate ();
	}
	else
	{	// not on ground, so little effect on velocity
		SV_AirAccelerate (wishvel);
	}
}

/*
===================
SV_ClientThink

the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
void SV_ClientThink (void)
{
	qVector3		v_angle;

	if (sv_player->v.movetype == MOVETYPE_NONE)
		return;

	onground = (int)sv_player->v.flags & FL_ONGROUND;

	origin = sv_player->v.origin;
	velocity = sv_player->v.velocity;

	DropPunchAngle ();

//
// if dead, behave differently
//
	if (sv_player->v.health <= 0)
		return;

//
// angles
// show 1/3 the pitch angle and all the roll angle
	cmd = host_client->cmd;
	angles = sv_player->v.angles;

	VectorAdd (sv_player->v.v_angle, sv_player->v.punchangle, v_angle);
	angles[ROLL] = qCView::CalcRoll (sv_player->v.angles, sv_player->v.velocity)*4;
	if (!sv_player->v.fixangle)
	{
		angles[PITCH] = -v_angle[PITCH]/3;
		angles[YAW] = v_angle[YAW];
	}

	if ( (int)sv_player->v.flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}
//
// walk
//
	if ( (sv_player->v.waterlevel >= 2)
	&& (sv_player->v.movetype != MOVETYPE_NOCLIP) )
	{
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}


/*
===================
SV_ReadClientMove
===================
*/
void SV_ReadClientMove (usercmd_t *move)
{
	int		i;
	qVector3	angle;
	int		bits;

// read ping time
	host_client->ping_times[host_client->num_pings%NUM_PING_TIMES]
		= sv.time - qCMessage::ReadFloat ();
	host_client->num_pings++;

// read current angles
	if (host_client->netconnection->Mod == MOD_PROQUAKE)
	{
		for (i=0 ; i<3 ; i++)
			angle[i] = qCMessage::ReadAngleProQuake ();
	}
	else
	{
		for (i=0 ; i<3 ; i++)
			angle[i] = qCMessage::ReadAngle ();
	}

	VectorCopy (angle, host_client->edict->v.v_angle);

// read movement
	move->forwardmove = qCMessage::ReadShort ();
	move->sidemove = qCMessage::ReadShort ();
	move->upmove = qCMessage::ReadShort ();

// read buttons
	bits = qCMessage::ReadByte ();
	host_client->edict->v.button0 = bits & 1;
	host_client->edict->v.button2 = (bits & 2)>>1;

	i = qCMessage::ReadByte ();
	if (i)
		host_client->edict->v.impulse = i;
}

/*
===================
SV_ReadClientMessage

Returns FALSE if the client should be killed
===================
*/
qBool SV_ReadClientMessage (void)
{
	int		ret;
	int		cmd;
	char		*s;

	do
	{
nextmsg:
		ret = NET_GetMessage (host_client->netconnection);
		if (ret == -1)
			return FALSE;

		if (!ret)
			return TRUE;

		qCMessage::BeginReading ();

		while (1)
		{
			if (!host_client->active)
				return FALSE;	// a command caused an error

			if (qCMessage::BadRead)
				return FALSE;

			cmd = qCMessage::ReadChar ();

			switch (cmd)
			{
			case -1:
				goto nextmsg;		// end of message

			default:
				return FALSE;

			case clc_nop:
				break;

			case clc_stringcmd:
				s = qCMessage::ReadString ();
				ret = 0;

				if (strnicmp(s, "status", 6) == 0)
					ret = 1;
				else if (strnicmp(s, "god", 3) == 0)
					ret = 1;
				else if (strnicmp(s, "notarget", 8) == 0)
					ret = 1;
				else if (strnicmp(s, "fly", 3) == 0)
					ret = 1;
				else if (strnicmp(s, "name", 4) == 0)
					ret = 1;
				else if (strnicmp(s, "noclip", 6) == 0)
					ret = 1;
				else if (strnicmp(s, "say", 3) == 0)
					ret = 1;
				else if (strnicmp(s, "say_team", 8) == 0)
					ret = 1;
				else if (strnicmp(s, "tell", 4) == 0)
					ret = 1;
				else if (strnicmp(s, "color", 5) == 0)
					ret = 1;
				else if (strnicmp(s, "kill", 4) == 0)
					ret = 1;
				else if (strnicmp(s, "pause", 5) == 0)
					ret = 1;
				else if (strnicmp(s, "spawn", 5) == 0)
					ret = 1;
				else if (strnicmp(s, "begin", 5) == 0)
					ret = 1;
				else if (strnicmp(s, "prespawn", 8) == 0)
					ret = 1;
				else if (strnicmp(s, "kick", 4) == 0)
					ret = 1;
				else if (strnicmp(s, "ping", 4) == 0)
					ret = 1;
				else if (strnicmp(s, "give", 4) == 0)
					ret = 1;
				else if (strnicmp(s, "ban", 3) == 0)
					ret = 1;
				if (ret == 2)
					Cbuf_InsertText (s);
				else if (ret == 1)
					Cmd_ExecuteString (s, src_client);
				else
					Con_DPrintf("%s tried to %s\n", host_client->name, s);
				break;

			case clc_disconnect:
				return FALSE;

			case clc_move:
				SV_ReadClientMove (&host_client->cmd);
				break;
			}
		}
	} while (ret == 1);

	return TRUE;
}


/*
==================
SV_RunClients
==================
*/
void SV_RunClients (void)
{
	qUInt32	i;

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (!SV_ReadClientMessage ())
		{
			SV_DropClient (FALSE);	// client misbehaved...
			continue;
		}

		if (!host_client->spawned)
		{
		// clear client movement until a new packet is received
			memset (&host_client->cmd, 0, sizeof(host_client->cmd));
			continue;
		}

// always pause in single player if in console or menus
		if (!sv.paused && (svs.maxclients > 1 || qCInput::GetKeyDestination() == qGAME) )
			SV_ClientThink ();
	}
}

