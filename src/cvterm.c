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
#include <locale.h>
#include <signal.h>

#include "vterm.h"
#include "pseudo.h"
#include "termwin.h"
#include "ya_getopt.h"
#include "clog.h"
#include "cvterm_utils.h"

static const VTermScreenCallbacks g_screen_cbs =
    {
      termwin_damage_callback,      // damage
      NULL,                         // moverect
      termwin_movecursor_callback,  // movecursor
      termwin_settermprop_callback, // settermprop
      termwin_bell_callback,        // bell
      NULL,                         // resize
      NULL,                         // sb_pushline
      NULL                          // sb_popline
    };

typedef struct cvterm_opts
{
    const char *env_term;
    const char *nc_term;
    const char *logfile;
    int wait_for_debugger;

    int argc;
    const char **argv;
    const char *argv_buf[ 2 ];
} cvterm_opts;

static VTerm *g_vterm = NULL;
static termwin *g_twin = NULL;
static int g_master_pty;
static struct sigaction g_winch_sigaction_old;

static void sigwinch( int how )
{
    sigset_t signal_set;

    if ( sigemptyset( &signal_set ) )
        FATAL_ERROR( sigemptyset );

    if ( sigaddset( &signal_set, SIGWINCH ) )
        FATAL_ERROR( sigaddset );

    if ( sigprocmask( how, &signal_set, NULL ) )
        FATAL_ERROR( sigprocmask );
}

static void sigwinch_handler( int signo )
{
    int rows, cols;

    if ( g_winch_sigaction_old.sa_handler )
        ( *g_winch_sigaction_old.sa_handler )( signo );

    vterm_screen_flush_damage( vterm_obtain_screen( g_vterm ) );

    termwin_resize( g_twin );
    termwin_getsize( g_twin, &rows, &cols );

    // Set pty size.
    const struct winsize size = { rows, cols, 0, 0 };
    if ( ioctl( g_master_pty, TIOCSWINSZ, &size ) != 0 )
        FATAL_ERROR( ioctl( TIOCSWINSZ ) );

    // Tell vterm our new size.
    vterm_set_size( g_vterm, rows, cols );
}

static void handle_input( VTerm *vt, int master )
{
    size_t buflen;
    char buf[ 8192 ];

    while ( vterm_output_get_buffer_remaining( vt ) > 0 )
    {
        int ch = termwin_getch( g_twin );
        if ( ch == -1 )
            break;

        vterm_keyboard_unichar( vt, ( uint32_t )ch, VTERM_MOD_NONE );
    }

    while ( ( buflen = vterm_output_get_buffer_current( vt ) ) > 0 )
    {
        buflen = MIN( buflen, sizeof( buf ) );
        buflen = vterm_output_read( vt, buf, buflen );

        ssize_t bytes_write = TEMP_FAILURE_RETRY( write( master, buf, buflen ) );
        if ( bytes_write != ( ssize_t )buflen )
            FATAL_ERROR( write );
    }
}

static int handle_output( VTerm *vt, int master )
{
    char buf[ 8192 ];

    for ( ;; )
    {
        ssize_t bytes_read = TEMP_FAILURE_RETRY( read( master, buf, sizeof( buf ) ) );

        // Check if master pty was closed.
        if ( !bytes_read )
            return -1;

        if ( bytes_read < 0 )
        {
            // EAGAIN: no data available.
            // EIO: last slave fd closed.
            if ( errno == EAGAIN )
                return 0;
            else if ( errno == EIO )
                return -1;
            else
                FATAL_ERROR( read );
        }

        vterm_input_write( vt, buf, bytes_read );
    }
}

