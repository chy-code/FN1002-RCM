
#include <string.h>

#include "stm32f10x.h"
#include "Application.h"
#include "rl_usb.h"

#include "UserData.h"
#include "USART.h"
#include "USBD_User_HID.h"
#include "MessageProc.h"
#include "LED.h"
#include "Sensor.h"
#include "Stepper.h"
#include "Button.h"
#include "IWDG.h"


#define STEPS_PER_REV	1600	// 步进电机转一圈的步数
#define MAX_STEPS_FOR_LIFTING	( STEPS_PER_REV * 30 )


// 任务优先级定义
#define PRIO_IDLE		0
#define PRIO_NORMAL		1
#define PRIO_HIGH		2


/*------------------------------------------------------------------
* 变量定义
*------------------------------------------------------------------*/

// 应用状态数据
// 在执行动作时更新
AppStatus _appStatus =
{
    DEVICE_IDLE, SLOT_INIT_POS, 0, __FALSE, __FALSE, __FALSE
};

// 引用_appStatus, 用于外部访问. 在 command.c 及 cu.c 中使用
const AppStatus * const c_appStatusRef =
    (const AppStatus*)&_appStatus;


// 传感器数据. 在 IdleTask 中更新
SensorData _sensorData = { 0 };

// 引用 _sensorData, 用于外部访问. 在 command.c 及 cu.c 中使用
const SensorData * const c_sensorDataRef =
    (const SensorData*)&_sensorData;


// 版本数据
const VersionData c_verData =
{
    "vF200613.1",
    "vF200613.1",
    "vF200613.1"
};

// 临时变量, 在 ResetDeviceTask 和 MoveCardTask 中使用.
const uint16_t _cSlotSenFlags[] =
{
    SEN_SLOT1, SEN_SLOT2, SEN_SLOT3, SEN_SLOT4, SEN_SLOT5
};


// 信号
static OS_SEM _sem_resetFinished;
static OS_SEM _sem_selectFinished;
static OS_SEM _sem_movingFinished;
static OS_SEM _sem_stopMoving;
static OS_SEM _sem_testFinished;


/*------------------------------------------------------------------
* 函数声明
*------------------------------------------------------------------*/
__task void InitTask(void);
__task void CommunicationTask(void);
__task void IdleTask(void);
__task void ResetDeviceTask(void);
__task void SelectSlotTask(void *argv);
__task void MoveCardTask(void *argv);
__task void PerformanceTestTask(void *argv);

void UpdateSensorData(void);
int ResetCam(Direction dir);
int SetStackToTop(void);
int SetStackToBottom(void);
int PositionSlot(int curSlot, int targetSlot);


/*------------------------------------------------------------------
* 应用程序入口
*------------------------------------------------------------------*/
int main()
{
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x4000);

    USART1_Init(38400, USART_Parity_No);

    LEDs_Init();
    Sensors_Init();
    Steppers_Init();
    ResetButton_Enable(); // 使能复位按键, 当按下时复位
    IWDG_Init(IWDG_PRE_CYCLE_6P4, 1562); // 喂狗间隔 6.4ms * 1562 = 10s

    InitUserDataROM();

    os_sys_init(InitTask);
}


/*------------------------------------------------------------------
* 初始化任务
*------------------------------------------------------------------*/
__task void InitTask(void)
{
    os_tsk_prio_self(0xff); // 提升当前任务最高优先级
    usbd_init();	// 初始化 USB 库
    usbd_connect(__TRUE); // 连接 USB 设备
    os_tsk_prio_self(1); // 降低当前任务优先级

    // 创建 Idle 和通信任务
    os_tsk_create(IdleTask, PRIO_IDLE);
    os_tsk_create(CommunicationTask, PRIO_NORMAL);

    // 复位设备
    StartResetDevice(__FALSE);

    os_tsk_delete_self();
}


/*------------------------------------------------------------------
* 通信任务
*------------------------------------------------------------------*/
__task void CommunicationTask(void)
{
    for (;;)
    {
        if (WaitForMessage())
		{
            HandleCurrentMessage();
		}
        else
		{
            os_tsk_pass();
		}
    }
}


/*------------------------------------------------------------------
* 空闲任务
*------------------------------------------------------------------*/

