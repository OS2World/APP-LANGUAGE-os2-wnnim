/****************************************************************************
 * wnnim.c                                                                  *
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
#define INCL_DOSNLS
#define INCL_WIN
#define INCL_PM
#define INCL_GPI
#define INCL_DOSEXCEPTIONS
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <uconv.h>

#include "wnnhook.h"
#include "codepage.h"
#include "ids.h"
#include "wnnim.h"
#include "wnnclient.h"



IMCLIENTDATA global = {0};      // our window's global data

// Subclassed window procedures
PFNWP pfnBtnProc;
PFNWP pfnTxtProc;


// ==========================================================================
// IMPLEMENTATION
// ==========================================================================

/* ------------------------------------------------------------------------- *
 * ClearInputBuffer                                                          *
 *                                                                           *
 * Clear/reset the input conversion buffers.  This should always be done     *
 * after calling SendCharacter.  Effectively this resets the current input   *
 * conversion to the empty state.  Note that this does not affect the clause *
 * conversion buffer.                                                        *
 *                                                                           *
 * ------------------------------------------------------------------------- */
void ClearInputBuffer( void )
{
    memset( global.szRomaji, 0, sizeof( global.szRomaji ));
    memset( global.szKana, 0, sizeof( global.szKana ));
}


/* ------------------------------------------------------------------------- *
 * ClearClauseBuffer                                                         *
 *                                                                           *
 * Clear/reset the clause conversion buffer.                                 *
 *                                                                           *
 * ------------------------------------------------------------------------- */
void ClearClauseBuffer( void )
{
    if ( global.pszClause )
        free( global.pszClause );
    global.pszClause = (PSZ) calloc( CLAUSE_INCZ, sizeof( char ));
}


/* ------------------------------------------------------------------------- *
 * SendCharacter                                                             *
 *                                                                           *
 * Send the contents of the specified character buffer to the source window. *
 * Typically this will cause the character to be inserted at the current     *
 * position, but in practice it's up to the application to decide what to do *
 * with it.                                                                  *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwndSource: HWND of source (application) window.                   *
 *   PSZ  pszBuffer : Character buffer whose contents will be sent.          *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SendCharacter( HWND hwndSource, PSZ pszBuffer )
{
    USHORT i,
           usLen,
           usChar;
    CHAR   achClassName[ 100 ] = {0};
    BOOL   fWorkAround = FALSE;

    usLen = strlen( pszBuffer );
    for ( i = 0; i < usLen; i++ ) {
        usChar = (USHORT) pszBuffer[ i ];

        // Some custom input windows can't handle combined double bytes
        if ( WinQueryClassName( hwndSource, 100, achClassName ) > 0 ) {
            // MED (MrED) text editor
            if ( strcmp( achClassName, "MRED_BUFWIN_CLASS") == 0 )
                fWorkAround = TRUE;
        }

        if ( !fWorkAround && IsDBCSLeadByte( usChar, global.dbcs ))
            usChar |= pszBuffer[ ++i ] << 0x8;
        WinSendMsg( hwndSource, WM_CHAR,
                    MPFROMSH2CH( KC_CHAR, 1, 0 ),
                    MPFROM2SHORT( usChar, 0 ));

        // Force a window redraw if workaround was used
        if ( fWorkAround ) WinInvalidateRect( hwndSource, NULL, FALSE );
    }
}


/* ------------------------------------------------------------------------- *
 * SupplyCharacter                                                           *
 *                                                                           *
 * Output a converted phonetic character.  If CJK conversion is active, add  *
 * it to the clause buffer and display it in the overlay window.  (TODO: or  *
 * if it is a 'candidate' character, i.e. valid but potentially modifiable,  *
 * set the pending buffer and display it in the overlay window.)  Otherwise, *
 * send it directly to the target window, and clear all buffers.             *
 *                                                                           *
 * ------------------------------------------------------------------------- */
void SupplyCharacter( HWND hwnd, HWND hwndSource, BYTE bStatus )
{
    /*
    if ( pShared->fsMode & MODE_CJK ) {
        // TODO append to clause buffer
        // TODO update/display overlay
        ClearInputBuffer();
    }
    else if ( bStatus == KANA_CANDIDATE )
        strncpy( global.szPending, global.szKana, sizeof( global.szPending ));
        // TODO update/display overlay
    else
    */
    {
        SendCharacter( hwndSource, global.szKana );
        ClearInputBuffer();
    }
}


