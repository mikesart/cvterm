#ifndef __KEY_H__
#define __KEY_H__

#include <tickit.h>
#include <stdint.h>

int char_message_data(const TickitKeyEventInfo *info, message_data *data);
int key_message_data(const TickitKeyEventInfo *info, message_data *data);

#endif //__KEY_H__
