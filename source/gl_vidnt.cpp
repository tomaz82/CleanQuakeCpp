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
// gl_vidnt.c -- NT GL vid component

#include	"quakedef.h"
#include	"winquake.h"
#include	<commctrl.h>

#include	"vid.h"
#include	"sys.h"
#include	"qCEndian.h"
#include	"draw.h"
#include	"qCCVar.h"
#include	"screen.h"
#include	"sound.h"
#include	"qCInput.h"
#include	"console.h"
#include	"glquake.h"

#define MAX_MODE_LIST	128

#define NO_MODE					-1
#define MODE_WINDOWED			 0
#define MODE_FULLSCREEN_DEFAULT	 1

typedef struct {
	int			width;
	int			height;
	int			modenum;
	int			fullscreen;
} vmode_t;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qBool		scr_skipupdate;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;

static DEVMODE	gdevmode;
static qBool	vid_initialized = FALSE;
static qBool	windowed;
static qBool vid_canalttab = FALSE;
static qBool vid_wassuspended = FALSE;
static int		windowed_mouse;

RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow;

int			vid_modenum = NO_MODE;
int			vid_default = MODE_WINDOWED;
static int	windowed_default;

HGLRC	baseRC;
HDC		maindc;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

unsigned	d_8to24table[256];

modestate_t	modestate = MS_UNINIT;

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

//====================================

qSCVar		_windowed_mouse = {"_windowed_mouse","1", TRUE};

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

void CenterWindow(HWND hWndCenter, int width, int height)
{
	SetWindowPos (hWndCenter, NULL, (GetSystemMetrics (SM_CXSCREEN) - width)  * 0.5f, (GetSystemMetrics (SM_CYSCREEN) - height) * 0.5f, 0, 0,	SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qBool VID_SetWindowedMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate, width, height;
	RECT			rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindowEx (
		 ExWindowStyle,
		 "CleanQuake",
		 "CleanQuake",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(mainwindow, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top);

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(mainwindow, hdc);

	if (vid.conheight > (unsigned int)modelist[modenum].height)
		vid.conheight = (unsigned int)modelist[modenum].height;
	if (vid.conwidth > (unsigned int)modelist[modenum].width)
		vid.conwidth = (unsigned int)modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	return TRUE;
}

qBool VID_SetFullDIBMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate, width, height;
	RECT			rect;

	gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	gdevmode.dmBitsPerPel = 32;
	gdevmode.dmPelsWidth = modelist[modenum].width;
	gdevmode.dmPelsHeight = modelist[modenum].height;
	gdevmode.dmSize = sizeof (gdevmode);

	if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		Sys_Error ("Couldn't set fullscreen DIB mode");

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindowEx (
		 ExWindowStyle,
		 "CleanQuake",
		 "CleanQuake",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(mainwindow, hdc);

	if (vid.conheight > (unsigned int)modelist[modenum].height)
		vid.conheight = (unsigned int)modelist[modenum].height;
	if (vid.conwidth > (unsigned int)modelist[modenum].width)
		vid.conwidth = (unsigned int)modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	return TRUE;
}


int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp;
	qBool		stat;
    MSG				msg;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode\n");
	}

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = TRUE;

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (!modelist[modenum].fullscreen)
	{
		if (_windowed_mouse.Value && qCInput::GetKeyDestination() == qGAME)
		{
			stat = VID_SetWindowedMode(modenum);
			qCInput::MouseActivate( TRUE );
			qCInput::MouseShow( FALSE );
		}
		else
		{
			stat = VID_SetWindowedMode(modenum);
			qCInput::MouseActivate( FALSE );
			qCInput::MouseShow( TRUE );
		}
	}
	else
	{
		stat = VID_SetFullDIBMode(modenum);
		qCInput::MouseActivate( TRUE );
		qCInput::MouseShow( FALSE );
	}

	VID_UpdateWindowStatus ();

	scr_disabled_for_loading = temp;

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);
	vid_modenum = modenum;

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();
	Con_SafePrintf ("Video mode %dx%d initialized.\n", modelist[ vid_modenum ].width, modelist[ vid_modenum ].height );

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return TRUE;
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	qCInput::MouseClipCursor();
}


//====================================

int		texture_extension_number = 1;

GLenum TEXTURE0_SGIS_ARB;
GLenum TEXTURE1_SGIS_ARB;

typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALFARPROC)( int );
PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;

