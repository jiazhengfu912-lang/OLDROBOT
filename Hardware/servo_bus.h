#ifndef __SERVO_BUS_H
#define __SERVO_BUS_H

#include "main.h"

/* -------------------- 舵机协议常量定义 -------------------- */

/* 协议帧头 */
#define SERVO_FRAME_HEADER              0x55

/* 常用指令 */
#define SERVO_MOVE_TIME_WRITE           1   // 立即转到目标位置
#define SERVO_MOVE_TIME_WAIT_WRITE      7   // 预设位置，不立即执行
#define SERVO_MOVE_START                11  // 启动预设动作
#define SERVO_MOVE_STOP                 12  // 停止运动

/* 舵机 ID 定义，根据你自己的设置修改 */
#define SERVO_ID_1                      1
#define SERVO_ID_2                      2
#define SERVO_ID_3                      3

/* 角度范围：0~1000 对应 0~240° */
#define SERVO_POS_MIN                   0
#define SERVO_POS_MAX                   1000

/* 时间范围：0~30000 ms */
#define SERVO_TIME_MIN                  0
#define SERVO_TIME_MAX                  30000

/* -------------------- 函数声明 -------------------- */

/* 计算校验和 */
uint8_t Servo_CheckSum(uint8_t *buf);

/* 发送一帧数据 */
void Servo_SendFrame(uint8_t *buf, uint8_t len);

/* 单个舵机立即运动 */
void Servo_MoveTimeWrite(uint8_t id, uint16_t position, uint16_t time);

/* 单个舵机预设运动 */
void Servo_MoveTimeWaitWrite(uint8_t id, uint16_t position, uint16_t time);

/* 启动预设动作 */
void Servo_MoveStart(uint8_t id);

/* 停止运动 */
void Servo_MoveStop(uint8_t id);

/* 回到初始姿态 */
void Servo_InitPose(void);

/* 动作组 1 */
void Action_Group1(void);

/* 动作组 2 */
void Action_Group2(void);

/* 动作组 3 */
void Action_Group3(void);

#endif