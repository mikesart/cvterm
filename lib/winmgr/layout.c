#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "layout.h"
#include "splitter.h"

// Manages a layout tree.
struct laymgr
{
    window *host; // host window (NULL for root window)
    handler h; // override handler for host
    handler h_old; // old handler for host
    layout *root; // root layout
    int update; // 1 if a layout update needs to occur
};

// To perform async update
#define LM_UPDATE (WM_USER + 0x1000)

// Min size of a layout (default). Window can respond to WM_GETMINSIZE
// to override.
#define LAYOUT_MIN_WIDTH    20
#define LAYOUT_MIN_HEIGHT   2

// Layout hold windows (and splitters), or other layouts in a hierarchy.
// A layout specifies flow direction (horz or vert) of its children.
struct layout
{
    laymgr *lm;

    // Pointer to next layout in a flow direction.
    layout *next;

    // Parent layout. Parent is a container of a list of layouts and
    // holds the flow direction.
    layout *parent;

    // First child of a contained layout list. If child != NULL, this is a
    // container layout, so client must be NULL.
    layout *child;

    // if client != NULL, layout holds this window.
    // if client == NULL, layout is a container of layouts in a flow direction.
    window *client;

    // Layouts can optionally have splitters, layed out in the order of splitter
    // then client in a given flow direction. The splitter for the first
    // layout is always NULL.
    splitter *spltr;

    // This is the flow direction for child layouts. If there are no children
    // of this layout, this field isn't used.
    int vert;

    // Actual size of the layout measured in the parent's flow direction.
    // Doesn't include the splitter.
    int size;

    // Desired percentage of the parent's layout space allocated to this layout.
    // Used when sizing and/or persisting.
    float pct;
};

layout *layout_alloc(laymgr *lm, layout *parent, window *client, int size);
void layout_free(layout *lay);
void laymgr_update(laymgr *lm, int async);
void layout_validate(layout *lay);
void update_child_size(layout *parent, int vert, int size_changed);

uint32_t host_proc(laymgr *lm, int id, const message_data *data)
{
    switch (id)
    {
    case WM_POSCHANGED:
        if (data->pos_changed.resized)
        {
            int height_old = data->pos_changed.rc_old->bottom - data->pos_changed.rc_old->top;
            int height_new = data->pos_changed.rc_new->bottom - data->pos_changed.rc_new->top;;
            if (height_old != height_new)
                update_child_size(lm->root, 0, height_new - height_old);
            int width_old = data->pos_changed.rc_old->right - data->pos_changed.rc_old->left;
            int width_new = data->pos_changed.rc_new->right - data->pos_changed.rc_new->left;
            if (width_old != width_new)
                update_child_size(lm->root, 1, width_new - width_old);
            laymgr_update(lm, true);
        }
        break;

    case LM_UPDATE:
        laymgr_update(lm, 0);
        break;
    }

    return handler_call(lm->h_old, id, data);
}

laymgr *laymgr_create(window *host)
{
    laymgr *lm = malloc(sizeof(laymgr));
    memset(lm, 0, sizeof(*lm));
    lm->host = host;
    lm->h = handler_create(lm, (handler_proc)host_proc);
    lm->h_old = window_set_handler(lm->host, lm->h);
    lm->root = layout_alloc(lm, NULL, NULL, 0);
    return lm;
}

void laymgr_destroy(laymgr *lm)
{
    layout_close(lm->root);
    window_set_handler(lm->host, lm->h_old);
    handler_destroy(lm->h);
    free(lm);
}

layout *laymgr_root(laymgr *lm)
{
    return lm->root;
}

layout *layout_find_helper(layout *lay, window *w)
{
    if (lay->client == w)
        return lay;

    for (layout *child = lay->child; child != NULL; child = child->next)
    {
        layout *found = layout_find_helper(child, w);
        if (found)
            return found;
    }

    return NULL;
}

layout *laymgr_find(laymgr *lm, window *w)
{
    return layout_find_helper(lm->root, w);
}