__task void IdleTask(void)
{
    const LEDType leds[] =
    {
        LED_SLOT1, LED_SLOT2, LED_SLOT3, LED_SLOT4, LED_SLOT5
    };

    uint32_t t1, t0 = os_time_get();

    for (;;)
    {
        t1 = os_time_get();
        if (t1 - t0 > 1000)
        {
            UpdateSensorData();

            for (int i = 0; i < NUM_PHYSICAL_SLOTS; i++)
            {
                if (_sensorData.isCardInSlots[i])
                    LED_On(leds[i]);
                else
                    LED_Off(leds[i]);
            }

            if (_sensorData.retainBinStat == RETAINBIN_FULL)
                LED_On(LED_RBFULL);
            else
                LED_Off(LED_RBFULL);

            if (_appStatus.deviceStat == DEVICE_FAULT)
                LED_On(LED_ERROR);
            else
                LED_Off(LED_ERROR);

            t0 = t1;
        }


        LED_ToggleState(LED_GREEN);
		IWDG_Feed();
		
        os_dly_wait(500);
    }
}


/*------------------------------------------------------------------
* 复位设备任务
*------------------------------------------------------------------*/

__task void ResetDeviceTask(void)
{
    int ret = 0;
    uint16_t prevVal, curVal;

	 _appStatus.deviceStat = DEVICE_BUSY;
    curVal = Sensor_Read(SEN_GATE);
	
    if (curVal) // 进卡口处有卡
    {
        prevVal = Sensor_Read(SEN_SLOTS); // 获取所有卡槽的状态
		
		// 启动步进电机
        Stepper_Start(STEPPER_IN_OUT, DIR_BACKWARD_OR_DOWN, STEPS_PER_REV * 20);

        while (1)
        {
            curVal = Sensor_Read(SEN_GATE | SEN_SLOTS);
            if (!(curVal & SEN_GATE)) // 进卡口处无卡
            {
                for (int i = 0; i < NUM_PHYSICAL_SLOTS; i++)
                {
                    if (!(prevVal & _cSlotSenFlags[i])
                            && (curVal & _cSlotSenFlags[i]) )
                    {
                        // 某个卡槽从无卡变为有卡状态时, 认为进卡完成
                        Stepper_Stop(STEPPER_IN_OUT);
                        goto NEXT_STEP;
                    }
                }

            }

            if (!Stepper_IsRunning(STEPPER_IN_OUT))
            {
                ret = ERR_CARD_JAM;
                goto END;
            }
			
			os_dly_wait(50);
        }
    }

NEXT_STEP:
	ret = ResetCam(DIR_BACKWARD_OR_DOWN);
	if (ret == 0)
	{
		curVal = Sensor_Read(SEN_TOP);
		if (!curVal)
		{
			ret = SetStackToTop();
		}
		else
		{
			ret = SetStackToBottom();
			if (ret == 0)
				ret = SetStackToTop();
		}
	}

END:
    if (ret == 0)
    {
        _appStatus.deviceStat = DEVICE_IDLE;
        _appStatus.currentSlot = SLOT_INIT_POS;
		
        if (!_appStatus.reseted)
            _appStatus.reseted = __TRUE;
    }
    else
    {
        _appStatus.deviceStat = DEVICE_FAULT;
        _appStatus.faultCode = ret;
    }

    os_sem_send(_sem_resetFinished);
    os_tsk_delete_self();
}


/*------------------------------------------------------------------
* 准备卡槽
* argv: 卡槽号 0~4 或 SLOT_INIT_POS, int 类型
*------------------------------------------------------------------*/

__task void SelectSlotTask(void *argv)
{
    int ret, targetSlot = *(int*)argv;

    _appStatus.deviceStat = DEVICE_BUSY;
	
	ret = ResetCam(DIR_BACKWARD_OR_DOWN);
	if (ret == 0)
		ret = PositionSlot(_appStatus.currentSlot, targetSlot);

    if (ret != 0)
    {
        _appStatus.deviceStat = DEVICE_FAULT;
        _appStatus.faultCode = ret;
    }
    else
    {
        _appStatus.deviceStat = DEVICE_IDLE;
        _appStatus.currentSlot = targetSlot;
    }

    os_sem_send(_sem_selectFinished);
    os_tsk_delete_self();
}


/*-----------------------------------------------------------------
* 吞卡或退卡
*------------------------------------------------------------------*/

