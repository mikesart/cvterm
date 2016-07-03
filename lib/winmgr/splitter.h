#ifndef __SPLITTER_H__
#define __SPLITTER_H__

#include "window.h"
#include "layout.h"

typedef struct splitter splitter;

splitter *splitter_create(window *parent, layout *lay);
void splitter_destroy(splitter *spltr);
void splitter_set_layout(splitter *spltr, layout *lay);
window *splitter_window(splitter *spltr);

#endif // __SPLITTER_H__

