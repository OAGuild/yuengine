/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// console.c

#include "client.h"


int g_console_field_width = 78;


#define	NUM_CON_TIMES 4

#define		CON_TEXTSIZE	32768

typedef struct {
	qboolean	initialized;

	short	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line

	int		linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at scr_conspeed
	float	finalFrac;		// 0.0 to 1.0 lines of console to display

	int		vislines;		// in scanlines

	int		times[NUM_CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines
	vec4_t	color;
} console_t;

#define	NUM_CON 4
#define CON_ALL 0
#define CON_SYS 1
#define CON_CHAT 2
#define CON_TCHAT 3

// names for the consoles
const char *conNames[] = {
	"all",
	"sys",
	"chat",
	"tchat",
};

// color indexes in g_color_table for consoles
const int conColors[] = {
	1,
	3,
	2,
	5
};

console_t	con[NUM_CON];
console_t	*activeCon = con;

cvar_t		*con_conspeed;
cvar_t		*con_autoclear;
cvar_t		*con_notifytime;

#define	DEFAULT_CONSOLE_WIDTH	78

/*
================
Con_AcceptLine

When the user enters a command in the console
================
*/
void Con_AcceptLine( void )
{
	int conNum = activeCon - con;
	qboolean isChat = conNum == CON_CHAT || conNum == CON_TCHAT;

	// reset if cmdmode
	if ( cmdmode ) {
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
		cmdmode = qfalse;
	}

	// if in sys-console or not in the game explicitly prepend a slash if
	// needed
	if ( (clc.state != CA_ACTIVE || conNum == CON_SYS) &&
			con_autochat->integer &&
			g_consoleField.buffer[0] &&
			g_consoleField.buffer[0] != '\\' &&
			g_consoleField.buffer[0] != '/' ) {
		char	temp[MAX_EDIT_LINE-1];

		Q_strncpyz( temp, g_consoleField.buffer, sizeof( temp ) );
		Com_sprintf( g_consoleField.buffer, sizeof( g_consoleField.buffer ), "\\%s", temp );
		g_consoleField.cursor++;
	}

	// print prompts for non-chat consoles
	if (!isChat)
		Com_Printf ( "]%s\n", g_consoleField.buffer );

	// leading slash is an explicit command (for non-chat consoles)
	if ( !isChat && (g_consoleField.buffer[0] == '\\' || g_consoleField.buffer[0] == '/') ) {
		Cbuf_AddText( g_consoleField.buffer+1 );	// valid command
		Cbuf_AddText ("\n");
	} else {
		// other text will be chat messages if in all-console with con_autochat
		// or when in chat-console
		if ( !g_consoleField.buffer[0] ) {
			return;	// empty lines just scroll the console without adding to history
		} else {
			if ( (con_autochat->integer && conNum == CON_ALL) || conNum == CON_CHAT ) {
				Cbuf_AddText ("cmd say \"");
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText ("\"");
			} else if ( conNum == CON_TCHAT ){
				Cbuf_AddText ("cmd say_team \"");
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText ("\"");
			} else {
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText ("\n");
			}
		}
	}

	// copy line to history buffer
	historyEditLines[nextHistoryLine % COMMAND_HISTORY] = g_consoleField;
	nextHistoryLine++;
	historyLine = nextHistoryLine;

	Field_Clear( &g_consoleField );

	g_consoleField.widthInChars = g_console_field_width;

	CL_SaveConsoleHistory( );

	if ( clc.state == CA_DISCONNECTED ) {
		SCR_UpdateScreen ();	// force an update, because the command
	}							// may take some time
}


/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
	// Can't toggle the console when it's the only thing available
	if ( clc.state == CA_DISCONNECTED && Key_GetCatcher( ) == KEYCATCH_CONSOLE ) {
		return;
	}

	// if in command mode, switch to regular console
	if ( cmdmode ) {
		cmdmode = qfalse;
		return;
	}

	if ( con_autoclear->integer ) {
		Field_Clear( &g_consoleField );
	}

	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify ();
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_CONSOLE );
}