void layout_min_size(layout *lay, int *width_min, int *height_min)
{
    // If there are children, the min sizes are derived from them.
    // If no children, the min sizes are derived from windows.
    int width = 0;
    int height = 0;
    for (layout *child = lay->child; child != NULL; child = child->next)
    {
        int width_min_child;
        int height_min_child;
        layout_min_size(child, &width_min_child, &height_min_child);

        if (lay->vert)
        {
            if (height_min_child > height)
                height = height_min_child;
            if (child->spltr)
                width++;
            width += width_min_child;
        }
        else
        {
            if (width_min_child > width)
                width = width_min_child;
            if (child->spltr)
                height++;
            height += height_min_child;
        }
    }

    if (lay->client)
    {
        width = LAYOUT_MIN_WIDTH;
        height = LAYOUT_MIN_HEIGHT;
        message_data data;
        data.size_min.width = &width;
        data.size_min.height = &height;
        handler_call(window_handler(lay->client), WM_GETMINSIZE, &data);
    }

    // The min size of a layout does not include the splitter when considering
    // allocating splittable space within it. However when allocating space of
    // a child from the splittable space of a parent, the child splitter must be
    // considered.
    *width_min = width;
    *height_min = height;
}

void layout_rect(layout *lay, rect *rc)
{
    // Calc the rect by clipping with layout sizes.
    if (lay->parent)
    {
        layout_rect(lay->parent, rc);

        if (lay->parent->vert)
        {
            // top and bottom stay as is. Adjust left and right
            // for layout position.
            for (layout *child = lay->parent->child; child != NULL; child = child->next)
            {
                // Adjust for splitter
                if (child->spltr)
                    rc->left++;
                if (child == lay)
                {
                    rc->right = rc->left + child->size;
                    return;
                }
                rc->left += child->size;
            }
        }
        else
        {
            // left and right stay as is. Adjust top and bottom
            // for layout position
            for (layout *child = lay->parent->child; child != NULL; child = child->next)
            {
                // Adjust for splitter
                if (child->spltr)
                    rc->top++;
                if (child == lay)
                {
                    rc->bottom = rc->top + child->size;
                    return;
                }
                rc->top += child->size;
            }
        }
    }
    else
    {
        window_rect(lay->lm->host, rc);
    }
}

void update_child_size(layout *parent, int vert, int size_changed)
{
    if (!parent->child)
        return;

    if (parent->vert != vert)
    {
        for (layout *child = parent->child; child != NULL; child = child->next)
        {
            update_child_size(child, vert, size_changed);
        }
    }
    else
    {
        // First add up the total client space.
        int child_size_total = 0;
        for (layout *child = parent->child; child != NULL; child = child->next)
            child_size_total += child->size;
        child_size_total += size_changed;

        // Allocate by child layout percentage
        int size_remaining = child_size_total;
        for (layout *child = parent->child; child != NULL; child = child->next)
        {
            int size_new = child->pct * child_size_total + 0.5f;
            if (size_new > size_remaining)
                size_new = size_remaining;
            size_changed = size_new - child->size;
            if (size_changed)
            {
                child->size = size_new;
                if (child->child)
                    update_child_size(child, vert, size_changed);
            }
            size_remaining -= size_new;
        }
    }
}

void update_child_pct(layout *parent)
{
    // Calc total child size
    int child_size_total = 0;
    for (layout *child = parent->child; child != NULL; child = child->next)
        child_size_total += child->size;

    // Update each child's pct of total
    for (layout *child = parent->child; child != NULL; child = child->next)
        child->pct = (float)child->size / (float)child_size_total;
}

void adjust_size(layout *lay, int size_adjust)
{
    // Update children sizes
    update_child_size(lay, lay->parent->vert, size_adjust);

    // Adjust the size of lay
    lay->size += size_adjust;

    // Update sibling size percentages
    if (lay->parent)
        update_child_pct(lay->parent);
}

void set_splitter_visible(layout *lay, int visible)
{
    if (visible)
    {
        if (!lay->spltr)
            lay->spltr = splitter_create(lay->lm->host, lay);
    }
    else
    {
        if (lay->spltr)
        {
            splitter_destroy(lay->spltr);
            lay->spltr = NULL;
        }
    }
}

layout *layout_alloc(laymgr *lm, layout *parent, window *client, int size)
{
    layout *lay = malloc(sizeof(layout));
    memset(lay, 0, sizeof(*lay));
    lay->lm = lm;
    lay->parent = parent;
    lay->client = client;
    lay->size = size;
    return lay;
}

void layout_free(layout *lay)
{
    set_splitter_visible(lay, 0);
    free(lay);
}