static void main_loop( VTerm *vt, int master )
{
    struct sigaction winch_sigaction;

    // Set up window size change signal handler
    winch_sigaction.sa_handler = sigwinch_handler;
    sigemptyset( &winch_sigaction.sa_mask );
    winch_sigaction.sa_flags = 0;

    if ( sigaction( SIGWINCH, &winch_sigaction, &g_winch_sigaction_old ) )
        FATAL_ERROR( sigaction );

    for ( ;; )
    {
        int ret;
        fd_set fds;
        // Update every 20ms.
        struct timeval timeout = { 0, 20000 };

        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds );
        FD_SET( master, &fds );

        sigwinch( SIG_UNBLOCK );
        ret = select( master + 1, &fds, NULL, NULL, &timeout );
        sigwinch( SIG_BLOCK );

        if ( ret == -1 )
        {
            if ( errno == EINTR )
                continue;

            FATAL_ERROR( select );
        }

        if ( FD_ISSET( master, &fds ) )
        {
            if ( handle_output( vt, master ) )
                return;
        }

        if ( FD_ISSET( STDIN_FILENO, &fds ) )
        {
            handle_input( vt, master );
        }

        termwin_refresh( g_twin );
    }
}

static void cvterm_shutdown()
{
    if ( _clog_loggers[ 0 ] )
        clog_debug( CLOG( 0 ), "atexit function called." );

    if ( g_vterm )
    {
        vterm_free( g_vterm );
        g_vterm = NULL;
    }

    termwin_free( g_twin );
    g_twin = NULL;

    clog_free( 0 );
}

static void opts_print( cvterm_opts *opts )
{
    int i;

    printf( "Options:\n" );
    printf( "  TERM: %s\n", opts->env_term );
    printf( "  NCTERM: %s\n", opts->nc_term );
    printf( "  logfile: %s\n", opts->logfile );
    printf( "  wait_for_debugger: %d\n", opts->wait_for_debugger );

    printf( "  cmd: " );
    for ( i = 0; i < opts->argc; i++ )
        printf( "%s ", opts->argv[ i ] );
    printf( "\n\n" );
}

static void opts_usage( cvterm_opts *opts, int argc, char *argv[] )
{
    printf( "%s [options] [CMD...]\n\n", argv[ 0 ] );

    printf( "  -w --wait_for_debugger     Wait for debugger to attach.\n" );
    printf( "  -l --logfile FILE          Set logfile name.\n" );
    printf( "  -h --help                  Show this help.\n" );

    exit( 1 );
}

static int opts_parse_args( cvterm_opts *opts, int argc, char **argv )
{
    static const struct option long_options[] =
        {
          { "help", ya_no_argument, 0, 0 },
          { "wait_for_debugger", ya_no_argument, 0, 0 },
          { "logfile", ya_required_argument, 0, 0 },
          { 0, 0, 0, 0 }
        };
    const char *env_shell = getenv( "SHELL" );
    const char *env_term = getenv( "TERM" );
    const char *env_ncterm = getenv( "NCTERM" );

    opts->env_term = env_term;
    opts->nc_term = env_ncterm ? env_ncterm : env_term;
    opts->logfile = "cvterm.log";
    opts->wait_for_debugger = 0;

    opts->argv_buf[ 0 ] = env_shell ? env_shell : "/bin/sh";
    opts->argv_buf[ 1 ] = NULL;
    opts->argc = 1;
    opts->argv = opts->argv_buf;

    for ( ;; )
    {
        int option_index = 0;
        int c = ya_getopt_long( argc, argv, "l:wh?", long_options, &option_index );
        if ( c == -1 )
            break;

        switch ( c )
        {
        case 0:
            if ( !strcmp( long_options[ option_index ].name, "help" ) )
                return -1;
            else if ( !strcmp( long_options[ option_index ].name, "wait_for_debugger" ) )
                opts->wait_for_debugger = 1;
            else if ( !strcmp( long_options[ option_index ].name, "logfile" ) )
                opts->logfile = ya_optarg;
            else
            {
                fprintf( stderr, "ERROR: Unhandled option '--%s'.\n",
                         long_options[ option_index ].name );
                return -1;
            }
            break;

        case 'l':
            opts->logfile = ya_optarg;
            break;

        case 'w':
            opts->wait_for_debugger = 1;
            break;

        case 'h':
        case '?':
            return -1;

        default:
            fprintf( stderr, "ERROR: getopt returned character code 0%o?\n", c );
            return -1;
        }
    }

    if ( ya_optind < argc )
    {
        opts->argc = argc - ya_optind;
        opts->argv = ( const char ** )&argv[ ya_optind ];
    }

    if ( !opts->logfile || !opts->logfile[ 0 ] )
        opts->logfile = "/dev/null";

    return 0;
}

