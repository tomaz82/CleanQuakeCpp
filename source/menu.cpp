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
#include	"winquake.h"
#include	"qCNetworkDatagram.h"
#include	"client.h"
#include	"qCClient.h"

#include	"vid.h"
#include	"draw.h"
#include	"qCCVar.h"
#include	"screen.h"
#include	"cmd.h"
#include	"sound.h"
#include	"render.h"
#include	"qCInput.h"
#include	"qCDemo.h"
#include	"server.h"
#include	"console.h"
#include	"menu.h"

typedef enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_keys, m_help, m_quit, m_lanconfig, m_gameoptions, m_search, m_slist} estate_t;
estate_t	m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);

qBool	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qBool	m_recursiveDraw;

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define	TCPIPConfig		(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

/*
===============
M_DrawCharacter
===============
*/
void M_DrawCharacter( const int X, const int Y, const int Num )
{
	Draw_Character( X + ( ( vid.width - 320 ) >> 1 ), Y + ( ( vid.height - 200 ) >> 1 ), Num );
}

void M_Print( int X, const int Y, const qChar* pString, const qBool White )
{
	Draw_String( X + ( ( vid.width - 320 ) >> 1 ), Y + ( ( vid.height - 200 ) >> 1 ), pString );
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320)>>1), y + ( ( vid.height - 200 ) >> 1 ), pic);
}

qUInt8 identityTable[256];
qUInt8 translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int		j;
	qUInt8	*dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}


void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y + ( ( vid.height - 200 ) >> 1 ), pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawPic (cx, cy+8, p);
}

//=============================================================================

int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = TRUE;

	if (qCInput::GetKeyDestination() == qMENU)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		qCInput::SetKeyDestination( qGAME );
		m_state = m_none;
		return;
	}
	if (qCInput::GetKeyDestination() == qCONSOLE)
	{
		Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}


//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
#define	MAIN_ITEMS	5


void M_Menu_Main_f (void)
{
	if (qCInput::GetKeyDestination() != qMENU)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	qCInput::SetKeyDestination( qMENU );
	m_state = m_main;
	m_entersound = TRUE;
}


void M_Main_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawPic (54, 32 + m_main_cursor * 20,Draw_CachePic( va("gfx/menudot%d.lmp", f+1 ) ) );
}


void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		qCInput::SetKeyDestination( qGAME );
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
			qCDemo::Next();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = TRUE;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f ();
			break;

		case 1:
			M_Menu_MultiPlayer_f ();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Help_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3


void M_Menu_SinglePlayer_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_singleplayer;
	m_entersound = TRUE;
}


void M_SinglePlayer_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawPic (54, 32 + m_singleplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%d.lmp", f+1 ) ) );
}


void M_SinglePlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = TRUE;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				if (!SCR_ModalMessage("Are you sure you want to\nstart a new game?\n"))
					break;
			qCInput::SetKeyDestination( qGAME );
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("map start\n");
			break;

		case 1:
			M_Menu_Load_f ();
			break;

		case 2:
			M_Menu_Save_f ();
			break;
		}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		12
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int		loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
	int		i, j;
	char	name[MAX_OSPATH];
	FILE	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = FALSE;
		snprintf( name, sizeof( name ), "%s/s%d.sav", com_gamedir, i);
		f = fopen (name, "r");
		if (!f)
			continue;
		fscanf (f, "%d\n", &version);
		fscanf (f, "%79s\n", name);
		strncpy (m_filenames[i], name, sizeof(m_filenames[i])-1);

	// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = TRUE;
		fclose (f);
	}
}

void M_Menu_Load_f (void)
{
	m_entersound = TRUE;
	m_state = m_load;
	qCInput::SetKeyDestination( qMENU );
	M_ScanSaves ();
}


void M_Menu_Save_f (void)
{
	if (!sv.active)
		return;
	if (cl.intermission)
		return;
	if (svs.maxclients != 1)
		return;
	m_entersound = TRUE;
	m_state = m_save;
	qCInput::SetKeyDestination( qMENU );
	M_ScanSaves ();
}


void M_Load_Draw (void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i=0 ; i< MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i], FALSE);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print (16, 32 + 8*i, m_filenames[i], FALSE);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		qCInput::SetKeyDestination( qGAME );

	// Host_Loadgame_f can't bring up the loading plaque because too much
	// stack space has been used, so do it now
		SCR_BeginLoadingPlaque ();

	// issue the load command
		Cbuf_AddText (va ("load s%d\n", load_cursor) );
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}