__task void MoveCardTask( void *argv )
{
    MoveMode mode = (MoveMode)((int*)argv)[0]; // 移动方式
    ControlParameters *cp = (ControlParameters*)(((int*)argv)[1]); // 控制参数
    uint16_t senFlag, prevVal, curVal = 0;
    uint32_t t0, timeout; // 毫秒单位

    switch (mode)
    {
    case MOVE_TO_SLOT:
    case MOVE_TO_RETAINBIN:
        // cp-moveInTimeOut 表示进卡超时, 单位是秒
        // cp->moveInDelay 表示进卡延时, 单位是100毫秒
        timeout = cp->moveInTimeOut * 1000;
        os_dly_wait(cp->moveInDelay * 100);

        Stepper_Start(STEPPER_IN_OUT,
                      DIR_BACKWARD_OR_DOWN,
                      INT32_MAX);
        break;

    case MOVE_TO_READER:
        timeout = cp->moveOutTimeOut * 1000;
        os_dly_wait(cp->moveOutDelay * 100);

        Stepper_Start(STEPPER_IN_OUT,
                      DIR_FORWARD_OR_UP,
                      INT32_MAX);
        break;
    }

	os_tsk_prio_self(PRIO_NORMAL);

    _appStatus.deviceStat = DEVICE_BUSY;
    _appStatus.isMovingCard = __TRUE;

    prevVal = Sensor_Read(SEN_GATE);
    t0 = os_time_get();

    for (;;)
    {
        if (os_sem_wait(_sem_stopMoving, 10) == OS_R_OK)
            break;

		curVal = Sensor_Read(SEN_GATE);
		
        if (mode == MOVE_TO_SLOT)
        {
			if (prevVal && !curVal)
				Stepper_SetLowVelocity(STEPPER_IN_OUT);
			
            senFlag = _cSlotSenFlags[_appStatus.currentSlot];
            curVal = Sensor_Read(senFlag);
            if (curVal & senFlag)
                break;
        }
        else
        {
            if (prevVal && !curVal)
                break;

            prevVal = curVal;
        }

        if (os_time_get() - t0 > timeout)
            break;

        os_tsk_pass();
    }

    Stepper_Stop(STEPPER_IN_OUT);

    if (curVal & SEN_GATE)
    {
        _appStatus.deviceStat = DEVICE_FAULT;
        _appStatus.faultCode = ERR_CARD_JAM;
    }
    else
    {
        _appStatus.deviceStat = DEVICE_IDLE;
		
		if (mode == MOVE_TO_READER)
			_appStatus.ejectFlag = __TRUE;
    }

    _appStatus.isMovingCard = __FALSE;

    os_sem_send(_sem_movingFinished);
    os_tsk_delete_self();
}


/*------------------------------------------------------------------
* 性能测试任务
*------------------------------------------------------------------*/

__task void PerformanceTestTask(void *argv)
{
    PerformanceData *outData = (PerformanceData*)argv;
    int ret = 0;
    uint32_t t0;
	uint16_t senVal;

    _appStatus.deviceStat = DEVICE_BUSY;

    do
    {
		senVal = Sensor_Read(SEN_TOP);
		if (!senVal)
		{
			ret = SetStackToTop();
			if (ret != 0)
				break;
		}

        t0 = os_time_get();
        ret = PositionSlot(SLOT_INIT_POS, 0);
        if (ret != 0)
            break;

        outData->DownTime[0] = (uint16_t)(os_time_get() - t0);
        os_dly_wait(500);

        for (int i = 1; i <= 4; i++)
        {
            t0 = os_time_get();
            ret = PositionSlot(i-1, i);
            if (ret != 0)
                break;

            outData->DownTime[i] = (uint16_t)(os_time_get() - t0);
            os_dly_wait(500);
        }

        for (int i = 4; i >= 1; i--)
        {
            t0 = os_time_get();
            ret = PositionSlot(i, i-1);
            if (ret != 0)
                break;

            outData->UpTime[i] = (uint16_t)(os_time_get() - t0);
            os_dly_wait(500);
        }

        t0 = os_time_get();
        ret = SetStackToTop();
        if (ret != 0)
            break;

        outData->UpTime[0] = (uint16_t)(os_time_get() - t0);

        t0 = os_time_get();
        Stepper_Start(STEPPER_IN_OUT, DIR_BACKWARD_OR_DOWN, STEPS_PER_REV);
        while (Stepper_IsRunning(STEPPER_IN_OUT))
            os_dly_wait(10);

        outData->TimePerRev1 = (uint16_t)(os_time_get() - t0);
        outData->TimePerRev2 = outData->TimePerRev1;
    }
    while (0);

    if (ret != 0)
    {
        _appStatus.deviceStat = DEVICE_FAULT;
        _appStatus.faultCode = ret;
    }
    else
    {
        _appStatus.deviceStat = DEVICE_IDLE;
		_appStatus.currentSlot = SLOT_INIT_POS;
    }

    os_sem_send(_sem_testFinished);
    os_tsk_delete_self();
}