/*
===================
Con_ToggleMenu_f
===================
*/
void Con_ToggleMenu_f( void ) {
	CL_KeyEvent( K_ESCAPE, qtrue, Sys_Milliseconds() );
	CL_KeyEvent( K_ESCAPE, qfalse, Sys_Milliseconds() );
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void) {
	chat_playerNum = -1;
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void) {
	chat_playerNum = -1;
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_CROSSHAIR_PLAYER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode4_f
================
*/
void Con_MessageMode4_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_LAST_ATTACKER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_CmdMode_f
================
*/
void Con_CmdMode_f (void) {
	Field_Clear( &g_consoleField );
	cmdmode = qtrue;
	con[CON_SYS].displayFrac = 0.0;
	activeCon = &con[CON_SYS]; // change to sys console
	g_consoleField.widthInChars = 34;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_CONSOLE );
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void) {
	int		i;

	for ( i = 0 ; i < CON_TEXTSIZE ; i++ ) {
		activeCon->text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}

	Con_Bottom();		// go to end
}

/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x, i;
	short	*line;
	fileHandle_t	f;
	int		bufferlen;
	char	*buffer;
	char	filename[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".txt" );

	if (!COM_CompareExtension(filename, ".txt"))
	{
		Com_Printf("Con_Dump_f: Only the \".txt\" extension is supported by this command!\n");
		return;
	}

	f = FS_FOpenFileWrite( filename );
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open %s.\n", filename);
		return;
	}

	Com_Printf ("Dumped console text to %s.\n", filename );

	// skip empty lines
	for (l = activeCon->current - activeCon->totallines + 1 ; l <= activeCon->current ; l++)
	{
		line = activeCon->text + (l%activeCon->totallines)*activeCon->linewidth;
		for (x=0 ; x<activeCon->linewidth ; x++)
			if ((line[x] & 0xff) != ' ')
				break;
		if (x != activeCon->linewidth)
			break;
	}

#ifdef _WIN32
	bufferlen = activeCon->linewidth + 3 * sizeof ( char );
#else
	bufferlen = activeCon->linewidth + 2 * sizeof ( char );
#endif

	buffer = Hunk_AllocateTempMemory( bufferlen );

	// write the remaining lines
	buffer[bufferlen-1] = 0;
	for ( ; l <= activeCon->current ; l++)
	{
		line = activeCon->text + (l%activeCon->totallines)*activeCon->linewidth;
		for(i=0; i<activeCon->linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x=activeCon->linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
#ifdef _WIN32
		Q_strcat(buffer, bufferlen, "\r\n");
#else
		Q_strcat(buffer, bufferlen, "\n");
#endif
		FS_Write(buffer, strlen(buffer), f);
	}

	Hunk_FreeTempMemory( buffer );
	FS_FCloseFile( f );
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	int		i;

	for ( i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		activeCon->times[i] = 0;
	}
}

/*
================
Con_SwitchConsole

Change to console number n
================
*/
void Con_SwitchConsole( int n ) {
	con[n].displayFrac = activeCon->displayFrac;
	con[n].finalFrac = activeCon->finalFrac;

	if ( n < 0 || n >= NUM_CON ) {
		Com_Printf( "Invalid console number %i\n", n );
	} else {
		activeCon = &con[n];
	}
}

/*
================
Con_NextConsole

Change to console n steps relative to current console, will wrap around, n can
be negative in which case it will switch backwards
================
*/
void Con_NextConsole( int n ) {
	Con_SwitchConsole( (NUM_CON + activeCon - con + n) % NUM_CON );
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
static void Con_CheckResize (console_t *con)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];

	width = (SCREEN_WIDTH / SMALLCHAR_WIDTH) - 2;

	if (width == con->linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
		con->linewidth = width;
		con->totallines = CON_TEXTSIZE / con->linewidth;
		for(i=0; i<CON_TEXTSIZE; i++)

			con->text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}
	else
	{
		oldwidth = con->linewidth;
		con->linewidth = width;
		oldtotallines = con->totallines;
		con->totallines = CON_TEXTSIZE / con->linewidth;
		numlines = oldtotallines;

		if (con->totallines < numlines)
			numlines = con->totallines;

		numchars = oldwidth;

		if (con->linewidth < numchars)
			numchars = con->linewidth;

		Com_Memcpy (tbuf, con->text, CON_TEXTSIZE * sizeof(short));
		for(i=0; i<CON_TEXTSIZE; i++)

			con->text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';


		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con->text[(con->totallines - 1 - i) * con->linewidth + j] =
						tbuf[((con->current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con->current = con->totallines - 1;
	con->display = con->current;
}

/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, qtrue );
	}
}