int check_split_size(layout *ref, window *client, int splitter, int size_requested, int dir)
{
    // ref will be split. Get the min size for ref to make sure
    // the resulting split is large enough for ref. Does not include splitter
    // because that is not part of the splittable space.
    int width_ref_min;
    int height_ref_min;
    layout_min_size(ref, &width_ref_min, &height_ref_min);

    // Get the min size for this client
    int width_client_min = LAYOUT_MIN_WIDTH;
    int height_client_min = LAYOUT_MIN_HEIGHT;
    message_data data;
    data.size_min.width = &width_client_min;
    data.size_min.height = &height_client_min;
    handler_call(window_handler(client), WM_GETMINSIZE, &data);

    // Get min sizes in the direction requested
    // The split gets created on the layout edge splitting ref.
    int size_ref_min;
    int size_client_min;
    if (IS_DIR_VERT(dir))
    {
        size_ref_min = width_ref_min;
        size_client_min = width_client_min + splitter ? 1 : 0;
    }
    else
    {
        size_ref_min = height_ref_min;
        size_client_min = height_client_min + splitter ? 1 : 0;
    }

    // 1 if the split will be inline with the current parent flow.
    // If not inline, then a container will be allocated with the
    // requested flow, and ref and the new layout will be children.
    int inline_split = (ref->parent && IS_DIR_VERT(dir) == ref->parent->vert) ? 1 : 0;

    // Get the min sizes in the direction requested.
    // direction requested (does not include splitter).
    int size_ref_current = ref->size;
    if (!inline_split)
    {
        rect rc;
        layout_rect(ref, &rc);
        if (IS_DIR_VERT(dir))
        {
            size_ref_current = rc.right - rc.left;
        }
        else
        {
            size_ref_current = rc.bottom - rc.top;
        }
    }

    // Does min size for ref and client fit in the current ref size?
    if (size_ref_min + size_client_min > size_ref_current)
        return -1;

    // Something fits; find the size closest to that requested.
    if (size_requested == SIZE_HALF)
        size_requested = size_ref_current / 2;
    if (size_ref_current - size_requested >= size_ref_min)
    {
        // Does the requested size fit for both min sizes? If so use it.
        if (size_requested >= size_client_min)
            return size_requested;

        // It's too small for size_client_min, so take
        // size_client_min as is (already know it fits).
        return size_client_min;
    }
    else
    {
        // Doesn't fit size_ref_min. Return the size that results
        // in size_ref_min left over after allocation (already know it fits).
        return size_ref_current - size_ref_min;
    }
}

layout *create_inline_split(layout *ref, window *client, int splitter, int size, int dir)
{
    // The no parent case is the root case and isn't handled here.
    if (!ref->parent)
        return NULL;

    // If no room to split, error
    size = check_split_size(ref, client, splitter, size, dir);
    if (size < 0)
        return NULL;

    // Split ref and create a new layout
    layout *lay = layout_alloc(ref->lm, ref->parent, client, size);
    if (IS_DIR_PREV(dir))
    {
        // Inserting prev direction
        layout *layT = lay->parent->child;
        if (layT == ref)
        {
            // Inserting first affects the next layout
            lay->next = ref;
            lay->parent->child = lay;
            adjust_size(ref, -lay->size - splitter);
            set_splitter_visible(ref, splitter);
        }
        else
        {
            // Inserting not first affects ref
            while (layT->next != ref)
                layT = layT->next;
            lay->next = ref;
            layT->next = lay;
            adjust_size(ref, -lay->size - splitter);
            set_splitter_visible(lay, ref->spltr != NULL);
            set_splitter_visible(ref, splitter);
        }
    }
    else
    {
        // Inserting next affects ref
        lay->next = ref->next;
        ref->next = lay;
        adjust_size(ref, -lay->size - splitter);
        set_splitter_visible(lay, splitter);
    }

    laymgr_update(ref->lm, true);
    return lay;
}

