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
// field.c -- functions for text fields

#include "q_shared.h"
#include "qcommon.h"

// get the last index of undobuf_t
#define UNDOBUF_END(u) (((u).start + (u).size) % MAX_UNDO_LEVELS)

// identify different commands and how they should how up in undo history
typedef enum {
	// commands not to put into undo history
	UNDO_MODE_SKIP = 0x1000,
	UNDO_NONE,
	UNDO_MOVE_BACK_CHAR,
	UNDO_MOVE_FORWARD_CHAR,
	UNDO_MOVE_BACK_WORD,
	UNDO_MOVE_FORWARD_WORD,
	UNDO_MOVE_LINE_START,
	UNDO_MOVE_LINE_END,

	// commands to put in undo history with repetition
	UNDO_MODE_REPEAT = 0x2000,
	UNDO_RUBOUT_LINE,
	UNDO_DELETE_LINE,
	UNDO_DELETE_WORD,
	UNDO_RUBOUT_WORD,
	UNDO_RUBOUT_LONG_WORD,
	UNDO_TOUPPER_WORD,
	UNDO_TOLOWER_WORD,
	UNDO_CAPITALIZE_WORD,
	UNDO_PASTE,
	UNDO_TRANSPOSE_CHARS,

	// commands that if repeated will be concatenated to one entry in undo
	// history
	UNDO_MODE_CONCAT = 0x3000,
	UNDO_INSERT_CHAR,	// insert alpha character
	UNDO_REPLACE_CHAR,	// replace with alpha character
	UNDO_DELETE_CHAR,	// delete alpha character
	UNDO_RUBOUT_CHAR,	// backspace over alpha character
	UNDO_AUTOCOMPLETE,	// tab

	// bitmask to check what history mode a command has
	UNDO_MODEBITS = 0xF000
} undocmd_t;

/*
=================
Field_MoveTo

Moves the cursor to position
=================
*/
static void Field_MoveTo( field_t *edit, int to )
{
	int len = strlen( edit->buffer );

	if ( to < 0 )
		to = 0;
	else if ( to > len )
		to = len;

	edit->cursor = to;

	// adjust scroll
	if ( edit->cursor < edit->scroll ) {
		edit->scroll = edit->cursor;
	} else if ( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len ) {
		edit->scroll = edit->cursor - edit->widthInChars + 1;
	}
}

/*
==================
Field_DeleteTo

Deletes text from the cursor to position
==================
*/
static void Field_DeleteTo( field_t *edit, int to )
{
	int cur = edit->cursor;
	size_t len = strlen( edit->buffer );

	if ( to < 0 )
		to = 0;
	else if ( to > len )
		to = len;

	if ( to < cur ) {
		// delete backwards and adjust cursor and scroll
		memmove( edit->buffer + to, edit->buffer + cur, len - cur + 1);
		Field_MoveTo( edit, to );
	} else {
		// delete forward
		memmove( edit->buffer + cur, edit->buffer + to, len - to + 1);
	}
}

/*
================
Field_PushUndo

Save the field state before command so it can be undone later. How the field
state is saved depends on the command entered.
================
*/
static void Field_PushUndo( field_t *edit, undo_cmd_t cmd )
{
	int end;

	// if the field doesn't have undo buffer
	if ( !edit->undobuf )
		return;

	if ( (cmd & UNDO_MODEBITS) == UNDO_MODE_SKIP ) {
		goto ret; // command shouldn't be put into history
	}

	if ( (cmd & UNDO_MODEBITS) == UNDO_MODE_CONCAT ) {
		// command should be put into history but without duplication
		if ( cmd == edit->undobuf->lastcmd )
			goto ret; // duplication
	}

	end = UNDOBUF_END( *edit->undobuf );

	edit->undobuf->cursors[end] = edit->cursor;
	strcpy( edit->undobuf->buffers[end], edit->buffer );

	if ( edit->undobuf->size < MAX_UNDO_LEVELS )
		edit->undobuf->size++;
	else
		edit->undobuf->start = (edit->undobuf->start + 1) % MAX_UNDO_LEVELS;

ret:
	edit->undobuf->lastcmd = cmd;
}

