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

// names for the consoles
const char *conNames[NUM_CON] = {
	"all",
	"sys",
	"chat",
	"tchat",
	"tell"
};

// color indexes in g_color_table for consoles
const int conColors[NUM_CON] = {
	1,
	3,
	2,
	5,
	6
};

// clientNum of the last client who send private message, or -1 if not
// available
int		tellClientNum = -1;

console_t	con[NUM_CON];
console_t	*activeCon = con;

cvar_t		*con_conspeed;
cvar_t		*con_autoclear;
cvar_t		*con_notifytime;

#define	DEFAULT_CONSOLE_WIDTH	78

/*
================
Con_HistPrev

Recall older line in history to field
================
*/
void Con_HistPrev( field_t *edit )
{
	if ( nextHistoryLine - historyLine < COMMAND_HISTORY && historyLine > 0 ) {
		historyLine--;
	}
	*edit = historyEditLines[ historyLine % COMMAND_HISTORY ];
}

/*
================
Con_HistNext

Recall newer line in history to field
================
*/
void Con_HistNext( field_t *edit )
{
	int width = edit->widthInChars;
	historyLine++;
	if (historyLine >= nextHistoryLine) {
		historyLine = nextHistoryLine;
		Field_Clear( edit );
		edit->widthInChars = width;
		return;
	}
	*edit = historyEditLines[ historyLine % COMMAND_HISTORY ];
}

/*
================
Con_HistAdd

Add line to history
================
*/
void Con_HistAdd( field_t *edit )
{
	// copy line to history buffer
	historyEditLines[nextHistoryLine % COMMAND_HISTORY] = *edit;
	nextHistoryLine++;
	historyLine = nextHistoryLine;
}

/*
================
Con_HistAbort

Abort scrolling in the history
================
*/
void Con_HistAbort( void )
{
	historyLine = nextHistoryLine;
}

/*
================
Con_PrependSlashIfNeeded

Prepend slash to command in field if needed
================
*/
void Con_PrependSlashIfNeeded( field_t *edit, int conType ) {
	// if in sys-console or not in the game explicitly prepend a slash if
	// needed
	if ( (clc.state != CA_ACTIVE || conType == CON_SYS) &&
			con_autochat->integer &&
			edit->buffer[0] &&
			edit->buffer[0] != '\\' &&
			edit->buffer[0] != '/' ) {
		char	temp[MAX_EDIT_LINE-1];

		Q_strncpyz( temp, edit->buffer, sizeof( temp ) );
		Com_sprintf( edit->buffer, sizeof( edit->buffer ), "\\%s", temp );
		edit->cursor++;
	}
}

/*
================
Con_AcceptLine

When the user enters a command in the console
================
*/
void Con_AcceptLine( void )
{
	int conNum = activeCon - con;
	qboolean isChat = CON_ISCHAT(conNum);

	Con_PrependSlashIfNeeded( &g_consoleField, conNum );

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
				Cbuf_AddText( "cmd say \"" );
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText( "\"" );
			} else if ( conNum == CON_TCHAT ) {
				Cbuf_AddText( "cmd say_team \"" );
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText( "\"" );
			} else if ( conNum == CON_TELL ) {
				if ( tellClientNum >= 0 ) {
					char cmd[MAX_EDIT_LINE + 16 ];
					Com_sprintf( cmd, sizeof cmd, "cmd tell %d \"%s\"\n",
							tellClientNum, g_consoleField.buffer);
					Cbuf_AddText( cmd );
				}
			} else {
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText( "\n" );
			}
		}
	}

	Con_HistAdd( &g_consoleField );
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

	if ( con_autoclear->integer ) {
		Field_Clear( &g_consoleField );
		Con_HistAbort();
	}

	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify ();

	// change to all-console
	activeCon = &con[CON_ALL];

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
	chat_type = CHAT_CHAT;
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
	chat_type = CHAT_TCHAT;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
IntDisplayWidth

