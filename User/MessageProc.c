
#include <string.h>
#include <RTL.h>
#include <rl_usb.h>

#include "Application.h"
#include "USART.h"
#include "USBD_User_HID.h"
#include "Command.h"
#include "crc16.h"
#include "MessageProc.h"


#define CHAR_STX	0x02
#define CHAR_ETX	0x03
#define CHAR_ACK	0x06
#define CHAR_BEL	0x07

#define MAX_MSG_SIZE		1024

static uint8_t _cmdMsgBuf[MAX_MSG_SIZE];
static int 	_cmdMsgLen;
static uint8_t _respMsgBuf[MAX_MSG_SIZE];

IOBuffer *_inQue = &HID_InQue;


void OutputStatusDetail(uint8_t *buf);


/**************************************************************
*
* .从当前的输入队列中获取命令消息
*
***************************************************************/

BOOL WaitForMessage(void)
{
    uint32_t t0 = os_time_get();

    // 等待 BEL
    while (1)
    {
        if (IOB_BytesCanRead(&HID_InQue) > 0)
        {
            IOB_ReadByte(&HID_InQue, _cmdMsgBuf);
            if (_cmdMsgBuf[0] == CHAR_BEL)
            {
                _inQue = &HID_InQue;
                break;
            }

        }
        else if (IOB_BytesCanRead(&USART1_InQue) > 0)
        {
            IOB_ReadByte(&USART1_InQue, _cmdMsgBuf);
            if (_cmdMsgBuf[0] == CHAR_BEL)
            {
                _inQue = &USART1_InQue;
                break;
            }
        }

        if (os_time_get() - t0 > 10)
            return __FALSE;
    }

    // 等待 STX LENGTH
    t0 = os_time_get();
    while (1)
    {
        if (IOB_BytesCanRead(_inQue) >= 3)
        {
            IOB_ReadData(_inQue, &_cmdMsgBuf[1], 3);
            if (_cmdMsgBuf[1] == CHAR_STX)
            {
                _cmdMsgLen = 4 + (_cmdMsgBuf[2] + (_cmdMsgBuf[3] << 8)) + 8;
                if (_cmdMsgLen >= MAX_MSG_SIZE)
                    return __FALSE;

                break;
            }
        }
        if (os_time_get() - t0 > 100)
            return __FALSE;
    }

    int n = _cmdMsgLen - 4;
	t0 = os_time_get();
	
    while (1)
    {
        if (IOB_BytesCanRead(_inQue) >= n)
        {
            IOB_ReadData(_inQue, &_cmdMsgBuf[4], n);
            break;
        }

        if (os_time_get() - t0 > 2000)
            return __FALSE;
    }

    uint16_t crc1 = CalcCRC16(0, _cmdMsgBuf, _cmdMsgLen - 2);
    uint16_t crc2 = _cmdMsgBuf[_cmdMsgLen-2] + (_cmdMsgBuf[_cmdMsgLen-1] << 8);
    if (crc1 != crc2)
        return __FALSE;

    return __TRUE;
}



/**************************************************************
*
* 处理当前的命令消息。
*
***************************************************************/

void HandleCurrentMessage(void)
{
    uint16_t crc;
    uint16_t respMsgLen = 0;
    uint16_t datalen;

    CMDContext ctx;
    ctx.param = &_cmdMsgBuf[5]; // 设置命令参数
    ctx.paramLen = _cmdMsgLen - 13; // 设置命令参数长度
    ctx.out = &_respMsgBuf[16]; // 设置响应参数输出指针
    ctx.outLen = 0;

    int ret = ExecCommand(_cmdMsgBuf[4], &ctx);

    datalen = ctx.outLen + 12;
    _respMsgBuf[respMsgLen++] = CHAR_ACK;
    _respMsgBuf[respMsgLen++] = CHAR_STX;
    _respMsgBuf[respMsgLen++] = (uint8_t)(datalen);
    _respMsgBuf[respMsgLen++] = (uint8_t)(datalen >> 8);
    _respMsgBuf[respMsgLen++] = _cmdMsgBuf[4]; // 指令码

    if (ret == 0)
    {
        _respMsgBuf[respMsgLen++] = 's';
        _respMsgBuf[respMsgLen++] = 0;
        _respMsgBuf[respMsgLen++] = 0;
    }
    else
    {
        _respMsgBuf[respMsgLen++] = 'e';
        _respMsgBuf[respMsgLen++] = (uint8_t)ret;
        _respMsgBuf[respMsgLen++] = (uint8_t)(ret >> 8);
    }

    OutputStatusDetail(&_respMsgBuf[respMsgLen]);
    respMsgLen += 6;

    _respMsgBuf[respMsgLen++] = 0;
    _respMsgBuf[respMsgLen++] = 0;

    respMsgLen += ctx.outLen;

    // 从当前命令消息缓冲区中复制ID
    _respMsgBuf[respMsgLen++] = _cmdMsgBuf[_cmdMsgLen - 8];
    _respMsgBuf[respMsgLen++] = _cmdMsgBuf[_cmdMsgLen - 7];
    _respMsgBuf[respMsgLen++] = _cmdMsgBuf[_cmdMsgLen - 6];
    _respMsgBuf[respMsgLen++] = _cmdMsgBuf[_cmdMsgLen - 5];
    _respMsgBuf[respMsgLen++] = CHAR_ACK;
    _respMsgBuf[respMsgLen++] = CHAR_ETX;

    crc = CalcCRC16(crc, _respMsgBuf, respMsgLen);
    _respMsgBuf[respMsgLen++] = (uint8_t)crc;
    _respMsgBuf[respMsgLen++] = (uint8_t)(crc >> 8);

    if (_inQue == &USART1_InQue)
        USART1_Send(_respMsgBuf, respMsgLen);
    else
        User_HID_Send(_respMsgBuf, respMsgLen);
}


/**************************************************************
*
* 设置状态明细。
*
***************************************************************/

void OutputStatusDetail(uint8_t *buf)
{
    buf[0] = c_appStatusRef->deviceStat;

    if (c_appStatusRef->currentSlot != SLOT_INIT_POS)
        buf[1] = c_appStatusRef->currentSlot + 3;
    else
        buf[1] = 0x1;

    buf[2] = c_sensorDataRef->isCardInFront ? 0x8 : 0;
    buf[2] += c_appStatusRef->ejectFlag ? 0x4: 0;

    if (c_sensorDataRef->retainBinStat != RETAINBIN_NOT_INPLACE)
    {
        if (c_sensorDataRef->retainBinStat == RETAINBIN_NOT_FULL)
            buf[2] += 1;
        else
            buf[2] += 2;
    }

    buf[3] = c_sensorDataRef->isCardInSlots[4] ? 1 : 2;

    buf[4] = c_sensorDataRef->isCardInSlots[3] ? 1 : 2;
    buf[4] <<= 2;
    buf[4] += c_sensorDataRef->isCardInSlots[2] ? 1 : 2;
    buf[4] <<= 2;
    buf[4] += c_sensorDataRef->isCardInSlots[1] ? 1 : 2;
    buf[4] <<= 2;
    buf[4] += c_sensorDataRef->isCardInSlots[0] ? 1 : 2;

    buf[5] = 0;
}