/* ------------------------------------------------------------------------- *
 * ProcessCharacter                                                          *
 *                                                                           *
 * Process an input character (byte) from the input hook and decide how to   *
 * deal with it.  If we made it this far, then we know that the IME is       *
 * enabled.  Here, we add the character to the romaji buffer, then call the  *
 * input conversion routine to see if it can be converted.                   *
 *                                                                           *
 * The romaji buffer accumulates characters until either a valid conversion  *
 * is possible, the maximum length is reached, or an illegal sequence is     *
 * reported.  It can be in one of five states:                               *
 *                                                                           *
 *  A. empty (not applicable if we've reached this point)                    *
 *  B. pending (contains a partial sequence which is potentially valid)      *
 *  C. complete (contains a valid romaji sequence ready for conversion)      *
 *  D. candidate (contains a valid romaji                                    *
 *  E. invalid (contains a sequence that is not nor could ever become valid) *
 *                                                                           *
 * A does not apply at this point.  In the case of B, we simply keep the     *
 * buffer and continue.  For the other three possibilities we call the       *
 * SupplyCharacter() function and let it decide how to proceed.              *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND   hwnd      : Our own window handle.                               *
 *   HWND   hwndSource: Handle of source (application) window.               *
 *   MPARAM mp1       : mp1 of original WM_CHAR message.                     *
 *   MPARAM mp2       : mp2 of original WM_CHAR message.                     *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void ProcessCharacter( HWND hwnd, HWND hwndSource, MPARAM mp1, MPARAM mp2 )
{
    UCHAR szChar[ 2 ];
    BYTE  bStatus;

    szChar[ 0 ] = (UCHAR) SHORT1FROMMP( mp2 );
    szChar[ 1 ] = 0;
    strncat( global.szRomaji, szChar, sizeof(global.szRomaji) - 1 );

#if 0
    // temp logic for testing
    if ( strlen( global.szRomaji ) > 1 ) {
        // temp for testing (0x82a0 is Japanese 'A' hiragana)
        global.szKana[ 0 ] = 0x82;
        global.szKana[ 1 ] = 0xA0;
        global.szKana[ 2 ] = 0;
        bStatus = KANA_COMPLETE;
    }
    else
        bStatus = KANA_PENDING;
#else
    bStatus = ConvertPhonetic();
#endif
    if ( bStatus != KANA_PENDING ) {
        SupplyCharacter( hwnd, hwndSource, bStatus );
    }
    // otherwise just keep the buffers and continue
}


/* ------------------------------------------------------------------------- *
 * CentreWindow                                                              *
 *                                                                           *
 * Centres one window relative to another (or to the screen).  The window    *
 * will always be placed on top of the z-order (HWND_TOP).                   *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND  hwndCentre  : the window to be centred                            *
 *   HWND  hwndRelative: the window relative to which hwndCentre will be     *
 *                       centred, or NULLHANDLE to centre on the screen      *
 *   ULONG flFlags     : additional flags for WinSetWindowPos (SWP_MOVE is   *
 *                       always assumed                                      *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void CentreWindow( HWND hwndCentre, HWND hwndRelative, ULONG flFlags )
{
    LONG x, y,       // x & y coordinates of hwndCentre (relative to hwndRelative)
         ox, oy,     // x & y offsets (i.e. coordinates of hwndRelative)
         rcx, rcy;   // width & height of hwndRelative
    SWP  wp;         // window-position structure


    if ( !hwndRelative ) {
        rcx = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
        rcy = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
        ox = 0;
        oy = 0;
    }
    else {
        if ( ! WinQueryWindowPos( hwndRelative, &wp )) return;
        rcx = wp.cx;
        rcy = wp.cy;
        ox = wp.x;
        oy = wp.y;
    }
    if ( WinQueryWindowPos( hwndCentre, &wp )) {
        x = ( rcx / 2 ) - ( wp.cx / 2 );
        y = ( rcy / 2 ) - ( wp.cy / 2 );
        WinSetWindowPos( hwndCentre, HWND_TOP,
                         x + ox, y + oy, wp.cx, wp.cy, SWP_MOVE | flFlags );
    }
}


/* ------------------------------------------------------------------------- *
 * Window procedure for 'About' (product info) dialog.                       *
 * See OS/2 PM reference for a description of input and output.              *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY AboutDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    CHAR achVersion[ MAX_VERSTRZ ];
    CHAR achBuf[ 256 ];

    switch ( msg ) {
        case WM_INITDLG:
            sprintf( achVersion, "Version %s", SZ_VERSION );
            WinSetDlgItemText( hwnd, IDD_VERSION, achVersion );
            sprintf( achBuf, "Copyright (C) %s Alexander Taylor.", SZ_COPYRIGHT );
            strncat( achBuf, "\r\n\nWnnIM for OS/2 is free software published under the terms of the GNU General Public License, version 2.  ", 255 );
            strncat( achBuf, "See the accompanying documentation for details.", 255 );
            WinSetDlgItemText( hwnd, IDD_NOTICES, achBuf );
            CentreWindow( hwnd, NULLHANDLE, SWP_SHOW );
            break;
    }
    return WinDefDlgProc (hwnd, msg, mp1, mp2) ;
}


/* ------------------------------------------------------------------------- *
 * Handle certain events in a subclassed control that we want to pass up to  *
 * that control's owner.  The main purpose of this is to make sure drag and  *
 * context-menu events on the control are handled by the dialog, rather than *
 * getting eaten by the control.                                             *
 *                                                                           *
 * Input/output are as per window procedure; see OS/2 PM reference.          *
 * ------------------------------------------------------------------------- */