void CheckMultiTextureExtensions(void)
{
	if( strstr( gl_extensions, "WGL_EXT_swap_control" ) != 0 )
	{
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress( "wglSwapIntervalEXT" );

		if( wglSwapIntervalEXT )
			wglSwapIntervalEXT( 0 );
	}

//////////////////////////////////////////////////////////////////////////

	if (strstr(gl_extensions, "GL_ARB_multitexture"))
	{
		Con_Printf("GL_ARB_multitexture enabled\n\n");
		qglMultiTexCoord2f	= (lpMTexFUNC) wglGetProcAddress("glMultiTexCoord2fARB");
		qglActiveTexture	= (lpSelTexFUNC) wglGetProcAddress("glActiveTextureARB");
		TEXTURE0_SGIS_ARB	= TEXTURE0_ARB_;
		TEXTURE1_SGIS_ARB	= TEXTURE1_ARB_;
		return;
	}

	if (strstr(gl_extensions, "GL_SGIS_multitexture"))
	{
		Con_Printf("GL_SGIS_multitexture enabled\n\n");
		qglMultiTexCoord2f	= (lpMTexFUNC) wglGetProcAddress("glMTexCoord2fSGIS");
		qglActiveTexture	= (lpSelTexFUNC) wglGetProcAddress("glSelectTextureSGIS");
		TEXTURE0_SGIS_ARB	= TEXTURE0_SGIS_;
		TEXTURE1_SGIS_ARB	= TEXTURE1_SGIS_;
		return;
	}

	Sys_Error( "Multitexture Not Found" );
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	gl_vendor		= ( const char* )glGetString( GL_VENDOR );
	gl_renderer		= ( const char* )glGetString( GL_RENDERER );
	gl_version		= ( const char* )glGetString( GL_VERSION );
	gl_extensions	= ( const char* )glGetString( GL_EXTENSIONS );

	Con_Printf( "GL_VENDOR:     %s\n", gl_vendor );
	Con_Printf( "GL_RENDERER:   %s\n", gl_renderer );
	Con_Printf( "GL_VERSION:    %s\n", gl_version );
//	Con_Printf( "GL_EXTENSIONS: %s\n", gl_extensions );

	CheckMultiTextureExtensions();

	glClearColor( 0, 0, 0, 255 );
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_SMOOTH);
	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *width, int *height)
{
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;
}


void GL_EndRendering (void)
{
	if (!scr_skipupdate)
		SwapBuffers(maindc);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse.Value) {
			if (windowed_mouse)	{
				qCInput::MouseActivate( FALSE );
				qCInput::MouseShow( TRUE );
				windowed_mouse = FALSE;
			}
		} else {
			windowed_mouse = TRUE;
			if (qCInput::GetKeyDestination() == qGAME && !qCInput::MouseIsActive() && ActiveApp) {
				qCInput::MouseActivate( TRUE );
				qCInput::MouseShow( FALSE );
			} else if (qCInput::MouseIsActive() && qCInput::GetKeyDestination() != qGAME) {
				qCInput::MouseActivate( FALSE );
				qCInput::MouseShow( TRUE );
			}
		}
	}
}

void VID_SetPalette( qUInt8* pPalette )
{
	unsigned int*	pTable	= d_8to24table;
	unsigned int	i;

	for( i=0; i<256; ++i )
	{
		qUInt8	Red		= *pPalette++;
		qUInt8	Green	= *pPalette++;
		qUInt8	Blue	= *pPalette++;

		*pTable++	= ( 255 << 24 ) + ( Red << 0 ) + ( Green << 8 ) + ( Blue << 16 );
	}

	d_8to24table[ 255 ] = 0;	// 255 is transparent
}

void VID_SetDefaultMode (void)
{
	qCInput::MouseActivate( FALSE );
}

void	VID_Shutdown (void)
{
   	HGLRC hRC;
   	HDC	  hDC;

	if (vid_initialized)
	{
		vid_canalttab = FALSE;
		hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

    	wglMakeCurrent(NULL, NULL);

    	if (hRC)
    	    wglDeleteContext(hRC);

		if (hDC && mainwindow)
			ReleaseDC(mainwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);

		AppActivate(FALSE, FALSE);
	}
}


//==========================================================================


BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGLz
	|  PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
	32,				// 32-bit z-buffer
	0,				// no stencil buffer
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}