Return how many characters it takes to display an integer
================
*/
int IntDisplayWidth( int i ) {
	int n = 0;

	if (i < 0) {
		i = -i;
		++n;
	}

	while ( i > 0 ) {
		i /= 10;
		++n;
	}

	return n;
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
	chat_type = CHAT_TELL;
	Field_Clear( &chatField );
	chatField.widthInChars = 28 - IntDisplayWidth( chat_playerNum );
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
	chat_type = CHAT_TELL;
	Field_Clear( &chatField );
	chatField.widthInChars = 28 - IntDisplayWidth( chat_playerNum );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_ReplyMode_f
================
*/
void Con_ReplyMode_f (void) {
	if ( tellClientNum < 0 ) {
		chat_playerNum = -1;
		return;
	}
	Field_Clear( &chatField );
	chat_type = CHAT_TELL;
	chat_playerNum = tellClientNum;
	chatField.widthInChars = 28 - IntDisplayWidth( chat_playerNum );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_CmdMode_f
================
*/
void Con_CmdMode_f (void) {
	Field_Clear( &chatField );
	chat_type = CHAT_CMD;
	chatField.widthInChars = 34;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_Reply_f
================
*/
void Con_Reply_f (void) {
	char	buf[MAX_STRING_CHARS];

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: reply <message>\n" );
		return;
	}

	if ( tellClientNum < 0 ) {
		Com_Printf( "Nobody to reply to!\n" );
		return;
	}

	Com_sprintf( buf, sizeof buf, "tell %i \"%s\"\n", tellClientNum, Cmd_Args() );

	CL_AddReliableCommand( buf, qfalse );
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
	if ( n >= 0 && n < NUM_CON ) {
		con[n].displayFrac = activeCon->displayFrac;
		con[n].finalFrac = activeCon->finalFrac;
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
	Cmd_AddCommand ("replymode", Con_ReplyMode_f);
	Cmd_AddCommand ("cmdmode", Con_CmdMode_f);
	Cmd_AddCommand ("reply", Con_Reply_f);
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
	Cmd_RemoveCommand("replymode");
	Cmd_RemoveCommand("cmdmode");
	Cmd_RemoveCommand("reply");
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
CL_InitConsoles

Initialize the consoles
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
CL_PlayerNameToClientNum

Convert player name to client num

Returns client num if there is an unique player with name, -1 if no player has
that name, -2 if more than one player has that name
================
*/
static int CL_PlayerNameToClientNum( const char *name, int n ) {
	const char *info = cl.gameState.stringData +
		cl.gameState.stringOffsets[CS_SERVERINFO];
	int count = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
	char cleanName[MAX_NAME_LENGTH];

	int i;
	int clientNum = -1;
	for( i = 0; i < count; i++ ) {

		info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS+i];
		Q_strncpyz( cleanName, Info_ValueForKey( info, "n" ), sizeof(cleanName) );
		Q_CleanStr( cleanName );

		if ( Q_strncmp( cleanName, name, n ) == 0 ) {
			if (clientNum >= 0)
				return -2; // more than 1 player with the name
			clientNum = i;
		}
	}
	return clientNum;
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
static void CL_ConsolePrintToCon( const char *txt, console_t *con ) {
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
	static int lastCmdNum;
	int cmdNum = clc.lastExecutedServerCommand;
	char *cmdStr = clc.serverCommands[cmdNum % MAX_RELIABLE_COMMANDS];
	int conNum = CON_SYS;

	if ( cmdNum > lastCmdNum ) {
		if ( Q_strncmp( cmdStr, "chat \"\x19[", 8 ) == 0 ) {
			int i;

			// seek to the end of player-name (magic byte '\x19')
			for ( i = 8 ; cmdStr[i] != '\0'; ++i ) {
				if ( cmdStr[i] == '\x19' )
					break;
			}

			// Name starts 8 characters after command string.
			//
			// Before the magic byte comes the string "^7" which we
			// don't want to compare with, so subtract 2 to the
			// length of the string.
			//
			// Then we are left with the player name string which
			// we can use to find the client number of the sender.
			int clientNum = CL_PlayerNameToClientNum( cmdStr+8, i-8-2 );

			// don't override the reply player if this messages comes from this
			// client
			if ( clientNum != clc.clientNum ) {
				tellClientNum = clientNum;
			}

			conNum = CON_TELL;
		} else if ( Q_strncmp( cmdStr, "chat", 4 ) == 0 ) {
			conNum = CON_CHAT;
		} else if ( Q_strncmp( cmdStr, "tchat", 5 ) == 0 ) {
			conNum = CON_TCHAT;
		}
	}
	lastCmdNum = cmdNum;

	CL_ConsolePrintToCon( txt, &con[CON_ALL] );
	CL_ConsolePrintToCon( txt, &con[conNum] );

	// alert user if a message comes from non-unique playername
	if (conNum == CON_TELL && tellClientNum == -2 ) {
		const char *msg = "^3Non-unique sender name: Reply has been disabled\n";
		CL_ConsolePrintToCon( msg, &con[CON_ALL] );
		CL_ConsolePrintToCon( msg, &con[CON_TELL] );
	}
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
		if (chat_type == CHAT_CHAT) {
			SCR_DrawBigString (8, v, "say:", 1.0f, qfalse );
			skip = 5;
		} else if (chat_type == CHAT_TCHAT) {
			SCR_DrawBigString (8, v, "say_team:", 1.0f, qfalse );
			skip = 10;
		} else if (chat_type == CHAT_CMD) {
			SCR_DrawBigString (8, v, "]", 1.0f, qfalse );
			skip = 1;
		} else if (chat_type == CHAT_TELL) {
			static char buf[32];
			Com_sprintf( buf, sizeof buf, "tell %d: %n", chat_playerNum, &skip );
			SCR_DrawBigString (8, v, buf, 1.0f, qfalse );
		} else {
			skip = 0;
		}
		Field_BigDraw( &chatField, 8 + skip * BIGCHAR_WIDTH, v,
			SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, qtrue, qtrue );
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
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
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
	activeCon->finalFrac = 0;				// none visible
	activeCon->displayFrac = 0;
}