/*
================
Field_MakeUpperTo

Makes text uppercase from the cursor to position
================
*/
static void Field_MakeUpperTo( field_t *edit, int to )
{
	int cur = edit->cursor;
	int len = strlen( edit->buffer );
	int i;

	if ( to < 0 )
		to = 0;
	else if ( to > len )
		to = len;

	for ( i = cur; i != to; to < cur ? i-- : i++ ) {
		edit->buffer[i] = toupper( edit->buffer[i] );
	}
	Field_MoveTo( edit, to );
}

/*
================
Field_MakeLowerTo

Makes text lowercase from the cursor to position
================
*/
static void Field_MakeLowerTo( field_t *edit, size_t to )
{
	int cur = edit->cursor;
	int len = strlen(edit->buffer);
	int i;

	if ( to < 0 )
		to = 0;
	else if ( to > len )
		to = len;

	for ( i = cur; i != to; to < cur ? i-- : i++ ) {
		edit->buffer[i] = tolower( edit->buffer[i] );
	}
	Field_MoveTo( edit, to );
}


/*
===========================================
command line completion
===========================================
*/

/*
==================
Field_Clear
==================
*/
void Field_Clear( field_t *edit ) {
	memset(edit->buffer, 0, MAX_EDIT_LINE);
	edit->cursor = 0;
	edit->scroll = 0;
	if (edit->undobuf)
		edit->undobuf->size = 0;
}

static const char *completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int	matchCount;
// field we are working on, passed to Field_AutoComplete(&g_consoleCommand for instance)
static field_t *completionField;

/*
===============
FindMatches

===============
*/
static void FindMatches( const char *s ) {
	int		i;

	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}
	matchCount++;
	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, s, sizeof( shortestMatch ) );
		return;
	}

	// cut shortestMatch to the amount common with s
	for ( i = 0 ; shortestMatch[i] ; i++ ) {
		if ( i >= strlen( s ) ) {
			shortestMatch[i] = 0;
			break;
		}

		if ( tolower(shortestMatch[i]) != tolower(s[i]) ) {
			shortestMatch[i] = 0;
		}
	}
}

/*
===============
PrintMatches

===============
*/
static void PrintMatches( const char *s ) {
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_Printf( "    %s\n", s );
	}
}

/*
===============
PrintCvarMatches

===============
*/
static void PrintCvarMatches( const char *s ) {
	char value[ TRUNCATE_LENGTH ];

	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_TruncateLongString( value, Cvar_VariableString( s ) );
		Com_Printf( "    %s = \"%s\"\n", s, value );
	}
}

/*
===============
Field_FindFirstSeparator
===============
*/
static char *Field_FindFirstSeparator( char *s )
{
	int i;

	for( i = 0; i < strlen( s ); i++ )
	{
		if( s[ i ] == ';' )
			return &s[ i ];
	}

	return NULL;
}

/*
===============
Field_Complete
===============
*/
static qboolean Field_Complete( void )
{
	int completionOffset;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 )
	{
		Q_strcat( completionField->buffer, sizeof( completionField->buffer ), " " );
		completionField->cursor++;
		return qtrue;
	}

	Com_Printf( "]%s\n", completionField->buffer );

	return qfalse;
}

#ifndef DEDICATED
/*
===============
Field_CompleteKeyname
===============
*/
void Field_CompleteKeyname( void )
{
	matchCount = 0;
	shortestMatch[ 0 ] = 0;

	Key_KeynameCompletion( FindMatches );

	if( !Field_Complete( ) )
		Key_KeynameCompletion( PrintMatches );
}
#endif

/*
===============
Field_CompleteFilename
===============
*/
void Field_CompleteFilename( const char *dir,
		const char *ext, qboolean stripExt, qboolean allowNonPureFilesOnDisk )
{
	matchCount = 0;
	shortestMatch[ 0 ] = 0;

	FS_FilenameCompletion( dir, ext, stripExt, FindMatches, allowNonPureFilesOnDisk );

	if( !Field_Complete( ) )
		FS_FilenameCompletion( dir, ext, stripExt, PrintMatches, allowNonPureFilesOnDisk );
}

