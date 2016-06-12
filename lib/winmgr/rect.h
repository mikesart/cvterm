#ifndef __RECT_H__
#define __RECT_H__

typedef struct
{
    int left;
    int top;
    int right;
    int bottom;
} rect;

void rect_set(rect *result, int left, int top, int right, int bottom);
void rect_set_empty(rect *rc);
void rect_union(rect *result, rect *rc1, rect *rc2);
int rect_intersect(rect *result, rect *rc1, rect *rc2);
int rect_empty(rect *rc);
int rect_equal(rect *rc1, rect *rc2);

#ifndef __WINMGR_H__
#include <tickit.h>
void rect_to_tickitrect(const rect *rc, TickitRect *trc);
void tickitrect_to_rect(const TickitRect *trc, rect *rc);
#endif

#endif // __RECT_H__
