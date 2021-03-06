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
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uconv.h>

#include "ids.h"
#include "wnnhook.h"
#include "wnnim.h"
#include "settings.h"


extern IMCLIENTDATA global;


BOOL  ListSelectDataItem( HWND hwnd, USHORT usID, ULONG ulHandle );
void  LocateProfile( PSZ pszProfile );
void  GetSelectedKey( HWND hwnd, USHORT usID, PUSHORT pusKC, PUSHORT pusVK );
void  SettingsPopulateKeyList( HWND hwnd, USHORT usID );
void  SettingsDlgPopulate( HWND hwnd );
BOOL  SettingsUpdateKeys( HWND hwnd );


static CHAR achErrText[ US_RES_MAXZ ];


/* ------------------------------------------------------------------------- *
 * LocateProfile                                                             *
 *                                                                           *
 * Figure out where to place our INI file.  This will be in the same         *
 * directory as OS2.INI (the OS/2 user profile).                             *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *     PSZ pszProfile: Character buffer to receive the INI filename.         *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void LocateProfile( PSZ pszProfile )
{
    ULONG ulRc;
    PSZ   pszUserIni,
          c;

    // Query the %USER_INI% environment variable which points to OS2.INI
    ulRc = DosScanEnv("USER_INI", &pszUserIni );
    if ( ulRc != NO_ERROR ) return;
    strncpy( pszProfile, pszUserIni, CCHMAXPATH );

    // Now change the filename portion to point to our own INI file
    if (( c = strrchr( pszProfile, '\\') + 1 ) != NULL ) {
        memset( c, 0, strlen(c) );
        strncat( pszProfile, INI_FILE, CCHMAXPATH - 1 );
    }
}


/* ------------------------------------------------------------------------- *
 * SettingsInit                                                              *
 *                                                                           *
 * Set the initial program settings.                                         *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND    hwnd: Our window handle.                                        *
 *   PPOINTL pptl: Pointer to window position.                               *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void _Optlink SettingsInit( HWND hwnd, PPOINTL pptl )
{
    HINI  hIni;                         // INI file
    CHAR  szIni[ CCHMAXPATH ]  = {0},   // full path of INI file
          szLang[ 6 ],                  // language (used as app name)
          szFont[ FACESIZE+4 ] = {0};   // font PP
    LONG  lClr,
          lVal;
    ULONG cb;


    LocateProfile( szIni );
    hIni = PrfOpenProfile( WinQueryAnchorBlock( hwnd ), szIni );

    switch ( pShared->fsMode & 0xF00 ) {
        case MODE_CN: strcpy( szLang, "zh_CN"); break;
        case MODE_TW: strcpy( szLang, "zh_TW"); break;
        case MODE_KR: strcpy( szLang, "ko_KR"); break;
        default:      strcpy( szLang, "ja_JP"); break;
    }

    // Colours (not saved)
    lClr = SYSCLR_DIALOGBACKGROUND;
    WinSetPresParam( hwnd, PP_BACKGROUNDCOLORINDEX, sizeof( lClr ), &lClr );
    lClr = SYSCLR_WINDOWTEXT;
    WinSetPresParam( hwnd, PP_FOREGROUNDCOLORINDEX, sizeof( lClr ), &lClr );

    // Font
    PrfQueryProfileString( hIni, PRF_APP_UI, PRF_KEY_UIFONT,
                           DEFAULT_GUI_FONT, &szFont, sizeof( szFont ) - 1 );
    WinSetPresParam( hwnd, PP_FONTNAMESIZE,
                     strlen( szFont ) + 1, (PVOID) szFont );

    // Position
    cb = sizeof( POINTL );
    if ( ! PrfQueryProfileData( hIni, PRF_APP_UI, PRF_KEY_UIPOS,
                                pptl, (PULONG) &cb ))
    {
        pptl->x = -1;
        pptl->y = -1;
    }

    // Startup mode
    cb = sizeof( global.sDefMode );
    if ( ! PrfQueryProfileData( hIni, szLang, PRF_KEY_STARTMODE,
                               &(global.sDefMode), &cb ))
        global.sDefMode = MODE_NONE;

    // Until we've finished initialization, fsLastMode will be used to store
    // the last input mode as read from the profile. This is a temporary duty.
    if ( global.sDefMode > 0 )
        global.fsLastMode = global.sDefMode;
    else {
        cb = sizeof( global.fsLastMode );
        if ( ! PrfQueryProfileData( hIni, szLang, PRF_KEY_INPUTMODE,
                               &(global.fsLastMode), &cb ))
            // Default to the first mode regardless of language
            global.fsLastMode = 1;
    }

    // Input font
    PrfQueryProfileString( hIni, szLang, PRF_KEY_INPUTFONT,
                           DEFAULT_INPUT_FONT, &(global.szInputFont),
                           sizeof( global.szInputFont ) - 1 );

    // Default hotkeys

    // Input conversion toggle
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_INPUT, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyInput = LOUSHORT( lVal );
        pShared->fsVKInput  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyInput = 0x20;
        pShared->fsVKInput  = KC_CTRL;
    }

    // Input mode switch
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_MODE, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyMode = LOUSHORT( lVal );
        pShared->fsVKMode  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyMode = 0x20;
        pShared->fsVKMode  = KC_SHIFT;
    }

    // CJK clause conversion toggle
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_CJK, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyCJK = LOUSHORT( lVal );
        pShared->fsVKCJK  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyCJK = 0x00;
        pShared->fsVKCJK  = KC_CTRL | KC_SHIFT;
    }

    // Convert current clause
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_CONVERT, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyConvert = LOUSHORT( lVal );
        pShared->fsVKConvert  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyConvert = 0x20;
        pShared->fsVKConvert  = 0;
    }

    // Accept current candidate
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_ACCEPT, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyAccept = LOUSHORT( lVal );
        pShared->fsVKAccept  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyAccept  = 0x0D;
        pShared->fsVKAccept   = 0;
    }

    // First/Next phrase
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_NEXT, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyNext = LOUSHORT( lVal );
        pShared->fsVKNext  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyNext    = VK_RIGHT;
        pShared->fsVKNext     = KC_CTRL | KC_VIRTUALKEY;
    }

    // Last/Previous phrase
    cb = sizeof( lVal );
    if ( PrfQueryProfileData( hIni, szLang, PRF_KEY_PREV, &lVal, &cb ) && ( cb == sizeof( lVal ))) {
        pShared->usKeyPrev = LOUSHORT( lVal );
        pShared->fsVKPrev  = HIUSHORT( lVal );
    }
    else {
        pShared->usKeyPrev    = VK_LEFT;
        pShared->fsVKPrev     = KC_CTRL | KC_VIRTUALKEY;
    }

    PrfCloseProfile( hIni );
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
 * GetSelectedKey                                                            *
 *                                                                           *
 * ------------------------------------------------------------------------- */
