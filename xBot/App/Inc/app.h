/**
* @file app.h
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 
* @description: 
**/
#ifndef __APP_H
#define __APP_H

#include "main.h"
#include "led.h"
#include "tb6612.h"
#include "encoder.h"

#pragma pack(1)

typedef struct
{
    unsigned char head1;//数据头1 'D'
    unsigned char head2;//数据头2 'A'
    unsigned char struct_size;//结构体长度

    short encoder1;//编码器当前值1
    short encoder2;//编码器当前值2

    short vbat_mv;//电池电压 mV
    unsigned char charger_connected;//是否连接充电器
    unsigned char fully_charged;//是否充满电

    unsigned char asr_id;//语音命令ID

    unsigned char end1;//数据尾1 'T'
    unsigned char end2;//数据尾2 'A'
    unsigned char end3;//数据尾3 '\r' 0x0d
    unsigned char end4;//数据尾4 '\n' 0x0a
}McuData;


typedef struct
{
    unsigned char head1;//数据头1 'D'
    unsigned char head2;//数据头2 'A'
    unsigned char struct_size;//结构体长度

    short pwm1;//油门PWM1
    short pwm2;//油门PWM2
    unsigned char enable_power;//开启5V雷达电源

    unsigned char end1;//数据尾1 'T'
    unsigned char end2;//数据尾2 'A'
    unsigned char end3;//数据尾3 '\r' 0x0d
    unsigned char end4;//数据尾4 '\n' 0x0a
}CmdData;

#pragma pack()


extern CmdData cmd_data;//香橙派下发的命令
extern McuData mcu_data;//MCU上传的数据

extern unsigned char *mcu_data_ptr;

extern int heart_beat_cnt;
extern int new_recv_flag;
extern unsigned char asr_id;
extern int asr_new_recv_flag;


/**
 * @brief app mian enter
 */
void app_main(void);

#endif //__APP_H
