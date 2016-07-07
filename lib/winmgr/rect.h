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
void rect_offset(rect *rc, int x, int y);
void rect_inflate(rect *rc, int x, int y);
void rect_union(rect *result, const rect *rc1, const rect *rc2);
int rect_intersect(rect *result, const rect *rc1, const rect *rc2);
int rect_empty(const rect *rc);
int rect_equal(const rect *rc1, const rect *rc2);
int rect_width(const rect *rc);
int rect_height(const rect *rc);

#endif // __RECT_H__