/*------------------------------------------------------------------
* 复位设备
*------------------------------------------------------------------*/

int StartResetDevice(BOOL wait)
{
    os_sem_init(_sem_resetFinished, 0);
    os_tsk_create(ResetDeviceTask, PRIO_HIGH);

    if (wait)
    {
        os_sem_wait(_sem_resetFinished, 0xffff);

        if (_appStatus.deviceStat == DEVICE_FAULT)
            return _appStatus.faultCode;
    }

    return 0;
}


/*------------------------------------------------------------------
* 启动选择卡槽任务
*------------------------------------------------------------------*/

int StartSelectSlot(int targetSlot)
{
    static int argv;
    argv = targetSlot;

    os_sem_init(_sem_selectFinished, 0);
    os_tsk_create_ex(SelectSlotTask, PRIO_HIGH, &argv);
    os_sem_wait(_sem_selectFinished, 0xffff);

    if (_appStatus.deviceStat == DEVICE_FAULT)
        return _appStatus.faultCode;

    return 0;
}


/*------------------------------------------------------------------
* 启动吞卡/退卡任务
*------------------------------------------------------------------*/

int StartMoveCard(MoveMode mode)
{
    static int argv[2];
    static ControlParameters cp;

    if (!ReadControlParameters(&cp))
        return ERR_READ_CONTROL_PARAMETERS;

    argv[0] = mode;
    argv[1] = (int)&cp;

    os_sem_init(_sem_movingFinished, 0);
    os_sem_init(_sem_stopMoving, 0);

    os_tsk_create_ex(MoveCardTask, PRIO_HIGH, &argv);

    return 0;
}


/*------------------------------------------------------------------
* 停止吞卡/退卡任务
*------------------------------------------------------------------*/

int StopMoveCard(void)
{
    if (_appStatus.isMovingCard)
    {
        os_sem_send(_sem_stopMoving);
        os_sem_wait(_sem_movingFinished, 0xffff);
    }
	
	return 0;
}


/*------------------------------------------------------------------
* 启动性能测试任务
*------------------------------------------------------------------*/

int StartPerformanceTest(PerformanceData *outData)
{
    memset(outData, 0, sizeof(PerformanceData));

    os_sem_init(_sem_testFinished, 0);
    os_tsk_create_ex(PerformanceTestTask, PRIO_HIGH, outData);
    os_sem_wait(_sem_testFinished, 0xffff);

    if (_appStatus.deviceStat == DEVICE_FAULT)
        return _appStatus.faultCode;

    return 0;
}


/*------------------------------------------------------------------
* 更新传感器数据 _sensorData
*------------------------------------------------------------------*/

void UpdateSensorData(void)
{
    uint16_t senVal = Sensor_Read(SEN_ALL);

    _sensorData.isCardInFront = (senVal & SEN_GATE) ? 1 : 0;
    _sensorData.isRearCoverOpened = (senVal & SEN_REAR_COVER) ? 0 : 1;

    if (senVal & SEN_RB_INPLACE) // 回收箱传感器覆盖
        _sensorData.retainBinStat = (senVal & SEN_RB_HIGH) ?
                                    RETAINBIN_FULL : RETAINBIN_NOT_FULL;
    else
        _sensorData.retainBinStat = RETAINBIN_NOT_INPLACE;

    _sensorData.isCardInSlots[0] = (senVal & SEN_SLOT1) ? 1 : 0;
    _sensorData.isCardInSlots[1] = (senVal & SEN_SLOT2) ? 1 : 0;
    _sensorData.isCardInSlots[2] = (senVal & SEN_SLOT3) ? 1 : 0;
    _sensorData.isCardInSlots[3] = (senVal & SEN_SLOT4) ? 1 : 0;
    _sensorData.isCardInSlots[4] = (senVal & SEN_SLOT5) ? 1 : 0;
}


