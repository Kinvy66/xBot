/**
* @file app.c
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 13:34
* @description: 
**/
#include "app.h"

#define M_PI 3.14159265

CmdData cmd_data;//香橙派下发的命令
McuData mcu_data;//MCU上传的数据

unsigned char *mcu_data_ptr;
int heart_beat_cnt;
int new_recv_flag;

unsigned char asr_id;
int asr_new_recv_flag;
unsigned char enable_power,last_enable_power;

/**
 * @brief app mian enter
 */
void app_main()
{
    //定义局部变量
    int loop_cnt=0;
    float stat_val=0;
    float vusb_val=0;
    float adc_val=0;

	//给MCU数据结构体的头和尾赋初始值
	mcu_data.head1 = 'D';
	mcu_data.head2 = 'A';
	mcu_data.struct_size = sizeof(McuData);
	mcu_data.end1 = 'T';
	mcu_data.end2 = 'A';
	mcu_data.end3 = '\r';
	mcu_data.end4 = '\n';

	mcu_data_ptr = (unsigned char *)&mcu_data;


	while(1)
	{

	}
}