/*
===============
Field_CompleteCommand
===============
*/
void Field_CompleteCommand( char *cmd,
		qboolean doCommands, qboolean doCvars )
{
	int		completionArgument = 0;

	// Skip leading whitespace and quotes
	cmd = Com_SkipCharset( cmd, " \"" );

	Cmd_TokenizeStringIgnoreQuotes( cmd );
	completionArgument = Cmd_Argc( );

	// If there is trailing whitespace on the cmd
	if( *( cmd + strlen( cmd ) - 1 ) == ' ' )
	{
		completionString = "";
		completionArgument++;
	}
	else
		completionString = Cmd_Argv( completionArgument - 1 );

#ifndef DEDICATED
	// add a '\' to the start of the buffer if it might be sent as chat otherwise
	if( con_autochat->integer && completionField->buffer[ 0 ] &&
			completionField->buffer[ 0 ] != '\\' )
	{
		if( completionField->buffer[ 0 ] != '/' )
		{
			// Buffer is full, refuse to complete
			if( strlen( completionField->buffer ) + 1 >=
				sizeof( completionField->buffer ) )
				return;

			memmove( &completionField->buffer[ 1 ],
				&completionField->buffer[ 0 ],
				strlen( completionField->buffer ) + 1 );
			completionField->cursor++;
		}

		completionField->buffer[ 0 ] = '\\';
	}
#endif

	if( completionArgument > 1 )
	{
		const char *baseCmd = Cmd_Argv( 0 );
		char *p;

#ifndef DEDICATED
		// This should always be true
		if( baseCmd[ 0 ] == '\\' || baseCmd[ 0 ] == '/' )
			baseCmd++;
#endif

		if( ( p = Field_FindFirstSeparator( cmd ) ) )
			Field_CompleteCommand( p + 1, qtrue, qtrue ); // Compound command
		else
			Cmd_CompleteArgument( baseCmd, cmd, completionArgument );
	}
	else
	{
		if( completionString[0] == '\\' || completionString[0] == '/' )
			completionString++;

		matchCount = 0;
		shortestMatch[ 0 ] = 0;

		if( strlen( completionString ) == 0 )
			return;

		if( doCommands )
			Cmd_CommandCompletion( FindMatches );

		if( doCvars )
			Cvar_CommandCompletion( FindMatches );

		if( !Field_Complete( ) )
		{
			// run through again, printing matches
			if( doCommands )
				Cmd_CommandCompletion( PrintMatches );

			if( doCvars )
				Cvar_CommandCompletion( PrintCvarMatches );
		}
	}
}

/*
===============
Field_AutoComplete

Perform Tab expansion
===============
*/
void Field_AutoComplete( field_t *field )
{
	Field_PushUndo( field, UNDO_AUTOCOMPLETE );
	completionField = field;
	Field_CompleteCommand( completionField->buffer, qtrue, qtrue );
}

/*
===============
Field_CompletePlayerName
===============
*/
static qboolean Field_CompletePlayerNameFinal( qboolean whitespace )
{
	int completionOffset;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 && whitespace )
	{
		Q_strcat( completionField->buffer, sizeof( completionField->buffer ), " " );
		completionField->cursor++;
		return qtrue;
	}

	return qfalse;
}

static void Name_PlayerNameCompletion( const char **names, int nameCount, void(*callback)(const char *s) )
{
	int i;

	for( i = 0; i < nameCount; i++ ) {
		callback( names[ i ] );
	}
}

