Yu Engine for OpenArena
=======================

Yu Engine is an engine fork from ioquake3 for use with OpenArena.  By default
it will connect to the OpenArenas servers.  Yu Engine strives to be up to date
with ioquake3 and implement features for increased convenience when playing
OpenArena.  It includes many new commands and cvars which are documentated
below.

New features implemented in Yu Engine (compared to ioquake3) include the
following:

- Cvar `cl_scaleSensWithFov` to automatically adjust mouse sensitivity as
  FOV changes to preserve the same mouse feel
- Randomized GUID using cvar `cl_randomguid`
- Override default font with internal one using cvar `r_useInternalFont`
- Command `cmdmode` (like `messagemode`) for quickly entering console
  commands
- Commands `say_target` and `say_attacker` to write to players in-game
- Command `reply` to reply to the most recent private message
- Command `replymode` (like `messagemode`) for quickly entering a reply
  message
- Hor+ FOV
- Additional keyboard editing shortcuts for editing text fields
- Improved line-editing in TTY console
- Tabbed console with filtered messages
- Notify line fade away effect
- Console styles

Compilation instructions
------------------------

### Building on \*nix
  1. Change to the directory containing this readme.
  2. Run `make`.

### Building on Windows,
  1. Please refer to [the build instructions on ioquake3 website][ioq3-build].

[ioq3-build]: http://wiki.ioquake3.org/Building_ioquake3

### Building on Mac OS X
  1. Install MacOSX SDK packages from XCode.  For maximum compatibility,
     install `MacOSX10.4u.sdk` and `MacOSX10.3.9.sdk`, and `MacOSX10.2.8.sdk`.
  2. Change to the directory containing this README file.
  3. Run `./make-macosx-ub.sh`
  4. Copy the resulting `yuoa.app` in `/build/release-darwin-ub` to your
     `/Applications/openarena` folder.

### Installation, for \*nix
  1. Set the `COPYDIR` variable in the shell to be where you have the OpenArena
     gamecode files.  By default it will be `/usr/local/games/openarena` if you
     haven't set it.  Note that the OpenArena gamecode shipped with Debian
     GNU/Linux packages does not work with Yu Engine.  Instead you can download
     the gamecode for OpenArena 0.8.8 [on the official website][oa-download].
     This will get you a zip file with multiple things, just copy the `baseoa`
     directory to the directory you want use as `COPYDIR`.
  2. Run `make copyfiles`.

It is also possible to cross compile for Windows under \*nix using MinGW. Your
distribution may have mingw32 packages available. On debian/Ubuntu, you need to
install `mingw-w64`. Thereafter cross compiling is simply a case running
`PLATFORM=mingw32 ARCH=x86 make` in place of `make`. ARCH may also be set to
x86\_64.

[baseoa-download]: http://openarena.ws/download.php

Build options
-------------

The following variables may be set, either on the command line or in
Makefile.local. The defaults of these differ depending on the target platform.
The one difference from ioquake3 is that `BUILD_RENDERER_OPENGL2` and
`USE_RENDERER_DLOPEN` is `0` (disabled) by default.  `BUILD_INTERNAL_FONT` is a
new option with default value `1` (enabled).

```
CFLAGS                 - use this for custom CFLAGS
V                      - set to show cc command line when building
DEFAULT_BASEDIR        - extra path to search for baseq3 and such
BUILD_SERVER           - build the 'yuoaded' server binary
BUILD_CLIENT           - build the 'yuoa' client binary
BUILD_RENDERER_OPENGL2 - build opengl2 renderer
BUILD_INTERNAL_FONT    - include the internal font in the renderer
SERVERBIN              - rename 'yuoaded' server binary
CLIENTBIN              - rename 'yuoa' client binary
USE_RENDERER_DLOPEN    - build and use the renderer in a library
USE_YACC               - use yacc to update code/tools/lcc/lburg/gram.c
USE_OPENAL             - use OpenAL where available
USE_OPENAL_DLOPEN      - link with OpenAL at runtime
USE_CURL               - use libcurl for http/ftp download support
USE_CURL_DLOPEN        - link with libcurl at runtime
USE_CODEC_VORBIS       - enable Ogg Vorbis support
USE_CODEC_OPUS         - enable Ogg Opus support
USE_MUMBLE             - enable Mumble support
USE_VOIP               - enable built-in VoIP support
USE_FREETYPE           - enable FreeType support for rendering fonts
USE_INTERNAL_LIBS      - build internal libraries instead of dynamically
                         linking against system libraries; this just sets
                         the default for USE_INTERNAL_ZLIB etc.
                         and USE_LOCAL_HEADERS
USE_INTERNAL_ZLIB      - build and link against internal zlib
USE_INTERNAL_JPEG      - build and link against internal JPEG library
USE_INTERNAL_OGG       - build and link against internal ogg library
USE_INTERNAL_OPUS      - build and link against internal opus/opusfile libraries
USE_LOCAL_HEADERS      - use headers local to ioq3 instead of system ones
DEBUG_CFLAGS           - C compiler flags to use for building debug version
COPYDIR                - the target installation directory
TEMPDIR                - specify user defined directory for temp files
```