MRESULT PassStdEvent( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    POINTL ptl;
    POINTS pts;
    HWND   hwndApp;

    ptl.x = SHORT1FROMMP( mp1 );
    ptl.y = SHORT2FROMMP( mp1 );
    hwndApp = WinQueryWindow( hwnd, QW_OWNER );
    WinMapWindowPoints( hwnd, hwndApp, &ptl, 1 );
    pts.x = (SHORT) ptl.x;
    pts.y = (SHORT) ptl.y;
    return (MRESULT) WinPostMsg( hwndApp, msg, MPFROM2SHORT(pts.x, pts.y), mp2 );
}


/* ------------------------------------------------------------------------- *
 * ButtonProc()                                                              *
 *                                                                           *
 * Subclassed window procedure for the button controls.                      *
 * See OS/2 PM reference for a description of input and output.              *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY ButtonProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg ) {
        // pass these messages up to the owner
        case WM_BEGINDRAG:
        case WM_CONTEXTMENU:
            return PassStdEvent( hwnd, msg, mp1, mp2 );
    }
    return (MRESULT) pfnBtnProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * StaticTextProc()                                                          *
 *                                                                           *
 * Subclassed window procedure for the static text control.                  *
 * See OS/2 PM reference for a description of input and output.              *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY StaticTextProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    ULONG id, cb, lClr;
    CHAR  szFont[ FACESIZE+4 ];

    switch( msg ) {
        // pass these messages up to the owner after converting the pointer pos
        case WM_BEGINDRAG:
        case WM_CONTEXTMENU:
            return PassStdEvent( hwnd, msg, mp1, mp2 );

        // pass colour and font changes up to the owner
        case WM_PRESPARAMCHANGED:
            id = (ULONG) mp1;
            switch ( id ) {
                case PP_BACKGROUNDCOLORINDEX:
                case PP_FOREGROUNDCOLORINDEX:
                case PP_BACKGROUNDCOLOR:
                case PP_FOREGROUNDCOLOR:
                    cb = WinQueryPresParam( hwnd, id, 0,
                                            NULL, sizeof( lClr ), &lClr, 0 );
                    if ( cb ) WinSetPresParam( WinQueryWindow( hwnd, QW_OWNER ),
                                               id, sizeof( lClr ), &lClr );
                    return 0;

                case PP_FONTNAMESIZE:
                    cb = WinQueryPresParam( hwnd, PP_FONTNAMESIZE, 0, NULL,
                                            sizeof( szFont ), szFont, 0 );
                    if ( cb ) WinSetPresParam( WinQueryWindow( hwnd, QW_OWNER ),
                                               id, strlen( szFont ) + 1, szFont );
                    return 0;
            }
            break;

    }
    return (MRESULT) pfnTxtProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * SetTopmost                                                                *
 *                                                                           *
 * Toggle the 'always on top' (float) setting of the window.                 *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SetTopmost( HWND hwnd )
{
    USHORT usState;
    ULONG  fl;

    if ( !global.hwndMenu ) return;
    usState = (USHORT) WinSendMsg( global.hwndMenu, MM_QUERYITEMATTR,
                                   MPFROM2SHORT( IDM_FLOAT, TRUE ),
                                   MPFROMSHORT( MIA_CHECKED ));
    if ( usState == MIA_CHECKED ) {
        fl = 0;
        WinCheckMenuItem( global.hwndMenu, IDM_FLOAT, FALSE );
    }
    else {
        fl = WS_TOPMOST;
        WinCheckMenuItem( global.hwndMenu, IDM_FLOAT, TRUE );
    }
    WinSetWindowBits( global.hwndFrame, QWL_STYLE, fl, WS_TOPMOST );
}


/* ------------------------------------------------------------------------- *
 * SizeWindow                                                                *
 *                                                                           *
 * Set the size of the window and its various controls.  (The window is not  *
 * directly resizable but we set the size dynamically based on the font.)    *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SizeWindow( HWND hwnd )
{
    FONTMETRICS fm;
    HPS         hps;
    ULONG       cxDesktop,      // width of desktop
                cxBorder,       // width of window border
                cyBorder,       // height of window border
                cxWin,          // width of our window
                cyWin,          // height of our window
                cxCtrl,         // width of current control
                xPos;           // position of current control

    cxDesktop = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    cxBorder = 2;
    cyBorder = 2;

    hps = WinGetPS( hwnd );
    GpiQueryFontMetrics( hps, sizeof( FONTMETRICS ), &fm );
    WinReleasePS( hps );

    cyWin  = fm.lMaxBaselineExt + 10;
    cxCtrl = ( 2 * fm.lEmInc ) + 4;
    xPos = 0;
    WinSetWindowPos( WinWindowFromID( hwnd, IDD_MODE ), HWND_TOP,
                     xPos, 0, cxCtrl, cyWin, SWP_SIZE | SWP_MOVE );
    xPos += cxCtrl;
    WinSetWindowPos( WinWindowFromID( hwnd, IDD_KANJI ), HWND_TOP,
                     xPos, 0, cxCtrl, cyWin, SWP_SIZE | SWP_MOVE );
    xPos += cxCtrl + 5;
    cxCtrl = 10 * fm.lEmInc;
#ifdef TESTHOOK
    WinSetWindowPos( WinWindowFromID( hwnd, IDD_TESTINPUT ), HWND_TOP,
                     xPos, 2, cxCtrl, cyWin - 4, SWP_SIZE | SWP_MOVE );
    xPos += 5 + cxCtrl;
#endif
    WinSetWindowPos( WinWindowFromID( hwnd, IDD_STATUS ), HWND_TOP,
                     xPos, cyBorder, cxCtrl, cyWin - (2 * cyBorder), SWP_SIZE | SWP_MOVE );

    cxWin = xPos + cxCtrl + ( 2 * cxBorder );
    WinSetWindowPos( global.hwndFrame, 0, cxDesktop - cxWin, 0,
                     cxWin, cyWin, SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ACTIVATE );
}


/* ------------------------------------------------------------------------- *
 * SetupWindow                                                               *
 *                                                                           *
 * Perform initial setup of our program window.                              *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SetupWindow( HWND hwnd )
{
    ULONG flBtn = WS_VISIBLE | BS_PUSHBUTTON | BS_NOPOINTERFOCUS | BS_USERBUTTON;
    LONG  lClr;

    lClr = SYSCLR_DIALOGBACKGROUND;
    WinSetPresParam( hwnd, PP_BACKGROUNDCOLORINDEX, sizeof( lClr ), &lClr );
    lClr = SYSCLR_WINDOWTEXT;
    WinSetPresParam( hwnd, PP_FOREGROUNDCOLORINDEX, sizeof( lClr ), &lClr );

    WinSetPresParam( hwnd, PP_FONTNAMESIZE,
                     strlen(SZ_DEFAULTFONT)+1, (PVOID) SZ_DEFAULTFONT );

    WinCreateWindow( hwnd, WC_BUTTON, "M", flBtn, 0, 0, 0, 0,
                     hwnd, HWND_TOP, IDD_MODE, NULL, NULL );
    WinCreateWindow( hwnd, WC_BUTTON, "C", flBtn, 0, 0, 0, 0,
                     hwnd, HWND_TOP, IDD_KANJI, NULL, NULL );
#ifdef TESTHOOK
    WinCreateWindow( hwnd, WC_MLE, "", WS_VISIBLE,
                     0, 0, 0, 0,  hwnd, HWND_TOP, IDD_TESTINPUT, NULL, NULL );
#endif
    WinCreateWindow( hwnd, WC_STATIC, "FreeWnn", WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                     0, 0, 0, 0,  hwnd, HWND_TOP, IDD_STATUS, NULL, NULL );

    pfnBtnProc = WinSubclassWindow( WinWindowFromID(hwnd, IDD_MODE), (PFNWP) ButtonProc );
    pfnBtnProc = WinSubclassWindow( WinWindowFromID(hwnd, IDD_KANJI), (PFNWP) ButtonProc );
    pfnTxtProc = WinSubclassWindow( WinWindowFromID(hwnd, IDD_STATUS), (PFNWP) StaticTextProc );

    SizeWindow( hwnd );
    SetTopmost( global.hwndFrame );
}


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
    // Default hotkeys (should eventually be configurable)
    pShared->usKeyMode    = 0x20;
    pShared->fsVKMode     = KC_CTRL;

    pShared->usKeyCJK     = 0x60;
    pShared->fsVKCJK      = KC_ALT;

    pShared->usKeyConvert = ' ';
    pShared->fsVKConvert  = 0;

    pShared->usKeyAccept  = 0;
    pShared->fsVKAccept   = VK_NEWLINE;
}


/* ------------------------------------------------------------------------- *
 * Set the status text to show the current mode.                             *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void UpdateStatus( HWND hwnd )
{
    CHAR szText[ MAX_STATUSZ ] = {0};
//    CHAR szBtn[ 3 ] = {0};
    USHORT usLang = pShared->fsMode & 0xF00,
           usMode = pShared->fsMode & 0xFF;

    switch ( usLang ) {
        case MODE_JP:
            switch ( usMode ) {
                case MODE_HIRAGANA:
                    WinEnableControl( hwnd, IDD_KANJI, TRUE );
//                    szBtn[ 0 ] = 0x82;
//                    szBtn[ 1 ] = 0xA0;
                    strcpy( szText,
                            pShared->fsMode & MODE_CJK? "Hiragana - Kanji": "Hiragana");
                    break;
                case MODE_KATAKANA:
//                    szBtn[ 0 ] = 0x83;
//                    szBtn[ 1 ] = 0x41;
                    WinEnableControl( hwnd, IDD_KANJI, TRUE );
                    strcpy( szText,
                            pShared->fsMode & MODE_CJK? "Katakana - Kanji": "Katakana");
                    break;
                case MODE_HALFWIDTH:
//                    szBtn[ 0 ] = 0xB1;
//                    szBtn[ 1 ] = '_';
                    WinEnableControl( hwnd, IDD_KANJI, TRUE );
                    strcpy( szText,
                            pShared->fsMode & MODE_CJK? "Halfwidth - Kanji": "Halfwidth Katakana");
                    break;
                case MODE_FULLWIDTH:
//                    szBtn[ 0 ] = 0x82;
//                    szBtn[ 1 ] = 0x60;
                    // Fullwidth mode doesn't support CJK
                    WinEnableControl( hwnd, IDD_KANJI, FALSE );
                    strcpy( szText, "Fullwidth ASCII");
                    break;
                case MODE_NONE:
//                    szBtn[ 0 ] = 'A';
//                    szBtn[ 1 ] = '_';
                    // Neither does None, obviously
                    WinEnableControl( hwnd, IDD_KANJI, FALSE );
                    strcpy( szText, "No conversion");
                    break;
            }
            break;
    }
//    szBtn[ 2 ] = 0;
//    WinSetDlgItemText( hwnd, IDD_MODE, szBtn );
    WinSetDlgItemText( hwnd, IDD_STATUS, szText );
}


/* ------------------------------------------------------------------------- *
 * SetKanjiMode                                                              *
 *                                                                           *
 * Toggle kanji (CJK) conversion on or off.                                  *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SetKanjiMode( HWND hwnd )
{
//    CHAR szBtn[ 3 ] = {0};

    if ( pShared->fsMode & MODE_CJK ) {
        pShared->fsMode &= ~MODE_CJK;
//        szBtn[ 0 ] = 0x82;
//        szBtn[ 1 ] = 0x6A;
    }
    else {
        pShared->fsMode |= MODE_CJK;
//        szBtn[ 0 ] = 0x8A;
//        szBtn[ 1 ] = 0xBF;
    }
//    szBtn[ 2 ] = 0;
//    WinSetDlgItemText( hwnd, IDD_KANJI, szBtn );

    UpdateStatus( hwnd );
}


/* ------------------------------------------------------------------------- *
 * SetInputMode                                                              *
 *                                                                           *
 * Change the current (phonetic) input mode.                                 *
 *                                                                           *
 * PARAMETERS:                                                               *
 *   HWND hwnd: Our window handle.                                           *
 *                                                                           *
 * RETURNS: n/a                                                              *
 * ------------------------------------------------------------------------- */