void M_Save_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		m_state = m_none;
		qCInput::SetKeyDestination( qGAME );
		Cbuf_AddText (va("save s%d\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3


void M_Menu_MultiPlayer_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_multiplayer;
	m_entersound = TRUE;
}


void M_MultiPlayer_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawPic (54, 32 + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%d.lmp", f+1 ) ) );

	if( !NET_GetTCPIPAvailable() )
		M_Print ((320/2) - ((27*8)/2), 148, "No Communications Available", FALSE);

}


void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = TRUE;
		switch (m_multiplayer_cursor)
		{
		case 0:
			if (NET_GetTCPIPAvailable())
				M_Menu_Net_f ();
			break;

		case 1:
			if (NET_GetTCPIPAvailable())
				M_Menu_Net_f ();
			break;

		case 2:
			M_Menu_Setup_f ();
			break;
		}
	}
}

//=============================================================================
/* SETUP MENU */

int		setup_cursor = 4;
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_hostname[16];
char	setup_myname[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_setup;
	m_entersound = TRUE;
	strcpy(setup_myname, g_CVarClientName.pString);
	strcpy(setup_hostname, hostname.pString);
	setup_top = setup_oldtop = ((int)g_CVarClientColor.Value) >> 4;
	setup_bottom = setup_oldbottom = ((int)g_CVarClientColor.Value) & 15;
}


void M_Setup_Draw (void)
{
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (64, 40, "Hostname", FALSE);
	M_DrawTextBox (160, 32, 16, 1);
	M_Print (168, 40, setup_hostname, FALSE);

	M_Print (64, 56, "Your name", FALSE);
	M_DrawTextBox (160, 48, 16, 1);
	M_Print (168, 56, setup_myname, FALSE);

	M_Print (64, 80, "Shirt color", FALSE);
	M_Print (64, 104, "Pants color", FALSE);

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes", FALSE);

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 72, p);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
}


void M_Setup_Key (int k)
{
	int			l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (OK)
		if (strcmp(g_CVarClientName.pString, setup_myname) != 0)
			Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
		if (strcmp(hostname.pString, setup_hostname) != 0)
			qCCVar::Set("hostname", setup_hostname);
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText( va ("color %d %d\n", setup_top, setup_bottom) );
		m_entersound = TRUE;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l+1] = 0;
				setup_hostname[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l+1] = 0;
				setup_myname[l] = k;
			}
		}
	}

	if (setup_top > 13)		setup_top = 0;
	if (setup_top < 0)		setup_top = 13;
	if (setup_bottom > 13)	setup_bottom = 0;
	if (setup_bottom < 0)	setup_bottom = 13;
}

//=============================================================================
/* NET MENU */

int	m_net_cursor;
int m_net_items;
int m_net_saveHeight;

char *net_helpMessage [] =
{
  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_net;
	m_entersound = TRUE;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	f = 32;
	f += 19;
	f += 19;
	f += 19;
	if (NET_GetTCPIPAvailable())	p = Draw_CachePic ("gfx/netmen4.lmp");
	else							p = Draw_CachePic ("gfx/dim_tcp.lmp");
	M_DrawPic (72, f, p);

	if (m_net_items == 5)	// JDC, could just be removed
	{
		f += 19;
		p = Draw_CachePic ("gfx/netmen5.lmp");
		M_DrawPic (72, f, p);
	}

	f = (320-26*8)/2;
	M_DrawTextBox (f, 134, 24, 4);
	f += 8;
	M_Print (f, 142, net_helpMessage[0], FALSE);
	M_Print (f, 150, net_helpMessage[1], FALSE);
	M_Print (f, 158, net_helpMessage[2], FALSE);
	M_Print (f, 166, net_helpMessage[3], FALSE);

	f = (int)(host_time * 10)%6;
	M_DrawPic (54, 32 + m_net_cursor * 20,Draw_CachePic( va("gfx/menudot%d.lmp", f+1 ) ) );
}


void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_ENTER:
		m_entersound = TRUE;

		switch (m_net_cursor)
		{
		case 2:
			M_Menu_LanConfig_f ();
			break;

		case 3:
			M_Menu_LanConfig_f ();
			break;

		case 4:
// multiprotocol
			break;
		}
	}

	if (m_net_cursor == 0)						goto again;
	if (m_net_cursor == 1)						goto again;
	if (m_net_cursor == 2)						goto again;
	if (m_net_cursor == 3 && !NET_GetTCPIPAvailable())	goto again;
}