New cvars
---------

### Cvar `cl_randomguid`

In OpenArena each player have a special value called "guid" used for
identification.   It is a 128-bit number printed in hexadecimal and can be is
available through the readonly cvar `cl_guid`.  The number is usually generated
by creating an md5 hash from the special file called `qkey` in your
`fs_homepath` folder.  This is used for server admins to identify a specific
player, which is useful when a specific player should have access to special
commands on the server.

Some people want to play completely anonymously, and then GUID will reveal
their identity.  By setting the cvar `cl_randomguid` to a non-zero value, a new
GUID will be generated before each connection to a server.  Note this is not
enough to be completely anonymous, playing behind a VPN or proxy server is also
required.

### Cvar `cl_scaleSensWithFov`

This cvar was added because the gamecode used in Quake III and OpenArena
calculates the zoom sensitivity with an incorrect formula.  This can cause aim
to be a bit inconsistent if you aren't used to it already.

When this cvar is set to non-zero.  The engine will automatically adjust mouse
sensitivity according to the current FOV to preserve the same feel.  This not
only affects the sensitivity when zooming but also when changing `cg_fov`.  The
exact value of the `sensitivity` cvar is used at 90 degree FOV.  Then it
increases or decreases as the FOV changes.

Default value for `cl_scaleSensWithFov` is `0`.

### Cvar `r_useInternalFont`

The default OA font used for the console and some in-game text doesn't look
very good.

It is possible to include an custom made internal font, which looks better (in
my opinion).  This is controlled by the Makefile variable `BUILD_INTERNAL_FONT`
(it is set to `1` by default) when building the renderer libs.  When the
renderer has been built using this option, there is a cvar called
`r_useInternalFont` which when set to a non-zero value will override the
default font with the internal one.   The default value for `r_useInternalFont`
is `0`.

When building the internal font, each renderer gets a copy of the font.  This
adds ~100KB of memory to each of the renderer libs.  This isn't very much
considering that the opengl1 renderer is over 3.5 MB without the included font.
But if you don't want to use this feature, you can disable the build option and
save some memory.

New commands
------------

### Command `cmdmode`

Works like `messagemode`.  When this command is entered a `]`-prompt shows up
in the top left.  Then the user can enter a command that will be executed when
`Enter` is pressed.  When in this mode the user has access to the editing
commands that are available in the normal console.

### Commands `say_target` and `say_attacker`

`say_target` works like the `say` command but will only write to the target
player in front of the cross-hair.  `say_attacker` also works in the same way
but will write only to the player who previously attacked your player.  These
commands are the console-only equivalent to `messagemode3` and `messagemode4`.

### Command `reply`

Used to quickly reply to a received private message.  Works like `tell` but the
target client is the client who send the last private message to your client.
Reply will not reply to players who don't have unique names (to avoid problems
caused from impersonation).

### Command `replymode`

Works like `messagemode`.  Used to quickly reply to a message without using the
console.  When this command is entered a `tell`-prompt shows up in the top
left, with the target being the last client who sent private message to your
client.  The text entered will be sent privately to the target client, as if
the `tell` command was used.

Hor+ FOV
--------