void SetInputMode( HWND hwnd )
{
    BYTE bInputMode = pShared->fsMode & 0xFF;
    // temp: for now just toggle between mode 0/1
    if ( bInputMode )
        pShared->fsMode &= 0xFF00;          // temp: turn off all
    else
        pShared->fsMode |= MODE_HIRAGANA;   // temp: turn on mode 1
    UpdateStatus( hwnd );
}


/* ------------------------------------------------------------------------- *
 * ------------------------------------------------------------------------- */
void Draw3DBorder( HPS hps, RECTL rcl, BOOL fInset )
{
    POINTL ptl;

    GpiSetColor( hps, fInset? CLR_BLACK: SYSCLR_BUTTONDARK );
    ptl.x = rcl.xLeft;
    ptl.y = rcl.yBottom;
    GpiMove( hps, &ptl );
    ptl.y = rcl.yTop - 1;
    GpiLine( hps, &ptl );
    GpiMove( hps, &ptl );
    ptl.x = rcl.xRight - 1;
    GpiLine( hps, &ptl );
    GpiSetColor( hps, fInset? SYSCLR_BUTTONDARK: CLR_BLACK );
    ptl.x = rcl.xLeft;
    ptl.y = rcl.yBottom;
    GpiMove( hps, &ptl );
    ptl.x = rcl.xRight - 1;
    GpiLine( hps, &ptl );
    GpiMove( hps, &ptl );
    ptl.y = rcl.yTop - 2;
    GpiLine( hps, &ptl );
    GpiSetColor( hps, fInset? SYSCLR_BUTTONLIGHT: SYSCLR_BUTTONDARK );
    ptl.x = rcl.xLeft + 1;
    ptl.y = rcl.yBottom + 1;
    GpiMove( hps, &ptl );
    ptl.x = rcl.xRight - 2;
    GpiLine( hps, &ptl );
    GpiMove( hps, &ptl );
    ptl.y = rcl.yTop - 2;
    GpiLine( hps, &ptl );
    GpiSetColor( hps, fInset? SYSCLR_BUTTONDARK: SYSCLR_BUTTONLIGHT );
    ptl.x = rcl.xLeft + 1;
    ptl.y = rcl.yBottom + 2;
    GpiMove( hps, &ptl );
    ptl.y = rcl.yTop - 2;
    GpiLine( hps, &ptl );
    GpiMove( hps, &ptl );
    ptl.x = rcl.xRight - 2;
    GpiLine( hps, &ptl );
}