layout *create_child_split(layout *ref, window *client, int splitter, int size, int dir)
{
    // Create a layout container with the desired flow direction and move 
    // ref into this container as a child, and then split ref from there.

    // If ref already has children this is an error
    if (ref->child)
        return NULL;

    // First see if the size is big enough to split. Note the split direction isn't the same
    // as the parent's flow direction.
    size = check_split_size(ref, client, splitter, size, dir);
    if (size < 0)
        return NULL;

    layout *cont = layout_alloc(ref->lm, ref->parent, NULL, ref->size);
    set_splitter_visible(cont, ref->spltr != NULL);
    cont->vert = IS_DIR_VERT(dir);
    cont->pct = ref->pct;

    // Replace ref with cont
    if (!ref->parent)
    {
        // The root has a window and is being parented.
        ref->lm->root = cont;
    }
    else
    {
        layout **pplay = &ref->parent->child;
        while (*pplay)
        {
            if (*pplay == ref)
            {
                *pplay = cont;
                cont->next = ref->next;
                break;
            }
            pplay = &((*pplay)->next);
        }
    }

    // Add ref to the container
    ref->parent = cont;
    ref->next = NULL;
    cont->child = ref;
    set_splitter_visible(ref, 0);

    // ref's size is the size of the container in the requested flow direction
    rect rc;
    layout_rect(cont, &rc);
    ref->size = IS_DIR_VERT(dir) ? rc.right - rc.left : rc.bottom - rc.top;
    ref->pct = 1.0f;
       
    // Now split this ref, which will be an inline split. This should succeed
    // because of the earlier size check.
    layout *result = layout_split(ref, client, splitter, size, dir);
    assert(result != NULL);
    return result;
}

layout *layout_split(layout *ref, window *w, int splitter, int size, int dir)
{
    // Split in the direction of the parent flow?
    if (ref->parent && IS_DIR_VERT(dir) == ref->parent->vert)
    {
        // This creates a new layout in line with ref
        return create_inline_split(ref, w, splitter, size, dir);
    }
    else
    {
        // This re-parents ref to be a child of a new containing parent
        // with a flow in the requested direction.
        return create_child_split(ref, w, splitter, size, dir);
    }
}

window *layout_window(layout *lay)
{
    return lay->client;
}

int layout_set_window(layout *lay, window *w)
{
    // Fail if this layout is a parent of other layouts
    if (lay->child)
        return 0;

    // Allow setting the window independent of min sizes
    lay->client = w;
    laymgr_update(lay->lm, true);
    return 1;
}

void layout_list_remove(layout *lay)
{
    // Remove lay from the list it is in
    layout **pplay = &lay->parent->child;
    while (*pplay)
    {
        if (*pplay == lay)
        {
            *pplay = lay->next;
            break;
        }
        pplay = &(*pplay)->next;
    }
}

void layout_promote_child(layout *child)
{
    // If a child is the only child in a container, promote the
    // child up and get rid of the container.
    layout *parent = child->parent;
    if (child && !child->next && parent->parent)
    {
        // Move the child up one parent level
        child->parent = parent->parent;
        child->next = parent->next;
        parent->next = child;
        parent->child = NULL;
        layout_list_remove(parent);

        // Adopt the parent's splitter state
        set_splitter_visible(child, parent->spltr != NULL);

        // Adopt the parent's size and update pcts
        child->size = parent->size;
        update_child_pct(child->parent);

        // Free the parent
        layout_free(parent);
    }

    // If the child is a container with the same
    // flow direction as the parent, promote the
    // contents of the child. The children sizes
    // should match the size of the parent, so no
    // adustment needed.
    if (child->child && child->parent->vert == child->vert)
    {
        // Link in children
        layout *last = child->child;
        while (last)
        {
            last->parent = child->parent;
            if (!last->next)
                break;
            last = last->next;
        }
        last->next = child->next;
        child->next = child->child;
        child->child = NULL;

        // The first child's splitter state needs to match
        // that of the parent.
        set_splitter_visible(child->next, child->spltr != NULL);
        layout_list_remove(child);

        // Update pcts now that child isn't part of list
        update_child_pct(child->parent);
        layout_free(child);
    }
}

void layout_close_helper(layout *lay, int promote)
{
    // Remove this layout and children
    while (lay->child != NULL)
        layout_close_helper(lay->child, 0);

    if (lay->parent != NULL)
    {
        // Adjust the adjacent layout size before removing this layout.
        layout *layT = lay->parent->child;
        if (lay == layT)
        {
            // Removing the first in the list means adjust the next layout size
            layout *next = lay->next;
            lay->parent->child = next;
            if (next)
            {
                adjust_size(next, lay->size + (lay->next->spltr ? 1 : 0));
                set_splitter_visible(next, 0);
            }
        }
        else
        {
            // Removing somewhere not the first means adjust the previous
            // layout size.
            while (layT->next != lay)
                layT = layT->next;
            layT->next = lay->next;
            adjust_size(layT, lay->size + (lay->spltr ? 1 : 0));
        }

        if (promote)
        {
            // Promote child if parent is a container with one layout.
            if (lay->parent->child && !lay->parent->child->next && lay->parent->parent)
                layout_promote_child(lay->parent->child);
        }
    }

    laymgr_update(lay->lm, true);
    layout_free(lay);
}

