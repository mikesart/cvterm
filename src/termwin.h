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
#ifndef _TERMWIN_H_
#define _TERMWIN_H_

typedef struct termwin termwin;

termwin *termwin_init( const char *nc_term );
void termwin_free( termwin *twin );

void termwin_setvterm( termwin *twin, VTerm *term );
int termwin_getch( termwin *twin, int *ch );
void termwin_refresh( termwin *twin );
void termwin_resize( termwin *twin );
void termwin_getsize( termwin *twin, int *rows, int *cols );

// libvterm callbacks
int termwin_damage_callback( VTermRect rect, void *user );
int termwin_movecursor_callback( VTermPos pos, VTermPos oldpos, int visible, void *user );
int termwin_bell_callback( void *user );
int termwin_settermprop_callback( VTermProp prop, VTermValue *val, void *user );

#endif // _TERMWIN_H_
