/****************************************************************************
 * wnnim.c                                                                  *
 *                                                                          *
 * NOTE: This module must be compiled with _cdecl linkage (with ICC: /Mc).  *
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
#define INCL_WINATOM
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uconv.h>
#include <wnn/jllib.h>
#include <wnn/rk_spclval.h>

#include "codepage.h"
#include "wnnhook.h"
#include "wnnim.h"
#include "wnnclient.h"


// Romkan API functions exported by WNN library (just the ones we actually use).
//

extern void     _cdecl romkan_set_lang( char * );
extern int      _cdecl romkan_init( char*, letter, char, letter (* _cdecl keyinfn)(),int (* _cdecl bytcntfn)() );
extern letter   _cdecl romkan_getc( void );
extern letter * _cdecl romkan_henkan( letter );
extern void     _cdecl romkan_clear( void );



// JLIB and other API functions exported by WNN library.  (Again, this is not
// comprehensive, just the ones we need.)
//
extern struct wnn_buf * _cdecl jl_open_lang( const char *, const char *, const char *, const char *, int (* _cdecl)(const char *), int (* _cdecl)(const char *), int );
extern int              _cdecl jl_isconnect_e( register struct wnn_env *env );
extern struct wnn_env * _cdecl jl_env_get( register struct wnn_buf *buf );
extern int              _cdecl jl_fuzokugo_get_e( register struct wnn_env *env, char *fname );
extern int              _cdecl jl_set_env_wnnrc( register struct wnn_env *, const char *, int (* _cdecl)(const char *), int (* _cdecl)(const char *) );
extern int              _cdecl jl_fuzokugo_set_e( struct wnn_env *env, char *fname );
extern void             _cdecl jl_close( register struct wnn_buf *buf );
extern int              _cdecl jl_dic_list_e( struct wnn_env *env, WNN_DIC_INFO **dicinfo );
extern int              _cdecl jl_kanji_len( struct wnn_buf *buf, register int bun_no, register int bun_no2 );
extern int              _cdecl jl_kill( struct wnn_buf *buf, register int bun_no, register int bun_no2 );
extern int              _cdecl jl_ren_conv( register struct wnn_buf *buf, register w_char *yomi, int bun_no, int bun_no2, int use_maep );
extern int              _cdecl jl_tan_conv( register struct wnn_buf *buf, w_char *yomi, register int bun_no, register int bun_no2, int use_maep, int ich_shop );
extern int              _cdecl jl_yomi_len( struct wnn_buf *buf, register int bun_no, register int bun_no2 );
extern int              _cdecl jl_zenkouho( register struct wnn_buf *buf, int bun_no, int use_maep, int uniq_level );
extern int              _cdecl jl_zenkouho_dai( register struct wnn_buf *buf, int bun_no, int bun_no2, int use_maep, int uniq_level );
extern int              _cdecl wnn_get_area( struct wnn_buf *buf, register int bun_no, register int bun_no2, w_char *area, int kanjip );


// --------------------------------------------------------------------------

extern IMCLIENTDATA global;

char   WnnErrorBuf[ 128 ] = {0};
BOOL   fInitRK            = FALSE;
BOOL   fInitCJK           = FALSE;
USHORT usCharIdx;           // index of next input character to be converted

UconvObject uconvEUC = NULL;


// ============================================================================
// CALLBACK FUNCTIONS
// ============================================================================

/* ------------------------------------------------------------------------- *
 * NextCharacter                                                             *
 *                                                                           *
 * Callback function registered with romkan_init(): returns the next input   *
 * byte for conversion.  The FreeWnn romkan_next() function (which is called *
 * internally by romkan_getc()) uses this to retrieve each byte.             *
 *                                                                           *
 * Since we don't use romkan_getc(), this is presumably unused in practice.  *
 * ------------------------------------------------------------------------- */
letter _cdecl NextCharacter()
{
    letter ltr;
    if ( usCharIdx >= sizeof( global.szRomaji ))
        ltr = EOLTTR;
     else
        ltr = global.szRomaji[ usCharIdx ] ?
                  (letter)( global.szRomaji[ usCharIdx++ ]) :
                  EOLTTR;
    return ltr;
}


/* ------------------------------------------------------------------------- *
 * CharacterByteCount                                                        *
 *                                                                           *
 * Callback function registered with romkan_init(): returns whether an       *
 * input character value is a single- or double-byte character for the       *
 * active input codepage.  Also used by romkan_next() via romkan_getc().     *
 *                                                                           *
 * Since we don't use romkan_getc(), this is presumably unused in practice.  *
 * ------------------------------------------------------------------------- */
