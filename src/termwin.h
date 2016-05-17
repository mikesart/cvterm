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
#ifndef _TERMWIN_H_
#define _TERMWIN_H_

typedef struct termwin termwin;

termwin *termwin_init( const char *nc_term );
void termwin_free( termwin *twin );

void termwin_setvterm( termwin *twin, VTerm *term );
int termwin_getch( termwin *twin );
void termwin_refresh( termwin *twin );
void termwin_resize( termwin *twin );
void termwin_getsize( termwin *twin, int *rows, int *cols );

// libvterm callbacks
int termwin_damage_callback( VTermRect rect, void *user );
int termwin_movecursor_callback( VTermPos pos, VTermPos oldpos, int visible, void *user );
int termwin_bell_callback( void *user );
int termwin_settermprop_callback( VTermProp prop, VTermValue *val, void *user );

#endif // _TERMWIN_H_