/* ------------------------------------------------------------------------- *
 * ------------------------------------------------------------------------- */
void PaintIMEButton( PUSERBUTTON pBtnData )
{
    FONTMETRICS fm;
    CHAR   szText[ MAX_BTN_LABELZ ] = {0};
    ULONG  cbPP,
           cbText;
    LONG   lClr,
           lStrW, lStrH;    // string dimensions
    POINTL ptl, aptl[ TXTBOX_COUNT ];
    RECTL  rcl;

    if ( !pBtnData ) return;
    if ( !WinQueryWindowRect( pBtnData->hwnd, &rcl )) return;

    // Draw the button background and border
    cbPP = WinQueryPresParam( pBtnData->hwnd, PP_BACKGROUNDCOLOR,
                              PP_BACKGROUNDCOLORINDEX, NULL,
                              sizeof( lClr ), &lClr, QPF_ID2COLORINDEX );
    if ( cbPP )
        GpiCreateLogColorTable( pBtnData->hps, 0, LCOLF_RGB, 0, 0, NULL );
    else
        lClr = GpiQueryRGBColor( pBtnData->hps, 0, SYSCLR_BUTTONMIDDLE );

    WinFillRect( pBtnData->hps, &rcl, lClr );
    Draw3DBorder( pBtnData->hps, rcl, (BOOL)(pBtnData->fsState & BDS_HILITED) );

    // Draw the text
    WinQueryWindowText( pBtnData->hwnd, MAX_BTN_LABELZ, szText );
    cbText = strlen( szText );
    GpiQueryFontMetrics( pBtnData->hps, sizeof( FONTMETRICS ), &fm );
    GpiQueryTextBox( pBtnData->hps, cbText, szText, TXTBOX_COUNT, aptl );
    lStrW = aptl[ TXTBOX_CONCAT ].x;
    lStrH = fm.lEmHeight - 1;

    if ( pBtnData->fsState & BDS_DISABLED )
        lClr  = GpiQueryRGBColor( pBtnData->hps, 0, SYSCLR_MENUDISABLEDTEXT );
    else {
        cbPP = WinQueryPresParam( pBtnData->hwnd, PP_FOREGROUNDCOLOR,
                                  PP_FOREGROUNDCOLORINDEX, NULL,
                                  sizeof( lClr ), &lClr, QPF_ID2COLORINDEX );
        if ( !cbPP )
            lClr = GpiQueryRGBColor( pBtnData->hps, 0, SYSCLR_WINDOWTEXT );
    }
    GpiSetColor( pBtnData->hps, SYSCLR_WINDOW );
    ptl.x = ( RECTL_WIDTH( rcl ) / 2 ) - ( lStrW / 2 );
    ptl.y = ( RECTL_HEIGHT( rcl ) / 2) - ( lStrH / 2 );
    GpiCharStringPosAt( pBtnData->hps, &ptl, &rcl, CHS_CLIP | CHS_LEAVEPOS, cbText, szText, NULL );
    GpiSetColor( pBtnData->hps, lClr );
    ptl.x--;
    ptl.y++;
    GpiCharStringPosAt( pBtnData->hps, &ptl, &rcl, CHS_CLIP | CHS_LEAVEPOS, cbText, szText, NULL );
}


