Yu Engine for OpenArena
=======================

Engine forked from ioquake3 for use with OpenArena. By default it will connect
use the OpenArena master server and use OpenArenas protocol number, removing the
need for a wrapper script like as is required with ioquake3.

Build instructions
------------------

See [the README of ioquake3](./ioq3-readme.md) for instructions on how to
compile and install this software. Note that code for basegame, missionpack and
auto-updater has been completely removed from the source tree. So Ignore all
documentation related to building those.

Feature list
------------

New features implemented in Yu Engine are the following:

- Hor+ FOV
- New cvar `cl_scaleSensWithFov` to automatically adjust mouse sensitivity
  automatically as FOV changes to preserve the same mouse feel
- Additional keyboard editing shortcuts for editing text fields
- `cmdmode` (like messagemodes) for quickly entering console commands
- Improved line-editing in TTY console
- Possible to include an internal font in the renderers

Hor+ FOV
--------

The value of `cg_fov` specify the FOV used when playing with an aspect ratio of
4:3. When playing on another aspect ratio, the game will preserve the same
vertical FOV as it would have on 4:3, but expand the horizontal FOV to fit the
screen. Previously Vert- FOV was used. See [this wikipedia article][fov] for
more information about Hor+ and Vert- FOV.

[fov]: https://en.wikipedia.org/wiki/Field_of_view_in_video_games

Cvar `cl_scaleSensWithFov`
--------------------------

This cvar was added because the gamecode used in Quake3 and OpenArena calculates
the zoom sensitivity with an incorrect formula. This can cause aim to be a bit
inconsistent if you aren't used to it already.

When this cvar is set to non-zero. The engine will automatically adjust mouse
sensitivity according to the current FOV to preserve the same feel. This not
only affects the sensitivity when zooming but also when changing `cg_fov`. The
exact value of the `sensitivity` cvar is used at 90 degree FOV. Then it
increases or decreases as the FOV changes.

Default value for `cl_scaleSensWithFov` is `0`.

Command `cmdmode`
-----------------

Works like `messagemode1` or `messagemode2`. When command is entered a
`]`-prompt shows up in the top left. Then the user can type a command that will
be executed when `Enter` is pressed. When in this mode the user has access to
all editing commands that are available in the normal console.

Editing text fields
-------------------

Additional keyboard commands have been added for editing text fields. These
commands are available when typing commands in the in-game console, when writing
chat messages or using the tty-console.

The keyboard shortcuts are similar to the shortcuts common in UNIX shells or
common in general GUI software.

### Complete list of field editing keyboard shortcuts

General GUI-style keyboard shortcuts:

- `Left`, move cursor left one character
- `Right`, move cursor right one character
- `CTRL-Left`, move cursor left one word (new)
- `CTRL-Right`, move cursor right one word (new)
- `Home`, move cursor start of line
- `End`, move cursor end of line
- `Delete`, delete character after cursor
- `CTRL-Delete`, delete word after cursor (new)
- `Backspace/CTRL-H`, delete character before cursor
- `CTRL-Backspace`, delete word before cursor (new)
- `CTRL-V/SHIFT-INS`, paste from clipboard
- `CTRL-Z`, undo last change (new)

UNIX commandline-style keyboard shortcuts (`^` is caret notation for control
character):

- `^A`, move cursor to start of line
- `^E`, move cursor to end of line
- `^U`, delete to beginning of line (new)
- `^K`, delete to end of line (new)
- `^D`, delete character after cursor (new)
- `^C`, clear field, this operation cannot be undone
- `ALT-D`, delete word after cursor (new)
- `Backspace/^H`, delete character before cursor
- `ALT-Backspace`/`ALT-^H`, delete word before cursor (new)
- `^W`, delete previous "large word" (new)
- `^B`, move cursor left one character (new)
- `ALT-B`, move cursor left one word (new)
- `^F`, move cursor right one character (new)
- `ALT-F`, move cursor right one word (new)
- `ALT-F`, move cursor right one word (new)
- `^T`, transpose characters (new)
- `ALT-T`, transpose words (new)
- `^_`, undo last change (new)
- `^Y`, yank from kill ring (new)
- `ALT-Y`, rotate kill ring (new)

### Complete list of console specific shortcuts

Console specific GUI-style keyboard shortcuts:

- `Up`, go to back in command in history
- `Down`, go to forward in command in history
- `CTRL-PgUp`, scroll up one line
- `CTRL-PgDn`, scroll down one line
- `CTRL-Home`, scroll to first line
- `CTRL-End`, scroll to last line

Console specific UNIX commandline-style keyboard shortcuts:

- `^P`, go back in command in history
- `^F`, go forward in command in history
- `^L`, clear console screen

Improved line editing in tty-console
------------------------------------

When running ioquake3 or OpenArena it is also possible to enter commands through
a tty-console. This is especially useful for dedicated servers without an
in-game console. Previously the editing-functionality was limited to entering
characters, and recalling previously entered commands in history.

This has been improved so the user has access to almost all keyboard commands
that are available in the in-game console. The commands not available are
`CTRL-Del` and `CTRL-Backspace`, and console-specific commands for scrolling and
clearing.

In the tty-console, the keyboard commands using ALT is accessed by prefixing the
key with `ESC`. So `ESC-b` is eqvivalent to `ALT-b` in the in-game console.
Most terminal emulators can be configured to make `ALT` send `ESC`-codes in this
way.

Internal font
-------------

The default OA font used for the console and some in-game text doesn't look
very good.

It is possible to include an custom made internal font, which looks better (in
my opinion). This is controlled by the Makefile variable `BUILD_INTERNAL_FONT`
(it is set to `1` by default) when building the renderer libs. When the
renderer has been built using this option, there is a cvar called
`r_useInternalFont` which when set to a non-zero value will override the
default font with the internal one.  The default value for `r_useInternalFont`
is `0`.

When building the internal font, each renderer gets a copy of the font. This
adds ~100KB of memory to each of the renderer libs. This isn't very much
considering that the opengl1 renderer is over 3.5 MB without the included font.
But if you don't want to use this feature, you can disable the build option and
save some memory.
