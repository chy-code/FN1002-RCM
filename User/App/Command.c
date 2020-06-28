#include <RTL.h>
#include <string.h>
#include <stm32f10x.h>

#include "Application.h"
#include "UserData.h"
#include "Command.h"
#include "Stepper.h"



/**************************************************************
* 设备初始化。
***************************************************************/

static int DeviceReset(CMDContext *ctx)
{
    ctx->outLen = 68; // 设置保留参数长度

    if (c_appStatusRef->deviceStat == DEVICE_BUSY)
        return ERR_COMMAND_SEQUENCE;

    switch (ctx->param[62])
    {
    case 0: // 复位升降梯
        return StartResetDevice(__TRUE);

    case 1:// 不复位升降梯
        break;
    }

    return 0;
}


/**************************************************************
* 获取设备状态。
***************************************************************/

static int GetDeviceStatus(CMDContext *ctx)
{
    ctx->outLen = 68;
	
	// 不需要做处理, IdleTask 会更新相关状态.
	
    return 0;
}


/**************************************************************
* 准备卡槽。
***************************************************************/

static int SelectSlot(CMDContext *ctx)
{
    int slotNum;
    uint16_t slotsFunc[NUM_RCM_SLOTS];

    ctx->outLen = 68;

    if (!c_appStatusRef->reseted)
        return ERR_NO_RESET;
	
	if (c_appStatusRef->deviceStat != DEVICE_IDLE)
		return ERR_COMMAND_SEQUENCE;
	
    slotNum = ctx->param[7];

    if (!(slotNum >= 1 && slotNum <= NUM_PHYSICAL_SLOTS))
    {
        if (slotNum != SLOT_INIT_POS)
            return ERR_INVALID_SLOT_NUM;
    }

    if (slotNum != SLOT_INIT_POS)
    {
        if (!ReadSlotSettings(slotsFunc))
            return ERR_READ_SLOTS_FUNC;

        if (slotsFunc[slotNum - 1] == SLOT_FUNC_DISABLE)
            return ERR_SLOT_DISABLED;
    }

    if (slotNum != c_appStatusRef->currentSlot)
        return StartSelectSlot(slotNum - 1);

    return 0;
}


/**************************************************************
* 启动或停止退卡或吞卡动作。
***************************************************************/

static int MoveCard(CMDContext *ctx)
{
    uint8_t mode = ctx->param[61];
    uint8_t curSlot = c_appStatusRef->currentSlot;
    int retainCount = 0;
    ControlParameters cp;

    ctx->outLen = 68;

    if (!c_appStatusRef->reseted)
        return ERR_NO_RESET;

	if (mode != 0)
	{ 
		if (c_appStatusRef->deviceStat == DEVICE_BUSY)
			return ERR_COMMAND_SEQUENCE;
	}

    switch (mode)
    {
    case 0:
        return StopMoveCard();

    case 1: // 吞卡
        if (curSlot == SLOT_INIT_POS)
            return ERR_INV_POS_FOR_INOUT;

        if (c_sensorDataRef->isCardInSlots[curSlot])
            return ERR_CARD_IN_SLOTn + curSlot;

        return StartMoveCard(MOVE_TO_SLOT);

    case 2:	// 退卡
        if (curSlot == SLOT_INIT_POS)
            return ERR_INV_POS_FOR_INOUT;

        if (!c_sensorDataRef->isCardInSlots[curSlot])
            return ERR_NO_CARD_IN_SLOTn + curSlot;

        return StartMoveCard(MOVE_TO_READER);

    case 3: // 永久吞卡

        // 检查回收箱是否到位
        if (c_sensorDataRef->retainBinStat == RETAINBIN_NOT_INPLACE)
            return ERR_RETAINBIN_ONT_INPLACE;

        if (curSlot != SLOT_INIT_POS)
            return ERR_INV_POS_FOR_RETAIN;

        ReadRetainCount(&retainCount);

        if (!ReadControlParameters(&cp))
            return ERR_READ_CONTROL_PARAMETERS;

        if (retainCount >= cp.retainCapacity)
            return ERR_RETAINBIN_FULL;

        return StartMoveCard(MOVE_TO_RETAINBIN);
    }

    return 0;
}


/**************************************************************
* 获取卡槽状态。
***************************************************************/

