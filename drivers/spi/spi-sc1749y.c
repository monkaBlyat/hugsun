#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

static struct device_node *node;
static int gpio_spi_cs,gpio_spi_clk,gpio_spi_mosi,gpio_spi_miso;

/* ioctrl magic set */
#define SC1749Y_IOC_MAGIC 'h'
#define IOCTL_SEND_CMD 	_IOW(SC1749Y_IOC_MAGIC, 0x10, sc1749y_txcmd_t)
#define IOCTL_RECEV_DATA 	_IOR(SC1749Y_IOC_MAGIC, 0x11, sc1749y_receive_t)

typedef struct 
{
    unsigned char sw[2];
    unsigned int data_len;
    unsigned char data[1024];
    unsigned char lrc2;
    unsigned char check_lrc2;
}sc1749y_receive_t;

typedef struct 
{
    unsigned char data[1024];
    unsigned len ;
}sc1749y_txcmd_t;

static int spi_request_gpio(void)   
{
    	enum of_gpio_flags flags;

	gpio_spi_cs = of_get_named_gpio_flags(node, "spi_cs", 0, &flags);
	if (gpio_is_valid(gpio_spi_cs)){
		if (gpio_request(gpio_spi_cs, "spi_cs_gpio")<0) {
			printk("%s: failed to get gpio_spi_cs.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_spi_cs, 1);
		printk("%s: get property: gpio,spi_cs = %d\n", __func__, gpio_spi_cs);
	}else{
		printk("get property gpio,spics failed \n");
		return -1;
	}

	gpio_spi_clk = of_get_named_gpio_flags(node, "spi_clk", 0, &flags);
	
	if (gpio_is_valid(gpio_spi_clk)){
		if (gpio_request(gpio_spi_clk, "spi_clk_gpio")<0) {
			printk("%s: failed to get gpio_spi_clk.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_spi_clk, 1);
		printk("%s: get property: gpio,spi_clk = %d\n", __func__, gpio_spi_clk);
	}else{
		printk("get property gpio,spiclk failed \n");
		return -1;
	} 

	gpio_spi_mosi = of_get_named_gpio_flags(node, "spi_mosi", 0, &flags);
	if (gpio_is_valid(gpio_spi_mosi)){
		if (gpio_request(gpio_spi_mosi, "gpio_spi_mosi")<0) {
			printk("%s: failed to get gpio_spi_mosi.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_spi_mosi, 1);
		printk("%s: get property: gpio,spi_mosi = %d\n", __func__, gpio_spi_mosi);
	}else{
		printk("get property gpio,spidata failed \n");
		return -1;
	} 

	gpio_spi_miso = of_get_named_gpio_flags(node, "spi_miso", 0, &flags);
	if (gpio_is_valid(gpio_spi_miso)){
		if (gpio_request(gpio_spi_miso, "gpio_spi_miso")<0) {
			printk("%s: failed to get gpio_spi_miso.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_spi_miso, 1);
		printk("%s: get property: gpio,spi_miso = %d\n", __func__, gpio_spi_miso);
	}else{
		printk("get property gpio,spidata failed \n");
		return -1;
	}  
     return 0;  
}

static void spi_init(void)  
{  
     gpio_direction_output(gpio_spi_cs, 1);  
     gpio_direction_output(gpio_spi_clk, 1);  
     gpio_direction_output(gpio_spi_mosi, 0);  
     gpio_direction_input(gpio_spi_miso);  
     gpio_set_value(gpio_spi_clk, 1);                    
     gpio_set_value(gpio_spi_mosi, 0);  
} 

/*  
从设备使能  
enable：为1时，使能信号有效，SS低电平  
为0时，使能信号无效，SS高电平  
*/  
static void ss_enable(int enable)  
{  
     if (enable)  
          gpio_set_value(gpio_spi_cs, 0);                  //SS低电平，从设备使能有效  
     else  
          gpio_set_value(gpio_spi_cs, 1);                  //SS高电平，从设备使能无效  
}  


/* SPI字节写 */  
static void spi_write_byte(unsigned char b)  
{  
     int i;  
     for (i=7; i>=0; i--) {  
          gpio_set_value(gpio_spi_clk, 0); 
          //udelay(5);//延时  
          gpio_set_value(gpio_spi_mosi, b&(1<<i));         //从高位7到低位0进行串行写入  
          udelay(5);//延时  
          gpio_set_value(gpio_spi_clk, 1);                // CPHA=1，在时钟的第一个跳变沿采样  
          udelay(5);//延时       
     }  
}  


/* SPI字节读 */  
static unsigned char spi_read_byte(void)  
{  
     int i;  
     unsigned char r = 0;  
     for (i=0; i<8; i++) {  
          gpio_set_value(gpio_spi_clk, 0);  
          udelay(5);//延时                              //延时  
          gpio_set_value(gpio_spi_clk, 1);                // CPHA=1，在时钟的第一个跳变沿采样  
          //udelay(5);//延时  
          r = (r <<1) | gpio_get_value(gpio_spi_miso);         //从高位7到低位0进行串行读出  
          udelay(5);//延时   
     }  
     return r;
}

/*  
SPI读操作  
buf：写缓冲区  
len：写入字节的长度  
*/  
static void spi_write (unsigned char* buf, int len)
{
     int i;  
     unsigned char lrc1=0;

     /* SPI端口初始化 */  
     spi_init();
     udelay(10);

     //printk("spi tx_buf=0x%x 0x%x 0x%x--\n",buf[0],buf[1],len);                                   
     ss_enable(1);                       
     udelay(60);                          
     
     spi_write_byte(0x55);        //发送命令报头0x55
                              
     //写入数据  CLA INS P1 P2 Len1 Len2 DATA LRC1
     len=len>1024?1024:len;
     for (i=0; i<len; i++){  
          spi_write_byte(buf[i]);  
          lrc1=lrc1^buf[i];
          }
     lrc1=~lrc1; 
     spi_write_byte(lrc1);    //写lrc1校验码

     udelay(5);//延时    
     ss_enable(0);  
}


/*  
SPI读操作  
buf：读缓冲区  
len：读入字节的长度  
*/  
static void spi_read(sc1749y_receive_t* rec_buf)  
{       
     unsigned char lrc2=0;
     unsigned char len_buf[2];
     int tmp,i=0;
     
     /* SPI端口初始化 */  
     spi_init();
     udelay(10);
     
     ss_enable(1);                           
     udelay(60);//延时                       

     i=0;
     do{
          if (spi_read_byte()==0x55)
               break;
          udelay(20);  
          i++;   
     }while(i<1000);

     rec_buf->sw[0] = spi_read_byte();
     rec_buf->sw[1] = spi_read_byte();
     len_buf[0] = spi_read_byte();
     len_buf[1] = spi_read_byte();  
     tmp = len_buf[0]; 
     rec_buf->data_len = (tmp<<8) | len_buf[1];
     rec_buf->data_len = rec_buf->data_len>1024?1024:rec_buf->data_len;
     lrc2=rec_buf->sw[0]^rec_buf->sw[1]^len_buf[0]^len_buf[1];
     //printk("--spi_read 0x%x%x--data_len=%x---\n",rec_buf->sw[0],rec_buf->sw[1],rec_buf->data_len);
     for (i=0; i<rec_buf->data_len; i++){
          rec_buf->data[i] = spi_read_byte(); 
          lrc2=lrc2^(rec_buf->data[i]);
          }
     lrc2=~lrc2;      
     rec_buf->lrc2=spi_read_byte();      //加密IC读回lrc2
     rec_buf->check_lrc2=lrc2;  //自校验lrc2
     //printk("---spi_read lrc2=0x%x--0x%x---\n",rec_buf->lrc2,rec_buf->check_lrc2);
     udelay(5);//延时    
     ss_enable(0);                           
}

static long sc1749y_ioctl(struct file *file, unsigned int cmd, unsigned long args) 
{
	void __user *argp = (void __user *)args;
     sc1749y_txcmd_t tx_buf;
     sc1749y_receive_t rx_buf;
	int ret = 0 ;
	switch (cmd){
		case IOCTL_SEND_CMD:
			if (args) {  
				ret = copy_from_user(&tx_buf,argp,sizeof(tx_buf));
			}
               spi_write(tx_buf.data,tx_buf.len);
			break;		
          case IOCTL_RECEV_DATA:     
               spi_read(&rx_buf);	
			if (args) {  
				ret = copy_to_user((sc1749y_receive_t *)args,&rx_buf,sizeof(rx_buf));
			}               
               break;	
		default:
			printk("ERROR: IOCTL CMD NOT FOUND!!!\n");
			break;			
	}	

	return 0;
}
 
int sc1749y_open(struct inode *inode,struct file *file)
{
	int ret;                                                                                                                  
	ret = nonseekable_open(inode, file);                                                                                       
	if(ret < 0)                                                                                                          
		return ret;           
     return 0;
}

static int sc1749y_release(struct inode *inode, struct file *file) 
{
	return 0;
}
 
static struct file_operations sc1749y_ops = { 
     .owner     = THIS_MODULE,
     .open     = sc1749y_open,
     .release  = sc1749y_release,
     .unlocked_ioctl =sc1749y_ioctl,      
};
 
static struct miscdevice sc1749y_dev = { 
     .minor    = MISC_DYNAMIC_MINOR,
     .fops    = &sc1749y_ops,
     .name    = "sc1749y",
};

static int sc1749y_probe(struct platform_device *pdev)
{
	int ret;      
     
     printk("sc1749y_probe!\n");
    
	//---------------------------
     node = pdev->dev.of_node;
     ret=spi_request_gpio();
     spi_init();

	printk("==========%s probe ok====ret=%d====\n", __func__,ret);    

     return ret;
}
 
static int sc1749y_remove(struct platform_device *pdev)
{
     printk("sc1749y_remove!\n");
     return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sc1749y_of_match[] = {
	{ .compatible = "spi_sc1749y" },
	{}
};
MODULE_DEVICE_TABLE(of, sc1749y_of_match);
#endif
 
static struct platform_driver sc1749y_spi_driver = {  
     .probe =    sc1749y_probe,
     .remove = sc1749y_remove,
     .driver = {
         .name  = "sc1749y",
         .owner = THIS_MODULE,
#ifdef CONFIG_OF         
         .of_match_table = sc1749y_of_match,
#endif         
     },
};
 
static int __init sc1749y_init(void)
{
     int ret;

     misc_register(&sc1749y_dev);
     ret = platform_driver_register(&sc1749y_spi_driver); 
	if (ret) {
		printk("[error] %s failed to register sc1749y driver module\n", __FUNCTION__);
		return -ENODEV;
	}     
     return 0;
}
 
static void __exit sc1749y_exit(void)
{
     misc_deregister(&sc1749y_dev);    
     platform_driver_unregister(&sc1749y_spi_driver); 
}
 
module_exit(sc1749y_exit);
module_init(sc1749y_init);
 
MODULE_AUTHOR("Hugsun");
MODULE_DESCRIPTION("decrypt driver for spi_sc1749y");
MODULE_LICENSE("GPL");

