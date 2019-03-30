/****************************************************************************
 * settings.c                                                               *
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
#define INCL_WIN
#define INCL_PM
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uconv.h>

#include "ids.h"
#include "wnnhook.h"
#include "wnnim.h"


// --------------------------------------------------------------------------
// CONSTANTS
//
#define SZ_DEFAULTFONT  "8.Helv"


// --------------------------------------------------------------------------
// MACROS
//

#define LIST_ADD_STRING( hw, ctl, str ) \
            (SHORT) WinSendDlgItemMsg( hw, ctl, LM_INSERTITEM,  MPFROMSHORT( LIT_END ), MPFROMP( str ))
#define LIST_SET_ITEMDATA( hw, ctl, idx, val ) \
            WinSendDlgItemMsg( hw, ctl, LM_SETITEMHANDLE, MPFROMSHORT( idx ), MPFROMLONG( val ))
#define LIST_GET_ITEMDATA( hw, ctl, idx ) \
            (ULONG)(WinSendDlgItemMsg( hw, ctl, LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), 0L ))
#define LIST_SELECT_ITEM( hw, ctl, idx ) \
            WinSendDlgItemMsg( hw, ctl, LM_SELECTITEM, MPFROMSHORT( idx ), MPFROMSHORT( TRUE ))


// ==========================================================================
// IMPLEMENTATION
// ==========================================================================


/* ------------------------------------------------------------------------- *
 * SettingsInit                                                              *
 *                                                                           *
 * Set the initial program settings.                                         *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SettingsInit( HWND hwnd )
{
    LONG  lClr;

    // Colours
    lClr = SYSCLR_DIALOGBACKGROUND;
    WinSetPresParam( hwnd, PP_BACKGROUNDCOLORINDEX, sizeof( lClr ), &lClr );
    lClr = SYSCLR_WINDOWTEXT;
    WinSetPresParam( hwnd, PP_FOREGROUNDCOLORINDEX, sizeof( lClr ), &lClr );

    // Font
    WinSetPresParam( hwnd, PP_FONTNAMESIZE,
                     strlen(SZ_DEFAULTFONT)+1, (PVOID) SZ_DEFAULTFONT );

    // Default hotkeys
    pShared->usKeyInput   = 0x20;
    pShared->fsVKInput    = KC_CTRL;

    pShared->usKeyMode    = 0x20;
    pShared->fsVKMode     = KC_SHIFT;

    pShared->usKeyCJK     = 0x00;
    pShared->fsVKCJK      = KC_CTRL | KC_SHIFT;

    pShared->usKeyConvert = 0x20;
    pShared->fsVKConvert  = 0;

    pShared->usKeyAccept  = VK_NEWLINE;
    pShared->fsVKAccept   = KC_VIRTUALKEY;

    pShared->usKeyNext    = VK_RIGHT;
    pShared->fsVKNext     = KC_CTRL | KC_VIRTUALKEY;

    pShared->usKeyPrev    = VK_LEFT;
    pShared->fsVKPrev     = KC_CTRL | KC_VIRTUALKEY;

}


/* ------------------------------------------------------------------------- *
 * ListSelectDataItem                                                        *
 *                                                                           *
 * Select the listbox/combobox item with the given item handle value.        *
 * ------------------------------------------------------------------------- */
BOOL ListSelectDataItem( HWND hwnd, USHORT usID, ULONG ulHandle )
{
    SHORT i;
    SHORT sIdx;
    ULONG ulData;

    sIdx = (SHORT) WinSendDlgItemMsg( hwnd, usID, LM_QUERYITEMCOUNT, 0L, 0L );
    if ( !sIdx ) return FALSE;

    for ( i = 0; i < sIdx; i ++ ) {
        ulData = LIST_GET_ITEMDATA( hwnd, usID, i );
        if ( ulData == ulHandle ) break;
    }
    LIST_SELECT_ITEM( hwnd, usID, i );
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * SettingsPopulateKeyList                                                   *
 *                                                                           *
 * Populate a key selection list with our standard set of supported keys.    *
 * ------------------------------------------------------------------------- */
void SettingsPopulateKeyList( HWND hwnd, USHORT usID )
{
    SHORT sIdx;
    sIdx = LIST_ADD_STRING( hwnd, usID, "Space");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, 0x20 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "Enter");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_NEWLINE );
    sIdx = LIST_ADD_STRING( hwnd, usID, "`");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)'`');
    sIdx = LIST_ADD_STRING( hwnd, usID, ".");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)'.');
    sIdx = LIST_ADD_STRING( hwnd, usID, ",");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)',');
    sIdx = LIST_ADD_STRING( hwnd, usID, "/");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)',');
    sIdx = LIST_ADD_STRING( hwnd, usID, "Left");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_LEFT );
    sIdx = LIST_ADD_STRING( hwnd, usID, "Right");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_RIGHT );
    sIdx = LIST_ADD_STRING( hwnd, usID, "Up");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_UP );
    sIdx = LIST_ADD_STRING( hwnd, usID, "Down");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_DOWN );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F1");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F1 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F2");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F2 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F3");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F3 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F4");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F4 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F5");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F5 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F6");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F6 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F7");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F7 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F8");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F8 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F9");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F9 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F10");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F10 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F11");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F11 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "F12");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, VK_F12 );
    sIdx = LIST_ADD_STRING( hwnd, usID, "");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, 0 );
}


