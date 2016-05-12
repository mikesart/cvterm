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
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#define CLOG_MAIN
#include "clog.h"

#include "cvterm_utils.h"
#include "pseudo.h"

int is_debugger_attached()
{
    int debugger_attached = 0;
    static const char TracerPid[] = "TracerPid:";

    FILE *fp = fopen( "/proc/self/status", "r" );
    if ( fp )
    {
        ssize_t chars_read;
        size_t line_len = 0;
        char *line = NULL;

        while ( ( chars_read = getline( &line, &line_len, fp ) ) != -1 )
        {
            char *tracer_pid = strstr( line, TracerPid );

            if ( tracer_pid )
            {
                debugger_attached = !!atoi( tracer_pid + sizeof( TracerPid ) - 1 );
                break;
            }
        }

        free( line );
        fclose( fp );
    }

    return debugger_attached;
}

void wait_for_debugger()
{
    int count = 0;

    fprintf( stdout, "Waiting 30 seconds for debugger to attach...\n" );
    while ( ( count++ < 30 ) && ( is_debugger_attached() == 0 ) )
        sleep( 1 );

    fprintf( stdout, "Debugger attached: %d\n", is_debugger_attached() );
}

// From http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
/**
 * \brief    Fast Square root algorithm
 *
 * Fractional parts of the answer are discarded. That is:
 *      - SquareRoot(3) --> 1
 *      - SquareRoot(4) --> 2
 *      - SquareRoot(5) --> 2
 *      - SquareRoot(8) --> 2
 *      - SquareRoot(9) --> 3
 *
 * \param[in] a_nInput - unsigned integer for which to find the square root
 *
 * \return Integer square root of the input value.
 */
uint32_t sqrt_uint32( uint32_t a_nInput )
{
    uint32_t op = a_nInput;
    uint32_t res = 0;
    uint32_t one = 1uL << 30; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type

    // "one" starts at the highest power of four <= than the argument.
    while ( one > op )
    {
        one >>= 2;
    }

    while ( one != 0 )
    {
        if ( op >= res + one )
        {
            op = op - ( res + one );
            res = res + 2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

// Get number of milliseconds since app started up
uint32_t get_ticks()
{
    struct timeval t;
    static struct timeval s_t0;
    static int s_inited = 0;

    if ( !s_inited )
    {
        s_inited = 1;
        if ( gettimeofday( &s_t0, NULL ) )
            FATAL_ERROR( gettimeofday );
    }

    if ( gettimeofday( &t, NULL ) )
        FATAL_ERROR( gettimeofday );

    return ( t.tv_sec - s_t0.tv_sec ) * 1000 + ( t.tv_usec - s_t0.tv_usec ) / 1000;
}
