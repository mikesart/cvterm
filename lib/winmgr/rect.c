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

void rect_offset(rect *rc, int x, int y)
{
    rc->left += x;
    rc->top += y;
    rc->right += x;
    rc->bottom += y;
}

void rect_inflate(rect *rc, int x, int y)
{
    rc->left += x;
    rc->top += y;
    rc->right -= x;
    rc->bottom -= y;
}

void rect_union(rect *result, const rect *rc1, const rect *rc2)
{
    if (rect_empty(rc1))
    {
        if (rect_empty(rc2))
        {
            rect_set_empty(result);
            return;
        }
        *result = *rc2;
        return;
    }
    else if (rect_empty(rc2))
    {
        *result = *rc1;
        return;
    }

    result->left = rc1->left < rc2->left ? rc1->left : rc2->left;
    result->top = rc1->top < rc2->top ? rc1->top : rc2->top;
    result->right = rc1->right > rc2->right ? rc1->right : rc2->right;
    result->bottom = rc1->bottom > rc2->bottom ? rc1->bottom : rc2->bottom;
}

int rect_intersect(rect *result, const rect *rc1, const rect *rc2)
{
    if (rect_empty(rc1) || rect_empty(rc2))
    {
        rect_set_empty(result);
        return 0;
    }

    // Intersect
    rect rcT;
    rcT.left = rc1->left > rc2->left ? rc1->left : rc2->left;
    rcT.top = rc1->top > rc2->top ? rc1->top : rc2->top;
    rcT.right = rc1->right < rc2->right ? rc1->right : rc2->right;
    rcT.bottom = rc1->bottom < rc2->bottom ? rc1->bottom : rc2->bottom;

    // Copy at the end in case result is also rc1 or rc2
    if (!rect_empty(&rcT))
    {
        *result = rcT;
        return 1;
    }

    rect_set_empty(result);
    return 0;
}

int rect_empty(const rect *rc)
{
    return (rc->left >= rc->right || rc->top >= rc->bottom);
}

int rect_equal(const rect *rc1, const rect *rc2)
{
    return (rc1->left == rc2->left && rc1->top == rc2->top &&
        rc1->right == rc2->right && rc1->bottom == rc2->bottom);
}
