#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <linux/input-event-codes.h>
#include "gpio_key.h"

extern int __dmc_board_type;

static ktime_t kt;
static struct hrtimer timer;
static struct input_dev *joystick_dev;
//static struct timer_list buttons_timer;

static struct timer_list timer_lst;

#define BUFSIZE	4
static char read_buf[BUFSIZE]={'0','0','\n',0};
static char write_buf[BUFSIZE]={0,0,0,0};
static unsigned int read_len=BUFSIZE;
struct proc_dir_entry* channel_file=NULL;

#define CHANNEL_DEBUG		0

static int channel_proc_open(struct inode *inode, struct file *file) {
	return 0;
}

static ssize_t channel_proc_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos) {
	if (*f_pos > 0)
		return 0;
#if CHANNEL_DEBUG
	printk("hanson: %s %c\n", __func__,read_buf[0]);
#endif
	if (copy_to_user(buffer, read_buf, read_len))
		return -EFAULT;
	*f_pos = *f_pos + read_len;
	return read_len;
}

static ssize_t channel_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos) {
	int write_len;

	if (count <= 0){
		printk("hanson: %s, Count is zero\n",__func__);
		return -EFAULT;
	}
	write_len = count > BUFSIZE ? BUFSIZE : count;
	memset(write_buf, 0, write_len + 1);
	if (copy_from_user(write_buf, buffer, write_len)){
		printk("hanson: %s, Failed to copy from user!\n",__func__);
		return -EFAULT;
	}
	printk("hanson: channel key disable %c", write_buf[0]);
	return write_len;
}

static struct file_operations vol_fops = { //
				.owner = THIS_MODULE, //
				.open = channel_proc_open, //
				.read = channel_proc_read, //
				.write = channel_proc_write, //
		};


void gpio_keys_gpio_report_event(unsigned char *buf, unsigned char *old_buf, int size) {
	int i;
	int value;
	int key_code;
	unsigned char tmp_buf[KEY_SIZE];
//	printk("%s-->start\n", __func__);
	for (i = 0; i < size; i++) {
		tmp_buf[i] = buf[i] ^ old_buf[i];
		if (tmp_buf[i]) {
			if (key_info[i].type == EV_MSC) {
				value = buf[i] ? 1 : 0;
				key_code = key_info[i].code;
				input_event(joystick_dev, EV_MSC, MSC_SCAN, key_info[i].value);
				input_event(joystick_dev, EV_KEY, key_code, value);
				input_sync(joystick_dev);
			} else if (key_info[i].type == EV_ABS) {
				value = buf[i] ? key_info[i].value : 0;
				key_code = key_info[i].code;
				input_event(joystick_dev, EV_ABS, key_code, value);
				input_sync(joystick_dev);
			} else if (key_info[i].type == EV_KEY) {
				value = buf[i] ? 1 : 0;
				key_code = key_info[i].code;

				if(key_code==KEY_VIDEO_NEXT){
					if(value==0){
						read_buf[0]='0';
					}else{
						read_buf[0]='1';
					}
					if(write_buf[0]!='1'){
						input_event(joystick_dev, EV_KEY, key_code, value);
						input_sync(joystick_dev);
					}
				}else{
					input_event(joystick_dev, EV_KEY, key_code, value);
					input_sync(joystick_dev);
				}				
			}
		}
	}

}

void timer_handler(unsigned long data) {
	printk(KERN_INFO"timer pending:%d\n", timer_pending(&timer_lst));
	mod_timer(&timer_lst, jiffies + msecs_to_jiffies(1000));
	printk(KERN_INFO"jiffies:%ld, data:%ld\n", jiffies, data);
}
int timer_init(void) {
	printk(KERN_INFO"%s jiffies:%ld\n", __func__, jiffies);
	printk(KERN_INFO"ji:%d,HZ:%d\n", jiffies_to_msecs(250), HZ);
	init_timer(&timer_lst);
	timer_lst.data = 45;
	timer_lst.function = timer_handler;
	timer_lst.expires = jiffies + HZ;
	add_timer(&timer_lst);
	printk(KERN_INFO"timer pending:%d\n", timer_pending(&timer_lst));
	return 0;
}