void GetSelectedKey( HWND hwnd, USHORT usID, PUSHORT pusKC, PUSHORT pusVK )
{
    SHORT sIdx;
    ULONG ulData;

    sIdx = (USHORT) WinSendDlgItemMsg( hwnd, usID, LM_QUERYSELECTION,
                                       MPFROMSHORT( LIT_FIRST ), 0 );
    if ( sIdx == LIT_NONE ) return;

    ulData = LIST_GET_ITEMDATA( hwnd, usID, sIdx );
    *pusVK |= (HIUSHORT( ulData ) & KC_VIRTUALKEY );
    *pusKC = LOUSHORT( ulData );
}


/* ------------------------------------------------------------------------- *
 * SettingsPopulateKeyList                                                   *
 *                                                                           *
 * Populate a key selection list with our standard set of supported keys.    *
 * Each item's handle (item data) has the key character code or virtual key  *
 * code in the low USHORT; the high USHORT is KC_VIRTUALKEY in the case of a *
 * virtual key or 0 otherwise.                                               *
 * ------------------------------------------------------------------------- */
void SettingsPopulateKeyList( HWND hwnd, USHORT usID )
{
    CHAR  achKeyName[ 32 ];
    SHORT sIdx;

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEYS_SPACE,
                         US_RES_MAXZ, achKeyName ))
        strcpy( achKeyName, "Space");
    sIdx = LIST_ADD_STRING( hwnd, usID, achKeyName );
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, 0x20 );

    sIdx = LIST_ADD_STRING( hwnd, usID, "Enter");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, 0x0D );
    sIdx = LIST_ADD_STRING( hwnd, usID, "`");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)'`');
    sIdx = LIST_ADD_STRING( hwnd, usID, ".");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)'.');
    sIdx = LIST_ADD_STRING( hwnd, usID, ",");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)',');
    sIdx = LIST_ADD_STRING( hwnd, usID, "/");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, (ULONG)'/');

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEYS_LEFT,
                         US_RES_MAXZ, achKeyName ))
        strcpy( achKeyName, "Left");
    sIdx = LIST_ADD_STRING( hwnd, usID, achKeyName );
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_LEFT, KC_VIRTUALKEY ));

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEYS_RIGHT,
                         US_RES_MAXZ, achKeyName ))
        strcpy( achKeyName, "Right");
    sIdx = LIST_ADD_STRING( hwnd, usID, achKeyName);
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_RIGHT, KC_VIRTUALKEY ));

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEYS_UP,
                         US_RES_MAXZ, achKeyName ))
        strcpy( achKeyName, "Up");
    sIdx = LIST_ADD_STRING( hwnd, usID, achKeyName );
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_UP, KC_VIRTUALKEY ));

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEYS_DOWN,
                         US_RES_MAXZ, achKeyName ))
        strcpy( achKeyName, "Down");
    sIdx = LIST_ADD_STRING( hwnd, usID, achKeyName );
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_DOWN, KC_VIRTUALKEY ));

    sIdx = LIST_ADD_STRING( hwnd, usID, "F1");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F1, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F2");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F2, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F3");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F3, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F4");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F4, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F5");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F5, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F6");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F6, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F7");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F7, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F8");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F8, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F9");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F9, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F10");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F10, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F11");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F11, KC_VIRTUALKEY ));
    sIdx = LIST_ADD_STRING( hwnd, usID, "F12");
    LIST_SET_ITEMDATA( hwnd, usID, sIdx, MAKEULONG( VK_F12, KC_VIRTUALKEY ));
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
    CHAR  achRes[ US_RES_MAXZ ];
    SHORT sIdx;

    // Set the static control text

    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_WINDOW_SETTINGS,
                        US_RES_MAXZ, achRes ))
        WinSetWindowText( hwnd, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_SET_OPTIONS,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_OPTIONS, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_SET_HOTKEYS,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_HOTKEYS, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_OPT_MODE,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_STARTUP_MODE, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_OPT_FONT,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_FONT, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_ACTIVATE,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_INPUT, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_MODE,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_MODE, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_CLAUSE,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_CLAUSE, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_CONVERT,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_CONVERT, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_ACCEPT,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_ACCEPT, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_NEXT,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_NEXT, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_PREV,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, IDD_STATIC_PREV, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_COMMON_OK,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, DID_OK, achRes );
    if ( WinLoadString( global.hab, 0, global.ulLangBase + IDS_COMMON_CANCEL,
                        US_RES_MAXZ, achRes ))
        WinSetDlgItemText( hwnd, DID_CANCEL, achRes );

    // Populate the list controls

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_OPT_USELAST,
                         US_RES_MAXZ, achRes ))
        strcpy( achRes, "Remember last used");
    sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, achRes );
    LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, -1 );

    if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_MODE_NONE,
                         US_RES_MAXZ, achRes ))
        strcpy( achRes, "No conversion");
    sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, achRes );
    LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_NONE );

    if ( IS_LANGUAGE( pShared->fsMode, MODE_JP )) {

        if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_MODE_JP_HIRAGANA,
                             US_RES_MAXZ, achRes ))
            strcpy( achRes, "Hiragana");
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, achRes );
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_HIRAGANA );

        if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_MODE_JP_KATAKANA,
                             US_RES_MAXZ, achRes ))
            strcpy( achRes, "Katakana");
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, achRes );
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_KATAKANA );

        if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_MODE_FULLWIDTH,
                             US_RES_MAXZ, achRes ))
            strcpy( achRes, "Fullwidth ASCII");
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, achRes );
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_FULLWIDTH );
    }
    else if ( IS_LANGUAGE( pShared->fsMode, MODE_KR )) {
        if ( !WinLoadString( global.hab, 0, global.ulLangBase + IDS_MODE_KO_HANGUL,
                             US_RES_MAXZ, achRes ))
            strcpy( achRes, "Hangul");
        sIdx = LIST_ADD_STRING( hwnd, IDD_STARTUP_MODE, achRes );
        LIST_SET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx, MODE_HANGUL );
    }
    //LIST_SELECT_ITEM( hwnd, IDD_STARTUP_MODE, (SHORT)(global.sDefMode) + 1 );
    ListSelectDataItem( hwnd, IDD_STARTUP_MODE, (ULONG) global.sDefMode );

    SettingsPopulateKeyList( hwnd, IDD_INPUT_KEY );
    ListSelectDataItem( hwnd, IDD_INPUT_KEY, MAKEULONG( pShared->usKeyInput, pShared->fsVKInput & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_INPUT_CTRL, (pShared->fsVKInput & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_INPUT_SHIFT, (pShared->fsVKInput & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_MODE_KEY );
    ListSelectDataItem( hwnd, IDD_MODE_KEY, MAKEULONG( pShared->usKeyMode, pShared->fsVKMode & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_MODE_CTRL, (pShared->fsVKMode & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_MODE_SHIFT, (pShared->fsVKMode & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_CLAUSE_KEY );
    ListSelectDataItem( hwnd, IDD_CLAUSE_KEY, MAKEULONG( pShared->usKeyCJK, pShared->fsVKCJK & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_CLAUSE_CTRL, (pShared->fsVKCJK & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_CLAUSE_SHIFT, (pShared->fsVKCJK & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_CONVERT_KEY );
    ListSelectDataItem( hwnd, IDD_CONVERT_KEY, MAKEULONG( pShared->usKeyConvert, pShared->fsVKConvert & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_CONVERT_CTRL, (pShared->fsVKConvert & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_CONVERT_SHIFT, (pShared->fsVKConvert & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_ACCEPT_KEY );
    ListSelectDataItem( hwnd, IDD_ACCEPT_KEY, MAKEULONG( pShared->usKeyAccept, pShared->fsVKAccept & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_ACCEPT_CTRL, (pShared->fsVKAccept & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_ACCEPT_SHIFT, (pShared->fsVKAccept & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_NEXT_KEY );
    ListSelectDataItem( hwnd, IDD_NEXT_KEY, MAKEULONG( pShared->usKeyNext, pShared->fsVKNext & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_NEXT_CTRL, (pShared->fsVKNext & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_NEXT_SHIFT, (pShared->fsVKNext & KC_SHIFT)? TRUE: FALSE );

    SettingsPopulateKeyList( hwnd, IDD_PREV_KEY );
    ListSelectDataItem( hwnd, IDD_PREV_KEY, MAKEULONG( pShared->usKeyPrev, pShared->fsVKPrev & KC_VIRTUALKEY ));
    WinCheckButton( hwnd, IDD_PREV_CTRL, (pShared->fsVKPrev & KC_CTRL)? TRUE: FALSE );
    WinCheckButton( hwnd, IDD_PREV_SHIFT, (pShared->fsVKPrev & KC_SHIFT)? TRUE: FALSE );
}


/* ------------------------------------------------------------------------- *
 * SettingsUpdateKeys                                                        *
 *                                                                           *
 * Update the active program hotkey settings based on the dialog contents.   *
 * ------------------------------------------------------------------------- */
BOOL SettingsUpdateKeys( HWND hwnd )
{
#define NUM_HOTKEYS 7

    USHORT fsKey[ NUM_HOTKEYS ] = {0},
           usKey[ NUM_HOTKEYS ] = {0},
           i, j;

    if ( CHKBOX_ISCHECKED( hwnd, IDD_INPUT_CTRL ))
        fsKey[ 0 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_INPUT_SHIFT ))
        fsKey[ 0 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_INPUT_KEY, &usKey[ 0 ], &fsKey[ 0 ] );

    if ( CHKBOX_ISCHECKED( hwnd, IDD_MODE_CTRL ))
        fsKey[ 1 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_MODE_SHIFT ))
        fsKey[ 1 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_MODE_KEY, &usKey[ 1 ], &fsKey[ 1 ] );

    if ( CHKBOX_ISCHECKED( hwnd, IDD_CLAUSE_CTRL ))
        fsKey[ 2 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_CLAUSE_SHIFT ))
        fsKey[ 2 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_CLAUSE_KEY, &usKey[ 2 ], &fsKey[ 2 ] );

    if ( CHKBOX_ISCHECKED( hwnd, IDD_CONVERT_CTRL ))
        fsKey[ 3 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_CONVERT_SHIFT ))
        fsKey[ 3 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_CONVERT_KEY, &usKey[ 3 ], &fsKey[ 3 ] );

    if ( CHKBOX_ISCHECKED( hwnd, IDD_ACCEPT_CTRL ))
        fsKey[ 4 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_ACCEPT_SHIFT ))
        fsKey[ 4 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_ACCEPT_KEY, &usKey[ 4 ], &fsKey[ 4 ] );

    if ( CHKBOX_ISCHECKED( hwnd, IDD_NEXT_CTRL ))
        fsKey[ 5 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_NEXT_SHIFT ))
        fsKey[ 5 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_NEXT_KEY, &usKey[ 5 ], &fsKey[ 5 ] );

    if ( CHKBOX_ISCHECKED( hwnd, IDD_PREV_CTRL ))
        fsKey[ 6 ] |= KC_CTRL;
    if ( CHKBOX_ISCHECKED( hwnd, IDD_PREV_SHIFT ))
        fsKey[ 6 ] |= KC_SHIFT;
    GetSelectedKey( hwnd, IDD_PREV_KEY, &usKey[ 6 ], &fsKey[ 6 ] );


    // Check for duplicates
    for ( i = 0; i < NUM_HOTKEYS; i++ ) {
        for ( j = 0; j < NUM_HOTKEYS; j++ ) {
            if ( i == j ) continue;
            if (( fsKey[ i ] == fsKey[ j ] ) && ( usKey[ i ] == usKey[ j ] )) {
                WinLoadString( global.hab, 0, global.ulLangBase + IDS_KEY_COLLISION,
                               US_RES_MAXZ, achErrText );
                ErrorPopup( hwnd, achErrText );
                return FALSE;
            }
        }
    }

    pShared->fsVKInput    = fsKey[ 0 ];
    pShared->usKeyInput   = usKey[ 0 ];
    pShared->fsVKMode     = fsKey[ 1 ];
    pShared->usKeyMode    = usKey[ 1 ];
    pShared->fsVKCJK      = fsKey[ 2 ];
    pShared->usKeyCJK     = usKey[ 2 ];
    pShared->fsVKConvert  = fsKey[ 3 ];
    pShared->usKeyConvert = usKey[ 3 ];
    pShared->fsVKAccept   = fsKey[ 4 ];
    pShared->usKeyAccept  = usKey[ 4 ];
    pShared->fsVKNext     = fsKey[ 5 ];
    pShared->usKeyNext    = usKey[ 5 ];
    pShared->fsVKPrev     = fsKey[ 6 ];
    pShared->usKeyPrev    = usKey[ 6 ];
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * Window procedure for settings dialog.                                     *
 * See OS/2 PM reference for a description of input and output.              *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY SettingsDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    UCHAR szFontPP[ FACESIZE + 4 ];
    SHORT sIdx;
    PSZ   psz;

    switch ( msg ) {
        case WM_INITDLG:
            SettingsDlgPopulate( hwnd );
            if ( global.szInputFont[0] ) {
                sprintf( szFontPP, "10.%s", global.szInputFont );
                WinSetPresParam( WinWindowFromID( hwnd, IDD_INPUT_FONT ),
                                 PP_FONTNAMESIZE, strlen(szFontPP)+1, szFontPP );
                psz = strchr( szFontPP, '.') + 1;
                WinSetDlgItemText( hwnd, IDD_INPUT_FONT, psz );
            }
            CentreWindow( hwnd, NULLHANDLE, SWP_SHOW );
            break;

        case WM_COMMAND:
            switch ( SHORT1FROMMP( mp1 )) {

                case IDD_FONT_SELECT:
                    if ( ! WinQueryPresParam( WinWindowFromID( hwnd, IDD_INPUT_FONT ),
                                              PP_FONTNAMESIZE, 0, NULL,
                                              sizeof(szFontPP), szFontPP, QPF_NOINHERIT ))
                        sprintf( szFontPP, "10.%.31s", DEFAULT_INPUT_FONT );
                    psz = strchr( szFontPP, '.') + 1;
                    if ( SelectFont( hwnd, psz,
                                     sizeof(szFontPP) - ( strlen(szFontPP) - strlen(psz) )))
                    {
                        WinSetPresParam( WinWindowFromID( hwnd, IDD_INPUT_FONT ),
                                         PP_FONTNAMESIZE, strlen(szFontPP)+1, szFontPP );
                        WinSetDlgItemText( hwnd, IDD_INPUT_FONT, psz );
                    }
                    return (MRESULT) FALSE;

                case DID_OK:
                    sIdx = (USHORT) WinSendDlgItemMsg( hwnd, IDD_STARTUP_MODE,
                                                       LM_QUERYSELECTION,
                                                       MPFROMSHORT( LIT_FIRST ), 0 );
                    if ( sIdx != LIT_NONE )
                        global.sDefMode = (SHORT)(LIST_GET_ITEMDATA( hwnd, IDD_STARTUP_MODE, sIdx ));

                    if( ! SettingsUpdateKeys( hwnd )) return (MRESULT) FALSE;

                    if ( WinQueryPresParam( WinWindowFromID( hwnd, IDD_INPUT_FONT ),
                                            PP_FONTNAMESIZE, 0, NULL,
                                            sizeof(szFontPP), szFontPP, QPF_NOINHERIT ))
                    {
                        psz = strchr( szFontPP, '.') + 1;
                        if ( !psz ) psz = szFontPP;
                        strncpy( global.szInputFont, psz, FACESIZE );
                    }
                    break;

            }
            break;

    }
    return WinDefDlgProc (hwnd, msg, mp1, mp2) ;
}


/* ------------------------------------------------------------------------- *
 * SelectFont                                                                *
 *                                                                           *
 * Pop up a font selection dialog.                                           *
 *                                                                           *
 * Parameters:                                                               *
 *   HWND   hwnd   : handle of the current window.                           *
 *   PSZ    pszFace: pointer to buffer containing current font name.         *
 *   USHORT cbBuf  : size of the buffer pointed to by 'pszFace'.             *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 * ------------------------------------------------------------------------- */
BOOL _Optlink SelectFont( HWND hwnd, PSZ pszFace, USHORT cbBuf )
{
    FONTDLG     fontdlg = {0};
    FONTMETRICS fm      = {0};
    LONG        lQuery  = 0;
    CHAR        szName[ FACESIZE ],
                szPreview[ FONT_PREVIEW_SIZE ] = "abcdABCD";
    HWND        hwndFD;
    HPS         hps;

    hps = WinGetPS( hwnd );
    strncpy( szName, pszFace, FACESIZE-1 );

    // Get the metrics of the current font (we'll want to know the weight class)
    lQuery = 1;
    GpiQueryFonts( hps, QF_PUBLIC, pszFace, &lQuery, sizeof(fm), &fm );

    // Get the preview string
    WinLoadString( global.hab, 0, global.ulLangBase + IDS_FONTDLG_SAMPLE_TEXT,
                   FONT_PREVIEW_SIZE, szPreview );

    // Set up the dialog.  We use a custom dialog template from the resources
    // - the resource ID is determined by the current UI language.
    fontdlg.cbSize         = sizeof( FONTDLG );
    fontdlg.hpsScreen      = hps;
    fontdlg.pszTitle       = NULL;
    fontdlg.pszPreview     = szPreview;
    fontdlg.pfnDlgProc     = NULL;
    fontdlg.pszFamilyname  = szName;
    fontdlg.usFamilyBufLen = sizeof( szName );
    fontdlg.fxPointSize    = ( fm.fsDefn & FM_DEFN_OUTLINE ) ?
                                MAKEFIXED( 10, 0 ) :
                                ( fm.sNominalPointSize / 10 );
    fontdlg.usWeight       = (USHORT) fm.usWeightClass;
    fontdlg.clrFore        = SYSCLR_WINDOWTEXT;
    fontdlg.clrBack        = SYSCLR_WINDOW;
    fontdlg.fl             = FNTS_CENTER | FNTS_CUSTOM;
    fontdlg.flStyle        = 0;
    fontdlg.flType         = ( fm.fsSelection & FM_SEL_ITALIC ) ? FTYPE_ITALIC : 0;
    fontdlg.usDlgId        = DLG_FONT_BASE + (( global.ulLangBase - 10000 ) / 100 );
    fontdlg.hMod           = NULLHANDLE;

    hwndFD = WinFontDlg( HWND_DESKTOP, hwnd, &fontdlg );
    WinReleasePS( hps );
    if ( !hwndFD ) {
        WinLoadString( global.hab, 0, global.ulLangBase + IDS_ERROR_FONTDLG,
                       US_RES_MAXZ, achErrText );
        ErrorPopup( hwnd, achErrText );
    }
    else if ( fontdlg.lReturn == DID_OK ) {
        strncpy( pszFace, fontdlg.fAttrs.szFacename, cbBuf-1 );
        return TRUE;
    }
    return FALSE;
}


/* ------------------------------------------------------------------------- *
 * SettingsSave                                                              *
 *                                                                           *
 * Saves various settings to the INI file.  Called on program exit.          *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd: Main window handle.                                          *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void SettingsSave( HWND hwnd )
{
    HINI   hIni;                        // handle of INI file
    CHAR   szIni[ CCHMAXPATH ]  = {0},  // full path of INI file
           szLang[ 6 ],                 // language (used as app name)
           szFont[ FACESIZE+4 ] = {0};  // font PP
    SWP    wp;                          // window size/position (as queried)
    POINTL ptl;                         // window position (as saved)
    LONG   lData;                       // INI data item


    LocateProfile( szIni );
    hIni = PrfOpenProfile( WinQueryAnchorBlock( hwnd ), szIni );

    switch ( pShared->fsMode & 0xF00 ) {
        case MODE_CN: strcpy( szLang, "zh_CN"); break;
        case MODE_TW: strcpy( szLang, "zh_TW"); break;
        case MODE_KR: strcpy( szLang, "ko_KR"); break;
        default:      strcpy( szLang, "ja_JP"); break;
    }

    // Save the window position
    if ( WinQueryWindowPos( global.hwndFrame, &wp )) {
        ptl.x = wp.x;
        ptl.y = wp.y;
        PrfWriteProfileData( hIni, PRF_APP_UI, PRF_KEY_UIPOS, &ptl, sizeof( ptl ));
    }

    // Save the UI font
    if ( WinQueryPresParam( hwnd, PP_FONTNAMESIZE, 0, NULL,
                            sizeof( szFont ), szFont, 0 ))
        PrfWriteProfileString( hIni, PRF_APP_UI, PRF_KEY_UIFONT, szFont );

    // Save the configuration settings
    PrfWriteProfileString( hIni, szLang, PRF_KEY_INPUTFONT, global.szInputFont );

    PrfWriteProfileData( hIni, szLang, PRF_KEY_STARTMODE,
                         &(global.sDefMode), sizeof( global.sDefMode ));

    lData = pShared->fsMode & 0xFF;
    PrfWriteProfileData( hIni, szLang, PRF_KEY_INPUTMODE,
                         &lData, sizeof( lData ));

    lData = MAKELONG( pShared->usKeyMode, pShared->fsVKMode );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_MODE,
                         &(lData), sizeof( lData ));

    lData = MAKELONG( pShared->usKeyInput, pShared->fsVKInput );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_INPUT,
                         &(lData), sizeof( lData ));

    lData = MAKELONG( pShared->usKeyCJK, pShared->fsVKCJK );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_CJK,
                         &(lData), sizeof( lData ));

    lData = MAKELONG( pShared->usKeyConvert, pShared->fsVKConvert );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_CONVERT,
                         &(lData), sizeof( lData ));

    lData = MAKELONG( pShared->usKeyAccept, pShared->fsVKAccept );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_ACCEPT,
                         &(lData), sizeof( lData ));

    lData = MAKELONG( pShared->usKeyNext, pShared->fsVKNext );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_NEXT,
                         &(lData), sizeof( lData ));

    lData = MAKELONG( pShared->usKeyPrev, pShared->fsVKPrev );
    PrfWriteProfileData( hIni, szLang, PRF_KEY_PREV,
                         &(lData), sizeof( lData ));

    PrfCloseProfile( hIni );
}

