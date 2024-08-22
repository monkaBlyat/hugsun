/*
 * gpio_key.h
 *
 *  Created on: 2021年1月4日
 *      Author: Administrator
 */

#ifndef GPIO_KEY_H_
#define GPIO_KEY_H_

#include "rockchip.h"
#include "input-event-codes.h"

struct key_desc {
	unsigned int pin;
	char *label;
	int type;
	int code;
	int value;
	int irq;
};
typedef void (*report_func)(unsigned char *, unsigned char *, int);

#define GPIO(b,n) b*32+n

#define KEY_PIN(n)     	KEY##n
#define KEY_NAME(n)     KEY##nNAME
#define KEY_TYPE(n)     KEY##nTYPE
#define KEY_CODE(n)     KEY##nCODE
#define KEY_VALUE(n)     KEY##nVALUE

#define KEY_SIZE	14

#define KEY1		GPIO(RK_GPIO1,RK_PA7)
#define KEY1_NAME	"BTN_Z"
#define KEY1_TYPE	EV_MSC
#define KEY1_CODE	BTN_Z
#define KEY1_VALUE	0x90006

#define KEY2		GPIO(RK_GPIO4,RK_PB5)
#define KEY2_NAME	"BTN_TR"
#define KEY2_TYPE	EV_MSC
#define KEY2_CODE	BTN_TR
#define KEY2_VALUE	0x90008

#define KEY3		GPIO(RK_GPIO1,RK_PB1)
#define KEY3_NAME	"BTN_TR2"
#define KEY3_TYPE	EV_MSC
#define KEY3_CODE	BTN_TR2
#define KEY3_VALUE	0x9000a

#define KEY4		GPIO(RK_GPIO1,RK_PB2)
#define KEY4_NAME	"BTN_MODE"
#define KEY4_TYPE	EV_MSC
#define KEY4_CODE	BTN_MODE
#define KEY4_VALUE	0x9000d

#define KEY5		GPIO(RK_GPIO1,RK_PB3)
#define KEY5_NAME	"BTN_SOUTH"
#define KEY5_TYPE	EV_MSC
#define KEY5_CODE	BTN_SOUTH
#define KEY5_VALUE	0x90001

#define KEY6		GPIO(RK_GPIO1,RK_PB4)
#define KEY6_NAME	"BTN_TL"
#define KEY6_TYPE	EV_MSC
#define KEY6_CODE	BTN_TL
#define KEY6_VALUE	0x90007

#define KEY7		GPIO(RK_GPIO1,RK_PC2)
#define KEY7_NAME	"ABS_HAT0X 1(Right)"
#define KEY7_TYPE	EV_ABS
#define KEY7_CODE	ABS_HAT0X
#define KEY7_VALUE	1

#define KEY8		GPIO(RK_GPIO2,RK_PB1)
#define KEY8_NAME	"ABS_HAT0Y 1(Down)"
#define KEY8_TYPE	EV_ABS
#define KEY8_CODE	ABS_HAT0Y
#define KEY8_VALUE	1

#define KEY9		GPIO(RK_GPIO4,RK_PB0)
#define KEY9_NAME	"ABS_HAT0Y -1(Up)"
#define KEY9_TYPE	EV_ABS
#define KEY9_CODE	ABS_HAT0Y
#define KEY9_VALUE	-1

#define KEY10		GPIO(RK_GPIO4,RK_PB1)
#define KEY10_NAME	"ABS_HAT0X -1(Left)"
#define KEY10_TYPE	EV_ABS
#define KEY10_CODE	ABS_HAT0X
#define KEY10_VALUE	-1

#define KEY11		GPIO(RK_GPIO4,RK_PB2)
#define KEY11_NAME	"BTN_THUMBR"
#define KEY11_TYPE	EV_MSC
#define KEY11_CODE	BTN_THUMBR
#define KEY11_VALUE	0x9000f

#define KEY12		GPIO(RK_GPIO4,RK_PB3)
#define KEY12_NAME	"BTN_THUMBL"
#define KEY12_TYPE	EV_MSC
#define KEY12_CODE	BTN_THUMBL
#define KEY12_VALUE	0x9000e

#define KEY13		GPIO(RK_GPIO4,RK_PD0)
#define KEY13_NAME	"KEY_VIDEO_NEXT"
#define KEY13_TYPE	EV_KEY
#define KEY13_CODE	KEY_VIDEO_NEXT
#define KEY13_VALUE	1

#define KEY14		GPIO(RK_GPIO0,RK_PA5)
#define KEY14_NAME	"KEY_POWER"
#define KEY14_TYPE	EV_KEY
#define KEY14_CODE	KEY_POWER
#define KEY14_VALUE	1

extern struct key_desc key_info[KEY_SIZE];
int init_key(void);
void deinit_key(void);
void key_scan(void);
void register_report_func(report_func fun);

#endif /* GPIO_KEY_H_ */