//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	14
#define	SLIDER_RANGE	10

int		options_cursor;

void M_Menu_Options_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_options;
	m_entersound = TRUE;

	if ((options_cursor == 13) && (modestate != MS_WINDOWED))
	{
		options_cursor = 0;
	}
}


void M_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 3:	// screen size
		scr_viewsize.Value += dir * 10;
		if (scr_viewsize.Value < 30)
			scr_viewsize.Value = 30;
		if (scr_viewsize.Value > 120)
			scr_viewsize.Value = 120;
		qCCVar::Set ("viewsize", scr_viewsize.Value);
		break;
	case 5:	// mouse speed
		g_CVarMouseSensitivity.Value += dir * 0.5;
		if (g_CVarMouseSensitivity.Value < 1)
			g_CVarMouseSensitivity.Value = 1;
		if (g_CVarMouseSensitivity.Value > 11)
			g_CVarMouseSensitivity.Value = 11;
		qCCVar::Set ("sensitivity", g_CVarMouseSensitivity.Value);
		break;
	case 6:
		break;
	case 7:	// sfx volume
		volume.Value += dir * 0.1;
		if (volume.Value < 0)
			volume.Value = 0;
		if (volume.Value > 1)
			volume.Value = 1;
		qCCVar::Set ("volume", volume.Value);
		break;

	case 8:	// allways run
		if (g_CVarClientSpeedForward.Value > 200)
		{
			qCCVar::Set ("cl_forwardspeed", 200);
			qCCVar::Set ("cl_backspeed", 200);
		}
		else
		{
			qCCVar::Set ("cl_forwardspeed", 400);
			qCCVar::Set ("cl_backspeed", 400);
		}
		break;

	case 9:	// invert mouse
		qCCVar::Set ("m_pitch", -g_CVarMousePitch.Value);
		break;

	case 13:	// _windowed_mouse
		qCCVar::Set ("_windowed_mouse", !_windowed_mouse.Value);
		break;
	}
}


void M_DrawSlider (int x, int y, float range)
{
	int	i;

	if (range < 0)	range = 0;
	if (range > 1)	range = 1;

	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox( const int X, const int Y, const int On )
{
	M_Print( X, Y, On ? "on": "off", FALSE );
}

void M_Options_Draw (void)
{
	float		r;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (16, 32, "    Customize controls", FALSE);
	M_Print (16, 40, "         Go to console", FALSE);
	M_Print (16, 48, "     Reset to defaults", FALSE);

	M_Print (16, 56, "           Screen size", FALSE);
	r = (scr_viewsize.Value - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 72, "           Mouse Speed", FALSE);
	r = (g_CVarMouseSensitivity.Value - 1)/10;
	M_DrawSlider (220, 72, r);

	M_Print (16, 88, "          Sound Volume", FALSE);
	r = volume.Value;
	M_DrawSlider (220, 88, r);

	M_Print (16, 96,  "            Always Run", FALSE);
	M_DrawCheckbox (220, 96, g_CVarClientSpeedForward.Value > 200);

	M_Print (16, 104, "          Invert Mouse", FALSE);
	M_DrawCheckbox (220, 104, g_CVarMousePitch.Value < 0);

	if (modestate == MS_WINDOWED)
	{
		M_Print (16, 136, "             Use Mouse", FALSE);
		M_DrawCheckbox (220, 136, _windowed_mouse.Value);
	}

// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		m_entersound = TRUE;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			m_state = m_none;
			Con_ToggleConsole_f ();
			break;
		case 2:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}

	if ((options_cursor == 13) && (modestate != MS_WINDOWED))
	{
		if (k == K_UPARROW)
			options_cursor = 12;
		else
			options_cursor = 0;
	}
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"impulse 10", 		"change weapon"},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"}
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_keys;
	m_entersound = TRUE;
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = qCInput::GetKeyBinding( j );
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = qCInput::GetKeyBinding( j );
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			qCInput::SetBinding (j, "");
	}
}