/* ------------------------------------------------------------------------- *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY ClientWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    HWND   hwndFocus;
    POINTL ptlMouse;
    LONG   cb, lClr;
    RECTL  rcl;
    HPS    hps;

    switch ( msg ) {

        case WM_CREATE:
            global.hwndMenu = WinLoadMenu(HWND_OBJECT, 0, IDM_POPUP);
            return 0;

        case WM_BEGINDRAG:
            WinSetFocus( HWND_DESKTOP, hwnd );
            WinSendMsg( WinQueryWindow( hwnd, QW_PARENT ), WM_TRACKFRAME, MPFROMSHORT(TF_MOVE), MPVOID );
            break;

        case WM_CONTEXTMENU:
            if ( global.hwndMenu != NULLHANDLE ) {
                WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
                WinPopupMenu(HWND_DESKTOP, hwnd, global.hwndMenu, ptlMouse.x, ptlMouse.y, 0,
                                PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 | PU_KEYBOARD);
                return 0;
            }
            break;

        case WM_COMMAND:
            switch( COMMANDMSG(&msg)->cmd ) {

                case IDD_MODE:
                    SetInputMode( hwnd );
                    if ( global.hwndLast ) WinSetFocus( HWND_DESKTOP, global.hwndLast );
                    break;

                case ID_HOTKEY_MODE:
                    SetInputMode( hwnd );
                    break;

                case IDD_KANJI:
                    SetKanjiMode( hwnd );
                    if ( global.hwndLast ) WinSetFocus( HWND_DESKTOP, global.hwndLast );
                    break;

                case ID_HOTKEY_KANJI:
                    SetKanjiMode( hwnd );
                    break;

                case IDM_FLOAT:
                    SetTopmost( hwnd );
                    return 0;

                case IDM_CLOSE:
                    WinSendMsg( hwnd, WM_CLOSE, 0L, 0L );
                    return 0;

                case IDM_ABOUT:
                    WinDlgBox( HWND_DESKTOP, hwnd, AboutDlgProc, NULLHANDLE, DLG_ABOUT, NULL );
                    return 0;
            }
            break;

        case WM_CONTROL:
            switch ( SHORT2FROMMP( mp1 )) {
                case BN_PAINT:
                    PaintIMEButton( (PUSERBUTTON) mp2 );
                    break;
            }
            break;

        case WM_FOCUSCHANGE:
            // If our window got focus, remember the previously-active window
            hwndFocus = (HWND)mp1;
            if (( SHORT1FROMMP( mp2 ) == TRUE ) && ! WinIsChild( hwndFocus, global.hwndFrame )) {
                global.hwndLast = hwndFocus;
            }
            break;

        case WM_PAINT:
            hps = WinBeginPaint( hwnd, NULLHANDLE, NULL );
            WinQueryWindowRect( hwnd, &rcl );
            cb = WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR, PP_BACKGROUNDCOLORINDEX,
                                    NULL, sizeof( lClr ), &lClr, QPF_ID2COLORINDEX );
            if ( cb )
                GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
            else
                lClr = GpiQueryRGBColor( hps, 0, SYSCLR_DIALOGBACKGROUND );
            WinFillRect( hps, &rcl, lClr );
            Draw3DBorder( hps, rcl, FALSE );
            WinEndPaint( hps );
            return 0;

        case WM_PRESPARAMCHANGED:
            switch ( (ULONG) mp1 ) {
                case PP_FONTNAMESIZE:
                    SizeWindow( hwnd );
                    break;
                default: break;
            }
            break;

        case WM_SIZE:
            SizeWindow( hwnd );
            break;


        // Custom msgs
        //
        default:
            if ( msg == pShared->wmAddChar ) {          // Typed character
                ProcessCharacter( hwnd, pShared->hwndSource, mp1, mp2 );
                return 0;
            }
            else if ( msg == pShared->wmDelChar ) {     // Backspace
                // If overlay is active, delete the last character.  Otherwise, pass on to hwndSource
            }
            break;

    }

    return WinDefWindowProc (hwnd, msg, mp1, mp2);
}


/* ------------------------------------------------------------------------- *
 * Set the language setting.                                                 *
 * ------------------------------------------------------------------------- */
