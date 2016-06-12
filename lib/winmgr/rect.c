#include "rect.h"

void rect_set(rect *result, int left, int top, int right, int bottom)
{
    result->left = left;
    result->top = top;
    result->right = right;
    result->bottom = bottom;
}

void rect_set_empty(rect *rc)
{
    rc->left = 0;
    rc->top = 0;
    rc->right = 0;
    rc->bottom = 0;
}

void rect_union(rect *result, rect *rc1, rect *rc2)
{
    if (rect_empty(rc1))
    {
        if (rect_empty(rc2))
        {
            rect_set_empty(result);
            return;
        }
        result = rc2;
        return;
    }
    else if (rect_empty(rc2))
    {
        result = rc1;
        return;
    }

    result->left = rc1->left < rc2->left ? rc1->left : rc2->left;
    result->top = rc1->top < rc2->top ? rc1->top : rc2->top;
    result->right = rc1->right > rc2->right ? rc1->right : rc2->right;
    result->bottom = rc1->bottom > rc2->bottom ? rc1->bottom : rc2->bottom;
}

int rect_intersect(rect *result, rect *rc1, rect *rc2)
{
    if (rect_empty(rc1) || rect_empty(rc2))
    {
        rect_set_empty(result);
        return 0;
    }

    result->left = rc1->left > rc2->left ? rc1->left : rc2->left;
    result->top = rc1->top > rc2->top ? rc1->top : rc2->top;
    result->right = rc1->right < rc2->right ? rc1->right : rc2->right;
    result->bottom = rc1->bottom < rc2->bottom ? rc1->bottom : rc2->bottom;

    return !rect_empty(result);
}

int rect_empty(rect *rc)
{
    return (rc->left >= rc->right || rc->top >= rc->bottom);
}

int rect_equal(rect *rc1, rect *rc2)
{
    return (rc1->left == rc2->left && rc1->top == rc2->top &&
        rc1->right == rc2->right && rc1->bottom == rc2->bottom);
}

void rect_to_ticketrect(const rect *rc, TickitRect *trc)
{
    trc->top = rc->top;
    trc->left = rc->left;
    trc->lines = rc->bottom - rc->top;
    trc->cols = rc->right - rc->left;
}

void ticketrect_to_rect(const TickitRect *trc, rect *rc)
{
    rc->left = trc->left;
    rc->top = trc->top;
    rc->right = trc->left + trc->cols;
    rc->bottom = trc->top + trc->lines;
}
