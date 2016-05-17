/**************************************************************************
 *
 * Copyright (c) 2016, Michael Sartain <mikesart@fastmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
