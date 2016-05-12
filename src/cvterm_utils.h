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
#ifndef _CVTERM_UTILS_H_
#define _CVTERM_UTILS_H_

#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#if defined( __GNUC__ )
__inline__ void __debugbreak() __attribute( ( always_inline ) );
__inline__ void __debugbreak()
{
    __asm__ volatile( "int $0x03" );
}
#endif

#if defined( __GNUC__ )
#define ATTRIBUTE_PRINTF( _x, _y ) __attribute__( ( __format__( __printf__, _x, _y ) ) )
#else
#define ATTRIBUTE_PRINTF( _x, _y )
#endif

#define FATAL_ERROR( _func )                                     \
    do                                                           \
    {                                                            \
        clog_error( CLOG( 0 ), "%s failed: %d", #_func, errno ); \
        if ( is_debugger_attached() )                            \
            __debugbreak();                                      \
        exit( -1 );                                              \
    } while ( 0 )

int is_debugger_attached();
void wait_for_debugger();

uint32_t sqrt_uint32( uint32_t a_nInput );

// Get number of milliseconds since app started up
uint32_t get_ticks();

#endif // _CVTERM_UTILS_H_