/* ------------------------------------------------------------------------- *
 * SettingsDlgPopulate                                                       *
 *                                                                           *
 * Populate the controls in the settings dialog and select the current       *
 * configuration values.                                                     *
 * ------------------------------------------------------------------------- */
void SettingsDlgPopulate( HWND hwnd )
{
    SHORT sIdx;

    sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, "Remember last used");
    sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, "None");
    LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_NONE );

    if ( IS_LANGUAGE( pShared->fsMode, MODE_JP )) {
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, "Hiragana");
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_HIRAGANA );
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, "Katakana");
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_KATAKANA );
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, "Fullwidth ASCII");
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_FULLWIDTH );
    }
    else if ( IS_LANGUAGE( pShared->fsMode, MODE_KR )) {
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, "Hangul");
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_HANGUL );
    }
    LIST_SELECT_ITEM( hwnd, IDD_STARTUP_MODE, 1 );

    SettingsPopulateKeyList( hwnd, IDD_INPUT_KEY );
    ListSelectDataItem( hwnd, IDD_INPUT_KEY, pShared->usKeyInput );
    WinCheckButton( hwnd, IDD_INPUT_CTRL, (pShared->fsVKInput & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_INPUT_SHIFT, (pShared->fsVKInput & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_MODE_KEY );
    ListSelectDataItem( hwnd, IDD_MODE_KEY, pShared->usKeyMode );
    WinCheckButton( hwnd, IDD_MODE_CTRL, (pShared->fsVKMode & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_MODE_SHIFT, (pShared->fsVKMode & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_CLAUSE_KEY );
    ListSelectDataItem( hwnd, IDD_CLAUSE_KEY, pShared->usKeyCJK );
    WinCheckButton( hwnd, IDD_CLAUSE_CTRL, (pShared->fsVKCJK & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_CLAUSE_SHIFT, (pShared->fsVKCJK & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_CONVERT_KEY );
    ListSelectDataItem( hwnd, IDD_CONVERT_KEY, pShared->usKeyConvert );
    WinCheckButton( hwnd, IDD_CONVERT_CTRL, (pShared->fsVKConvert & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_CONVERT_SHIFT, (pShared->fsVKConvert & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_ACCEPT_KEY );
    ListSelectDataItem( hwnd, IDD_ACCEPT_KEY, pShared->usKeyAccept );
    WinCheckButton( hwnd, IDD_ACCEPT_CTRL, (pShared->fsVKAccept & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_ACCEPT_SHIFT, (pShared->fsVKAccept & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_NEXT_KEY );
    ListSelectDataItem( hwnd, IDD_NEXT_KEY, pShared->usKeyNext );
    WinCheckButton( hwnd, IDD_NEXT_CTRL, (pShared->fsVKNext & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_NEXT_SHIFT, (pShared->fsVKNext & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_PREV_KEY );
    ListSelectDataItem( hwnd, IDD_PREV_KEY, pShared->usKeyPrev );
    WinCheckButton( hwnd, IDD_PREV_CTRL, (pShared->fsVKPrev & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_PREV_SHIFT, (pShared->fsVKPrev & KC_SHIFT)? TRUE: FALSE );
}


/* ------------------------------------------------------------------------- *
 * Window procedure for settings dialog.                                     *
 * See OS/2 PM reference for a description of input and output.              *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY SettingsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{

    switch ( msg ) {
        case WM_INITDLG:
            SettingsDlgPopulate( hwnd );
            CentreWindow( hwnd, NULLHANDLE, SWP_SHOW );
            break;

    }
    return WinDefDlgProc (hwnd, msg, mp1, mp2) ;
}