int _cdecl CharacterByteCount( char *pChar )
{
    return 1;       // We only pass ASCII characters as input anyway, so...
}


/* ------------------------------------------------------------------------- *
 * ErrorFunc                                                                 *
 *                                                                           *
 * Callback function registered with jl_open_lang(): used to output error    *
 * messages.                                                                 *
 * ------------------------------------------------------------------------- */
int _cdecl ErrorFunc( const char *pcsz )
{
    if ( pcsz && *pcsz )
        strncpy( WnnErrorBuf, pcsz, sizeof( WnnErrorBuf ));
    else
        WnnErrorBuf[ 0 ] = 0;
    return 1;
}



// ============================================================================
// OTHER FUNCTIONS
// ============================================================================

/* ------------------------------------------------------------------------- *
 * InitConversionMethod                                                      *
 *                                                                           *
 * Initialize the CJK conversion engine.  In this implementation (FreeWnn),  *
 * this involves connecting to the FreeWnn server and initializing the       *
 * associated environment.                                                   *
 *                                                                           *
 * ------------------------------------------------------------------------- */
INT _Optlink InitConversionMethod( USHORT usLang, PVOID *ppSession )
{
    struct wnn_env *wnnenv = NULL;  // Wnn environment object
    struct wnn_buf *bdata = NULL;   // Wnn session buffer

    PSZ     pszEnv,                 // Return pointer for getenv()
            pszServer,              // Host address of jserver
            pszUser;                // User/environment name to use on the server
    CHAR    fzk[ 1024 ] = {0};
    INT     result = 0;

    if ( !fInitRK ) return 1;
    if ( uconvEUC == NULL ) return 2;

    // Get the server and user names to use.
    pszEnv = getenv("JSERVER");
    pszServer = strdup( pszEnv? pszEnv: "localhost");
    pszEnv = getenv("USER");
    pszUser = strdup( pszEnv? pszEnv: "root");

    // Connect to the server.
    bdata = jl_open_lang( pszUser, pszServer, "ja_JP", NULL, *ErrorFunc, *ErrorFunc, 0 );
    if (( bdata == NULL ) || ( jl_isconnect( bdata ) == 0 )) {
        if ( WnnErrorBuf[0] )
            strncpy( global.szEngineError, WnnErrorBuf, sizeof( global.szEngineError ) - 1 );
        result = 1;
        goto done_connect;
    }

    if ( jl_fuzokugo_get( bdata, fzk ) == -1 ) {
        //  Environment isn't active on server, so initialize it now.
        wnnenv = jl_env_get( bdata );
        jl_set_env_wnnrc( wnnenv, "wnnenvrc", (int *) WNN_CREATE, NULL );
    }

    *ppSession = bdata;
    fInitCJK = TRUE;

done_connect:
    free( pszServer );
    free( pszUser );
    return result;
}


/* ------------------------------------------------------------------------- *
 * InitInputMethod                                                           *
 *                                                                           *
 * Initialize the phonetic input conversion engine.  In this implementation  *
 * (FreeWnn romkan), this involves loading the romkan table files.           *
 *                                                                           *
 * ------------------------------------------------------------------------- */
INT _Optlink InitInputMethod( PSZ pszPath, USHORT usLang )
{
    USHORT cpEUC;
    CHAR   szLang[ 6 ];
    CHAR   szModeHyo[ CCHMAXPATH ];
    int    rc;


    switch ( usLang ) {
        case MODE_CN: strcpy( szLang, "zh_CN"); break;
        case MODE_TW: strcpy( szLang, "zh_TW"); break;
        case MODE_KR: strcpy( szLang, "ko_KR"); break;
        default:      strcpy( szLang, "ja_JP"); break;
    }

    if ( pszPath == NULL ) {
        pszPath = getenv("ROMKAN_TABLE");
        if ( pszPath )
            strncpy( szModeHyo, pszPath, CCHMAXPATH - 1 );
        else
            sprintf( szModeHyo, "/@unixroot/usr/lib/Wnn/%s/rk/mode", szLang );
        pszPath = szModeHyo;
    }

    romkan_set_lang( szLang );     // This may not actually be needed (?)

    // romkan_init() parameters:
    //   pszPath                Filespec of the main 'mode' table
    //   0x08                   Value of the 'delete' character code
    //   *NextCharacter         Pointer to character-read function
    //   *CharacterByteCount    Pointer to character byte-count function

    rc = romkan_init( pszPath, 0x08, 0, *NextCharacter, *CharacterByteCount );

    if ( rc )
        sprintf( global.szEngineError, "Failed to initialize romkan (Wnn) library (error %u)", rc );
    fInitRK = ( rc == 0 )? TRUE: FALSE;

    if ( fInitRK && ( uconvEUC == NULL )) {
        cpEUC = GetEucCodepage( usLang );
        rc = CreateUconvObject( cpEUC, &uconvEUC );
        if ( rc )
            sprintf( global.szEngineError, "Failed to create conversion object for codeoage %u (error %u). The OS/2 codepage file might not be installed.", cpEUC, rc );
    }

    return rc;
}


