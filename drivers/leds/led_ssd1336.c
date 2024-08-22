#include "asm-generic/delay.h"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h> 
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>

#define IIC_SCK_0  gpio_set_value(gpio_i2c_scl,0)       
#define IIC_SCK_1  gpio_set_value(gpio_i2c_scl,1)    
#define IIC_SDA_0  gpio_set_value(gpio_i2c_sda,0)  
#define IIC_SDA_1  gpio_set_value(gpio_i2c_sda,1)  

#define OLED_COLUMN_NUMBER 256
#define OLED_ROW_NUMBER 64

#define LCD_EXT_DEBUG_INFO
#ifdef LCD_EXT_DEBUG_INFO
#define DBG_PRINT(...)		printk(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

#define DEVICE_NAME  "led_ssd1336"

typedef struct 
{
    unsigned int x_s;
    unsigned int y_s;
	unsigned int x_e;
	unsigned int y_e;
}ssd1336_add_t;

/* ioctrl magic set */
#define SSD1336_IOC_MAGIC 'h'
#define IOCTL_CHAR_ADDR 	_IOWR(SSD1336_IOC_MAGIC, 0x10, ssd1336_add_t)
#define IOCTL_CHAR_CMD 		_IOWR(SSD1336_IOC_MAGIC, 0x11, unsigned char)
#define IOCTL_CHAR_DATA 	_IOWR(SSD1336_IOC_MAGIC, 0x12, unsigned char)
#define IOCTL_LED_CLEAR		_IO(SSD1336_IOC_MAGIC, 0x13)
#define	IOCTL_LED_FULL		_IO(SSD1336_IOC_MAGIC, 0x14)

static int gpio_i2c_scl,gpio_i2c_sda;

#define delay_us(x) udelay(x)
#define delay_ms(x) do {msleep(x);} while(0) 
#define SSD1336_CLK 1

static void IIC_write(unsigned char date)
{
	unsigned char i, temp;
	temp = date;
			
	for(i=0; i<8; i++)
	{	IIC_SCK_0;
		
        if ((temp&0x80)==0)
            IIC_SDA_0;
        else IIC_SDA_1;
		temp = temp << 1;
		delay_us(SSD1336_CLK);			  
		IIC_SCK_1;
		delay_us(SSD1336_CLK);
		
	}
	IIC_SCK_0;
	delay_us(SSD1336_CLK);
	IIC_SDA_1;
	delay_us(SSD1336_CLK);
	IIC_SCK_1;
	delay_us(SSD1336_CLK);
	IIC_SCK_0;
	delay_us(SSD1336_CLK);	
}

static void IIC_start(void)
{
	IIC_SDA_1;
	delay_us(SSD1336_CLK);
	IIC_SCK_1;
	delay_us(SSD1336_CLK);				   
	IIC_SDA_0;
	delay_us(SSD1336_CLK*3);
	IIC_SCK_0;
	//delay_us(SSD1336_CLK);
	IIC_write(0x78);        
}

static void IIC_stop(void)
{
	IIC_SDA_0;
	delay_us(SSD1336_CLK);
	IIC_SCK_1;
	delay_us(SSD1336_CLK*3);
	IIC_SDA_1;	
	delay_us(SSD1336_CLK);
}

static void OLED_send_cmd(unsigned char o_command)
{
	IIC_start();
	IIC_write(0x00); 
	IIC_write(o_command);
	IIC_stop();
}

static void OLED_send_data(unsigned char o_data)
{ 
	IIC_start();
	IIC_write(0x40);
	IIC_write(o_data);
	IIC_stop();
}

static void Column_set(unsigned char start_column,unsigned char end_column)
{
	OLED_send_cmd(0x15);   
	OLED_send_cmd(start_column);   
	OLED_send_cmd(end_column);       
}

static void Row_set(unsigned char start_row,unsigned char end_row)
{
	OLED_send_cmd(0X75);
	OLED_send_cmd(start_row);       //0-3f
	OLED_send_cmd(end_row);
}

static void Add_set(unsigned int x_s,unsigned int y_s,unsigned int x_e,unsigned int y_e)
{
	Column_set(x_s/2,x_e/2);
	Row_set(y_s,y_e);
	OLED_send_cmd(0x5C);
}

static void OLED_clear(void)
{
	unsigned int ROW,column;
	OLED_send_cmd(0x15);
	OLED_send_cmd(0x00);
	OLED_send_cmd(0x7F);

	OLED_send_cmd(0x75);
	OLED_send_cmd(0x00);
	OLED_send_cmd(0x3f);
	OLED_send_cmd(0x5C);
	for(ROW=0;ROW<OLED_ROW_NUMBER;ROW++) {            //ROW loop
	  for(column=0;column<OLED_COLUMN_NUMBER/2;column++){  //column loop
		  OLED_send_data(0x00);
		}
	}
}

