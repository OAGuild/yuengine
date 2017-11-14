Yu Engine for OpenArena
=======================

Engine forked from ioquake3 for use with OpenArena. By default it will connect
use the OpenArena master server and use OpenArenas protocol number, removing the
need for a wrapper script like as is required with ioquake3.

Build instructions
------------------

See [the README of ioquake3](./ioq3-readme.md) for instructions on how to
compile and install this software.

Feature list
------------

New features implemented in Yu Engine are the following:

- Hor+ FOV
- New cvar `cl_scaleSensWithFov` to automatically adjust mouse sensitivity
  automatically as FOV changes to preserve the same mouse feel
- Additional keyboard editing shortcuts for editing text fields

The code for the autoupdater, gamecode and UI has been removed from the ioquake3
source tree.

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

Editing text fields
-------------------

Additional keyboard commands have been added for editing text fields. These
commands are available when typing commands in the in-game console and when
writing chat messages.

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