BOOL SetupDBCSLanguage( USHORT usLangMode )
{
    COUNTRYCODE cc = {0};
//    CHAR szModeHyo[ CCHMAXPATH ] = {0};
    INT rc;

    switch ( usLangMode ) {
        case MODE_JP:
            cc.country  = 81;       // Japan
            cc.codepage = 943;      // Japanese SJIS
//            strcpy( szModeHyo, "/@unixroot/usr/local/lib/wnn/ja_JP/rk/mode");   // temp
            break;

        case MODE_KR:
            cc.country  = 82;       // Korea
            cc.codepage = 949;      // Korean KS-Code
//            strcpy( szModeHyo, "/@unixroot/usr/local/lib/wnn/ko_KR/rk/mode");   // temp
            break;

        case MODE_CN:
            cc.country  = 86;       // China PRC
            cc.codepage = 1386;     // Chinese GBK
//            strcpy( szModeHyo, "/@unixroot/usr/local/lib/wnn/zh_CN/rk/mode");   // temp
            break;

        case MODE_TW:
            cc.country  = 88;       // Taiwan
            cc.codepage = 950;      // Chinese Big-5
//            strcpy( szModeHyo, "/@unixroot/usr/local/lib/wnn/zh_TW/rk/mode");   // temp
            break;
    }
    DosQueryDBCSEnv( sizeof( global.dbcs ), &cc, global.dbcs );
    global.codepage = cc.codepage;
    pShared->fsMode = usLangMode;   // no conversion, will set later

    if ( CreateUconvObject( cc.codepage, &(global.uconvOut) ) != ULS_SUCCESS ) {
        ErrorPopup("Failed to create conversion object for selected codepage.");
        return FALSE;
    }
    rc = InitInputMethod( NULL, usLangMode );
    if ( rc ) {
        ErrorPopup( global.szEngineError );
        return FALSE;
    }

    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * ------------------------------------------------------------------------- */
VOID APIENTRY ExeTrap()
{
    WnnHookTerm();
    DosExitList( EXLST_EXIT, (PFNEXITLIST) ExeTrap );
}


/* ------------------------------------------------------------------------- *
 * ------------------------------------------------------------------------- */
int main( int argc, char **argv )
{
    static PSZ clientClass = "WnnIM2";
    HAB   hab;
    HMQ   hmq;
    QMSG  qmsg;
    ULONG frameFlags = FCF_TASKLIST;
    HMODULE hm;
    CHAR    szErr[ 256 ];

    pShared = WnnGlobalData();
    ClearInputBuffer();

    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0 );

    DosExitList( EXLST_ADD, (PFNEXITLIST) ExeTrap );    // trap exceptions to ensure hooks released

    WinRegisterClass( hab, clientClass, ClientWndProc, CS_CLIPCHILDREN, 0 );
    global.hwndFrame = WinCreateStdWindow( HWND_DESKTOP, 0L, &frameFlags, clientClass,
                                           "FreeWnnIME", 0L, 0, ID_ICON, &global.hwndClient );
    SettingsInit( global.hwndClient );
    SetupWindow( global.hwndClient );
    SetupDBCSLanguage( MODE_JP );                                   // for now
    SetInputMode( global.hwndClient );

    // Now do our stuff
    DosLoadModule( szErr, sizeof(szErr), "wnnhook.dll", &hm );     // increment the DLL use counter for safety
    if ( WnnHookInit( global.hwndClient )) {
        while ( WinGetMsg( hab, &qmsg, NULLHANDLE, 0, 0 ))
            WinDispatchMsg( hab, &qmsg );
        WnnHookTerm();
    }
    if ( hm ) DosFreeModule( hm );

    WinDestroyWindow( global.hwndFrame );
    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    return 0;
}

