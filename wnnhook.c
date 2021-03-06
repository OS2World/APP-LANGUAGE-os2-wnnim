/****************************************************************************
 * wnnhook.c                                                                *
 * PM hook DLL which intercepts keystrokes and processes them as needed.    *
 *                                                                          *
 ****************************************************************************
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
#define INCL_DOSERRORS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_WIN
#include <os2.h>
#include <stdio.h>

#include "wnnhook.h"
#include "ids.h"

BOOL      g_fHookActive;
HMODULE   g_hMod;
HAB       g_hab;
HMQ       g_hmq;
HWND      g_hwndClient;
HATOMTBL  g_hATSys;
WNNSHARED global;



// This is a bit convoluted; basically, IS_HOTKEY takes as input the WM_CHAR
// message's flags (mf), virtual-key code (mv) and character key code (mc), plus
// the flags (hf) and character key code (hc) of the specified hotkey.
// It returns true if:
//  - the message flags (mf) match the hotkey's virtual key flags (hf), AND
//    - either KC_VIRTUALKEY is set (in mf) and the virtual-key code (mv) matches the hotkey's (hc), OR
//    - the character code (mc) matches the hotkey's (hc).
#define IS_HOTKEY(mf, mv, mc, hf, hc)  \
                            ((BOOL)((( mf & hf ) == hf ) && \
                                    ((( mf & KC_VIRTUALKEY) && ( mv == hc )) || \
                                                               ( mc == hc ))))

/* -------------------------------------------------------------------------- *
 * Hook for posted messages.                                                  *
 * Return TRUE for messages processed here, or FALSE to pass the message on.  *
 * -------------------------------------------------------------------------- */
BOOL EXPENTRY WnnHookInput( HAB hab, PQMSG pQmsg, USHORT fs )
{
    UCHAR  c;
    UCHAR  scan;
    USHORT fsFlags;
    USHORT usVK;


    switch( pQmsg->msg ) {
        case WM_CHAR:

            fsFlags = SHORT1FROMMP( pQmsg->mp1 );
            if ( fsFlags & KC_KEYUP ) break;    // don't process key-up events

            c    = (UCHAR)( SHORT1FROMMP( pQmsg->mp2 ) & 0xFF );
            scan = CHAR4FROMMP( pQmsg->mp2 );
            usVK = SHORT2FROMMP( pQmsg->mp2 );

            // Check for hotkey commands first (regardless of mode)

//          if ((( fsFlags & global.fsVKInput ) == global.fsVKInput ) && ( c == global.usKeyInput )) {
            if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKInput, global.usKeyInput )) {
                // Toggle input hotkey
                WinPostMsg( g_hwndClient, WM_COMMAND,
                            MPFROMSHORT( ID_HOTKEY_INPUT ),
                            MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                return TRUE;
            }
            else if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKCJK, global.usKeyCJK )) {
                // Toggle CJK hotkey
                WinPostMsg( g_hwndClient, WM_COMMAND,
                            MPFROMSHORT( ID_HOTKEY_KANJI ),
                            MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                return TRUE;
            }
            else if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKMode, global.usKeyMode )) {
                // Switch input mode hotkey
                WinPostMsg( g_hwndClient, WM_COMMAND,
                            MPFROMSHORT( ID_HOTKEY_MODE ),
                            MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                return TRUE;
            }

            // The following are only applicable during clause entry
            else if ( global.fsMode & MODE_CJK_ENTRY ) {
                if (( fsFlags & KC_VIRTUALKEY ) &&
                         (( usVK == VK_BACKSPACE ) || ( usVK == VK_DELETE )))
                {
                    // Delete last character
                    WinPostMsg( g_hwndClient, global.wmDelChar, pQmsg->mp1, pQmsg->mp2 );
                    return TRUE;
                }
                else if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKNext, global.usKeyNext )) {
                    // Select next (or first) phrase within clause
                    WinPostMsg( g_hwndClient, WM_COMMAND,
                                MPFROMSHORT( ID_HOTKEY_NEXT ),
                                MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                    return TRUE;
                }
                else if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKPrev, global.usKeyPrev )) {
                    // Select previous (or last) phrase within clause
                    WinPostMsg( g_hwndClient, WM_COMMAND,
                                MPFROMSHORT( ID_HOTKEY_PREV ),
                                MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                    return TRUE;
                }
                else if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKConvert, global.usKeyConvert )) {
                    // Convert clause buffer to CJK characters
                    WinPostMsg( g_hwndClient, WM_COMMAND,
                                MPFROMSHORT( ID_HOTKEY_CONVERT ),
                                MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                    return TRUE;
                }
                else if ( IS_HOTKEY( fsFlags, usVK, c, global.fsVKAccept, global.usKeyAccept )) {
                    // Accept current CJK conversion candidate
                    WinPostMsg( g_hwndClient, WM_COMMAND,
                                MPFROMSHORT( ID_HOTKEY_ACCEPT ),
                                MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                    return TRUE;
                }
                else if (( fsFlags & KC_VIRTUALKEY ) && ( usVK == VK_ESC )) {
                    // Cancel conversion
                    WinPostMsg( g_hwndClient, WM_COMMAND,
                                MPFROMSHORT( ID_HOTKEY_CANCEL ),
                                MPFROM2SHORT( CMDSRC_OTHER, FALSE ));
                    return TRUE;
                }
            }


            // Check for input characters
            if ( fsFlags & KC_CHAR ) {

                // Special treatment for certain Japanese keyboard scancodes
                if ( !( usVK & VK_SHIFT ) && !( usVK & VK_ALT ) && !( usVK & VK_CTRL )) {
                    if ( scan == 0x7D )      c = 0x7E;  // halfwidth yen --> ASCII backslash
                    else if ( scan == 0x73 ) c = 0xFE;  // Japanese backslash --> special value 0xFE
                }

                if ( global.fsMode & 0xFF ) {           // Any conversion mode is active
                    if ( !( usVK & VK_NUMLOCK ) && (( c > 0x20 && c < 0x7F ) || ( c >= 0xFE ))) {
                        // Convertible byte value
                        global.hwndSource = pQmsg->hwnd;
                        pQmsg->mp2 = MPFROM2SHORT( SHORT1FROMMP( pQmsg->mp2 ) | c, usVK );
                        WinPostMsg( g_hwndClient, global.wmAddChar, pQmsg->mp1, pQmsg->mp2 );
                        return TRUE;
                    }
                }
            }

            // Don't pass keys through if we're in the middle of clause entry
            if ( global.fsMode & MODE_CJK_ENTRY )
                return TRUE;

            // Otherwise leave everything else for to the source window to handle
            break;

    }
    return FALSE;
}


