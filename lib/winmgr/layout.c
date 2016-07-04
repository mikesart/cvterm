#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

// dir is passed into layout split. Convenience macros.
#define IS_VERT(dir) (!((dir) & 1))
#define IS_PREV(dir) (!((dir) & 2))
#define DIR_VERT(dir) (!!IS_VERT(dir))

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

    // layouts can optionally have splitters, layed out in the order of splitter
    // then client in a given flow direction. The splitter for the first
    // layout is always NULL.
    splitter *spltr;

    // This is the flow direction for child layouts. If there are no children
    // of this layout, this field isn't used.
    int vert;

    // size of the layout measured in the parent's flow direction. This is the size
    // of the space reserved for the client window. Doesn't include the splitter.
    int size;
};

layout *layout_alloc(laymgr *lm, layout *parent, window *client, int size);
void layout_free(layout *lay);
void laymgr_update(laymgr *lm, int async);
void layout_validate(layout *lay);

uint32_t host_proc(laymgr *lm, int id, const message_data *data)
{
    switch (id)
    {
// TODO(scottlu): handle host window resizing
#if 0
    case WM_POSCHANGED:
        if (data->pos_changed.resized)
        {
        }
        break;
#endif

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
            if (width_min_child > width)
                width = width_min_child;
            if (child->spltr)
                height++;
            height += height_min_child;
        }
        else
        {
            if (height_min_child > height)
                height = height_min_child;
            if (child->spltr)
                width++;
            width += width_min_child;
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

void adjust_size_helper(layout *lay, int vert, int size_adjust)
{
    // Adjust the size of this layout.
    if (lay->parent->vert == vert)
        lay->size += size_adjust;

    if (lay->child)
    {
        if (lay->vert != vert)
        {
            for (layout *child = lay->child; child != NULL; child = child->next)
            {
                adjust_size_helper(child, vert, size_adjust);
            }
        }
        else
        {
            // Distribute the size adjustment proportionally among children.
            // First add up the total client space.
            int child_size_total = 0;
            for (layout *child = lay->child; child != NULL; child = child->next)
                child_size_total += child->size;

            // Add in proportional size to each child layout
            float scale = 1.0f + (float)size_adjust / (float)child_size_total;
            float size_scaled = 0;
            for (layout *child = lay->child; child != NULL; child = child->next)
            {
                size_scaled += (float)child->size * scale;
                int size_new = (int)(size_scaled + 0.5f);
                if (child->size != size_new)
                    adjust_size_helper(child, vert, size_new - child->size);
                size_scaled -= size_new;
            }
        }
    }
}

void adjust_size(layout *lay, int split)
{
    // lay is the layout being added or removed. Adjust the size of the
    // adjacent layout, and account for splitter visibility.
    if (lay->parent->child == lay)
    {
        if (lay->next)
        {
            // lay has been inserted first, so adjust the size of
            // the next layout. If it now has a splitter, adjust
            // for that as well.
            int size_adjust = (split ? -lay->size : lay->size);
            if (lay->next->spltr)
                size_adjust += (split ? -1 : 1);
            adjust_size_helper(lay->next, lay->parent->vert, size_adjust);
        }
    }
    else
    {
        // Find the layout just before lay.
        layout *prev = lay->parent->child;
        while (prev->next != lay)
            prev = prev->next;

        // Add lay's size and splitter size to prev
        int size_adjust = (split ? -lay->size : lay->size);
        if (lay->spltr)
            size_adjust += (split ? -1 : 1);
        adjust_size_helper(prev, lay->parent->vert, size_adjust);
    }
}

void layout_set_split(layout *lay, int split)
{
    if (split)
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
    layout_set_split(lay, 0);
    free(lay);
}

int check_split_size(layout *ref, window *client, int split, int size_requested, int dir)
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
    if (IS_VERT(dir))
    {
        size_ref_min = width_ref_min;
        size_client_min = width_client_min + split ? 1 : 0;
    }
    else
    {
        size_ref_min = height_ref_min;
        size_client_min = height_client_min + split ? 1 : 0;
    }

    // 1 if the split will be inline with the current parent flow.
    // If not inline, then a container will be allocated with the
    // requested flow, and ref and the new layout will be children.
    int inline_split = (ref->parent && DIR_VERT(dir) == ref->parent->vert) ? 1 : 0;

    // Get the min sizes in the direction requested.
    // direction requested (does not include splitter).
    int size_ref_current = ref->size;
    if (!inline_split)
    {
        rect rc;
        layout_rect(ref, &rc);
        if (IS_VERT(dir))
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

layout *create_inline_split(layout *ref, window *client, int split, int size, int dir)
{
    // The no parent case is the root case and isn't handled here.
    if (!ref->parent)
        return NULL;

    // If no room to split, error
    size = check_split_size(ref, client, split, size, dir);
    if (size < 0)
        return NULL;

    // Split ref and create a new layout
    layout *lay = layout_alloc(ref->lm, ref->parent, client, size);
    if (IS_PREV(dir))
    {
        // Insert before ref
        layout **pplay = &ref->parent->child;
        while (*pplay)
        {
            if (*pplay == ref)
            {
                *pplay = lay;
                lay->next = ref;
                break;
            }
            pplay = &(*pplay)->next;
        }

        if (ref->parent->child == lay)
        {
            layout_set_split(lay->next, split);
        }
        else
        {
            layout_set_split(lay, split);
        }
    }
    else
    {
        // Insert after ref
        lay->next = ref->next;
        ref->next = lay;
        layout_set_split(lay, split);
    }

    // Adjust layout size and async update
    adjust_size(lay, 1);
    laymgr_update(ref->lm, true);
    return lay;
}

layout *create_child_split(layout *ref, window *client, int split, int size, int dir)
{
    // Create a layout container with the desired flow direction and move 
    // ref into this container as a child, and then split ref from there.

    // If ref already has children this is an error
    if (ref->child)
        return NULL;

    // First see if the size is big enough to split. Note the split direction isn't the same
    // as the parent's flow direction.
    size = check_split_size(ref, client, split, size, dir);
    if (size < 0)
        return NULL;

    layout *cont = layout_alloc(ref->lm, ref->parent, NULL, ref->size);
    layout_set_split(cont, ref->spltr != NULL);
    cont->vert = DIR_VERT(dir);

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
    layout_set_split(ref, 0);

    // ref's size is the size of the container in the requested flow direction
    rect rc;
    layout_rect(cont, &rc);
    ref->size = DIR_VERT(dir) ? rc.right - rc.left : rc.bottom - rc.top;
       
    // Now split this ref, which will be an inline split. This should succeed
    // because of the earlier size check.
    layout *result = layout_split(ref, client, split, size, dir);
    assert(result != NULL);
    return result;
}

layout *layout_split(layout *ref, window *w, int split, int size, int dir)
{
    // Split in the direction of the parent flow?
    if (ref->parent && DIR_VERT(dir) == ref->parent->vert)
    {
        // This creates a new layout in line with ref
        return create_inline_split(ref, w, split, size, dir);
    }
    else
    {
        // This re-parents ref to be a child of a new containing parent
        // with a flow in the requested direction.
        return create_child_split(ref, w, split, size, dir);
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

    // Ensure the window fits within this layout
    int width_min = LAYOUT_MIN_WIDTH;
    int height_min = LAYOUT_MIN_HEIGHT;
    message_data data;
    data.size_min.width = &width_min;
    data.size_min.height = &height_min;
    handler_call(window_handler(w), WM_GETMINSIZE, &data);

    // Need the size of the layout. Note the splitter isn't a concern
    // because the window occupies the inside of the layout.
    rect rc;
    layout_rect(lay, &rc);
    if (rc.right - rc.left < width_min)
        return 0;
    if (rc.bottom - rc.top < height_min)
        return 0;

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
        layout_set_split(child, parent->spltr != NULL);

        // Adopt the parent's size
        child->size = parent->size;

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
        layout_set_split(child->next, child->spltr != NULL);

        // Unlink and free
        layout_list_remove(child);
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
        adjust_size(lay, 0);

        // Remove this layout from the list
        layout_list_remove(lay);

        // Make sure the first child doesn't have a splitter
        if (lay->parent->child)
            layout_set_split(lay->parent->child, 0);

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

void update_layout_tree(layout *lay, const rect *rc)
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
            update_layout_tree(child, &rcC);
            rcC.left = rcC.right;
        }
        else
        {
            rcC.bottom = rcC.top + splitter_size + child->size;
            update_layout_tree(child, &rcC);
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
    update_layout_tree(lm->root, &rc);
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