static int GetStatusForSlots(CMDContext *ctx)
{
    int rlen = 0;
    uint16_t slotsFunc[NUM_RCM_SLOTS];

    ctx->outLen = 68;

    if (!ReadSlotSettings(slotsFunc))
        return ERR_READ_SLOTS_FUNC;

    for (int i = 0; i < NUM_PHYSICAL_SLOTS; i++)
    {
        ctx->out[rlen++] = slotsFunc[i];
        ctx->out[rlen++] = 1; // 物理上存在
    }

    for (int i = NUM_PHYSICAL_SLOTS; i < NUM_RCM_SLOTS; i++)
    {
        ctx->out[rlen++] = SLOT_FUNC_DISABLE;
        ctx->out[rlen++] = 0; // 物理上不存在
    }

    return 0;
}


/**************************************************************
* 设置卡槽中的卡片信息。
***************************************************************/

static int SetCardInfoForSlot(CMDContext *ctx)
{
    uint8_t slotNum = ctx->param[7];

    ctx->outLen = 68;

    if (! (slotNum >= 1 && slotNum <= NUM_PHYSICAL_SLOTS))
        return ERR_INVALID_SLOT_NUM;

    CardInfo cardInfo;
    memcpy(cardInfo.cardnum, &ctx->param[8], SZ_CARD_NUM);
    memcpy(cardInfo.idinfo, &ctx->param[8 + SZ_CARD_NUM], SZ_ID_INFO);
    cardInfo.valid = __TRUE;

    if (! ProgramCardInfoSlot(slotNum - 1, &cardInfo))
        return ERR_PROGRAM_CARD_INFO;

    return 0;
}



/**************************************************************
* 获取卡片信息。
***************************************************************/

static int GetCardInfoForSlots(CMDContext *ctx)
{
    int rlen = 0;
    ctx->outLen = 448;

    for (int i = 0; i < NUM_RCM_SLOTS; i++)
    {
        if (!ReadCardInfoSlot(i, (CardInfo*)&ctx->out[rlen]))
            return ERR_READ_CARD_INFO;

        rlen += sizeof(CardInfo);
    }

    return 0;
}



/**************************************************************
* 更新回收计数。
***************************************************************/
static int UpdateRetainCount(CMDContext *ctx)
{
    uint8_t mode = ctx->param[62];
    int retainCount = 0;

    ctx->outLen = 68;
    ReadRetainCount(&retainCount);

    switch (mode)
    {
    case 0: // 读取计数
        break;

    case 1: // 计数加1
        retainCount++;
        ProgramRetainCount(retainCount);
        break;

    case 2: // 清零计数
        retainCount = 0;
        ProgramRetainCount(retainCount);
        break;
    }

    ctx->out[47] = (uint8_t)retainCount;

    return 0;
}


/**************************************************************
* 获取版本信息。
***************************************************************/
static int GetVersionInfo(CMDContext *ctx)
{
    int rlen = 17;
    ctx->outLen = 68;

    memcpy(&ctx->out[rlen], c_verData.fw_ver, 10);
    rlen += 10;

    memcpy(&ctx->out[rlen], c_verData.hw_ver, 10);
    rlen += 10;

    memcpy(&ctx->out[rlen], c_verData.struct_ver, 10);

    return 0;
}



/**************************************************************
* 恢复出厂设置。
***************************************************************/

static int RestoreFactoryDefaults(CMDContext *ctx)
{
    ctx->outLen = 36;

    if (! ProgramDefaultControlParameters())
        return ERR_PROGRAM_CONTROL_PARAMETERS;

    return 0;
}


/**************************************************************
* 器件控制。
***************************************************************/

static int StepperOrLEDControl(CMDContext *ctx)
{
	uint8_t type = ctx->param[20];
    uint8_t *op = &ctx->param[21];
    StepperType which;
	
	ctx->outLen = 36;
	
    switch (type)
    {
    case 0x01: // 马达
        if (op[0] == 1)
            which = STEPPER_LIFTING;
        else
            which = STEPPER_IN_OUT;

        switch (op[1])
        {
        case 0:
            Stepper_Stop(which);
            break;
		
        case 1:
            if (!Stepper_IsRunning(which))
                Stepper_Start(which, DIR_FORWARD_OR_UP, 0xffff);
            else
                Stepper_SetDirection(which, DIR_FORWARD_OR_UP);
            break;
			
        case 2:
            if (!Stepper_IsRunning(which))
                Stepper_Start(which, DIR_BACKWARD_OR_DOWN, 0xffff);
            else
                Stepper_SetDirection(which, DIR_BACKWARD_OR_DOWN);

            break;
        }
        break;

    case 0x02: // LED
        break;
    }

    return 0;
}


