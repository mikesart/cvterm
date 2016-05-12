/**************************************************************************
 *
 * Copyright (c) 2016 Michael Sartain <mikesart@fastmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/
#define _GNU_SOURCE
#include <stdint.h>
#include <limits.h>

#if defined( __APPLE__ )
#include <ncurses.h>
#else
#include <ncursesw/curses.h>
#endif

#include "vterm.h"
#include "termwin.h"
#include "clog.h"
#include "cvterm_utils.h"

/*
    http://stackoverflow.com/questions/18551558/how-to-use-terminal-color-palette-with-curses

    There are 256 colors (defined by the first 8 bits).
    The other bits are used for additional attributes, such as highlighting.
    Passing the number -1 as color falls back to the default background and foreground colors.
    The color pair 0 (mod 256) is fixed on (-1, -1).
    The colors 0 to 15 are the terminal palette colors.
*/

#define NCURSES_CHECK( _ret, _func, ... )                            \
    do                                                               \
    {                                                                \
        _ret = _func( __VA_ARGS__ );                                 \
        if ( _ret == ERR )                                           \
        {                                                            \
            clog_error( CLOG( 0 ), "%s failed: %d", #_func, errno ); \
            if ( is_debugger_attached() )                            \
                __debugbreak();                                      \
            exit( -1 );                                              \
        }                                                            \
    } while ( 0 )

#define MAX_ANSI_COLORS 256

struct termwin
{
    VTerm *vt;
    WINDOW *win;
    int numcolors;
    int resized;
    VTermRect damage_rect;
    int pairid_count;
    short pair_table[ MAX_ANSI_COLORS * MAX_ANSI_COLORS ];
    VTermColor ansi_colors[ MAX_ANSI_COLORS ];
    uint16_t vterm_color_hash[ 32768 ]; // 2^(5+5+5)
};

// http://dev.networkerror.org/utf8/
static const char g_utf8_horz[] = "\xe2\x94\x80";        // BOX_UTF8_HORZ
static const char g_utf8_vert[] = "\xe2\x94\x82";        // BOX_UTF8_VERT
static const char g_utf8_topleft[] = "\xe2\x94\x8c";     // BOX_UTF8_TOPLEFT
static const char g_utf8_topright[] = "\xe2\x94\x90";    // BOX_UTF8_TOPRIGHT
static const char g_utf8_bottomleft[] = "\xe2\x94\x94";  // BOX_UTF8_BOTTOMLEFT
static const char g_utf8_bottomright[] = "\xe2\x94\x98"; // BOX_UTF8_BOTTOMRIGHT

static void draw_borders( WINDOW *win )
{
    int i;
    int maxy = getmaxy( win );
    int maxx = getmaxx( win );

    mvwprintw( win, 0, 0, g_utf8_topleft );
    mvwprintw( win, maxy - 1, 0, g_utf8_bottomleft );
    mvwprintw( win, 0, maxx - 1, g_utf8_topright );
    mvwprintw( win, maxy - 1, maxx - 1, g_utf8_bottomright );

    for ( i = 1; i < ( maxy - 1 ); i++ )
    {
        mvwprintw( win, i, 0, g_utf8_vert );
        mvwprintw( win, i, maxx - 1, g_utf8_vert );
    }

    for ( i = 1; i < ( maxx - 1 ); i++ )
    {
        mvwprintw( win, 0, i, g_utf8_horz );
        mvwprintw( win, maxy - 1, i, g_utf8_horz );
    }
}

termwin *termwin_init( const char *nc_term )
{
    int ret;
    int maxx, maxy;
    WINDOW *win = NULL;

    if ( nc_term && setenv( "TERM", nc_term, 1 ) )
        FATAL_ERROR( setenv );

    initscr();

    if ( !has_colors() )
    {
        clog_error( CLOG( 0 ), "has_colors failed: %d", errno );
        return NULL;
    }

    NCURSES_CHECK( ret, start_color );
    NCURSES_CHECK( ret, use_default_colors );

    NCURSES_CHECK( ret, raw );
    NCURSES_CHECK( ret, noecho );
    NCURSES_CHECK( ret, nonl );

    maxy = getmaxy( stdscr );
    maxx = getmaxx( stdscr );
    win = newwin( maxy - 10, maxx - 10, 5, 5 );

    NCURSES_CHECK( ret, nodelay, stdscr, true );
    NCURSES_CHECK( ret, keypad, stdscr, false );
    NCURSES_CHECK( ret, nodelay, win, true );
    NCURSES_CHECK( ret, keypad, win, false );

    termwin *twin = ( termwin * )malloc( sizeof( *twin ) );
    twin->win = win;
    twin->vt = NULL;
    twin->numcolors = 0;
    twin->resized = 0;

    memset( &twin->damage_rect, 0, sizeof( twin->damage_rect ) );
    memset( twin->ansi_colors, 0, sizeof( twin->ansi_colors ) );
    memset( twin->pair_table, 0xff, sizeof( twin->pair_table ) );
    memset( twin->vterm_color_hash, 0xff, sizeof( twin->vterm_color_hash ) );

    // First pairid is set by ncurses.
    twin->pairid_count = 1;
    twin->pair_table[ 0 ] = 0;

    return twin;
}

void termwin_free( termwin *twin )
{
    if ( twin )
    {
        int ret;

        NCURSES_CHECK( ret, delwin, twin->win );
        twin->win = NULL;
        twin->vt = NULL;

        NCURSES_CHECK( ret, endwin );

        free( twin );
    }
}

static int vterm_color_equal( const VTermColor *a, const VTermColor *b )
{
    return ( a->red == b->red ) &&
           ( a->green == b->green ) &&
           ( a->blue == b->blue );
}

static int vterm_color_distance( const VTermColor *a, const VTermColor *b )
{
    int red = a->red - b->red;
    int green = a->green - b->green;
    int blue = a->blue - b->blue;

    return red * red + green * green + blue * blue;
}

// Get index into vterm_color_hash array.
static int vterm_color_hashid( VTermColor *color )
{
    // We're using the high five bits for each color channel.
    int hashid = ( ( color->red >> 3 ) << 10 ) |
                 ( ( color->green >> 3 ) << 5 ) |
                 ( color->blue >> 3 );

    return hashid & 0x7fff;
}

static int get_ncurses_colorid( termwin *twin, VTermColor *color )
{
    int hashid = vterm_color_hashid( color );

    if ( twin->vterm_color_hash[ hashid ] == 0xffff )
    {
        int i;
        int idx = 0;
        int distance = INT_MAX;

        for ( i = 0; i < twin->numcolors; i++ )
        {
            if ( vterm_color_equal( &twin->ansi_colors[ i ], color ) )
            {
                idx = i;
                break;
            }

            int d = vterm_color_distance( &twin->ansi_colors[ i ], color );
            if ( d < distance )
            {
                distance = d;
                idx = i;
            }
        }

        twin->vterm_color_hash[ hashid ] = idx;
    }

    return twin->vterm_color_hash[ hashid ];
}

static int get_ncurses_pairid( termwin *twin, int fgid, int bgid )
{
    int pairidx = ( fgid << 8 ) + bgid;

    if ( twin->pair_table[ pairidx ] == -1 )
    {
        int ret;

        NCURSES_CHECK( ret, init_pair, twin->pairid_count, fgid, bgid );

        twin->pair_table[ pairidx ] = twin->pairid_count++;
    }

    return twin->pair_table[ pairidx ];
}

void termwin_setvterm( termwin *twin, VTerm *vterm )
{
    int i;
    int ret;
    int fgid, bgid;
    VTermState *state = vterm_obtain_state( vterm );

    twin->vt = vterm;

    twin->numcolors = sqrt_uint32( COLOR_PAIRS );
    if ( twin->numcolors > COLORS )
        twin->numcolors = COLORS;

    clog_info( CLOG( 0 ), "COLORS:%d COLOR_PAIRS:%d numcolors:%d\n",
               COLORS, COLOR_PAIRS, twin->numcolors );

    if ( twin->numcolors > MAX_ANSI_COLORS )
        twin->numcolors = MAX_ANSI_COLORS;

    for ( i = 0; i < twin->numcolors; i++ )
        vterm_state_get_palette_color( state, i, &twin->ansi_colors[ i ] );

    if ( can_change_color() )
    {
        for ( i = 16; i < twin->numcolors; i++ )
        {
            short r = ( twin->ansi_colors[ i ].red * 1000 ) / 255;
            short g = ( twin->ansi_colors[ i ].green * 1000 ) / 255;
            short b = ( twin->ansi_colors[ i ].blue * 1000 ) / 255;

            ret = init_color( i, r, g, b );
            if ( ret == ERR )
            {
                clog_warn( CLOG( 0 ), "init_color( %d, %d, %d, %d ) failed: %d", i, r, g, b, errno );
                break;
            }
        }
    }

    for ( i = 16; i < twin->numcolors; i++ )
    {
        short r, g, b;

        NCURSES_CHECK( ret, color_content, i, &r, &g, &b );

        twin->ansi_colors[ i ].red = r * 255 / 1000;
        twin->ansi_colors[ i ].green = g * 255 / 1000;
        twin->ansi_colors[ i ].blue = b * 255 / 1000;
    }

    for ( bgid = 0; bgid < twin->numcolors; bgid++ )
    {
        for ( fgid = 0; fgid < twin->numcolors; fgid++ )
        {
            get_ncurses_pairid( twin, fgid, bgid );
        }
    }

    const VTermColor default_color = { 0, 0, 0 };
    vterm_state_set_default_colors( state, &default_color, &default_color );
}

int termwin_getch( termwin *twin, int *ch )
{
    *ch = wgetch( twin->win );

    // Kill terminal resize events.
    while ( *ch == KEY_RESIZE )
        *ch = wgetch( twin->win );

    if ( *ch == ERR )
    {
        clog_warn( CLOG( 0 ), "wgetch failed: %d", errno );
        return -1;
    }

    return 0;
}

static void termwin_drawcell( termwin *twin, VTermScreen *vts, int row, int col )
{
    int ret;
    cchar_t cch;
    const wchar_t *wch;
    VTermScreenCell cell;
    VTermPos pos = { row, col };
    static const wchar_t s_blankchar[] = L" ";

    vterm_screen_get_cell( vts, pos, &cell );

    attr_t attr = A_NORMAL;
    if ( cell.attrs.bold )
        attr |= A_BOLD;
    if ( cell.attrs.underline )
        attr |= A_UNDERLINE;
    if ( cell.attrs.blink )
        attr |= A_BLINK;
    if ( cell.attrs.reverse )
        attr |= A_REVERSE;

    int fgid = get_ncurses_colorid( twin, &cell.fg );
    int bgid = get_ncurses_colorid( twin, &cell.bg );
    int pairid = get_ncurses_pairid( twin, fgid, bgid );

    wch = cell.chars[ 0 ] ? ( wchar_t * )&cell.chars[ 0 ] : s_blankchar;

    NCURSES_CHECK( ret, setcchar, &cch, wch, attr, pairid, NULL );

    NCURSES_CHECK( ret, wmove, twin->win, row + 1, col + 1 );

    NCURSES_CHECK( ret, wadd_wch, twin->win, &cch );
}

int termwin_damage_callback( VTermRect rect, void *user )
{
    termwin *twin = ( termwin * )user;

    if ( twin->damage_rect.end_col || twin->damage_rect.end_row )
    {
        twin->damage_rect.start_col = MIN( twin->damage_rect.start_col, rect.start_col );
        twin->damage_rect.start_row = MIN( twin->damage_rect.start_row, rect.start_row );
        twin->damage_rect.end_col = MAX( twin->damage_rect.end_col, rect.end_col );
        twin->damage_rect.end_row = MAX( twin->damage_rect.end_row, rect.end_row );
    }
    else
    {
        twin->damage_rect = rect;
    }
    return 1;
}

static int termwin_draw( termwin *twin )
{
    if ( twin->damage_rect.end_col || twin->damage_rect.end_row )
    {
        int ret;
        int row, col;
        int y = getcury( twin->win );
        int x = getcurx( twin->win );
        int maxy = getmaxy( twin->win ) - 2;
        int maxx = getmaxx( twin->win ) - 2;
        int endrow = MIN( maxy, twin->damage_rect.end_row );
        int endcol = MIN( maxx, twin->damage_rect.end_col );
        VTermScreen *vts = vterm_obtain_screen( twin->vt );

        if ( ( twin->damage_rect.start_row == 0 ) ||
             ( twin->damage_rect.start_col == 0 ) ||
             ( endrow > maxy ) ||
             ( endcol > maxx ) )
        {
            draw_borders( twin->win );
        }

        for ( row = twin->damage_rect.start_row; row < endrow; row++ )
        {
            for ( col = twin->damage_rect.start_col; col < endcol; col++ )
            {
                termwin_drawcell( twin, vts, row, col );
            }
        }

        NCURSES_CHECK( ret, wmove, twin->win, y, x );

        memset( &twin->damage_rect, 0, sizeof( twin->damage_rect ) );

        return 1;
    }

    return 0;
}

void termwin_refresh( termwin *twin )
{
    int ret;

    if ( termwin_draw( twin ) )
    {
        if ( twin->resized )
        {
            refresh();
            twin->resized = 0;
        }

        NCURSES_CHECK( ret, wrefresh, twin->win );
    }
}

int termwin_movecursor_callback( VTermPos pos, VTermPos oldpos, int visible, void *user )
{
    int ret;
    termwin *twin = ( termwin * )user;
    int maxy = getmaxy( twin->win ) - 2;
    int maxx = getmaxx( twin->win ) - 2;

    if ( pos.row >= maxy || pos.col >= maxx )
    {
        clog_warn( CLOG( 0 ), "bad pos: %d/%d %d/%d", pos.row, pos.col, maxy, maxx );
        return 1;
    }

    NCURSES_CHECK( ret, wmove, twin->win, pos.row + 1, pos.col + 1 );
    return 1;
}

int termwin_bell_callback( void *user )
{
    int ret;

    NCURSES_CHECK( ret, beep );
    return 1;
}

int termwin_settermprop_callback( VTermProp prop, VTermValue *val, void *user )
{
    switch ( prop )
    {
    case VTERM_PROP_CURSORVISIBLE:
        clog_info( CLOG( 0 ), "VTERM_PROP_CURSORVISIBLE:%d", val->boolean );
        curs_set( !!val->boolean );
        return 1;
    case VTERM_PROP_ALTSCREEN:
        clog_debug( CLOG( 0 ), "NYI PROP_ALTSCREEN NYI" );
        return 1;
    case VTERM_PROP_TITLE:
        clog_debug( CLOG( 0 ), "NYI PROP_TITLE: %s", val->string );
        return 1;
    case VTERM_PROP_MOUSE:
        clog_debug( CLOG( 0 ), "NYI PROP_MOUSE:%d", val->number );
        return 1;
    case VTERM_PROP_CURSORBLINK:
    case VTERM_PROP_ICONNAME:
    case VTERM_PROP_REVERSE:
    case VTERM_PROP_CURSORSHAPE:
    default:
        clog_debug( CLOG( 0 ), "NYI prop:%d", prop );
        return 0;
    }
}

void termwin_getsize( termwin *twin, int *rows, int *cols )
{
    *rows = getmaxy( twin->win ) - 2;
    *cols = getmaxx( twin->win ) - 2;
}

void termwin_resize( termwin *twin )
{
    int ret;
    int maxy = getmaxy( stdscr );
    int maxx = getmaxx( stdscr );

    NCURSES_CHECK( ret, wresize, twin->win, maxy - 10, maxx - 10 );

    // Damage entire window.
    twin->damage_rect.start_row = 0;
    twin->damage_rect.start_col = 0;
    twin->damage_rect.end_row = maxy;
    twin->damage_rect.end_col = maxx;

    twin->resized = 1;
}