static void OLED_full(void)
{
	unsigned int ROW,column;
	OLED_send_cmd(0x15);
	OLED_send_cmd(0x00);
	OLED_send_cmd(0x7F);

	OLED_send_cmd(0x75);
	OLED_send_cmd(0x00);
	OLED_send_cmd(0x3f);
	OLED_send_cmd(0x5C);
	for(ROW=0;ROW<OLED_ROW_NUMBER;ROW++){             //ROW loop
		for(column=0;column<OLED_COLUMN_NUMBER/2 ;column++){ //column loop
				OLED_send_data(0xff);
			  }
	  }
}

static void OLED_init(void)
{
	IIC_stop();
	////SSD1336
	OLED_send_cmd(0XFD); //SET COMMAND LOCK
	OLED_send_cmd(0x12); //UNLOCK

	OLED_send_cmd(0xAE); //close display

	OLED_send_cmd(0XA8);	//multiplex ratio
	OLED_send_cmd(0X3F);   //duty = 1/64

	OLED_send_cmd(0XAB);	//Internal VDD switch
	OLED_send_cmd(0X01);    //open

	OLED_send_cmd(0X15);	//Set Column Address
	OLED_send_cmd(0X00);   //start
	OLED_send_cmd(0X7F);   //up to

	OLED_send_cmd(0X75);   //set row add
	OLED_send_cmd(0X00);   //start
	OLED_send_cmd(0X3F);   //up to

	OLED_send_cmd(0XA0);	//set remap
	OLED_send_cmd(0XC3);	//OLED_send_cmd(0XC3); 


	OLED_send_cmd(0XA1);	//start ROW
	OLED_send_cmd(0X00);

	OLED_send_cmd(0xA2);    //set offset
	OLED_send_cmd(0x00);

	OLED_send_cmd(0xAD);    //set external IREF
	OLED_send_cmd(0x8E);

	OLED_send_cmd(0x81);    //set Contrast Current
	OLED_send_cmd(0x7F);

	OLED_send_cmd(0XB8);	//Gray scale setting
	OLED_send_cmd(0X02);
	OLED_send_cmd(0X09);
	OLED_send_cmd(0X0D);
	OLED_send_cmd(0X14);
	OLED_send_cmd(0X1B);
	OLED_send_cmd(0X24);
	OLED_send_cmd(0X2D);
	OLED_send_cmd(0X38);
	OLED_send_cmd(0X45);
	OLED_send_cmd(0X54);
	OLED_send_cmd(0X64);
	OLED_send_cmd(0X75);
	OLED_send_cmd(0X88);
	OLED_send_cmd(0X9E);
	OLED_send_cmd(0XB4);

	OLED_send_cmd(0XB1);	//SET PHASE LENGTH
	OLED_send_cmd(0X22);

	OLED_send_cmd(0XB3);	//DISPLAY DIVIDE CLOCKRADIO/OSCILLATAR FREQUANCY
	OLED_send_cmd(0XC0);

	OLED_send_cmd(0XB6);	//SET SECOND PRE-CHARGE PERIOD
	OLED_send_cmd(0X04);

	OLED_send_cmd(0XB9);	//Select Default Linear Gray Scale Table

	OLED_send_cmd(0XBC);	//SET PRE-CHARGE VOLTAGE
	OLED_send_cmd(0X1F);

	OLED_send_cmd(0XBD);	//Pre-charge voltage capacitor Selection
	OLED_send_cmd(0X01);

	OLED_send_cmd(0XBE);	//Set COM deselect voltage level
	OLED_send_cmd(0X05);	//0.82*Vcc


	OLED_send_cmd(0XA4);	//normal display

	OLED_send_cmd(0xAF);	//open display  
}

static int ssd1336_open(struct inode *inode, struct file *file) 
{                                                                                                                                   
	int ret;                                                                                                                  
	ret = nonseekable_open(inode, file);                                                                                       
	if(ret < 0)                                                                                                          
		return ret;            
	return 0;                                                                                                     
}

static int ssd1336_release(struct inode *inode, struct file *file) 
{
	return 0;
}

