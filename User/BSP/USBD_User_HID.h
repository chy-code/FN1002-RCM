#ifndef _USBD_USER_HID_H
#define _USBD_USER_HID_H

#include "IOBuffer.h"

extern IOBuffer HID_InQue;

void User_HID_Send(uint8_t *data, int datalen);

#endif
