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

#define LCD_EXT_DEBUG_INFO
#ifdef LCD_EXT_DEBUG_INFO
#define DBG_PRINT(...)		printk(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif
#define DEVICE_NAME  "lcd_vk2c21"
/* ioctrl magic set */
#define CH455_IOC_MAGIC 'h'
#define IOCTL_CHAR_DISPLAY 	_IOWR(CH455_IOC_MAGIC, 0x10, unsigned char)
#define IOCTL_DOT_DISPLAY  	_IOWR(CH455_IOC_MAGIC, 0x11, unsigned char)
#define IOCTL_COLON_DISPLAY 	_IOWR(CH455_IOC_MAGIC, 0x12, unsigned char)
#define IOCTL_PWR_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x13, unsigned char)
#define IOCTL_LAN_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x14, unsigned char)
#define IOCTL_LAN_OFF	_IOWR(CH455_IOC_MAGIC, 0x15, unsigned char)
#define IOCTL_WIFI_LOW_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x16, unsigned char)
#define IOCTL_WIFI_FINE_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x17, unsigned char)
#define IOCTL_WIFI_OFF	_IOWR(CH455_IOC_MAGIC, 0x18, unsigned char)
#define IOCTL_LED_ON		_IOWR(CH455_IOC_MAGIC, 0x19, unsigned char)
#define	IOCTL_LED_OFF		_IOWR(CH455_IOC_MAGIC, 0x1a, unsigned char)

//���¹ܽ����������ݿͻ���Ƭ������Ӧ���޸�
#define Vk2c21_SCL_H() 				gpio_set_value(gpio_i2c_scl,1) 
#define Vk2c21_SCL_L() 				gpio_set_value(gpio_i2c_scl,0) 

#define Vk2c21_SDA_H() 				gpio_set_value(gpio_i2c_sda,1) 
#define Vk2c21_SDA_L() 				gpio_set_value(gpio_i2c_sda,0) 

#define Vk2c21_GET_SDA() 			gpio_get_value(gpio_i2c_sda)
#define Vk2c21_SET_SDA_IN() 		gpio_direction_input(gpio_i2c_sda)
#define Vk2c21_SET_SDA_OUT() 		gpio_direction_output(gpio_i2c_sda, 1)

static int gpio_i2c_scl,gpio_i2c_sda;
/**
******************************************************************************
* @file    vk2c21.c
* @author  kevin_guo
* @version V1.2
* @date    05-17-2020
* @brief   This file contains all the vk2c21 functions. 
*          ���ļ������� VK2c21
******************************************************************************
* @attention
******************************************************************************
*/	
#include "lcd_vk2c21.h"
  
#define VK2c21_CLK 100 //SCL�ź���Ƶ��,��delay_nusʵ�� 50->10kHz 10->50kHz 5->100kHz
//����seg��
//4com 
//VK2C21A/B/C/D
#define 	Vk2c21_SEGNUM				13
#define 	Vk2c21A_MAXSEGNUM			20
#define 	Vk2c21B_MAXSEGNUM			16
#define 	Vk2c21C_MAXSEGNUM			12
#define 	Vk2c21D_MAXSEGNUM			8

//segtab[]�����Ӧʵ�ʵ�оƬ��LCD���ߣ����߼�-VK2c21�ο���·
//4com 
//Vk2c21A 
unsigned char vk2c21_segtab[Vk2c21_SEGNUM]={
	18,17,16,15,14,13,12,11,10,
	9,8,7,6
};

//4com
unsigned char vk2c21_dispram[Vk2c21_SEGNUM/2];//4COMʱÿ���ֽ����ݶ�Ӧ2��SEG
//8com
//unsigned char vk2c21_dispram[Vk2c21_SEGNUM];//8COMʱÿ���ֽ����ݶ�Ӧ1��SEG