/* ------------------------------------------------------------------------- *
 * ConvertPhonetic                                                           *
 *                                                                           *
 * Convert the input ('romaji') buffer into phonetic characters ('kana') for *
 * the current language.  (We use the term 'kana', which is Japanese, but    *
 * this also applies to Korean Hangul.)                                      *
 *                                                                           *
 * Note that the converted result may consist of several bytes, possibly     *
 * even more than two in the case of composite characters.                   *
 *                                                                           *
 * ------------------------------------------------------------------------- */
BYTE _Optlink ConvertPhonetic( void )
{
    CHAR   szOutput[ 8 ];
    USHORT i, j,
           len;
    BYTE   result;
    letter ltr;
    letter *converted, *c;

//FILE *f;


    if ( !fInitRK ) return KANA_INVALID;

//f = fopen("debug.log", "a");

    i = 0;
    ltr = 0;
    result = KANA_INVALID;
    len = strlen( global.szRomaji );

    memset( szOutput, 0, sizeof( szOutput ));

#if 0       // Using romkan_getc() seems to be problematic due to its internal loop logic
    usCharIdx = 0;
    do {
        if ( usCharIdx > len ) break;
        if ( i >= ( sizeof( global.szKana ) -  1 )) break;

        prev = ltr;
        ltr = romkan_getc();

        if ( ltr == LTREOF )
            break;           // end of input, exit with the last status result
        else if ( ltr && is_HON( ltr )) {
            if ( result == KANA_PENDING && ( ltr == (prev & 0xFF ))) continue;
            szOutput[ i++ ] = (CHAR)(ltr & 0xFF);
            result = KANA_COMPLETE;
        }
        else if ( ltr == 0x80000008 ) {     // 'delete' character
            result = KANA_INVALID;
            break;
        }
        else if ( ltr & 0x80000000 )
            result = KANA_PENDING;
        else // if ( ltr == NISEBP )
            break;          // error, exit with the last status result

        // TODO not sure how to identify KANA_CANDIDATE yet (only used for Korean)
    } while ( ltr );

#else       // We iterate the buffer ourselves and call romkan_henkan() directly

    for( j = 0; j < len; j++ ) {

//fprintf( f, "romaji: %s, start: %d, converted: %d\t", global.szRomaji, j, i );

        ltr = (letter)(global.szRomaji[ j ]);
        if ( ltr == 0 ) {
            converted = NULL;
            ltr = LTREOF;
        }
        else {
            converted = romkan_henkan( ltr & 0xFF );
            c = converted;
            while (( *c != EOLTTR ) && i < sizeof( szOutput )) {
                ltr = *c;
                if ( is_HON( ltr )) {
//fprintf( f, "%02X ", ltr );
                    szOutput[ i++ ] = (CHAR)(ltr & 0xFF);
                    result = KANA_COMPLETE;
                }
                c++;
                if (( c - converted ) > 10 ) break;     // simple sanity check
            }
        }
//fprintf( f, " (%08X)\n", ltr );

        if ( ltr == NISEBP ) {
            result = KANA_INVALID;
            break;
        }
        else if ( ltr == EOLTTR || ltr == LTREOF )
            break;           // end of input, exit with the last status result
        else if ( ltr & 0x80000000 )
            result = KANA_PENDING;
        else
            break;          // exit with the last status result
        // TODO not sure how to identify KANA_CANDIDATE yet (only used for Korean)
    }

#endif

    // TODO convert szOutput to target codepage
    if ( i ) {
        StrConvert( szOutput, global.szKana, uconvEUC, global.uconvOut );
        //strncpy( global.szKana, szOutput, sizeof( global.szKana ) - 1 );
//fprintf( f, "Converted: %s\n", szOutput );
    }
    romkan_clear();

//fprintf( f, "Returning result: %d\n", result );

//fclose( f );

    return result;
}