void layout_close(layout *lay)
{
    layout_close_helper(lay, 1);
}

layout *find_move_layout(layout *lay, int edge)
{
    // Find the layout whose size will be adjusted
    layout *layT = lay;
    while (layT)
    {
        // Can't move an edge of the root
        if (!layT->parent)
            return NULL;

        // If the parent isn't layed out in the direction
        // requested to move, then try the next parent.
        if (IS_DIR_VERT(edge) != layT->parent->vert)
        {
            layT = layT->parent;
            continue;
        }

        // Even though this layout is in a direction
        // that matches the move direction, the edge
        // requested to move may be in a parent.
        if ((IS_DIR_PREV(edge) && layT == layT->parent->child) ||
            (!IS_DIR_PREV(edge) && !layT->next))
        {
            layT = layT->parent;
            continue;
        }
        break;
    }

    return layT;
}

int layout_move_edge(layout *lay, int delta, int edge)
{
    // Find the layout whose size will be adjusted
    layout *layT = find_move_layout(lay, edge);

    // Try the opposite edge if the passed in edge didn't work
    if (!layT)
    {
        edge = DIR_REVERSE(edge);
        layT = find_move_layout(lay, edge);
        if (!layT)
            return 0;
    }

    // If the edge is prev, then it is the previous
    // layout will be sizing too. Otherwise it is this layout
    // and the next layout
    layout *lay1;
    layout *lay2;
    if (IS_DIR_PREV(edge))
    {
        layout *prev = layT->parent->child;
        while (prev->next != layT)
            prev = prev->next;
        lay1 = prev;
        lay2 = layT;
    }
    else
    {
        lay1 = layT;
        lay2 = layT->next;
    }

    // Make sure the layout shrinking doesn't get smaller than its
    // min size
    if (delta < 0)
    {
        int width_min;
        int height_min;
        layout_min_size(lay1, &width_min, &height_min);
        int size_min = lay1->parent->vert ? width_min : height_min;
        if (lay1->size + delta < size_min)
            delta = size_min - lay1->size;
        if (delta >= 0)
            return 0;
    }
    else
    {
        int width_min;
        int height_min;
        layout_min_size(lay2, &width_min, &height_min);
        int size_min = lay2->parent->vert ? width_min : height_min;
        if (lay2->size - delta < size_min)
            delta = lay2->size - size_min;
        if (delta <= 0)
            return 0;
    }

    // Adjust the layout sizes
    adjust_size(lay1, delta);
    adjust_size(lay2, -delta);

    // Update
    laymgr_update(lay->lm, true);
    return 1;
}

int interval_distance(int i, int i1, int i2)
{
    if (i < i1)
    {
        return i1 - i;
    }
    else if (i >= i2)
    {
        return i - i2;
    }

    return 0;
}

layout *find_closest_layout(layout *lay, int x, int y)
{
    layout *closest = NULL;
    int dist_min = INT_MAX;
    if (lay->child)
    {
        int dist;
        rect rc;
        layout_rect(lay, &rc);
        for (layout *child = lay->child; child != NULL; child = child->next)
        {
            if (lay->vert)
            {
                if (child->spltr)
                    rc.left++;
                rc.right = rc.left + child->size;
                dist = interval_distance(x, rc.left, rc.right - 1);
                rc.left = rc.right;
            }
            else
            {
                if (child->spltr)
                    rc.top++;
                rc.bottom = rc.top + child->size;
                dist = interval_distance(y, rc.top, rc.bottom - 1);
                rc.top = rc.bottom;
            }
            if (dist < dist_min)
            {
                dist_min = dist;
                closest = child;
            }
        }
    }

    if (closest)
        return find_closest_layout(closest, x, y);

    return lay;
}