The value of `cg_fov` specify the FOV used when playing with an aspect ratio of
4:3.  When playing on another aspect ratio, the game will preserve the same
vertical FOV as it would have on 4:3, but expand the horizontal FOV to fit the
screen.  Previously Vert- FOV was used.  See [this wikipedia article][fov] for
more information about Hor+ and Vert- FOV.

[fov]: https://en.wikipedia.org/wiki/Field_of_view_in_video_games

Editing text fields
-------------------

Additional keyboard commands have been added for editing text fields.  These
commands are available when typing commands in the in-game console, when
writing chat messages or using the tty-console.

The keyboard shortcuts are similar to the shortcuts common in UNIX shells or
common in general GUI software.

### Complete list of field editing keyboard shortcuts

General GUI-style keyboard shortcuts:

| Keyboard shortcut | Command                         |
| ----------------- | ------------------------------- |
| Left              | Move cursor left one character  |
| Right             | Move cursor right one character |
| CTRL-Left         | Move cursor left one word       |
| CTRL-Right        | Move cursor right one word      |
| Home              | Move cursor start of line       |
| End               | Move cursor end of line         |
| Delete            | Delete character after cursor   |
| CTRL-Delete       | Delete word after cursor        |
| Backspace         | Delete character before cursor  |
| CTRL-Backspace    | Delete word before cursor       |
| CTRL-V, SHIFT-INS | Paste from clipboard            |
| CTRL-Z            | Undo last change                |

General UNIX commandline-style keyboard shortcuts:

| Keyboard shortcut | Command                                      |
| ----------------- | -------------------------------------------- |
| CTRL-A            | Move cursor to start of line                 |
| CTRL-E            | Move cursor to end of line                   |
| CTRL-U            | Delete to beginning of line                  |
| CTRL-K            | Delete to end of line                        |
| CTRL-D            | Delete character after cursor                |
| CTRL-C            | Clear field, this operation cannot be undone |
| ALT-D             | Delete word after cursor                     |
| Backspace         | Delete character before cursor               |
| ALT-Backspace     | Delete word before cursor                    |
| CTRL-W            | Delete previous "large word"                 |
| CTRL-B            | Move cursor left one character               |
| ALT-B             | Move cursor left one word                    |
| CTRL-F            | Move cursor right one character              |
| ALT-F             | Move cursor right one word                   |
| CTRL-T            | Transpose characters                         |
| ALT-T             | Transpose words                              |
| CTRL-\_           | Undo last change                             |
| CTRL-Y            | Yank from kill ring                          |
| ALT-Y             | Rotate kill ring                             |

### Complete list of console specific shortcuts

Console specific keyboard shortcuts:

| Keyboard shortcut | Command                          |
| ----------------- | -------------------------------- |
| PgUp              | Scroll up one line               |
| PgDn              | Scroll down one line             |
| CTRL-Home         | Scroll to first line             |
| CTRL-End          | Scroll to last line              |
| Up, CTRL-P        | Go back in command in history    |
| Down, CTRL-F      | Go forward in command in history |
| CTRL-L            | Clear console screen             |
| ALT-P, ALT-Left   | Previous console tab             |
| ALT-N, ALT-Right  | Next console tab                 |
| ALT-*Digit*       | Console tab number *Digit*       |

### Control key synonyms

There are some synonym ways to trigger keyboard commands using CTRL (when using
the game client).  These probably also available while running the dedicated
server through a terminal emulator or console.

| Key    | Synonym for |
| ------ | ----------- |
| CTRL-H | Backspace   |
| CTRL-8 | Backspace   |
| CTRL-/ | CTRL-_      |
| CTRL-7 | CTRL-_      |

Improved line editing in tty-console
------------------------------------

When running ioquake3 or OpenArena it is also possible to enter commands
through a tty-console.  This is especially useful for dedicated servers without
an in-game console.  Previously the editing-functionality was limited to
entering characters, backspacing, and recalling previously entered commands in
history.

This has been improved so the user has access to almost all keyboard commands
that are available in the in-game console.  The commands not available are
`CTRL-Del` and `CTRL-Backspace`, and console-specific commands for scrolling.

In the tty-console, the keyboard commands using ALT is accessed by prefixing
the key with `ESC`.  So `ESC-b` is equivalent to `ALT-b` in the in-game
console.  Most terminal emulators can be configured to make `ALT` send
`ESC`-codes in this way.