void timer_exit(void) {
	printk(KERN_INFO"%s jiffies:%ld\n", __func__, jiffies);
	del_timer(&timer_lst);
}

//static void buttons_timer_function(unsigned long data) {
//	printk("hanson: %s-->start\n", __func__);
//}

static enum hrtimer_restart hrtimer_hander(struct hrtimer *timer) {
	key_scan();
	hrtimer_forward(timer, timer->base->get_time(), kt);
	return HRTIMER_RESTART;
}

static int init_hrtimer(void) {
	printk("hanson: %s-->start\n", __func__);
	kt = ktime_set(0, 2 * 1000 * 1000); //2ms
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_start(&timer, kt, HRTIMER_MODE_REL);
	timer.function = hrtimer_hander;
	return 0;
}

static void deinit_hrtimer(void) {
	printk("hanson: %s-->start\n", __func__);
	hrtimer_cancel(&timer);
}

static int joystick_init(void) {
	int i;
	int ret;
	const char *joystick_name = "test_joystick";
	printk("hanson: %s-->start\n", __func__);


	memset(write_buf,0,sizeof(write_buf));
	printk("hanson: %s create \n",__func__);
	channel_file = proc_create("channel_key", 0644, NULL, &vol_fops);
	if(channel_file==NULL){
		printk("hanson: Failed to create channel_key!\n");
	}
	read_buf[0]='0';
	read_buf[1]='\n';
	read_buf[2]='\0';
	read_buf[3]='\0';

	write_buf[0]='0';
	
	joystick_dev = input_allocate_device();


	if(__dmc_board_type == HA2819){
		set_bit(EV_SYN, joystick_dev->evbit);
		set_bit(EV_KEY, joystick_dev->evbit);	
		for (i = 12; i < KEY_SIZE; i++) {
			if (key_info[i].type != EV_ABS) {
				set_bit(key_info[i].code, joystick_dev->keybit);
			}
		}	
	}else{
		set_bit(EV_SYN, joystick_dev->evbit);
		set_bit(EV_KEY, joystick_dev->evbit);
		set_bit(EV_ABS, joystick_dev->evbit);
		set_bit(EV_MSC, joystick_dev->evbit);
		
		for (i = 0; i < KEY_SIZE; i++) {
			if (key_info[i].type != EV_ABS) {
				set_bit(key_info[i].code, joystick_dev->keybit);
			}
		}

		set_bit(MSC_SCAN, joystick_dev->mscbit);
		input_set_abs_params(joystick_dev, ABS_HAT0X, -1, 1, 0, 0);
		input_set_abs_params(joystick_dev, ABS_HAT0Y, -1, 1, 0, 0);
		for (i = 0x130; i < 0x13f; i++) {
			input_set_capability(joystick_dev, EV_KEY, i);
		}	
	}	


	joystick_dev->name = joystick_name;

	joystick_dev->name = "ATG game console"; // pdata->name ? : pdev->name;
	joystick_dev->phys = "gpio-keys/input0";
//	joystick_dev->dev.parent = &pdev->dev;
//	joystick_dev->open = gpio_keys_open;
//	joystick_dev->close = gpio_keys_close;

	joystick_dev->id.bustype = BUS_HOST;
	joystick_dev->id.vendor = 0x0838;
	joystick_dev->id.product = 0x8819;
	joystick_dev->id.version = 0x0111;

	ret = input_register_device(joystick_dev);

	init_hrtimer();
	init_key();
	register_report_func(gpio_keys_gpio_report_event);

//	init_timer(&buttons_timer);
//	buttons_timer.function = buttons_timer_function;
//	add_timer(&buttons_timer);
//	timer_init();

	return 0;
}

static void joystick_exit(void) {
	printk("hanson: %s-->start\n", __func__);
	deinit_key();
	deinit_hrtimer();
//	timer_exit();
	input_unregister_device(joystick_dev);
	if(channel_file){
		proc_remove(channel_file);
	}
}

module_init(joystick_init);
module_exit(joystick_exit);
MODULE_LICENSE("GPL");