unsigned char shuzi_zimo[15]= //���ֺ��ַ���ģ
{
	//0    1    2    3    4    5    6    7    8    9    -    L    o    H    i 
	0xf5,0x05,0xb6,0x97,0x47,0xd3,0xf3,0x85,0xf7,0xd7,0x02,0x70,0x33,0x67,0x50
};
unsigned char vk2c21_segi,vk2c21_comi;
unsigned char vk2c21_maxcom;//������com��VK2C23A������4com����8com
unsigned char vk2c21_maxseg;//Vk2c21A=20 Vk2c21B=16 Vk2c21C=12 Vk2c21D=8
/* Private function prototypes -----------------------------------------------*/
unsigned char Vk2c21_InitSequence(void);
/* Private function ----------------------------------------------------------*/


/*******************************************************************************
* Function Name  : delay_nus
* Description    : ��ʱ1uS����
* Input          : n->��ʱʱ��nuS
* Output         : None
* Return         : None
*******************************************************************************/
static void delay_nus(unsigned int n)	   
{
	ndelay(n);
}
/*******************************************************************************
* Function Name			  : I2CStart   I2CStop  I2CSlaveAck
* I2CStart Description    : ʱ���߸�ʱ���������ɸߵ��͵����䣬��ʾI2C��ʼ�ź�
* I2CStop Description	  : ʱ���߸�ʱ���������ɵ͵��ߵ����䣬��ʾI2Cֹͣ�ź�
* I2CSlaveAck Description : I2C�ӻ��豸Ӧ���ѯ
*******************************************************************************/
static void Vk2c21_I2CStart( void )
{
  Vk2c21_SCL_H();
  Vk2c21_SDA_H();
  delay_nus(VK2c21_CLK);
  Vk2c21_SDA_L();
  delay_nus(VK2c21_CLK);
}

static void Vk2c21_I2CStop( void )
{
  Vk2c21_SCL_H();
  Vk2c21_SDA_L();
  delay_nus(VK2c21_CLK);
  Vk2c21_SDA_H();
  delay_nus(VK2c21_CLK);
}