Tabbed console with filtered messages
-------------------------------------

A very convenient feature originally implemented in L0neStarr's f0cus-engine
(fx3).

When opening the console, there are 5 tabs at the bottom (named `all`, `sys`,
`chat`, `tchat`, and `tell`).  You can change console tabs using the shortcuts
listed above.  Each console has a specific purpose and their differences are
summarized in the following table.

| Number | Name    | Messages displayed | Command-type              |
| ------ | ------- | ------------------ | ------------------------- |
| 1      | `all`   | All messages       | Depends on `con_autochat` |
| 2      | `sys`   | Non-chat messages  | Command                   |
| 3      | `chat`  | All-chat messages  | `say`                     |
| 4      | `tchat` | Team-chat messages | `say_team`                |
| 5      | `tell`  | Private messages   | Reply to message          |

The `all` console works just like the normal quake console.  It will display
all messages and is the default console when the client starts.  If
`con_autochat` is non-zero, lines typed in this console without a preceding
slash will be interpreted as chat commands.

In the `sys` console all lines are interpreted as commands, independent of
`con_autochat`.

In the `chat` and `tchat` consoles, all lines typed will be interpreted as chat
commands.  So the line `Hello, world!` will become the command `say Hello,
world!` in the `chat` console or `say_team Hello, world!` in the `tchat`
console.  This is true even if the line contains a preceding slash.

In the `tell` console, lines will be interpreted as a command to reply to the
last received message.  So the line `Hello, world!` will become the command
`reply Hello, world!`.  This is true even if the line contains a preceding
slash.

The console filtering is especially useful for keeping chat history through map
switches, as all the system messages will not get displayed in the chat
consoles.  Similarly chat messages will not get displayed in the `sys` console.

The `condump`-command takes an optional parameter specifying which console to
dump text from.  The parameter can be the index (starting at 0) of the console,
or the name of the console.  If this optional parameter is omitted, then the
`all` console is assumed.

Notify line fade effect
-----------------------

When a notify line is disappearing, it will fade away the notify lines below it
will move up to take its place.  This is only a visual effect, but it is quite
nice.  The time it takes for a line to fade away is determined by the cvar
`con_notifyFadeTime`.

Customizable console height
---------------------------

The console height can be changed using `cl_consoleHeight` which is a value
between `0.0` and `1.0` where `1.0` means that the console takes up the entire
screen.  The default value is `0.5`.

Pressing console key while holding SHIFT or ALT will open the console with a
height of `1.0`.

Console styles
--------------

It is possible to change the looks of the game console using the cvar
`cl_consoleType`.  When the cvar is set to `0`, the console will look like it
normally does, with a background image and a red border.  When the cvar is set
to `1`, the console will look like `0`, but the transparency of the border is
changed using `cl_consoleColorAlpha` (this mode is here for compatibility with
the official OpenArena engine).  When the cvar is set to `2` the background of
the console is the color specified by `cl_consoleColorRed`,
`cl_consoleColorGreen` and `cl_consoleColorBlue`, and the transparency is
specified by `cl_consoleColorAlpha`.  When the cvar is set to `3`, the console
will look like in `2`, but instead of dropping down/up it will fade in/out.
When the cvar is set to `4`, the console will look like in `2`, but the console
will both fade in/out and drop down/up.  The modes `3` and `4` are new in Yu
Engine, the other modes exist also in the official OpenArena engine.

Other documentation
-------------------

This README only documents the features added by Yu Engine.  Not the features
added by ioquake3.  See [the ioquake3 README](./ioq3-readme.md) for
documentation about features added ioquake3.  Note that all things related to
gamecode, UI and autoupdater have been removed from the source tree, so
documentation about those should be ignored.

Contributing
------------

Suggestions and bug reports are always welcome, please create issue at [the
Github repo][yuengine-repo].  Pull-requests are also welcome, but might not be
accepted.  Security and network related things should generally be fixed in
[the ioquake3 Github repo][ioq3-repo] instead,  because it has maintainers more
experienced with dealing with that kind of stuff.  All fixes made there will
eventually be backported to Yu Engine (atleast that is the plan).

[yuengine-repo]: https://github.com/oaguild/yuengine
[ioq3-repo]: https://github.com/ioquake/ioq3
