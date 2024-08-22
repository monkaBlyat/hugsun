#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/timer.h>



#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

void fise_create_sys_gpio_file(void);

static int gpio1_pin;   //GPIO3_A1_d  USB wifi Reset

enum of_gpio_flags gpio0_flags;
struct device_node *np;
int ret = -1;

//add by pengfeng for quectel 4g
static int hgs_gpio1_enable = 0;
static int gpio1_pin_status=1;

struct iio_channel *chan3;
struct platform_device *mDev;
static ssize_t hgs_gpio1_state_store(struct class *class,struct class_attribute *attr,   const char *buf, size_t count)
{
	int value = simple_strtoul(buf, NULL, 0);
	gpio1_pin_status = 1;
	if(value)
	{
		gpio_direction_output(gpio1_pin,1);
		hgs_gpio1_enable = 1;
	}
	else
	{
		gpio_direction_output(gpio1_pin,0);
		hgs_gpio1_enable = 0;
	}
	return count;
}
 
static ssize_t hgs_gpio1_state_show(struct class *class,struct class_attribute *attr,   char *buf)
{   
    if(gpio1_pin_status == 1)
	{
		gpio_direction_input(gpio1_pin);
		gpio1_pin_status = 0;
	}
	hgs_gpio1_enable = gpio_get_value(gpio1_pin);
	return sprintf(buf, "%d\n", hgs_gpio1_enable?1:0);
}

static CLASS_ATTR_RW(hgs_gpio1_state);


static struct attribute *hgs_gpio1_control_attrs[] = {
	&class_attr_hgs_gpio1_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(hgs_gpio1_control);

static struct class hgs_gpio1_control = {
        .name = "hgs_gpio1",
        .class_groups = hgs_gpio1_control_groups,
};

void fise_create_sys_gpio_file(void)
{
	class_register(&hgs_gpio1_control);

}

static const struct of_device_id hgs_gpio_of_ids[] = {
	{.compatible = "rockchip,hgs_gpio",},
	{}
};

static int hgs_gpio_probe(struct platform_device *pdev){
	//struct device *dev = &pdev->dev;

	np = of_find_matching_node(NULL, hgs_gpio_of_ids);
	if(np <= 0)
	{
		printk("###hgs_gpio___%s_________ERROR!###\n",__FUNCTION__);
	}


	//add by pengfeng for usb wifi  reset
	gpio1_pin = of_get_named_gpio_flags(np, "gpio1_number", 0,&gpio0_flags);
    ret = gpio_request(gpio1_pin, "gpio1_number");
	printk("###hgs_gpio_probe(gpio1_pin=%d)\n",gpio1_pin);
	gpio_direction_output(gpio1_pin, 0);
	//gpio_set_pull(gpio1_pin, GPIO_PULL_UP);
    if (ret < 0) 
    {
        printk("###2Failed to request GPIO:%d, ERRNO:%d",(s32)gpio1_pin, ret);
		    gpio_free(gpio1_pin);
    }
  gpio_set_value(gpio1_pin, 0);
	msleep(100);
  gpio_set_value(gpio1_pin, 1);


	fise_create_sys_gpio_file();

	return 0;
}



#ifdef CONFIG_PM
static int hgs_gpio_suspend(struct device *dev)
{
//	gpio_direction_output(gpio1_pin,0);
	printk("###hgs_gpio_suspend()");
	return 0;
}

static int hgs_gpio_resume(struct device *dev)
{ 
	/*
	enum of_gpio_flags gpio0_flags;
	struct device_node *np;
	int ret = -1;
	printk("=========hgs_gpio_resume()");
	//add by pengfeng for usb wifi  reset
   	np = of_find_matching_node(NULL, hgs_gpio_of_ids);
//	gpio1_pin = of_get_named_gpio_flags(np, "gpio1_number", 0,&gpio0_flags);
//  ret = gpio_request(gpio1_pin, "gpio1_number");
//  if (ret < 0) 
//  {
   //   printk("###2Failed to request gpio1_pin:%d, ERRNO:%d",(s32)gpio1_pin, ret);
	//    gpio_free(gpio1_pin);
  //}
  printk("====1=====hgs_gpio_resume()");
 //add by pengfeng for sdio wifi power on
	gpio2_pin = of_get_named_gpio_flags(np, "gpio2_number", 0,&gpio0_flags);
	ret = gpio_request(gpio2_pin, "gpio2_number");
  if (ret < 0) 
  {
      printk("=====Failed to request gpio2_pin:%d, ERRNO:%d",(s32)gpio2_pin, ret);
	    gpio_free(gpio2_pin);
  }
  printk("====2=====hgs_gpio_resume()");
 //for usb wifi  en
 gpio3_pin = of_get_named_gpio_flags(np, "gpio3_number", 0,&gpio0_flags);
 ret = gpio_request(gpio3_pin, "gpio3_number");
  if (ret < 0) 
  {
      printk("###2Failed to request gpio3_pin:%d, ERRNO:%d",(s32)gpio3_pin, ret);
	    gpio_free(gpio3_pin);
	}
  printk("=====3====hgs_gpio_resume()");
 // gpio_direction_output(gpio1_pin,1);
  gpio_direction_output(gpio2_pin,1);
  gpio_direction_output(gpio3_pin,1);
  */
  printk("===hgs_gpio_resume==end==\n");
	return 0;
}
#else
#define hgs_gpio_suspend NULL
#define hgs_gpio_resume NULL
#endif

static const struct dev_pm_ops hgs_gpio_pm_ops = {
	.suspend	= hgs_gpio_suspend,
	.resume		= hgs_gpio_resume,
};


static int hgs_gpio_remove(struct platform_device *pdev){
	printk("hgs_gpio_remove.\n");
	return 0;
}

static struct platform_driver hgs_gpio_plat_driver = {
	.probe	= hgs_gpio_probe,
	.remove	= hgs_gpio_remove,
	.driver = {
		.name 	= "hgs_gpio",
		.pm	= &hgs_gpio_pm_ops,
#ifdef CONFIG_OF
		 .of_match_table = hgs_gpio_of_ids,
#endif
	    },
	
	
};


static int __init hgs_gpio_init(void)
{
	int ret;
	ret = platform_driver_register(&hgs_gpio_plat_driver);

	if(ret){
		printk("hgs_gpio register driver failed_____________\n");
		return -ENODEV;
	}
	printk("hgs_gpio register device and driver ok______________\n");
	return 0;
}

static void __exit  hgs_gpio_exit(void)
{
	platform_driver_unregister(&hgs_gpio_plat_driver);
}

subsys_initcall(hgs_gpio_init);
module_exit(hgs_gpio_exit);

MODULE_AUTHOR("Fise");
MODULE_DESCRIPTION("FISE GPIO CONTROL driver");
MODULE_LICENSE("GPL");