static unsigned char Vk2c21_I2CSlaveAck( void )
{
  unsigned int TimeOut;
  unsigned char RetValue;
	
  Vk2c21_SCL_L();
	//��Ƭ��SDA��Ϊ����IOҪ��Ϊ�����
	Vk2c21_SET_SDA_IN();
  delay_nus(VK2c21_CLK);
  Vk2c21_SCL_H();//��9��sclk������

  TimeOut = 10000;
  while( TimeOut-- > 0 )
  {
    if( Vk2c21_GET_SDA()!=0 )//��ȡack
    {
      RetValue = 1;
    }
    else
    {
      RetValue = 0;
      break;
    }
  } 
	Vk2c21_SCL_L();
	//��Ƭ��SDA��Ϊ����IOҪ��Ϊ�����
	Vk2c21_SET_SDA_OUT();
  //printk("---%s----ret=%d------\n",__func__,RetValue);
  return RetValue;
}
/*******************************************************************************
* Function Name  : I2CWriteByte
* Description    : I2Cдһ�ֽ�����,�������͸�λ
* Input          : byte-Ҫд�������
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_I2CWriteByte( unsigned char byte )
{
	unsigned char i=8;
	while (i--)
	{ 
		Vk2c21_SCL_L();
		if(byte&0x80)
			Vk2c21_SDA_H();
		else
			Vk2c21_SDA_L();
		byte<<=1; 
		delay_nus(VK2c21_CLK);
		Vk2c21_SCL_H();     
		delay_nus(VK2c21_CLK);
	}
}

/*************************************************************
*��������: WriteCmdVk2c21
*��������: ��Vk2C23д1������
*�������: addr  Dmod��ַ
           data  д�������
*���������SET: д��������RESET:д�����
*��    ע��
**************************************************************/
static unsigned char WriteCmdVk2c21(unsigned char cmd, unsigned char data )
{
	Vk2c21_I2CStart();

	Vk2c21_I2CWriteByte( Vk2c21_ADDR|0x00 );
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop(); 
		return 0;   
	}
	Vk2c21_I2CWriteByte( cmd );
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop(); 
		return 0;   
	}
	Vk2c21_I2CWriteByte( data );
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop(); 
		return 0;   
	}
  Vk2c21_I2CStop();
 
  return 0;    //���ز����ɰܱ�־
}
/*******************************************************************************
* Function Name  : Write1Data
* Description    : д1�ֽ����ݵ���ʾRAM
* Input          : Addr-д��ram�ĵ�ַ
*                : Dat->д��ram������
* Output         : None
* Return         : 0-ok 1-fail
*******************************************************************************/
static unsigned char Write1DataVk2c21(unsigned char Addr,unsigned char Dat)
{
	//START �ź�
	Vk2c21_I2CStart(); 
	//SLAVE��ַ
	Vk2c21_I2CWriteByte(Vk2c21_ADDR); 
	if( 1 == Vk2c21_I2CSlaveAck() )
	{		
		Vk2c21_I2CStop();
		return 1; 
	}
	//д��ʾRAM����
	Vk2c21_I2CWriteByte(Vk2c21_RWRAM); 						
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;
	}
	//��ʾRAM��ַ
	Vk2c21_I2CWriteByte(Addr); 
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();
		return 1;
	}
	//��ʾ���ݣ�1�ֽ����ݰ���2��SEG
	Vk2c21_I2CWriteByte(Dat);
	if( Vk2c21_I2CSlaveAck()==1 )
	{
		Vk2c21_I2CStop();
		return 1;
	}
	//STOP�ź�
 Vk2c21_I2CStop();
 return 0;   
}
/*******************************************************************************
* Function Name  : WritenData
* Description    : д������ݵ���ʾRAM
* Input          : Addr-д��ram����ʼ��ַ
*                : Databuf->д��ram������bufferָ��
*                : Cnt->д��ram�����ݸ���
* Output         : None
* Return         : 0-ok 1-fail
*******************************************************************************/
static unsigned char  WritenDataVk2c21(unsigned char Addr,unsigned char *Databuf,unsigned char Cnt)
{
	unsigned char n;
	
	//START�ź�	
	Vk2c21_I2CStart(); 									
	//SLAVE��ַ
	Vk2c21_I2CWriteByte(Vk2c21_ADDR); 	
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;										
	}
	//д��ʾRAM����
	Vk2c21_I2CWriteByte(Vk2c21_RWRAM); 						
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;
	}
	//��ʾRAM��ʼ��ַ
	Vk2c21_I2CWriteByte(Addr); 						
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;
	}
	//����Cnt�����ݵ���ʾRAM
	for(n=0;n<Cnt;n++)
	{ 
		Vk2c21_I2CWriteByte(*Databuf++);
		if( Vk2c21_I2CSlaveAck()==1 )
		{
			Vk2c21_I2CStop();													
			return 0;
		}
	}
	//STOP�ź�
	 Vk2c21_I2CStop();											
	 return 0;    
}
/*******************************************************************************
* Function Name  : Vk2c21_DisAll
* Description    : ����SEG��ʾͬһ�����ݣ�bit7/bit3-COM3 bit6/bit2-COM2 bit5/bit1-COM1 bit4/bit0-COM0
* 					     : ���磺0xffȫ�� 0x00ȫ�� 0x55�������� 0xaa�������� 0x33�������� 
* Input          ��dat->д��ram������(1���ֽ����ݶ�Ӧ2��SEG)  
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_DisAll(unsigned char dat)
{
	unsigned char segi;
	unsigned char dispram[16];
	
	if(vk2c21_maxcom==4)
	{
		for(segi=0;segi<10;segi++)
		{				
			dispram[segi]=dat;
		}
		WritenDataVk2c21(0,dispram,10);//������8bit���ݶ�Ӧ2��SEG��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
	}
	else
	{
		for(segi=0;segi<16;segi++)
		{
			dispram[segi]=dat;
		}
		WritenDataVk2c21(0,dispram,16);//������8bit���ݶ�Ӧ1��SEG��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
	}
}
/*******************************************************************************
* Function Name  : DisSegComOn
* Description    : ����1����(1��seg��1��com�����Ӧ����ʾ��)
* Input          ��seg->���Ӧ��seg��  
* 		           ��com->���Ӧcom��  
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_DisSegComOn(unsigned char seg,unsigned char com)
{
	if(vk2c21_maxcom==4)
	{
		if(seg%2==0)
			Write1DataVk2c21(seg/2,(1<<(com)));//������8λ���ݵ�4bit��Ч��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK)
		else
			Write1DataVk2c21(seg/2,(1<<(4+com)));//������8λ���ݸ�4bit��Ч��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
	}
	else
	{
		Write1DataVk2c21(seg,(1<<(com)));//������8λ���ݵ�4bit��Ч��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
	}
}
/*******************************************************************************
* Function Name  : DisSegComOff
* Description    : �ر�1����(1��seg��1��com�����Ӧ����ʾ��)
* Input          ��seg->���Ӧ��seg��  
* 		           ��com->���Ӧcom��  
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_DisSegComOff(unsigned char seg,unsigned char com)
{
	if(vk2c21_maxcom==4)
	{
		if(seg%2==0)
			Write1DataVk2c21(seg/2,~(1<<com));//������8λ���ݵ�4bit��Ч��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
		else
			Write1DataVk2c21(seg/2,~(1<<(4+com)));//������8λ���ݸ�4bit��Ч��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
	}
	else
	{
		Write1DataVk2c21(seg,~(1<<com));//������8λ���ݵ�4bit��Ч��ÿ8bit���ݵ�ַ��1��ÿ8λ����1��ACK
	}
}

static void disp_3num(unsigned int dat)
{		
	unsigned dat8;
	
	if(vk2c21_maxcom==4)
	{
		dat8=dat/100%10;	//��ʾ��λ
		vk2c21_dispram[0]&=0xf0;
		vk2c21_dispram[0]|=shuzi_zimo[dat8]&0x0f;
		vk2c21_dispram[0]&=0x8f;
		vk2c21_dispram[0]|=shuzi_zimo[dat8]&0xf0;
		
		dat8=dat/10%10; 	//��ʾʮλ
		vk2c21_dispram[1]&=0xf0;
		vk2c21_dispram[1]|=shuzi_zimo[dat8]&0x0f;
		vk2c21_dispram[1]&=0x8f;
		vk2c21_dispram[1]|=shuzi_zimo[dat8]&0xf0;
		
		dat8=dat%10;			//��ʾ��λ
		vk2c21_dispram[2]&=0xf0;
		vk2c21_dispram[2]|=shuzi_zimo[dat8]&0x0f;
		vk2c21_dispram[2]&=0x8f;
		vk2c21_dispram[2]|=shuzi_zimo[dat8]&0xf0;
			
		if(dat<100)				//����С��100����λ����ʾ
		{
			vk2c21_dispram[0]&=0xf0;
			vk2c21_dispram[0]&=0x8f;
		}
		if(dat<10) 	//����С��10��ʮλ����ʾ
		{
			vk2c21_dispram[1]&=0xf0;
			vk2c21_dispram[1]&=0x8f;
		}
		//SEG������1��1��������
		Write1DataVk2c21(vk2c21_segtab[2]/2,vk2c21_dispram[0]);
		Write1DataVk2c21(vk2c21_segtab[4]/2,vk2c21_dispram[1]);
		Write1DataVk2c21(vk2c21_segtab[6]/2,vk2c21_dispram[2]);
		Write1DataVk2c21(vk2c21_segtab[8]/2,vk2c21_dispram[0]);
		Write1DataVk2c21(vk2c21_segtab[10]/2,vk2c21_dispram[1]);
		Write1DataVk2c21(vk2c21_segtab[12]/2,vk2c21_dispram[2]);
		//SEG�����Ͷ������
	}
	else
	{
		dat8=dat/100%10;	//��ʾ��λ
		vk2c21_dispram[8]&=0x80;
		vk2c21_dispram[8]|=shuzi_zimo[dat8];
		
		dat8=dat/10%10; 	//��ʾʮλ
		vk2c21_dispram[9]&=0x80;
		vk2c21_dispram[9]|=shuzi_zimo[dat8];
		
		dat8=dat%10;			//��ʾ��λ
		vk2c21_dispram[15]&=0x80;
		vk2c21_dispram[15]|=shuzi_zimo[dat8];
			
		if(dat<100)				//����С��100����λ����ʾ
		{
			vk2c21_dispram[8]&=0x80;
		}
		if(dat<10) 	//����С��10��ʮλ����ʾ
		{
			vk2c21_dispram[9]&=0x80;
		}
		//SEG������1��1��������
		Write1DataVk2c21(8,vk2c21_dispram[8]);
		Write1DataVk2c21(9,vk2c21_dispram[9]);
		Write1DataVk2c21(15,vk2c21_dispram[15]);
		//SEG�����Ͷ������
	}
}	

/*******************************************************************************
* Function Name  : Init
* Description    : ��ʼ������
* Input          ��None 
* Output         : None
* Return         : None
*******************************************************************************/
static int Vk2c21_Init(void)
{	
	int ret=0;
	WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_3__4); 	//ģʽ����  1/3 Bais 1/4 Duty
	//WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_4__4); //ģʽ����  1/4 Bais 1/4 Duty
	vk2c21_maxcom=4;
	vk2c21_maxseg=Vk2c21A_MAXSEGNUM;//Vk2c21A=20 Vk2c21B=16 Vk2c21C=12 Vk2c21D=8
