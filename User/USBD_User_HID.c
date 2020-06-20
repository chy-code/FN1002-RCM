/*----------------------------------------------------------------------------
 *      RL-ARM - USB
 *----------------------------------------------------------------------------
 *      Name:    usbd_user_hid.c
 *      Purpose: Human Interface Device Class User module
 *      Rev.:    V4.70
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include <RTL.h>
#include <rl_usb.h>
#include <stm32f10x.h>
#include <string.h>

#include "USBD_User_HID.h"

#define __NO_USB_LIB_C
#include "USB_Config.c"


uint8_t outBuf[USBD_HID_OUTREPORT_MAX_SZ];

IOBuffer HID_InQue = { 0 };
uint8_t feat;

void User_HID_Send(uint8_t *data, int datalen)
{
    uint8_t sz;
    uint8_t *p = data;

    while (datalen > 0)
    {
        sz = USBD_HID_OUTREPORT_MAX_SZ - 1;
        if (datalen < sz)
            sz = datalen;

        outBuf[0] = sz;
        memcpy(&outBuf[1], p, sz);
        usbd_hid_get_report_trigger(0, outBuf, USBD_HID_OUTREPORT_MAX_SZ);
        datalen -= sz;
        p += sz;
		
		os_dly_wait(3);
    }
}


// 回调函数

void usbd_hid_init (void)
{
}


int usbd_hid_get_report (U8 rtype, U8 rid, U8 *buf, U8 req)
{
    switch (rtype)
    {
    case HID_REPORT_INPUT:
        switch (rid)
        {
        case 0:
            switch (req)
            {
            case USBD_HID_REQ_EP_CTRL:
            case USBD_HID_REQ_PERIOD_UPDATE:
                return 0;
			
            case USBD_HID_REQ_EP_INT:
                break;
            }
            break;
        }
        break;
    case HID_REPORT_FEATURE:
        buf[0] = feat;
        return 1;
    }
	
    return 0;
}


void usbd_hid_set_report (U8 rtype, U8 rid, U8 *buf, int len, U8 req)
{
    switch (rtype)
    {
    case HID_REPORT_OUTPUT:
        if (buf[0] <= len - 1)
            IOB_BufferData(&HID_InQue, &buf[1], buf[0]);
        break;

    case HID_REPORT_FEATURE:
        feat = buf[0];
        break;
    }
}