layout *layout_navigate_dir(layout *lay, int x, int y, int dir)
{
    // Find the layout with the edge we want to navigate across
    layout *layT = find_move_layout(lay, dir);
    if (layT == NULL)
        return NULL;

    // Either prev or next
    if (IS_DIR_PREV(dir))
    {
        layout *prev = layT->parent->child;
        while (prev->next != layT)
            prev = prev->next;
        layT = prev;
    }
    else
    {
        layT = layT->next;
    }

    // Now find the child, if any, that is closest to x, y
    window_map_point(lay->client, lay->lm->host, &x, &y);
    return find_closest_layout(layT, x, y);
}

layout *find_child_ordered(layout *lay, int next)
{
    // Recurse down to find the lowest beginning or ending
    // layout.
    if (lay->child)
    {
        if (next)
        {
            return find_child_ordered(lay->child, next);
        }
        else
        {
            layout *prev = lay->child;
            while (prev->next != NULL)
                prev = prev->next;
            return find_child_ordered(prev, next);
        }
    }

    return lay;
}

layout *navigate_ordered_helper(layout *lay, int next)
{
    // Find the navigatable layout in the direction requested
    layout *layT = lay;
    while (layT)
    {
        if (!layT->parent)
            return NULL;

        // If we're at an end, a parent may be navigatable
        if ((!next && layT == layT->parent->child) ||
            (next && !layT->next))
        {
            layT = layT->parent;
            continue;
        }
        break;
    }
    if (!layT)
        return NULL;

    // Next or prev is possible now; do that.
    if (!next)
    {
        layout *prev = layT->parent->child;
        while (prev->next != layT)
            prev = prev->next;
        layT = prev;
    }
    else
    {
        layT = layT->next;
    }

    // Now find the appropriate ordered child
    return find_child_ordered(layT, next);
}

layout *layout_navigate_ordered(layout *lay, int next)
{
    // Try to go in the direction requested
    layout *layT = navigate_ordered_helper(lay, next);
    if (layT)
        return layT;

    // Nothing in that direction. Start at the other end.
    for (layout *layE = lay; layE != NULL; layE = navigate_ordered_helper(layE, !next))
        layT = layE;
    return layT;
}

void apply_layout(layout *lay, const rect *rc)
{
    // Handle splitter layout
    rect rcL = *rc;
    if (lay->spltr)
    {
        window *w = splitter_window(lay->spltr);
        if (lay->parent)
        {
            if (lay->parent->vert)
            {
                rcL.right = rcL.left + 1;
                window_set_pos(w, &rcL);
                rcL.left = rcL.right;
                rcL.right = rcL.left + lay->size;
            }
            else
            {
                rcL.bottom = rcL.top + 1;
                window_set_pos(w, &rcL);
                rcL.top = rcL.bottom;
                rcL.bottom = rcL.top + lay->size;
            }
        }
    }

    rect rcC = rcL;
    for (layout *child = lay->child; child != NULL; child = child->next)
    {
        // Calc rect for this layout, including splitter
        int splitter_size = child->spltr ? 1 : 0;
        if (lay->vert)
        {
            rcC.right = rcC.left + splitter_size + child->size;
            apply_layout(child, &rcC);
            rcC.left = rcC.right;
        }
        else
        {
            rcC.bottom = rcC.top + splitter_size + child->size;
            apply_layout(child, &rcC);
            rcC.top = rcC.bottom;
        }
    }

    // Handle window layout
    if (lay->client)
    {
        window_set_pos(lay->client, &rcL);
    }
}

void laymgr_update(laymgr *lm, int async)
{
    layout_validate(lm->root);

    if (async)
    {
        if (!lm->update)
        {
            message_post(lm->h, LM_UPDATE, NULL);
            lm->update = 1;
        }
        return;
    }
    lm->update = 0;

    // Update layout positions
    rect rc;
    window_rect(lm->host, &rc);
    apply_layout(lm->root, &rc);
}

void layout_validate(layout *lay)
{
#ifdef DEBUG
    if (lay->child)
    {
        assert(!lay->client);

        rect rc;
        layout_rect(lay, &rc);

        int size_children = 0;
        for (layout *child = lay->child; child != NULL; child = child->next)
        {
            // First child shouldn't have a splitter
            assert(child != lay->child || !child->spltr);
            if (child->spltr)
                size_children++;
            size_children += child->size;

            if (child->child)
                layout_validate(child);
        }

        if (lay->vert)
        {
            assert(rc.right - rc.left == size_children);
        }
        else
        {
            assert(rc.bottom - rc.top == size_children);
        }
    }
    else
    {
        assert(lay == lay->lm->root || lay->client);
    }
#endif
}
