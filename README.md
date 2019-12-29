WnnIM for OS/2
==============

WnnIM for OS/2 (a.k.a. 'WnnIM/2' or just 'WnnIM') is an Input Method Editor for
typing East Asian (CJK) text under Presentation Manager.

Specifically, WnnIM itself is the front-end processor component, and is designed
to work in conjunction with the open source FreeWnn IME engine on the back-end.
As such, it requires the FreeWnn server for the desired language to be installed 
and running (not necessarily on the same computer, but accessible via TCP/IP).

Only Presentation Manager (OS/2 graphical) sessions are supported; DOS and
Win-OS/2 sessions are not.  OS/2 command lines are not supported either,
_except_ for windowed VIO sessions running on DBCS versions of OS/2.

The current version supports Japanese input.  Support for other Asian languages
is planned for future releases.


For Users
---------

### Installing FreeWnn (prerequisite)

The FreeWnn server and runtime files are required.  These are provided in the
FreeWnn RPM packages, which should soon be available from a repository near you;
in the meantime, you can get `FreeWnn-1.1.1-0.a023.1.i386.rpm` from
[here](https://drive.google.com/drive/folders/0B_CmLQmhb3PzelRpakJ6OXl3YnM).

Reboot after installing the FreeWnn RPM.

#### Running FreeWnn remotely (optional)

If you prefer not to install the FreeWnn server locally, you can set it up
to use an installation of FreeWnn anywhere on your network (for example, on
a separate Linux box).  In this case, you will still need to install the 
runtime files locally, which you can do by following these steps:

1. Extract `wnn0.dll` from the RPM and place it in a directory on your LIBPATH.
2. Extract the contents of `usr\lib\wnn` (including all subdirectories); 
   place them somewhere convenient (e.g. `E:\usr\local\lib\wnn`).  (Note: The
   maximum allowed length of this path is 200 characters.)
3. Set the environment variable `WNNLIB` to the fully-qualified path of
   the directory in step (2) (e.g. `SET WNNLIB=E:\usr\local\lib\wnn`).
   This allows WnnIM to locate the Wnn configuration files.
4. Set the environment variable `JSERVER` to the hostname and instance
   number of the FreeWnn server on your network, in the form `hostname:#`
   (e.g. `SET JSERVER=localhost:1`).  (The instance number is usually 1,
   at least for the Japanese FreeWnn server; it corresponds to the `-N` 
   parameter specified on the server command line.)
5. Shut down and reboot.

The above steps are not needed if you installed the FreeWnn RPM.

### Installing WnnIM

Make sure FreeWnn is available (using either method described above).  

You can install WnnIM from RPM using the rpm command line or a program like 
ANPM.  Alternatively, you can install it manually using the following steps:

1. Place `wnnhook.dll` and `wnnim.exe` together in a directory of your choice.
2. Place the `rk` directory and its contents somewhere.  Edit your CONFIG.SYS
   file and define the environment variable `ROMKAN_TABLE` as the fully-
   qualified path to either the `modew` or `modec` file (see below for the
   difference) in this directory.  (Reboot after updating CONFIG.SYS.)
3. Create a program object for `wnnim.exe` if you desire.

Two different configuration files for romaji-to-kanji input conversion are
provided.  You can choose which one to use, according to your preference:

 * `modew` is designed to work similarly to the Japanese IME on Windows.
   Of note is that in order to enter a 'ん' or 'ン' kana, you must type 'NN'.
   (Thus, to enter the word 'みんな', you type 'MINNNA'.)

 * `modec` is a custom configuration file with a few key differences from
   `modew`.  First, you enter 'ん' or 'ン' by typing 'N' _once_, followed by
   either an apostrophe ('), a punctuation character, or any letter other than
   a vowel -- or, in clause conversion mode, activating clause conversion or
   accepting the current clause.  (Thus, to enter the word 'みんな', you type
   'MINNA'.)  This behaviour is more like that of older non-Windows IMEs.

   Second, with this configuration file, typing 'TU' or 'DU' while in katakana
   input mode will produce トゥ or ドゥ, respectively (type 'TSU'/'TZU' to 
   produce 'ツ'/'ヅ'); and typing 'TI/DI' will similarly produce 'ティ'/'ディ' 
   (type 'CHI'/'DZI' for 'チ'/'ヂ').  These special sequences apply _only_ when
   typing in katakana (not hiragana).

### Using WnnIM

Run `wnnim.exe` to start the IME.  The WnnIM user interface consists of a small
window located (by default) at the bottom right of your screen (you can move it
around by dragging).  This UI consists of two small buttons and a status panel.

 * The button labelled 'I' (or '入') toggles input conversion on or off.  You 
   can also toggle this setting using Ctrl+Space.
 * The button labelled 'C' (or '変') toggles CJK clause conversion on or off.  
   You can also toggle this setting using Ctrl+Shift.

There is also a popup context menu which allows you to do various things, such
as select the input conversion mode (currently hiragana, katakana and fullwidth
ASCII are supported), adjust settings, or close the program.

You can also change the input conversion mode using Shift+Space, which will
cycle through the available modes.

When in clause conversion mode, the following hotkey commands apply:

    Space:            Convert the current clause text
    Enter or Return:  Accept the current text as shown
    Esc:              Cancel conversion (this will clear the current clause text)
    Backspace or Del: Delete the last character

The currently-active mode is system wide, i.e. switching from one program to 
another will retain the current conversion mode.  (This may change in the 
future.)

Most of the aforementioned hotkey commands (except for Esc and Backspace) can be 
changed via the settings dialog.

Romaji input works much the same way as it does in other IMEs (subject to the
differences between the `modew` and `modec` configuration files noted above).
However, some possibly non-obvious input sequences include:

	TYA	ちゃ		DYA	ぢゃ
	TYI	てぃ		DYI	でぃ
	TYU	ちゅ		DYU	ぢゅ
	TYE	ちぇ		DYE	ぢぇ
	TYO	ちょ		DYO	ぢょ
	XTI	てぃ		XDI	でぃ
	XDU	どぅ		XDE	でぇ
	XDO	どぉ		XWI	うぃ
	XWE	うぇ		XWO	うぉ
	Z.	…		Z-	〜
	Z/	＼

(The above apply to both hiragana and katakana input.)


For Developers
--------------

The included Makefile requires the IBM C Compiler (preferably version 3.65).
It might be possible to build with GCC, but any functions which are explicitly
declared `_Optlink` might have to be changed to `_System` or `_cdecl`.  Also,
the PM hook DLL (see below) is built with the subsystem runtime (ICC `/Rn`
switch) to avoid side effects due to its data segment being SINGLE SHARED; using
GCC or another compiler would probably require some changes to this approach.

Building the client application requires the FreeWnn library and header files;
these are included in the `FreeWnn-devel` RPM (provisionally available from
[here](https://drive.google.com/drive/folders/0B_CmLQmhb3PzelRpakJ6OXl3YnM).)

All the FreeWnn-specific code is isolated in one or two source files.  In
principle, the rest of the WnnIM code could probably be used to implement an IME
based on a different engine such as Canna or Anthy (although I have no plans to
try doing such a thing myself).

### Concepts

WnnIM's operation is based around two 'mode' settings which work in conjunction
with each other: _input mode_ and _CJK conversion mode_.

**Note:** Not all of the following has actually been implemented yet.

The input mode controls how typed characters are converted into basic phonetic
characters for the language in question.  The number of modes supported varies
according to language, although 'none' (i.e. no conversion) is always available.
(Japanese, for example, has Hiragana, Katakana, and Fullwidth ASCII modes; 
Korean has only Hangul.)

If the target language uses phonetic characters, then how it works is that all
typed characters are saved into a buffer (known internally as the 'romaji
buffer') until the buffer contents can be successfully converted into phonetic
characters (the results of which are placed in the 'kana buffer').  Once
conversion is successful and unambiguous, the converted characters are inserted
into the application window where the original characters were typed -- unless
CJK conversion is also active, in which case see below.  If conversion is
unsuccessful (i.e. the buffer contents do not represent a valid character in the
target language), then the conversion attempt is abandoned and the current
buffer contents are inserted into the application window unchanged.

[TODO 1: If conversion is successful but ambiguous (that is, the buffer
represents a valid character as-is, but could potentially become a different
character if more letters were added, as may be the case with Korean), then the
current conversion is offered as a 'candidate' for the user to either accept or
continue typing.]

[TODO 2: If the target language does not use phonetic characters, then all of
the above is bypassed: typed characters are simply added to the clause buffer
in place of phonetic characters, and the logic otherwise proceeds as below.
This will probably be the case with Chinese (both types).]

The other mode is CJK (or 'clause') conversion mode.  This mode is a simple
on-or-off toggle.  When active, the phonetic characters which have been
confirmed for entry (per above) will not be sent directly to the application,
but rather added to a 'clause buffer' which is displayed on-screen as an overlay
at the cursor position.  When the user hits a 'convert' key (Space by default),
the current clause buffer will be converted (if possible) into the CJK glyphs 
(kanji/hanzi/hanja) applicable to the current language.  There are usually
multiple possible conversions for a given phonetic string; hitting the 
conversion key multiple times will cycle through the candidates.  Once a 
candidate is accepted by pressing the appropriate key (Enter by default), the 
full converted clause will be inserted in the application window.

In either case, once a conversion (phonetic or clause) is accepted and the
result inserted in the application, all buffers are cleared and the process
starts over.

If the input mode is set to 'none', then all conversion (input and CJK) is
disabled, and typed characters are simply sent to the application as they
are.

### Source Code Organization

`wnnclient.*` implements the interface to the FreeWnn engine (it is linked into
`wnnim.exe`).  If WnnIM were to be adapted to use a different engine, this is
the module that would need to be replaced.

`wnnhook.*` implements a global PM hook, which is loaded by the client (below)
when the latter starts up.  This hook is responsible for intercepting keystrokes
in any running PM application, and either passing it on to the application
directly or running it through WnnIM's conversion logic.

`wnnim.*` is the main client (front-end) application.  It activates the PM hook
(above), and presents the UI controls for the user to operate the IME.  It also
implements most of the actual keypress processing logic (triggered as needed by
the PM hook), for the sake of keeping the hook DLL as lightweight as possible.

`convwin.*` implements the clause conversion (overlay) window.

`settings.*` contains client routines specific to managing user settings.

`clipfuncs.*` contains some utility functions for clipboard logic.

`codepage.*` contains various useful functions for dealing with text encodings.

`wnnconv.c` contains some FreeWnn text conversion routines; they are not 
exported by the library and so were simply lifted from the sources.

`ids.h` defines various resource and message IDs used by both the client and the
PM hook.

### Environment variables

The following environment variables affect the operation of WnnIM/2:

 * `JSERVER`: Indicates the host and instance number where the FreeWnn server
   is running.  For most local installations this will be `localhost:1`.

 * `WNNLIB`: Indicates the path to the FreeWnn configuration files.  (This path
    may be no more than 200 characters long.)  If not defined, WnnIM/2 will look
    for them under the directory `%UNIXROOT%\usr\lib\wnn`.  

 * `ROMKAN_TABLE`: Indicates a custom romaji input conversion table to be used.
    If this value includes a path specifier, it is taken as the fully-qualified
    name of the conversion table file.  If it contains a filename only (maximum
    allowed length 49 characters), that file is assumed to be located under the
    usual FreeWnn configuration file directory (determined as described above),
    in the subdirectory `<lang>\rk`.  If not defined, the default FreeWnn romaji
    conversion table (`mode` in the aforementioned directory) will be used.


Limitations
-----------

1. In general, an application must be running under the corresponding DBCS
   codepage in order for WnnIM text entry to work.  This is especially true
   for applications that try to interpret text intelligently (e.g. by
   converting into Unicode).  For reference, the relevant codepages are:

    - Japanese: 932
    - Korean: 949
    - Simplified Chinese: 1386
    - Traditional Chinese: 950

   Note that running under these codepages without also using the corresponding
   language OS version and keyboard driver may prevent you from typing certain 
   characters like tilde (~) and backslash (\).

2. For the characters to display properly in the application, it will need to
   use fonts that support the language in question.  In other words, Japanese
   text requires the target program to be using a Japanese font (in addition
   to the Japanese codepage, as noted above).  If you would rather not change
   a program's font to Japanese, then it may be possible to make use of
   Presentation Manager's "font association" feature (`PM_SystemFonts` ->
   `PM_AssociateFont` in OS2.INI), at least for programs which use standard PM
   controls or GPI text rendering.  However, this does have certain limitations.

3. Not all applications support positioning of the clause overlay window at the
   cursor.  (Qt-based applications are an example, as is OpenOffice.)  In such
   cases, the window will appear, enlarged, at the bottom left of the screen
   instead.  

4. Some applications do not support input of double-byte text in the normal way. 
   In some cases this can be worked around, but WnnIM has to explicitly add
   logic for these applications.  If you encounter such an application, please
   open a ticket for it so that it can be addressed.

5. Apache OpenOffice is a somewhat special case.  Current OS/2 builds do not
   support double-byte character input at all.  A workaround has been
   implemented to use the clipboard (instead of input messages) to insert the
   text in OpenOffice windows.  However, the nature of this workaround means
   that _anything in the clipboard will be lost_ whenever text is entered into
   OpenOffice using WnnIM.


Notices
-------

WnnIM for OS/2  
(C) 2019 Alexander Taylor

Some of the PM hook logic was derived from `xray` by Michael Shillingford, and
his [EDM/2 programming article](http://www.edm2.com/0501/hooks.html) on the
subject.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307  USA

WnnIM program source: [https://github.com/altsan/os2-wnnim]()