/**************************************************************
* 性能测试。
***************************************************************/

static int PerformanceTest(CMDContext *ctx)
{
    ctx->outLen = sizeof(PerformanceData);
	
	if (c_appStatusRef->deviceStat != DEVICE_IDLE)
		return ERR_COMMAND_SEQUENCE;
	
    return StartPerformanceTest((PerformanceData*)ctx->out);
}


/**************************************************************
* 设置控制参数。
***************************************************************/

static int SetControlParameters(CMDContext *ctx)
{
    ControlParameters *cp = (ControlParameters*)ctx->param;
    ctx->outLen = 36;

    if ( !(cp->moveOutDelay <= 100) )
        return ERR_PARAM_MOVE_OUT_DELAY;

    if ( !(cp->moveOutTimeOut <= 100) )
        return ERR_PARAM_MOVE_OUT_TIMEOUT;

    if ( !(cp->moveInDelay <= 100) )
        return ERR_PARAM_MOVE_IN_DELAY;

    if ( !(cp->moveInTimeOut <= 100) )
        return ERR_PARAM_MOVE_IN_TIMEOUT;

    if ( !(cp->retryTimes <= 5) )
        return ERR_PARAM_RETRY_TIMES;

    if ( !(cp->retainCapacity >= 4 && cp->retainCapacity <= 8) )
        return ERR_PARAM_RETAIN_CAPACITY;

    if ( !ProgrmaControlParameters(cp) )
        return ERR_PROGRAM_CONTROL_PARAMETERS;

    return 0;
}


/**************************************************************
* 获取控制参数。
***************************************************************/

static int GetControlParameters(CMDContext *ctx)
{
    ctx->outLen = 36;

    if (!ReadControlParameters((ControlParameters *)ctx->out))
        return ERR_READ_CONTROL_PARAMETERS;

    return 0;
}


/**************************************************************
* 设置卡槽功能。
***************************************************************/

static int SetFunctionAllSlots(CMDContext *ctx)
{
    ctx->outLen = 36;

    if (!ProgramSlotSettings((uint16_t*)&ctx->out[7]))
        return ERR_PROGRAM_SLOTS_FUNC;

    return 0;
}


/**************************************************************
* 设置设备信息。
***************************************************************/

static int SetDeviceInfo(CMDContext *ctx)
{
    ctx->outLen = 36;
    ProgramDeviceInfo(ctx->out);
    return 0;
}


/**************************************************************
* 获取设备信息。
***************************************************************/

static int GetDeviceInfo(CMDContext *ctx)
{
    ctx->outLen = 36;
    ReadDeviceInfo(ctx->out);
    return 0;
}


/**************************************************************
* 执行命令。
***************************************************************/

int ExecCommand(uint8_t cc, CMDContext *ctx)
{
    int ret;
    switch (cc)
    {
    case 0x49:
        ret = DeviceReset(ctx);
        break;
    case 0x51:
        ret = GetDeviceStatus(ctx);
        break;
    case 0x50:
        ret = SelectSlot(ctx);
        break;
    case 0x44:
        ret = MoveCard(ctx);
        break;
    case 0x47:
        ret = GetStatusForSlots(ctx);
        break;
    case 0x53:
        ret = SetCardInfoForSlot(ctx);
        break;
    case 0x46:
        ret = GetCardInfoForSlots(ctx);
        break;
    case 0x55:
        ret = UpdateRetainCount(ctx);
        break;
    case 0x56:
        ret = GetVersionInfo(ctx);
        break;
    case 0x66:
        ret = RestoreFactoryDefaults(ctx);
        break;
	case 0x64:
        ret = StepperOrLEDControl(ctx);
        break;
    case 0x6D:
        ret = PerformanceTest(ctx);
        break;
    case 0x73:
        ret = SetControlParameters(ctx);
        break;
    case 0x67:
        ret = GetControlParameters(ctx);
        break;
    case 0x63:
        ret = SetFunctionAllSlots(ctx);
        break;
    case 0x61:
        ret = GetDeviceInfo(ctx);
        break;
    case 0x62:
        ret = SetDeviceInfo(ctx);
        break;
    default:
        ret = ERR_UNDEFINED_COMMAND;
        break;
    }

    return ret;
}