void M_Keys_Draw (void)
{
	int		i, l;
	int		keys[2];
	char	*name;
	int		x, y;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action", FALSE);
	else
		M_Print (18, 32, "Enter to change, backspace to clear", FALSE);

// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1], FALSE);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (140, y, "???", FALSE);
		}
		else
		{
			name = qCInput::KeyNumToString (keys[0]);
			M_Print (140, y, name, FALSE);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or", FALSE);
				M_Print (140 + x + 32, y, qCInput::KeyNumToString (keys[1]), FALSE);
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Keys_Key (int k)
{
	char	cmd[80];
	int		keys[2];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = FALSE;
		}
		else if (k != '`')
		{
			snprintf (cmd, sizeof(cmd),"bind \"%s\" \"%s\"\n", qCInput::KeyNumToString (k), bindnames[keys_cursor][0]);
			Cbuf_InsertText (cmd);
		}

		bind_grab = FALSE;
		return;
	}

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = TRUE;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_help;
	m_entersound = TRUE;
	help_page = 0;
}



void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%d.lmp", help_page)) );
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = TRUE;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = TRUE;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
int		m_quit_prevstate;
qBool	wasInMenus;

char *quitMessage [] =
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",

  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this TRUE? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",

  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",

  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",

  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",

  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
};

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (qCInput::GetKeyDestination() == qMENU);
	qCInput::SetKeyDestination( qMENU );
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = TRUE;
	msgNumber = rand()&7;
}


void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = (estate_t)m_quit_prevstate;
			m_entersound = TRUE;
		}
		else
		{
			qCInput::SetKeyDestination( qGAME );
			m_state = m_none;
		}
		break;

	case 'Y':
	case 'y':
		qCInput::SetKeyDestination( qCONSOLE );
		Host_Quit_f ();
		break;

	default:
		break;
	}

}


void M_Quit_Draw (void)
{
	if (wasInMenus)
	{
		m_state = (estate_t)m_quit_prevstate;
		m_recursiveDraw = TRUE;
		M_Draw ();
		m_state = m_quit;
	}

	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[msgNumber*4+0], FALSE);
	M_Print (64, 92,  quitMessage[msgNumber*4+1], FALSE);
	M_Print (64, 100, quitMessage[msgNumber*4+2], FALSE);
	M_Print (64, 108, quitMessage[msgNumber*4+3], FALSE);
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
int		lanConfig_cursor_table [] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS	3

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_lanconfig;
	m_entersound = TRUE;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)	lanConfig_cursor = 2;
		else							lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = NET_GetDefaultHostPort();
	snprintf(lanConfig_portname, sizeof(lanConfig_portname),"%u", lanConfig_port);

	qCNetworkDatagram::SetReturnOnError( FALSE );
	qCNetworkDatagram::SetReturnReason( "" );
}


void M_LanConfig_Draw (void)
{
	qpic_t	*p;
	int		basex;
	char	*startJoin;
	char	*protocol;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";

	protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol), FALSE);
	basex += 8;

	M_Print (basex, 52, "Address:", FALSE);
	M_Print (basex+9*8, 52, NET_GetMyTCPIPAddress(), FALSE);

	M_Print (basex, lanConfig_cursor_table[0], "Port", FALSE);
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname, FALSE);

	if (JoiningGame)
	{
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...", FALSE);
		M_Print (basex, 108, "Join game at:", FALSE);
		M_DrawTextBox (basex+8, lanConfig_cursor_table[2]-8, 22, 1);
		M_Print (basex+16, lanConfig_cursor_table[2], lanConfig_joinname, FALSE);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print (basex+8, lanConfig_cursor_table[1], "OK", FALSE);
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

	if( qCNetworkDatagram::GetReturnReason()[ 0 ] )
		M_Print( basex, 148, qCNetworkDatagram::GetReturnReason(), FALSE );
}