/*
================
Con_Init
================
*/
void Con_Init (void) {
	int		i;

	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);
	con_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	con_autoclear = Cvar_Get("con_autoclear", "1", CVAR_ARCHIVE);

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;
	for ( i = 0 ; i < COMMAND_HISTORY ; i++ ) {
		Field_Clear( &historyEditLines[i] );
		historyEditLines[i].widthInChars = g_console_field_width;
	}
	CL_LoadConsoleHistory( );

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglemenu", Con_ToggleMenu_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("messagemode3", Con_MessageMode3_f);
	Cmd_AddCommand ("messagemode4", Con_MessageMode4_f);
	Cmd_AddCommand ("cmdmode", Con_CmdMode_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown(void)
{
	Cmd_RemoveCommand("toggleconsole");
	Cmd_RemoveCommand("togglemenu");
	Cmd_RemoveCommand("messagemode");
	Cmd_RemoveCommand("messagemode2");
	Cmd_RemoveCommand("messagemode3");
	Cmd_RemoveCommand("messagemode4");
	Cmd_RemoveCommand("cmdmode");
	Cmd_RemoveCommand("clear");
	Cmd_RemoveCommand("condump");
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (console_t *con, qboolean skipnotify)
{
	int		i;

	// mark time for transparent overlay
	if (con->current >= 0)
	{
    if (skipnotify)
		  con->times[con->current % NUM_CON_TIMES] = 0;
    else
		  con->times[con->current % NUM_CON_TIMES] = cls.realtime;
	}

	con->x = 0;
	if (con->display == con->current)
		con->display++;
	con->current++;
	for(i=0; i<con->linewidth; i++)
		con->text[(con->current%con->totallines)*con->linewidth+i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
}

/*
================
================
*/
static void CL_InitConsoles( void ) {
	int i;
	for ( i = 0; i < NUM_CON; ++i ) {
		if ( !con[i].initialized ) {
			con[i].color[0] =
			con[i].color[1] =
			con[i].color[2] =
			con[i].color[3] = 1.0f;
			con[i].linewidth = -1;
			Con_CheckResize( &con[i] );
			con[i].initialized = qtrue;
		}
	}
}

/*
================
CL_ConsolePrintToCon

Print text to target console

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
static void CL_ConsolePrintToCon( char *txt, console_t *con ) {
	int		y, l;
	unsigned char	c;
	unsigned short	color;
	qboolean skipnotify = qfalse;		// NERVE - SMF
	int prev;							// NERVE - SMF

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}

	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}

	if (!con->initialized) {
		CL_InitConsoles();
	}

	color = ColorIndex(COLOR_WHITE);

	while ( (c = *((unsigned char *) txt)) != 0 ) {
		if ( Q_IsColorString( txt ) ) {
			color = ColorIndex( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		for (l=0 ; l< con->linewidth ; l++) {
			if ( txt[l] <= ' ') {
				break;
			}

		}

		// word wrap
		if (l != con->linewidth && (con->x + l >= con->linewidth) ) {
			Con_Linefeed(con, skipnotify);

		}

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed (con, skipnotify);
			break;
		case '\r':
			con->x = 0;
			break;
		default:	// display character and advance
			y = con->current % con->totallines;
			con->text[y*con->linewidth+con->x] = (color << 8) | c;
			con->x++;
			if(con->x >= con->linewidth)
				Con_Linefeed(con, skipnotify);
			break;
		}
	}


	// mark time for transparent overlay
	if (con->current >= 0) {
		// NERVE - SMF
		if ( skipnotify ) {
			prev = con->current % NUM_CON_TIMES - 1;
			if ( prev < 0 )
				prev = NUM_CON_TIMES - 1;
			con->times[prev] = 0;
		}
		else
		// -NERVE - SMF
			con->times[con->current % NUM_CON_TIMES] = cls.realtime;
	}
}

void CL_ConsolePrint( char *txt )
{
	int cmdNum = clc.lastExecutedServerCommand;
	char *cmdStr = clc.serverCommands[cmdNum % MAX_RELIABLE_COMMANDS];
	int conNum = CON_SYS;
	static int lastCmdNum;

	if ( cmdNum > lastCmdNum ) {
		if ( Q_strncmp( cmdStr, "chat", sizeof "chat" - 1 ) == 0 ) {
			conNum = CON_CHAT;
		} else if ( Q_strncmp( cmdStr, "tchat", sizeof "tchat" - 1 ) == 0 ) {
			conNum = CON_TCHAT;
		}
	}
	lastCmdNum = cmdNum;

	CL_ConsolePrintToCon( txt, &con[CON_ALL] );
	CL_ConsolePrintToCon( txt, &con[conNum] );
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput (void) {
	int		y;

	if ( clc.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	y = activeCon->vislines - ( SMALLCHAR_HEIGHT * 2 );

	re.SetColor( activeCon->color );

	SCR_DrawSmallChar( activeCon->xadjust + 1 * SMALLCHAR_WIDTH, y, ']' );

	Field_Draw( &g_consoleField, activeCon->xadjust + 2 * SMALLCHAR_WIDTH, y,
		SCREEN_WIDTH - 3 * SMALLCHAR_WIDTH, qtrue, qtrue );
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	short	*text;
	int		i;
	int		time;
	int		skip;
	int		currentColor;

	currentColor = 7;
	re.SetColor( g_color_table[currentColor] );

	v = 2;
	for (i= activeCon->current-NUM_CON_TIMES+1 ; i<=activeCon->current ; i++)
	{
		if (i < 0)
			continue;
		time = activeCon->times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->value*1000)
			continue;
		text = activeCon->text + (i % activeCon->totallines)*activeCon->linewidth;

		if (cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
			continue;
		}

		for (x = 0 ; x < activeCon->linewidth ; x++) {
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}
			if ( ColorIndexForNumber( text[x]>>8 ) != currentColor ) {
				currentColor = ColorIndexForNumber( text[x]>>8 );
				re.SetColor( g_color_table[currentColor] );
			}
			SCR_DrawSmallChar( cl_conXOffset->integer + activeCon->xadjust + (x+1)*SMALLCHAR_WIDTH, v, text[x] & 0xff );
		}

		v += SMALLCHAR_HEIGHT;
	}

	re.SetColor( NULL );

	if (Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
		return;
	}

	// draw the chat line
	if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
	{
		if (chat_team)
		{
			SCR_DrawBigString (8, v, "say_team:", 1.0f, qfalse );
			skip = 10;
		}
		else
		{
			SCR_DrawBigString (8, v, "say:", 1.0f, qfalse );
			skip = 5;
		}
		Field_BigDraw( &chatField, 8 + skip * BIGCHAR_WIDTH, v,
			SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, qtrue, qtrue );
	}
	else if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE && cmdmode )
	{
		SCR_DrawBigString (8, v, "]", 1.0f, qfalse );
		Field_BigDraw( &g_consoleField, 8 + BIGCHAR_WIDTH, v,
			SCREEN_WIDTH - BIGCHAR_WIDTH, qtrue, qtrue );
	}

}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( float frac ) {
	int				i, x, y;
	int				rows;
	short			*text;
	int				row;
	int				lines;
//	qhandle_t		conShader;
	int				currentColor;

	lines = cls.glconfig.vidHeight * frac;
	if (lines <= 0)
		return;

	if (lines > cls.glconfig.vidHeight )
		lines = cls.glconfig.vidHeight;

	// on wide screens, we will center the text
	activeCon->xadjust = 0;
	SCR_AdjustFrom640( &activeCon->xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT;
	if ( y < 1 ) {
		y = 0;
	}
	else {
		SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.consoleShader );
	}

	// draw the text
	activeCon->vislines = lines;
	rows = (lines-SMALLCHAR_HEIGHT)/SMALLCHAR_HEIGHT;		// rows of text to draw

	y = lines - (SMALLCHAR_HEIGHT*3);

	// draw from the bottom up
	if (activeCon->display != activeCon->current)
	{
	// draw arrows to show the buffer is backscrolled
		re.SetColor( g_color_table[ColorIndex(COLOR_RED)] );
		for (x=0 ; x<activeCon->linewidth ; x+=4)
			SCR_DrawSmallChar( activeCon->xadjust + (x+1)*SMALLCHAR_WIDTH, y, '^' );
		y -= SMALLCHAR_HEIGHT;
		rows--;
	}

	row = activeCon->display;

	if ( activeCon->x == 0 ) {
		row--;
	}

	currentColor = 7;
	re.SetColor( g_color_table[currentColor] );

	for (i=0 ; i<rows ; i++, y -= SMALLCHAR_HEIGHT, row--)
	{
		if (row < 0)
			break;
		if (activeCon->current - row >= activeCon->totallines) {
			// past scrollback wrap point
			continue;
		}

		text = activeCon->text + (row % activeCon->totallines)*activeCon->linewidth;

		for (x=0 ; x<activeCon->linewidth ; x++) {
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}

			if ( ColorIndexForNumber( text[x]>>8 ) != currentColor ) {
				currentColor = ColorIndexForNumber( text[x]>>8 );
				re.SetColor( g_color_table[currentColor] );
			}
			SCR_DrawSmallChar(  activeCon->xadjust + (x+1)*SMALLCHAR_WIDTH, y, text[x] & 0xff );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();

	re.SetColor( NULL );

	// draw the version number
	re.SetColor( g_color_table[1] );
	i = strlen( Q3_VERSION );
	for (x=0 ; x<i ; x++) {
		SCR_DrawSmallChar( cls.glconfig.vidWidth - ( i - x + 1 ) * SMALLCHAR_WIDTH,
			lines - SMALLCHAR_HEIGHT, Q3_VERSION[x] );
	}

	int tabWidth;
	int horOffset = SMALLCHAR_WIDTH, vertOffset = lines - SMALLCHAR_HEIGHT;

	// draw the tabs
	for (x=0 ; x<NUM_CON ; x++) {
		const char *name = conNames[x];

		tabWidth = SMALLCHAR_WIDTH * (strlen( name ) + 2);

		if (&con[x] == activeCon) {
			SCR_FillRect(horOffset, vertOffset, tabWidth,
					SMALLCHAR_HEIGHT,
					g_color_table[conColors[x]]);
			SCR_DrawSmallStringExt(horOffset + SMALLCHAR_WIDTH,
					vertOffset, name, g_color_table[0],
					qfalse, qtrue);
		} else {
			SCR_DrawSmallStringExt(horOffset + SMALLCHAR_WIDTH,
					vertOffset, name,
					g_color_table[conColors[x]], qfalse,
					qtrue);
		}
		horOffset += tabWidth;
	}
}



/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {
	// check for console width changes from a vid mode change
	Con_CheckResize( activeCon );

	// if disconnected, render console full screen
	if ( clc.state == CA_DISCONNECTED ) {
		if ( !( Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME)) ) {
			Con_DrawSolidConsole( 1.0 );
			return;
		}
	}

	if ( activeCon->displayFrac ) {
		Con_DrawSolidConsole( activeCon->displayFrac );
	} else {
		// change to all-console otherwise some messages might not get
		// printed
		con[CON_ALL].displayFrac = 0.0;
		activeCon = &con[CON_ALL];

		// draw notify lines
		if ( clc.state == CA_ACTIVE ) {
			Con_DrawNotify ();
		}
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole (void) {
	// decide on the destination height of the console
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE && !cmdmode )
		activeCon->finalFrac = 0.5;		// half screen
	else
		activeCon->finalFrac = 0;				// none visible

	// scroll towards the destination height
	if (activeCon->finalFrac < activeCon->displayFrac)
	{
		activeCon->displayFrac -= con_conspeed->value*cls.realFrametime*0.001;
		if (activeCon->finalFrac > activeCon->displayFrac)
			activeCon->displayFrac = activeCon->finalFrac;

	}
	else if (activeCon->finalFrac > activeCon->displayFrac)
	{
		activeCon->displayFrac += con_conspeed->value*cls.realFrametime*0.001;
		if (activeCon->finalFrac < activeCon->displayFrac)
			activeCon->displayFrac = activeCon->finalFrac;
	}

}


void Con_PageUp( void ) {
	activeCon->display -= 2;
	if ( activeCon->current - activeCon->display >= activeCon->totallines ) {
		activeCon->display = activeCon->current - activeCon->totallines + 1;
	}
}

void Con_PageDown( void ) {
	activeCon->display += 2;
	if (activeCon->display > activeCon->current) {
		activeCon->display = activeCon->current;
	}
}

void Con_Top( void ) {
	activeCon->display = activeCon->totallines;
	if ( activeCon->current - activeCon->display >= activeCon->totallines ) {
		activeCon->display = activeCon->current - activeCon->totallines + 1;
	}
}

void Con_Bottom( void ) {
	activeCon->display = activeCon->current;
}


void Con_Close( void ) {
	if ( !com_cl_running->integer ) {
		return;
	}
	Con_ClearNotify ();
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	cmdmode = qfalse;
	activeCon->finalFrac = 0;				// none visible
	activeCon->displayFrac = 0;
}