static long ssd1336_ioctl(struct file *file, unsigned int cmd, unsigned long args) 
{
	void __user *argp = (void __user *)args;
	ssd1336_add_t ssd1336_add;
	unsigned char send_cmd,send_data;
	int ret = 0 ;
	switch (cmd){
		case IOCTL_CHAR_ADDR:
			if (args) {  
				ret = copy_from_user(&ssd1336_add, argp,sizeof(ssd1336_add));
			}
			Add_set(ssd1336_add.x_s,ssd1336_add.y_s,ssd1336_add.x_e,ssd1336_add.y_e);
			break;
		case IOCTL_CHAR_CMD:
			if (args) {  
				ret = copy_from_user(&send_cmd, argp,sizeof(send_cmd));
			}		
			OLED_send_cmd(send_cmd);			
			break;
		case IOCTL_CHAR_DATA:
			if (args) {  
				ret = copy_from_user(&send_data, argp,sizeof(send_data));
			}
			OLED_send_data(send_data);			
			break;
		case IOCTL_LED_CLEAR:
			OLED_clear();	
			break;
		case IOCTL_LED_FULL:
			OLED_full();	
			break;						
		default:
			printk("ERROR: IOCTL CMD NOT FOUND!!!\n");
			break;			
	}	

	return 0;
}

static struct file_operations ssd1336_fops ={
	.owner =THIS_MODULE,
	.open  =ssd1336_open,
	.release =ssd1336_release,
	.unlocked_ioctl =ssd1336_ioctl,
};

static int ssd1336_probe(struct platform_device *pdev)
{
	static struct class * scull_class;
	struct device_node *node = pdev->dev.of_node;
	enum of_gpio_flags flags;
	int ret; 

	ret =register_chrdev(0,DEVICE_NAME,&ssd1336_fops);
	if(ret<0){
		printk("can't register device led_ssd1336.\n");
		return ret;
	}
	printk("register device led_ssd1336 success.\n");

	scull_class = class_create(THIS_MODULE,DEVICE_NAME);
	if(IS_ERR(scull_class))
	{
		printk(KERN_ALERT "Err:faile in scull_class!\n");
		return -1;
	}
	device_create(scull_class, NULL, MKDEV(ret,0), NULL, DEVICE_NAME);

	//---------------------------
	gpio_i2c_scl = of_get_named_gpio_flags(node, "i2c_scl", 0, &flags);
	if (gpio_is_valid(gpio_i2c_scl)){
		if (gpio_request(gpio_i2c_scl, "i2c_scl_gpio")<0) {
			printk("%s: failed to get gpio_i2c_scl.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_i2c_scl, 1);
		printk("%s: get property: gpio,i2c_scl = %d\n", __func__, gpio_i2c_scl);
	}else{
		printk("get property gpio,i2c oled ssd1336 failed \n");
		return -1;
	}

	gpio_i2c_sda = of_get_named_gpio_flags(node, "i2c_sda", 0, &flags);
	
	if (gpio_is_valid(gpio_i2c_sda)){
		if (gpio_request(gpio_i2c_sda, "i2c_sda_gpio")<0) {
			printk("%s: failed to get gpio_i2c_sda.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_i2c_sda, 1);
		printk("%s: get property: gpio,i2c_sda = %d\n", __func__, gpio_i2c_sda);
	}else{
		printk("get property gpio,i2c oled ssd1336 failed \n");
		return -1;
	} 

	OLED_init();	
	printk("==========%s probe ok========\n", DEVICE_NAME);

	return 0;
}

static int ssd1336_remove(struct platform_device *pdev)
{
	unregister_chrdev(0,DEVICE_NAME);
	return 0;
}

static void ssd1336_shutdown (struct platform_device *pdev)
{
}

#ifdef CONFIG_OF
static const struct of_device_id ssd1336_dt_match[]={
	{ .compatible = "led_ssd1336",},
	{}
};
MODULE_DEVICE_TABLE(of, ssd1336_dt_match);
#endif

static struct platform_driver ssd1336_driver = {
	.probe  = ssd1336_probe,
	.remove = ssd1336_remove,
	.shutdown     = ssd1336_shutdown,
	.driver = {
		.name  = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ssd1336_dt_match),
#endif
	},
};

static int __init led_ssd1336_init(void)
{
	int ret;
	DBG_PRINT("%s\n=============================================\n", __FUNCTION__);
	ret = platform_driver_register(&ssd1336_driver);
	if (ret) {
		printk("[error] %s failed to register ssd1336 driver module\n", __FUNCTION__);
		return -ENODEV;
	}
	return ret;
}

static void __exit led_ssd1336_exit(void)
{
	platform_driver_unregister(&ssd1336_driver);
}

module_init(led_ssd1336_init);
module_exit(led_ssd1336_exit);

MODULE_AUTHOR("Hugsun");
MODULE_DESCRIPTION("LCD Extern driver for led_ssd1336");
MODULE_LICENSE("GPL");