void Field_CompletePlayerName( const char **names, int nameCount )
{
	qboolean whitespace;

	matchCount = 0;
	shortestMatch[ 0 ] = 0;

	if( nameCount <= 0 )
		return;

	Name_PlayerNameCompletion( names, nameCount, FindMatches );

	if( completionString[0] == '\0' )
	{
		Com_PlayerNameToFieldString( shortestMatch, sizeof( shortestMatch ), names[ 0 ] );
	}

	//allow to tab player names
	//if full player name switch to next player name
	if( completionString[0] != '\0'
		&& Q_stricmp( shortestMatch, completionString ) == 0
		&& nameCount > 1 )
	{
		int i;

		for( i = 0; i < nameCount; i++ ) {
			if( Q_stricmp( names[ i ], completionString ) == 0 )
			{
				i++;
				if( i >= nameCount )
				{
					i = 0;
				}

				Com_PlayerNameToFieldString( shortestMatch, sizeof( shortestMatch ), names[ i ] );
				break;
			}
		}
	}

	if( matchCount > 1 )
	{
		Com_Printf( "]%s\n", completionField->buffer );

		Name_PlayerNameCompletion( names, nameCount, PrintMatches );
	}

	whitespace = nameCount == 1? qtrue: qfalse;
	if( !Field_CompletePlayerNameFinal( whitespace ) )
	{

	}
}

/*
===================
Returns the position back to start of word

Words are delimited by space, numbers, and punctuation
===================
*/
static int Field_BackWord( field_t *edit )
{
	int i = edit->cursor;

	while ( i > 0 && !isalpha( edit->buffer[i - 1] ) )
		i--;

	while ( i > 0 && isalpha( edit->buffer[i - 1] ) )
		i--;

	return i;
}

/*
===================
Field_ForwardWord

Returns the position forward to end of word

Words are delimited by space, numbers, and punctuation
===================
*/
static int Field_ForwardWord( field_t *edit )
{
	int i = edit->cursor;

	while ( edit->buffer[i] && !isalpha( edit->buffer[i] ) )
		i++;

	while ( isalpha( edit->buffer[i] ) )
		i++;

	return i;
}

/*
==================
Field_BackLongWord

Returns the position back to start of "long word"

Long words are only delimited by space
==================
*/
static int Field_BackLongWord( field_t *edit )
{
	int i = edit->cursor;

	while ( i > 0 && isspace( edit->buffer[i - 1] ) )
		i--;

	while ( i > 0 && !isspace( edit->buffer[i - 1] ) )
		i--;

	return i;
}

/*
===========================================
Movement functions
===========================================
*/

void Field_MoveForwardChar( field_t *edit )
{
	Field_PushUndo( edit, UNDO_MOVE_FORWARD_CHAR );
	Field_MoveTo( edit, edit->cursor + 1 );
}

void Field_MoveBackChar( field_t *edit )
{
	Field_PushUndo( edit, UNDO_MOVE_BACK_CHAR );
	Field_MoveTo( edit, edit->cursor - 1 );
}

void Field_MoveForwardWord( field_t *edit )
{
	Field_PushUndo( edit, UNDO_MOVE_FORWARD_WORD );
	Field_MoveTo( edit, Field_ForwardWord( edit ) );
}

void Field_MoveBackWord( field_t *edit )
{
	Field_PushUndo( edit, UNDO_MOVE_BACK_WORD );
	Field_MoveTo( edit, Field_BackWord( edit ) );
}

void Field_MoveLineStart( field_t *edit )
{
	Field_PushUndo( edit, UNDO_MOVE_LINE_START );
	Field_MoveTo( edit, 0 );
}

void Field_MoveLineEnd( field_t *edit )
{
	Field_PushUndo( edit, UNDO_MOVE_LINE_END );
	Field_MoveTo( edit, strlen( edit->buffer ) );
}

/*
===========================================
Editing functions
===========================================
*/

/*
================
Field_Undo

Restore the fields state to the last saved state
================
*/
void Field_Undo( field_t *edit )
{
	int end;

	// if the field doesn't have undo buffer
	if ( !edit->undobuf )
		return;

	if ( edit->undobuf->size == 0 )
		return; // nothing to restore

	edit->undobuf->size--;
	end = UNDOBUF_END( *edit->undobuf );

	strcpy( edit->buffer, edit->undobuf->buffers[end] );
	Field_MoveTo( edit, edit->undobuf->cursors[end] );

	edit->undobuf->lastcmd = UNDO_NONE;
}