//	WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_3__8); //ģʽ����  1/3 Bais 1/8 Duty
//	WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_4__8); //ģʽ����  1/4 Bais 1/8 Duty
//	vk2c21_maxcom=8;
//	vk2c21_maxseg=Vk2c21A_MAXSEGNUM;//Vk2c21A=16 Vk2c21B=12 Vk2c21C=8 Vk2c21D=4
	WriteCmdVk2c21(Vk2c21_SYSSET,SYSON_LCDON); 		//�ڲ�ϵͳ��������lcd����ʾ
	WriteCmdVk2c21(Vk2c21_FRAMESET,FRAME_80HZ); 	//֡Ƶ��80Hz
//	WriteCmdVk2c21(Vk2c21_FRAMESET,FRAME_160HZ);//֡Ƶ��160Hz
	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_OFF); 		//��˸�ر�	
//	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_2HZ); 		//��˸2HZ
//	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_1HZ); 		//��˸1HZ
//	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_0D5HZ); 	//��˸0.5HZ
	//SEG/VLCD���ý���ΪVLCD���ڲ���ѹ�������ܹر�,VLCD��VDD�̽�VR=0ƫ�õ�ѹ=VDD
	//WriteCmdVk2c21(Vk2c21_IVASET,VLCDSEL_IVAOFF_R0); 
	//SEG/VLCD���ý���ΪVLCD���ڲ���ѹ�������ܹر�,VLCD��VDD���ӵ���VR>0ƫ�õ�ѹ=VLCD
	WriteCmdVk2c21(Vk2c21_IVASET,VLCDSEL_IVAOFF_R1); 
	//SEG/VLCD���ý���ΪSEG���ڲ�ƫ�õ�ѹ������1/3bias=0.652VDD 1/4bias=0.714VDD
	//WriteCmdVk2c21(Vk2c21_IVASET,SEGSEL_IVA02H);	

	//test
	Vk2c21_DisAll(0x00);
	disp_3num(456);
	//Vk2c21_DisAll(0xff);			//LCDȫ��
	//disp_3num(1234);
	return ret;
}

