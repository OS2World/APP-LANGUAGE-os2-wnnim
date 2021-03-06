/****************************************************************************
 * wnnim.h                                                                  *
 *                                                                          *
 *  This program is free software; you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation; either version 2 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  This program is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *  You should have received a copy of the GNU General Public License       *
 *  along with this program; if not, write to the Free Software             *
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA                *
 *  02111-1307  USA                                                         *
 *                                                                          *
 ****************************************************************************/

#include <PMPRINTF.H>

// --------------------------------------------------------------------------
// CONSTANTS
//

#define SZ_VERSION      "0.3.0"
#define SZ_COPYRIGHT    "2019"
#define MAX_VERSTRZ     32
#define MAX_STATUSZ     64
#define US_RES_MAXZ     256
#define MAX_BTN_LABELZ  12
#define MAX_CHAR_BUFZ   9
#define MAX_KANA_BUFZ   32
#define MAX_ENGINE_ERRZ 128
#define CLAUSE_INCZ     32

// Status returned by ConvertPhonetic()
#define KANA_INVALID   -1   // Phonetic conversion failed
#define KANA_PENDING    0   // Phonetic sequence is incomplete but potentially valid
#define KANA_COMPLETE   1   // Phonetic sequence is valid and complete
#define KANA_CANDIDATE  2   // Phonetic sequence is valid as-is but may still be modifiable

// Status returned by various clause conversion functions
#define CONV_FAILED    -1   // Operation failed (check error message from Wnn)
#define CONV_CONNECT   -2   // Operation not attempted due to no connection to server
#define CONV_NOOP       0   // Operation cancelled or nothing to convert
#define CONV_OK         1   // Operation succeeded

// Flags for the low byte of global.fsClause (the high byte contains the
// phrase number to which CLAUSE_PHRASE_READY refers)
#define CLAUSE_READY        0x01  // current clause has been converted
#define CLAUSE_PHRASE_READY 0x02  // current phrase has been converted

// Indicates codepages compatible with a given language
#define ISRUCODEPG( cp )    ( cp == 866 )
#define ISJPCODEPG( cp )    ( cp == 932 || cp == 942 || cp == 943 )
#define ISKOCODEPG( cp )    ( cp == 949 )
#define ISTWCODEPG( cp )    ( cp == 950 )
#define ISCNCODEPG( cp )    ( cp == 1381 || cp == 1386 )


#ifndef WS_TOPMOST
#define WS_TOPMOST      0x00200000L
#endif


// --------------------------------------------------------------------------
// MACROS
//

// useful rectangle macros
#define RECTL_HEIGHT(rcl)       (rcl.yTop - rcl.yBottom)
#define RECTL_WIDTH(rcl)        (rcl.xRight - rcl.xLeft)

//#define ErrorPopup( owner, text ) \
//    WinMessageBox( HWND_DESKTOP, owner, text, "Error", 0, MB_OK | MB_ERROR )


// --------------------------------------------------------------------------
// TYPES
//

// Global client data which does not have to be shared with wnnhook.dll.
typedef struct _WnnClientData {
    HAB         hab;                    // anchor block handle
    ULONG       ulLangBase;             // base message ID for current UI language

    // Window handles
    HWND        hwndFrame,              // our frame
                hwndClient,             // our client window
                hwndMenu,               // our context menu
                hwndLast,               // last window to have focus
                hwndInput,              // latest window to be the conversion target
                hwndClause,             // clause conversion overlay window
                hwndCandidates;         // conversion candidate menu (TODO)

    // User options
    SHORT       sDefMode;                          // default input mode (MODE_* or 0xFF for last used)
    UCHAR       szInputFont[ FACESIZE ];           // default conversion window font

    // General state variables
    BYTE        dbcs[ 12 ];                        // DBCS information vector (byte-ranges)
    ULONG       codepage;                          // DBCS output codepage
    USHORT      fsLastMode;                        // last active input mode
    PRECTL      pRclConv;                          // position from WM_QUERYCONVERTPOS

    // Current conversion data
    CHAR        szRomaji[ MAX_CHAR_BUFZ ];         // input buffer (characters as typed)
    UniChar     uszKana[ MAX_KANA_BUFZ ];          // buffer for converted phonetic characters
    UniChar     uszPending[ MAX_KANA_BUFZ ];       // buffer for 'candidate' phonetic characters (Korean only)
    UniChar    *puszClause;                        // original unconverted clause string
    UconvObject uconvOut;                          // conversion object for DBCS output codepage
    USHORT      fsClause;                          // misc. clause state flags
    CHAR        szEngineError[ MAX_ENGINE_ERRZ ];  // error messages from the IME engine
    PVOID       pSession;                          // IME engine session object

} IMCLIENTDATA, *PIMCLIENTDATA;


// --------------------------------------------------------------------------
// GLOBALS
//

PWNNSHARED   pShared;           // data shared with the dll


// --------------------------------------------------------------------------
// COMMON FUNCTIONS
//

void CentreWindow( HWND hwndCentre, HWND hwndRelative, ULONG flFlags );
void ErrorPopup( HWND hwnd, PSZ pszText );

