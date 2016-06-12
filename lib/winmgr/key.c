#include <string.h>
#include "window.h"
#include "message.h"
#include "key.h"
#include "clog.h"

uint8_t convert_modifiers(int mod)
{
    uint8_t modifiers = 0;
    if (mod & TICKIT_MOD_SHIFT)
        modifiers |= MOD_SHIFT;
    if (mod & TICKIT_MOD_ALT)
        modifiers |= MOD_ALT;
    if (mod & TICKIT_MOD_CTRL)
        modifiers |= MOD_CTRL;
    return modifiers;
}

int char_message_data(const TickitKeyEventInfo *info, message_data *data)
{
    memset(data, 0, sizeof(*data));
    strncpy(data->chr.utf8, info->str, sizeof(data->chr.utf8) - 1);
    data->chr.modifiers = convert_modifiers(info->mod);
    return 1;
}

int key_message_data(const TickitKeyEventInfo *info, message_data *data)
{
    // evt holds the result of calling termkey_strfkey
    // (see got_key in term.c), which is a string, making it
    // a pain to parse.
    // TODO: parse it! (see termkey_strpkey)
    clog_info(CLOG(0), "Received TICKIT_KEYEV_KEY: %s", info->str);
    return 0;
}