static int Vk2c21_open(struct inode *inode, struct file *file) 
{                                                                                                                                   
	int ret;                                                                                                                  
	ret = nonseekable_open(inode, file);                                                                                       
	if(ret < 0)                                                                                                          
		return ret;            
	return 0;                                                                                                     
}

static int Vk2c21_release(struct inode *inode, struct file *file) 
{
	return 0;
}

static long Vk2c21_ioctl(struct file *file, unsigned int cmd, unsigned long args) 
{
	int ret = 0 , i=0,pos_temp=0;
	void __user *argp = (void __user *)args;
	unsigned char display_arg[6]={0};
	switch (cmd){
		case IOCTL_CHAR_DISPLAY:
			if (args) {  
				ret = copy_from_user(display_arg, argp,sizeof(display_arg)/sizeof(display_arg[0]));
			}
			for(i=0;i<6;i++)
			{
				if(display_arg[i] <= '9' && display_arg[i] >= '0')
				{
					pos_temp = display_arg[i] - '0';  
					vk2c21_dispram[i]&=0xf0;
					vk2c21_dispram[i]|=shuzi_zimo[pos_temp]&0x0f;
					vk2c21_dispram[i]&=0x8f;
					vk2c21_dispram[i]|=shuzi_zimo[pos_temp]&0xf0;
					Write1DataVk2c21(vk2c21_segtab[2*i+2]/2,vk2c21_dispram[i]);
				}else if(display_arg[i] <= 'z' && display_arg[i] >= 'a'){	
				}else if(display_arg[i] <= 'Z' && display_arg[i] >= 'A'){
				}
			}
			break;
		case IOCTL_DOT_DISPLAY:
			break;
		case IOCTL_COLON_DISPLAY:
			break;
		case IOCTL_PWR_DISPLAY:
			break;
		case IOCTL_LAN_DISPLAY:
			Vk2c21_DisSegComOn(vk2c21_segtab[6],0x1);
			break;
		case IOCTL_LAN_OFF:
			Vk2c21_DisSegComOff(vk2c21_segtab[6],0x1);
			break;
		case IOCTL_WIFI_LOW_DISPLAY:
			break;
		case IOCTL_WIFI_FINE_DISPLAY:
			break;
		case IOCTL_WIFI_OFF:
			break;
		case IOCTL_LED_ON:
			Vk2c21_DisAll(0xff);			//LCDȫ��
			break;
		case IOCTL_LED_OFF:
			Vk2c21_DisAll(0x00);			//LCDȫ��
			break;
		default:
			printk("ERROR: IOCTL CMD NOT FOUND!!!\n");
			break;
	}
	return 0;
}
static struct file_operations Vk2c21_fops ={
	.owner =THIS_MODULE,
	.open  =Vk2c21_open,
	.release =Vk2c21_release,
	.unlocked_ioctl =Vk2c21_ioctl,
};