static int opts_init( cvterm_opts *opts, int argc, char *argv[] )
{
    memset( opts, 0, sizeof( *opts ) );

    // Parse command line options.
    if ( opts_parse_args( opts, argc, argv ) != 0 )
    {
        opts_usage( opts, argc, argv );
        return 1;
    }
    opts_print( opts );

    // Initialize logging.
    clog_init_path( 0, opts->logfile );
    clog_set_fmt( 0, "%m" );
    clog_info( CLOG( 0 ), "\n" );
    clog_set_fmt( 0, "%d %d: %m\n" );
    clog_info( CLOG( 0 ), "Starting %s...", argv[ 0 ] );
    clog_set_fmt( 0, "%f(%F:%n): %l: %m\n" );

    if ( opts->wait_for_debugger )
        wait_for_debugger();

    return 0;
}

int main( int argc, char *argv[] )
{
    cvterm_opts opts;

    sigwinch( SIG_BLOCK );
    setlocale( LC_ALL, "" );

    // Initialize options.
    if ( opts_init( &opts, argc, argv ) )
        return 1;

    // Call cvterm_shutdown on exit.
    atexit( cvterm_shutdown );

    // Get stdin termios parameters.
    struct termios child_termios;
    if ( tcgetattr( STDIN_FILENO, &child_termios ) != 0 )
        FATAL_ERROR( tcgetattr );

    // Initialize our terminal window.
    int rows, cols;
    g_twin = termwin_init( opts.nc_term );
    if ( !g_twin )
        FATAL_ERROR( termwin_init );
    termwin_getsize( g_twin, &rows, &cols );

    // Create our vterm object.
    g_vterm = vterm_new( rows, cols );
    vterm_set_utf8( g_vterm, 1 );

    termwin_setvterm( g_twin, g_vterm );

    // Initialize vterm screen.
    VTermScreen *vtscreen = vterm_obtain_screen( g_vterm );
    vterm_screen_enable_altscreen( vtscreen, 1 );
    vterm_screen_reset( vtscreen, 1 );
    vterm_screen_set_callbacks( vtscreen, &g_screen_cbs, g_twin );

    {
        char slavename[ 128 ];
        const struct winsize size = { rows, cols, 0, 0 };

        pid_t child = pty_fork( &g_master_pty, slavename, sizeof( slavename ), &child_termios, &size );
        clog_info( CLOG( 0 ), "pty_fork child:%d slavename:%s", child, slavename );

        if ( child == 0 )
        {
            if ( opts.env_term )
            {
                if ( setenv( "TERM", opts.env_term, 1 ) )
                    FATAL_ERROR( setenv );
            }
            else
            {
                if ( unsetenv( "TERM" ) )
                    FATAL_ERROR( unsetenv );
            }

            sigwinch( SIG_UNBLOCK );

            execvp( opts.argv[ 0 ], ( char *const * )opts.argv );
            FATAL_ERROR( execvp );
        }
    }

    // Make g_master_py non-blocking.
    if ( fcntl( g_master_pty, F_SETFL, fcntl( g_master_pty, F_GETFL ) | O_NONBLOCK ) < 0 )
        FATAL_ERROR( fcntl );

    main_loop( g_vterm, g_master_pty );

    cvterm_shutdown();
    return 0;
}