void M_LanConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = TRUE;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_Search_f();
			break;
		}

		if (lanConfig_cursor == 2)
		{
			qCNetworkDatagram::SetReturnOnError( TRUE );
			qCInput::SetKeyDestination( qGAME );
			m_state = m_none;
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
			break;
		}

		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (lanConfig_cursor == 2)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l+1] = 0;
				lanConfig_joinname[l] = key;
			}
		}

		if (key < '0' || key > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = key;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 2)
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;

	l =  atoi(lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	snprintf(lanConfig_portname, sizeof(lanConfig_portname),"%u", lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t		levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t	episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

int	startepisode;
int	startlevel;
qUInt32 maxplayers;
qBool m_serverInfoMessage = FALSE;
double m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_gameoptions;
	m_entersound = TRUE;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	qpic_t	*p;
	int		x;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game", FALSE);

	M_Print (0, 56, "      Max players", FALSE);
	M_Print (160, 56, va("%d", maxplayers), FALSE);

	M_Print (0, 64, "        Game Type", FALSE);
	if (coop.Value)
		M_Print (160, 64, "Cooperative", FALSE);
	else
		M_Print (160, 64, "Deathmatch", FALSE);

	M_Print (0, 72, "        Teamplay", FALSE);

	{
		char *msg;

		switch((int)teamplay.Value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg, FALSE);
	}

	M_Print (0, 80, "            Skill", FALSE);
	if (skill.Value == 0)
		M_Print (160, 80, "Easy difficulty", FALSE);
	else if (skill.Value == 1)
		M_Print (160, 80, "Normal difficulty", FALSE);
	else if (skill.Value == 2)
		M_Print (160, 80, "Hard difficulty", FALSE);
	else
		M_Print (160, 80, "Nightmare difficulty", FALSE);

	M_Print (0, 88, "       Frag Limit", FALSE);
	if (fraglimit.Value == 0)
		M_Print (160, 88, "none", FALSE);
	else
		M_Print (160, 88, va("%d frags", (int)fraglimit.Value), FALSE);

	M_Print (0, 96, "       Time Limit", FALSE);
	if (timelimit.Value == 0)
		M_Print (160, 96, "none", FALSE);
	else
		M_Print (160, 96, va("%d minutes", (int)timelimit.Value), FALSE);

	M_Print (0, 112, "         Episode", FALSE);
    M_Print (160, 112, episodes[startepisode].description, FALSE);

	M_Print (0, 120, "           Level", FALSE);
    M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description, FALSE);
   M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name, FALSE);

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ", FALSE);
			M_Print (x, 154, " requires using command ", FALSE);
			M_Print (x, 162, "line parameters; please ", FALSE);
			M_Print (x, 170, "   see techinfo.txt.    ", FALSE);
		}
		else
		{
			m_serverInfoMessage = FALSE;
		}
	}
}


void M_NetStart_Change (int dir)
{
	int count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = TRUE;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		qCCVar::Set ("coop", coop.Value ? 0 : 1);
		break;

	case 3:
		count = 2;

		qCCVar::Set ("teamplay", teamplay.Value + dir);
		if (teamplay.Value > count)
			qCCVar::Set ("teamplay", 0.0f);
		else if (teamplay.Value < 0)
			qCCVar::Set ("teamplay", count);
		break;

	case 4:
		qCCVar::Set ("skill", skill.Value + dir);
		if (skill.Value > 3)
			qCCVar::Set ("skill", 0.0f);
		if (skill.Value < 0)
			qCCVar::Set ("skill", 3);
		break;

	case 5:
		qCCVar::Set ("fraglimit", fraglimit.Value + dir*10);
		if (fraglimit.Value > 100)
			qCCVar::Set ("fraglimit", 0.0f);
		if (fraglimit.Value < 0)
			qCCVar::Set ("fraglimit", 100);
		break;

	case 6:
		qCCVar::Set ("timelimit", timelimit.Value + dir*5);
		if (timelimit.Value > 60)
			qCCVar::Set ("timelimit", 0.0f);
		if (timelimit.Value < 0)
			qCCVar::Set ("timelimit", 60);
		break;

	case 7:
		startepisode += dir;
		count = 7;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );
			SCR_BeginLoadingPlaque ();

			Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

qBool	searchComplete = FALSE;
double		searchCompleteTime;

void M_Menu_Search_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_search;
	m_entersound = FALSE;
	slistSilent = TRUE;
	slistLocal = FALSE;
	searchComplete = FALSE;
	NET_Slist_f();

}