static int Vk2c21_probe(struct platform_device *pdev)
{
	static struct class * scull_class;
	struct device_node *node = pdev->dev.of_node;
	enum of_gpio_flags flags;
	int ret; 

	ret =register_chrdev(0,DEVICE_NAME,&Vk2c21_fops);
	if(ret<0){
		printk("can't register device lcd_vk2c21.\n");
		return ret;
	}
	printk("register device lcd_vk2c21 success.\n");

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
		printk("get property gpio,i2c vk2c21 failed \n");
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
		printk("get property gpio,i2c vk2c21 failed \n");
		return -1;
	} 

	printk("==========%s probe ok========\n", DEVICE_NAME);

	ret = Vk2c21_Init();	
	if(ret < 0)
		return -1;


	return 0;
}

static int Vk2c21_remove(struct platform_device *pdev)
{
	unregister_chrdev(0,DEVICE_NAME);
	return 0;
}


static void Vk2c21_shutdown (struct platform_device *pdev)
{
		WriteCmdVk2c21(Vk2c21_SYSSET,SYSOFF_LCDOFF);
}

#ifdef CONFIG_OF
static const struct of_device_id Vk2c21_dt_match[]={
	{ .compatible = "lcd_vk2c21",},
	{}
};
MODULE_DEVICE_TABLE(of, Vk2c21_dt_match);
#endif

static struct platform_driver Vk2c21_driver = {
	.probe  = Vk2c21_probe,
	.remove = Vk2c21_remove,
	.shutdown     = Vk2c21_shutdown,
	.driver = {
		.name  = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(Vk2c21_dt_match),
#endif
	},
};


static int __init led_vk2c21_init(void)
{
	int ret;
	DBG_PRINT("%s\n=============================================\n", __FUNCTION__);
	ret = platform_driver_register(&Vk2c21_driver);
	if (ret) {
		printk("[error] %s failed to register vk2c21 driver module\n", __FUNCTION__);
		return -ENODEV;
	}
	return ret;
}

static void __exit led_vk2c21_exit(void)
{
	platform_driver_unregister(&Vk2c21_driver);
}

module_init(led_vk2c21_init);
module_exit(led_vk2c21_exit);

MODULE_AUTHOR("Hugsun");
MODULE_DESCRIPTION("LCD Extern driver for lcd_vk2c21");
MODULE_LICENSE("GPL");