/*------------------------------------------------------------------
* 复位凸轮
*------------------------------------------------------------------*/

int ResetCam(Direction dir)
{
    uint16_t senVal = Sensor_Read(SEN_CAM);
	if (senVal)
		return 0;
	
    Stepper_Start(STEPPER_IN_OUT,
                  dir,
                  STEPS_PER_REV * 2);

    while (1)
    {
        senVal = Sensor_Read(SEN_CAM);
        if (senVal)
            break;

        if (!Stepper_IsRunning(STEPPER_IN_OUT))
            return ERR_SENSOR;
		
		os_dly_wait(10);
    }

    Stepper_Stop(STEPPER_IN_OUT);

    return 0;
}


/*------------------------------------------------------------------
* 设置卡栈到顶部
*------------------------------------------------------------------*/

int SetStackToTop(void)
{
    uint16_t senVal;
	
    Stepper_Start(STEPPER_LIFTING,
                  DIR_FORWARD_OR_UP,
                  MAX_STEPS_FOR_LIFTING);

    while (1)
    {
        senVal = Sensor_Read(SEN_TOP);
        if (senVal)
            break;

        if (!Stepper_IsRunning(STEPPER_LIFTING))
            return ERR_SENSOR;
		
		os_dly_wait(50);
    }

    Stepper_Stop(STEPPER_LIFTING);

    return 0;
}


/*------------------------------------------------------------------
* 设置卡栈到底部
* 在调用该函数之前需保证卡栈在顶部.
*------------------------------------------------------------------*/

int SetStackToBottom(void)
{
    uint16_t senVal;
    int slotsPassed = 0;
    BOOL increment = __TRUE;

    Stepper_Start(STEPPER_LIFTING,
                  DIR_BACKWARD_OR_DOWN,
                  MAX_STEPS_FOR_LIFTING);

    while (slotsPassed < NUM_PHYSICAL_SLOTS)
    {
        senVal = Sensor_Read(SEN_BOTTOM);
        if (senVal)
        {
            increment = __TRUE;
        }
        else  if (increment)
        {
            slotsPassed++;
            increment = __FALSE;
        }

        if (!Stepper_IsRunning(STEPPER_LIFTING))
            return ERR_SENSOR;
		
		os_dly_wait(50);
    }

    Stepper_Stop(STEPPER_LIFTING);

    return 0;
}


/*------------------------------------------------------------------
* 定位指定的卡槽
*------------------------------------------------------------------*/

int PositionSlot(int curSlot, int targetSlot)
{
    uint16_t prevVal, curVal;
    int ret, count;

    if (targetSlot == SLOT_INIT_POS)
    {
        curVal = Sensor_Read(SEN_TOP);
        if (!curVal)
        {
            ret = SetStackToTop();
            if (ret != 0)
                return ret;
        }
    }
    else
    {
        prevVal = Sensor_Read(SEN_BOTTOM);

        if (curSlot == SLOT_INIT_POS)
        {
            count = targetSlot + 1;
            Stepper_Start( STEPPER_LIFTING,
                           DIR_BACKWARD_OR_DOWN,
                           MAX_STEPS_FOR_LIFTING);
        }
        else
        {
            if (targetSlot > curSlot)
            {
                count = targetSlot - curSlot;
                Stepper_Start(STEPPER_LIFTING,
                              DIR_BACKWARD_OR_DOWN,
                              MAX_STEPS_FOR_LIFTING);
            }
            else
            {
                count = curSlot - targetSlot;
                Stepper_Start(STEPPER_LIFTING,
                              DIR_FORWARD_OR_UP,
                              MAX_STEPS_FOR_LIFTING);
            }
        }

        while (count > 0)
        {
            curVal = Sensor_Read(SEN_BOTTOM);
            if (!curVal && (prevVal != curVal))
                count--;

            prevVal = curVal;

            if (!Stepper_IsRunning(STEPPER_LIFTING))
                return ERR_SENSOR;
			
			os_dly_wait(50);
        }

        Stepper_Stop(STEPPER_LIFTING);
    }

    return 0;
}