void M_Search_Draw (void)
{
	qpic_t	*p;
	int x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	x = (320/2) - ((12*8)/2) + 4;
	M_DrawTextBox (x-8, 32, 12, 1);
	M_Print (x, 40, "Searching...", FALSE);

	if(slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (! searchComplete)
	{
		searchComplete = TRUE;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f ();
		return;
	}

	M_Print ((320/2) - ((22*8)/2), 64, "No Quake servers found", FALSE);
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

qSInt32		slist_cursor;
qBool slist_sorted;

void M_Menu_ServerList_f (void)
{
	qCInput::SetKeyDestination( qMENU );
	m_state = m_slist;
	m_entersound = TRUE;
	slist_cursor = 0;
	qCNetworkDatagram::SetReturnOnError( FALSE );
	qCNetworkDatagram::SetReturnReason( "" );
	slist_sorted = FALSE;
}


void M_ServerList_Draw (void)
{
	qUInt32		n;
	char	string [64];
	qpic_t	*p;

	if (!slist_sorted)
	{
		if (hostCacheCount > 1)
		{
			qUInt32	i,j;
			qSNetworkHostCache temp;
			for (i = 0; i < hostCacheCount; i++)
				for (j = i+1; j < hostCacheCount; j++)
					if (strcmp(hostcache[j].Name, hostcache[i].Name) < 0)
					{
						memcpy(&temp, &hostcache[j], sizeof(qSNetworkHostCache));
						memcpy(&hostcache[j], &hostcache[i], sizeof(qSNetworkHostCache));
						memcpy(&hostcache[i], &temp, sizeof(qSNetworkHostCache));
					}
		}
		slist_sorted = TRUE;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	for (n = 0; n < hostCacheCount; n++)
	{
		if (hostcache[n].MaxUsers)
			snprintf( string, sizeof( string ), "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].Name, hostcache[n].Map, hostcache[n].Users, hostcache[n].MaxUsers);
		else
			snprintf( string, sizeof( string ), "%-15.15s %-15.15s\n", hostcache[n].Name, hostcache[n].Map);
		M_Print (16, 32 + 8*n, string, FALSE);
	}
	M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

	if( qCNetworkDatagram::GetReturnReason()[ 0 ] )
		M_Print( 16, 148, qCNetworkDatagram::GetReturnReason(), FALSE );
}


void M_ServerList_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= ( qSInt32 )hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		qCNetworkDatagram::SetReturnOnError( TRUE );
		slist_sorted = FALSE;
		qCInput::SetKeyDestination( qGAME );
		m_state = m_none;
		Cbuf_AddText ( va ("connect \"%s\"\n", hostcache[slist_cursor].CName) );
		break;

	default:
		break;
	}

}

//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void M_Draw (void)
{
	if (m_state == m_none || qCInput::GetKeyDestination() != qMENU)
		return;

	if (!m_recursiveDraw)
	{
		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
			S_ExtraUpdate ();
		}
		else
			Draw_FadeScreen ();
	}
	else
	{
		m_recursiveDraw = FALSE;
	}

	switch (m_state)
	{
		case m_none:			{							} break;
		case m_main:			{ M_Main_Draw();			} break;
		case m_singleplayer:	{ M_SinglePlayer_Draw();	} break;
		case m_load:			{ M_Load_Draw();			} break;
		case m_save:			{ M_Save_Draw();			} break;
		case m_multiplayer:		{ M_MultiPlayer_Draw();		} break;
		case m_setup:			{ M_Setup_Draw();			} break;
		case m_net:				{ M_Net_Draw();				} break;
		case m_options:			{ M_Options_Draw();			} break;
		case m_keys:			{ M_Keys_Draw();			} break;
		case m_help:			{ M_Help_Draw();			} break;
		case m_quit:			{ M_Quit_Draw();			} break;
		case m_lanconfig:		{ M_LanConfig_Draw();		} break;
		case m_gameoptions:		{ M_GameOptions_Draw();		} break;
		case m_search:			{ M_Search_Draw();			} break;
		case m_slist:			{ M_ServerList_Draw();		} break;
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = FALSE;
	}

	S_ExtraUpdate ();
}


void M_Keydown( const int Key )
{
	switch( m_state )
	{
		case m_none:			{								} break;
		case m_main:			{ M_Main_Key( Key );			} break;
		case m_singleplayer:	{ M_SinglePlayer_Key( Key );	} break;
		case m_load:			{ M_Load_Key( Key );			} break;
		case m_save:			{ M_Save_Key( Key );			} break;
		case m_multiplayer:		{ M_MultiPlayer_Key( Key );		} break;
		case m_setup:			{ M_Setup_Key( Key );			} break;
		case m_net:				{ M_Net_Key( Key );				} break;
		case m_options:			{ M_Options_Key( Key );			} break;
		case m_keys:			{ M_Keys_Key( Key );			} break;
		case m_help:			{ M_Help_Key( Key );			} break;
		case m_quit:			{ M_Quit_Key( Key );			} break;
		case m_lanconfig:		{ M_LanConfig_Key( Key );		} break;
		case m_gameoptions:		{ M_GameOptions_Key( Key );		} break;
		case m_search:			{ M_Search_Key( Key );			} break;
		case m_slist:			{ M_ServerList_Key( Key );		} break;
	}
}


void M_ConfigureNetSubsystem(void)
{
// enable/disable net systems to match desired config

	Cbuf_AddText ("stopdemo\n");

	if (TCPIPConfig)
		NET_SetHostPort( lanConfig_port );
}
