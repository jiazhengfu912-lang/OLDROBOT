#include "servo_bus.h"
#include "usart.h"   // 使用 huart1
#include "gpio.h"

/*
 * 说明：
 * 1. 这里默认 USART1 已经在 CubeMX 中初始化完成；
 * 2. 默认使用 huart1 给舵机发送协议帧；
 * 3. 如果你的舵机侧是通过“驱动板”接入，并且 STM32 侧只需要普通串口发送，
 *    这版代码可以直接使用；
 * 4. 如果后面确认你的驱动板需要严格半双工方向控制，再在这里补 TXEN/RXEN 控制即可。
 */

/**
  * @brief  限幅函数，防止位置和时间超出协议允许范围
  * @param  value: 输入值
  * @param  min: 最小值
  * @param  max: 最大值
  * @retval 限幅后的值
  */
static uint16_t Servo_LimitU16(uint16_t value, uint16_t min, uint16_t max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
  * @brief  计算舵机协议校验和
  * @param  buf: 数据缓冲区
  *         buf[0] = 0x55
  *         buf[1] = 0x55
  *         buf[2] = ID
  *         buf[3] = Length
  *         buf[4] = Cmd
  *         ...
  * @retval 校验和
  *
  * 协议说明：
  * Checksum = ~(ID + Length + Cmd + Param1 + ... + ParamN)
  */
uint8_t Servo_CheckSum(uint8_t *buf)
{
    uint8_t i;
    uint16_t sum = 0;

    /* buf[3] 是 Length，从 ID 开始到 Checksum 一共是 Length 个字节中的前 Length-1 个参与求和 */
    for (i = 2; i < buf[3] + 2; i++)
    {
        sum += buf[i];
    }

    return (uint8_t)(~sum);
}

/**
  * @brief  发送一帧舵机协议数据
  * @param  buf: 数据缓冲区
  * @param  len: 总长度
  * @retval 无
  */
void Servo_SendFrame(uint8_t *buf, uint8_t len)
{
    HAL_UART_Transmit(&huart1, buf, len, 100);
}

/**
  * @brief  单个舵机立即运动到目标位置
  * @param  id: 舵机 ID
  * @param  position: 目标位置，0~1000
  * @param  time: 运动时间，单位 ms，0~30000
  * @retval 无
  *
  * 指令格式：
  * 0x55 0x55 ID Length Cmd PosL PosH TimeL TimeH Checksum
  * Length = 7
  * Cmd    = 1
  */
void Servo_MoveTimeWrite(uint8_t id, uint16_t position, uint16_t time)
{
    uint8_t buf[10];

    /* 限制参数范围 */
    position = Servo_LimitU16(position, SERVO_POS_MIN, SERVO_POS_MAX);
    time     = Servo_LimitU16(time, SERVO_TIME_MIN, SERVO_TIME_MAX);

    buf[0] = SERVO_FRAME_HEADER;
    buf[1] = SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 7;                          // Length
    buf[4] = SERVO_MOVE_TIME_WRITE;      // Cmd
    buf[5] = position & 0xFF;            // Pos Low
    buf[6] = (position >> 8) & 0xFF;     // Pos High
    buf[7] = time & 0xFF;                // Time Low
    buf[8] = (time >> 8) & 0xFF;         // Time High
    buf[9] = Servo_CheckSum(buf);        // Checksum

    Servo_SendFrame(buf, 10);
}

/**
  * @brief  单个舵机预设目标位置，不立即执行
  * @param  id: 舵机 ID
  * @param  position: 目标位置，0~1000
  * @param  time: 运动时间，单位 ms
  * @retval 无
  *
  * 指令：
  * SERVO_MOVE_TIME_WAIT_WRITE = 7
  */
void Servo_MoveTimeWaitWrite(uint8_t id, uint16_t position, uint16_t time)
{
    uint8_t buf[10];

    position = Servo_LimitU16(position, SERVO_POS_MIN, SERVO_POS_MAX);
    time     = Servo_LimitU16(time, SERVO_TIME_MIN, SERVO_TIME_MAX);

    buf[0] = SERVO_FRAME_HEADER;
    buf[1] = SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 7;
    buf[4] = SERVO_MOVE_TIME_WAIT_WRITE;
    buf[5] = position & 0xFF;
    buf[6] = (position >> 8) & 0xFF;
    buf[7] = time & 0xFF;
    buf[8] = (time >> 8) & 0xFF;
    buf[9] = Servo_CheckSum(buf);

    Servo_SendFrame(buf, 10);
}

/**
  * @brief  启动预设动作
  * @param  id: 舵机 ID，若要多个舵机同时动作，可分别预设后再对各个舵机发送启动
  * @retval 无
  *
  * 数据帧：
  * 0x55 0x55 ID 3 11 Checksum
  */
void Servo_MoveStart(uint8_t id)
{
    uint8_t buf[6];

    buf[0] = SERVO_FRAME_HEADER;
    buf[1] = SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = SERVO_MOVE_START;
    buf[5] = Servo_CheckSum(buf);

    Servo_SendFrame(buf, 6);
}

/**
  * @brief  停止舵机运动
  * @param  id: 舵机 ID
  * @retval 无
  */
void Servo_MoveStop(uint8_t id)
{
    uint8_t buf[6];

    buf[0] = SERVO_FRAME_HEADER;
    buf[1] = SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = SERVO_MOVE_STOP;
    buf[5] = Servo_CheckSum(buf);

    Servo_SendFrame(buf, 6);
}

/**
  * @brief  回到初始姿态
  * @param  无
  * @retval 无
  *
  * 说明：
  * 这里用“预设 + 同步启动”的方式，让 3 个舵机尽量同时开始动作。
  * 你可以根据你的机械结构修改位置值。
  */
void Servo_InitPose(void)
{
    uint16_t move_time = 1000;  // 1 秒到位

    /* 先预设三个舵机的位置 */
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 610, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_2, 480, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_3, 490, move_time);
    HAL_Delay(5);

    /* 再统一启动 */
    Servo_MoveStart(SERVO_ID_1);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_2);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_3);

    /* 等待动作完成 */
    HAL_Delay(move_time + 100);
}

/**
  * @brief  动作组 1
  * @param  无
  * @retval 无
  *
  * 示例动作：3 个舵机运动到不同位置
  */
void Action_Group1(void)
{
    uint16_t move_time = 800;

    Servo_MoveTimeWaitWrite(SERVO_ID_1, 300, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_2, 700, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_3, 500, move_time);
    HAL_Delay(5);

    Servo_MoveStart(SERVO_ID_1);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_2);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_3);

    HAL_Delay(move_time + 100);
}

/**
  * @brief  动作组 2
  * @param  无
  * @retval 无
  */
void Action_Group2(void)
{
    uint16_t move_time = 800;

    Servo_MoveTimeWaitWrite(SERVO_ID_1, 700, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_2, 300, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_3, 650, move_time);
    HAL_Delay(5);

    Servo_MoveStart(SERVO_ID_1);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_2);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_3);

    HAL_Delay(move_time + 100);
}

/**
  * @brief  动作组 3
  * @param  无
  * @retval 无
  */
void Action_Group3(void)
{
    uint16_t move_time = 1000;

    Servo_MoveTimeWaitWrite(SERVO_ID_1, 200, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_2, 500, move_time);
    HAL_Delay(5);

    Servo_MoveTimeWaitWrite(SERVO_ID_3, 800, move_time);
    HAL_Delay(5);

    Servo_MoveStart(SERVO_ID_1);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_2);
    HAL_Delay(2);
    Servo_MoveStart(SERVO_ID_3);

    HAL_Delay(move_time + 100);
}