/* -------------------------------------------------------------------------- *
 *                                                                            *
 * -------------------------------------------------------------------------- */
PWNNSHARED EXPENTRY _Export WnnGlobalData( void )
{
    return &global;
}


/* -------------------------------------------------------------------------- *
 *                                                                            *
 * -------------------------------------------------------------------------- */
BOOL EXPENTRY _Export WnnHookInit( HWND hwnd )
{
#ifndef TESTHOOK
    g_hmq = NULLHANDLE;
#else
    g_hmq = HMQ_CURRENT;
#endif

    g_hwndClient = hwnd;
    if ( g_fHookActive ) return TRUE;

    g_hATSys = WinQuerySystemAtomTable();
    global.wmAddChar = WinAddAtom( g_hATSys, "WnnAddChar");
    global.wmDelChar = WinAddAtom( g_hATSys, "WnnDelChar");

    if ( DosQueryModuleHandle("wnnhook", &g_hMod )) return FALSE;
    g_hab = WinQueryAnchorBlock( hwnd );
    WinSetHook( g_hab, g_hmq, HK_INPUT, (PFN) WnnHookInput, g_hMod );
    g_fHookActive = TRUE;

    return TRUE;
}


/* -------------------------------------------------------------------------- *
 *                                                                            *
 * -------------------------------------------------------------------------- */
BOOL EXPENTRY _Export WnnHookTerm( void )
{
    if ( g_fHookActive ) {
        WinReleaseHook( g_hab, g_hmq, HK_INPUT, (PFN) WnnHookInput, g_hMod );
        WinDeleteAtom( g_hATSys, global.wmAddChar );
        WinDeleteAtom( g_hATSys, global.wmDelChar );
        g_fHookActive = FALSE;
    }

    return TRUE;
}