qUInt8        scantokey[128] =
					{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*',
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE  ,    0  , K_HOME,
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11,
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7
					};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return scantokey[key];
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	for( qUInt32 KeyIndex=0; KeyIndex<256; ++KeyIndex )
		qCInput::KeyEvent( KeyIndex, FALSE );

	qCInput::ClearStates();
}

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
 	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		sound_active = FALSE;
	}
	else if (ActiveApp && !sound_active)
	{
		sound_active = TRUE;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			qCInput::MouseActivate( TRUE );
			qCInput::MouseShow( FALSE );
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = FALSE;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);
				MoveWindow(mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, FALSE);
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.Value && qCInput::GetKeyDestination() == qGAME)
		{
			qCInput::MouseActivate( TRUE );
			qCInput::MouseShow( FALSE );
		}
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			qCInput::MouseActivate( FALSE );
			qCInput::MouseShow( TRUE );
			if (vid_canalttab) {
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = TRUE;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.Value)
		{
			qCInput::MouseActivate( FALSE );
			qCInput::MouseShow( TRUE );
		}
	}
}


/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LONG    lRet = 0;
	int		fActive, fMinimized, temp;

    switch (uMsg)
    {
		case WM_ERASEBKGND:
			return 1;

		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			qCInput::KeyEvent (MapKey(lParam), TRUE);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			qCInput::KeyEvent (MapKey(lParam), FALSE);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			qCInput::MouseEvent( temp );

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL:
			if ((short) HIWORD(wParam) > 0) {
				qCInput::KeyEvent(K_MWHEELUP, TRUE);
				qCInput::KeyEvent(K_MWHEELUP, FALSE);
			} else {
				qCInput::KeyEvent(K_MWHEELDOWN, TRUE);
				qCInput::KeyEvent(K_MWHEELDOWN, FALSE);
			}
			break;

    	case WM_SIZE:
            break;

   	    case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
			if (mainwindow)
				DestroyWindow (mainwindow);

            PostQuitMessage (0);
        }
        break;

    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}

void VID_InitDIB (HINSTANCE hInstance)
{
	WNDCLASS		wc;

	/* Register the frame class */
    wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "CleanQuake";

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	if (COM_CheckParm("-width"))
		modelist[0].width = atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height= atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].fullscreen = 0;

	nummodes = 1;
}


/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (void)
{
	DEVMODE	devmode;
	int		i, modenum, originalnummodes, existingmode;
	BOOL	stat;

// enumerate 32 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 32) &&
			(devmode.dmPelsWidth >= 640) &&
			(devmode.dmPelsHeight >= 480) &&
			(nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL |
							   DM_PELSWIDTH |
							   DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].fullscreen = 1;

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int		i;
	int		basenummodes, width, height;
	HDC		hdc;
	DEVMODE	devmode;

	memset(&devmode, 0, sizeof(devmode));

	qCCVar::Register (&_windowed_mouse);

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB ();

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = TRUE;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = FALSE;

		if (COM_CheckParm("-width"))
		{
			width = atoi(com_argv[COM_CheckParm("-width")+1]);
		}
		else
		{
			width = 640;
		}

		if (COM_CheckParm("-height"))
		{
			height = atoi(com_argv[COM_CheckParm("-height")+1]);

			for (i=1, vid_default=0 ; i<nummodes ; i++)
			{
				if ((modelist[i].width == width) &&
					(modelist[i].height == height))
				{
					vid_default = i;
					break;
				}
			}
		}
		else
		{
			for (i=1, vid_default=0 ; i<nummodes ; i++)
			{
				if ((modelist[i].width == width))
				{
					vid_default = i;
					break;
				}
			}
		}

		if (!vid_default)
		{
			Sys_Error ("Specified video mode not available");
		}
	}

	vid_initialized = TRUE;

	if ((i = COM_CheckParm ("-conwidth")) != 0)
	{
		vid.conwidth = atoi (com_argv[i + 1]);
		vid.conwidth &= 0xfff8; // make it a multiple of eight

		// pick a conheight that matches with correct aspect
		vid.conheight = vid.conwidth * 3 / 4;
	}
	else
	{
		vid.conwidth = modelist[vid_default].width;
		vid.conheight = modelist[vid_default].height;
	}

	if ((i = COM_CheckParm ("-conheight")) != 0)
		vid.conheight = atoi (com_argv[i + 1]);

	if (vid.conwidth < 320) vid.conwidth = 320;
	if (vid.conheight < 200) vid.conheight = 200;

	vid.colormap = host_colormap;
	vid.fullbright = 256 - qCEndian::LittleInt32(*((int *)vid.colormap + 2048));

	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

    maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

    baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
    if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();

	vid_canalttab = TRUE;
}