void Field_Yank( field_t *edit )
{
}

void Field_RuboutChar( field_t *edit )
{
		Field_PushUndo( edit, UNDO_RUBOUT_CHAR );
		Field_DeleteTo( edit, edit->cursor - 1 );
}

void Field_RuboutWord( field_t *edit )
{
		Field_PushUndo( edit, UNDO_RUBOUT_WORD );
		Field_DeleteTo( edit, Field_BackWord( edit ) );
}

void Field_RuboutLongWord( field_t *edit )
{
		Field_PushUndo( edit, UNDO_RUBOUT_LONG_WORD );
		Field_DeleteTo( edit, Field_BackLongWord( edit ) );
}

void Field_RuboutLine( field_t *edit )
{
		Field_PushUndo( edit, UNDO_RUBOUT_LINE );
		Field_DeleteTo( edit, 0 );
}

void Field_DeleteChar( field_t *edit )
{
		Field_PushUndo( edit, UNDO_DELETE_CHAR );
		Field_DeleteTo( edit, edit->cursor + 1 );
}

void Field_DeleteWord( field_t *edit )
{
		Field_PushUndo( edit, UNDO_DELETE_WORD );
		Field_DeleteTo( edit, Field_ForwardWord( edit ) );
}

void Field_DeleteLine( field_t *edit )
{
		size_t len = strlen( edit->buffer );

		if ( edit->cursor == len )
				return;

		Field_PushUndo( edit, UNDO_DELETE_LINE );
		Field_DeleteTo( edit, len );
}


void Field_TransposeChars( field_t *edit )
{
		size_t len = strlen( edit->buffer );
		if ( edit->cursor == 0 || len < 2 ) {
			return;
		}
		if ( edit->cursor == len ) {
			edit->cursor--;
		}

		Field_PushUndo( edit, UNDO_TRANSPOSE_CHARS );
		char tmp = edit->buffer[edit->cursor];
		edit->buffer[edit->cursor] = edit->buffer[edit->cursor - 1];
		edit->buffer[edit->cursor - 1] = tmp;
		edit->cursor++;
}


void Field_MakeWordUpper( field_t *edit )
{
		Field_PushUndo( edit, UNDO_TOUPPER_WORD );
		Field_MakeUpperTo( edit, Field_ForwardWord( edit ) );
}

void Field_MakeWordLower( field_t *edit )
{
		Field_PushUndo( edit, UNDO_TOLOWER_WORD );
		Field_MakeLowerTo( edit, Field_ForwardWord( edit ) );
}

void Field_MakeWordCapitalized( field_t *edit )
{
		Field_PushUndo( edit, UNDO_CAPITALIZE_WORD );
		Field_MakeUpperTo( edit, Field_BackWord( edit ) );
		edit->cursor++;
		Field_MakeLowerTo( edit, Field_ForwardWord( edit ) );
}

void Field_InsertChar( field_t *edit, char ch )
{

	Field_PushUndo( edit, UNDO_INSERT_CHAR );

	size_t len = strlen( edit->buffer );

	// - 2 to leave room for the leading slash and trailing \0
	if ( len == MAX_EDIT_LINE - 2 ) {
		return; // all full
	}
	memmove( edit->buffer + edit->cursor + 1,
		edit->buffer + edit->cursor, len + 1 - edit->cursor );
	edit->buffer[edit->cursor] = ch;

	Field_MoveTo( edit, edit->cursor + 1 );
}

void Field_ReplaceChar( field_t *edit, char ch )
{
	Field_PushUndo( edit, UNDO_REPLACE_CHAR );

	// - 2 to leave room for the leading slash and trailing \0
	if ( edit->cursor == MAX_EDIT_LINE - 2 )
		return;
	edit->buffer[edit->cursor] = ch;
	Field_MoveTo( edit, edit->cursor + 1 );
}
