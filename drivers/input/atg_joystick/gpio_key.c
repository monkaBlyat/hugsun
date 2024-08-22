/*
 * gpio_key.c
 *
 *  Created on: 2021年1月5日
 *      Author: Administrator
 */
#include <linux/gpio.h>
#include <linux/types.h>

#include "gpio_key.h"

#define	KEY_DEBOUNCE_C 10

extern int __dmc_board_type;

static unsigned char keybufin[KEY_SIZE];
static unsigned char keybufchk[KEY_SIZE];
static unsigned char keybufchkold[KEY_SIZE];
static unsigned char keybufcvt[KEY_SIZE];
static volatile unsigned int debounce = 0;
static volatile int key_debounced = 0;

//static unsigned char key_scancode[KEY_SIZE];

static report_func report_cb;

struct key_desc key_info[KEY_SIZE] = { //
		{ KEY1, KEY1_NAME, KEY1_TYPE, KEY1_CODE, KEY1_VALUE, 0 }, //
				{ KEY2, KEY2_NAME, KEY2_TYPE, KEY2_CODE, KEY2_VALUE, 0 }, //
				{ KEY3, KEY3_NAME, KEY3_TYPE, KEY3_CODE, KEY3_VALUE, 0 }, //
				{ KEY4, KEY4_NAME, KEY4_TYPE, KEY4_CODE, KEY4_VALUE, 0 }, //
				{ KEY5, KEY5_NAME, KEY5_TYPE, KEY5_CODE, KEY5_VALUE, 0 }, //
				{ KEY6, KEY6_NAME, KEY6_TYPE, KEY6_CODE, KEY6_VALUE, 0 }, //
				{ KEY7, KEY7_NAME, KEY7_TYPE, KEY7_CODE, KEY7_VALUE, 0 }, //
				{ KEY8, KEY8_NAME, KEY8_TYPE, KEY8_CODE, KEY8_VALUE, 0 }, //
				{ KEY9, KEY9_NAME, KEY9_TYPE, KEY9_CODE, KEY9_VALUE, 0 }, //
				{ KEY10, KEY10_NAME, KEY10_TYPE, KEY10_CODE, KEY10_VALUE, 0 }, //
				{ KEY11, KEY11_NAME, KEY11_TYPE, KEY11_CODE, KEY11_VALUE, 0 }, //
				{ KEY12, KEY12_NAME, KEY12_TYPE, KEY12_CODE, KEY12_VALUE, 0 }, //
				{ KEY13, KEY13_NAME, KEY13_TYPE, KEY13_CODE, KEY13_VALUE, 0 }, //
				{ KEY14, KEY14_NAME, KEY14_TYPE, KEY14_CODE, KEY14_VALUE, 0 }, //
		};

int init_key(void) {
	int i = 0;
	int ret;

	printk("hanson: %s board_type=%d\n",__func__, __dmc_board_type);
	memset(keybufin, 0, KEY_SIZE);
	memset(keybufchk, 0, KEY_SIZE);
	memset(keybufchkold, 0, KEY_SIZE);
	memset(keybufcvt, 0, KEY_SIZE);

	for (i = 0; i < KEY_SIZE; i++) {
		if(i==0){
			if(__dmc_board_type == HA2819){
				continue;
			}
		}
		if (!gpio_is_valid(key_info[i].pin)) {
			continue;
		}
		ret = gpio_request_one(key_info[i].pin, GPIOF_IN, key_info[i].label);
		if (ret < 0) {
			printk(KERN_ERR "hanson: %s-->Failed to request gpio %d\n", __func__, key_info[i].pin);
			continue;
		}
//		ret = gpio_direction_input(key_info[i].pin);
//		if (ret < 0) {
//			printk("hanson: %s-->Failed to set gpio_direction_input %d\n", __func__, key_info[i].pin);
//		}
	}
	return 0;
}

void deinit_key(void) {
	int i;
	for (i = 0; i < KEY_SIZE; i++) {
		if(i==0){
			if(__dmc_board_type == HA2819){
				continue;
			}
		}		
		if (!gpio_is_valid(key_info[i].pin)) {
			continue;
		}
		gpio_free(key_info[i].pin);
	}
}

static void get_key_state(void) {
	int i;
	char ret;
	memset(keybufin, 0, KEY_SIZE);
	for (i = 0; i < KEY_SIZE; i++) {
		/*		if (!gpio_is_valid(key_info[i].pin)) {
		 continue;
		 }*/
		ret = (char) gpio_get_value(key_info[i].pin);
		keybufin[i] = !ret;
//		if (keybufin[i]) {
//			printk("hanson: %s-->%s down\n", __func__, key_info[i].label);
//		}
	}
}

static void key_convert(unsigned char *buf, unsigned char *old_buf, int size) {
//	int i;
	report_cb(buf, old_buf, size);
#if 0
	unsigned char tmp_buf[KEY_SIZE];
//	memcpy(key_scancode, buf, size);

	for (i = 0; i < size; i++) {
		tmp_buf[i] = buf[i] ^ old_buf[i];
		if (tmp_buf[i]) {
			if (buf[i]) {
				//down
				printk("hanson: %s-->%s down\n", __func__, key_info[i].label);
			} else {
				//up
				printk("hanson: %s-->%s up\n", __func__, key_info[i].label);
			}
		}
	}
#endif
}

void key_scan(void) {
	get_key_state();
	if (memcmp(keybufin, keybufchk, KEY_SIZE) != 0) {
		memcpy(keybufchk, keybufin, KEY_SIZE);
		debounce = KEY_DEBOUNCE_C;
		key_debounced = 0;
	} else {
		if (debounce > 0) {
			debounce--;
		} else {
			if (key_debounced == 0) {
				key_debounced = 1;
				if (memcmp(keybufchk, keybufchkold, KEY_SIZE) != 0) {
					key_convert(keybufchk, keybufchkold, KEY_SIZE);
					memcpy(keybufchkold, keybufchk, KEY_SIZE);
				}
			}
		}
	}
}

void register_report_func(report_func fun) {
	report_cb = fun;
}
