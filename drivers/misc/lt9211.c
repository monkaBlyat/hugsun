#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "lt9211.h"

uint16_t hact, vact;
uint16_t hs, vs;
uint16_t hbp, vbp;
uint16_t htotal, vtotal;
uint16_t hfp, vfp;

#define LT9211_I2C_NAME "lt9211"
static struct i2c_client *this_client;
#define I2C_SPEED 	200 * 1000
static int reset_gpio;
struct gpio_desc *ledpwd_gpio;   //control lvds backlight


struct video_timing *pVideoFormat;

#define MIPI_LANE_CNT  MIPI_4_LANE // 0: 4lane
#define MIPI_SETTLE_VALUE 0x0a //0x05  0x0a
#define LT9211_OUTPUT_PATTERN 0 //Enable it for output 9211 internal PATTERN, 1-enable
//											//hfp, hs, hbp,hact,htotal,vfp, vs, vbp, vact,vtotal,pixclk_khz
struct video_timing video_640x400_60Hz     ={ 96, 64,  58, 640,   858,   87,  6,  32, 400,   525,  27000};
struct video_timing video_640x480_60Hz     ={ 8, 96,  40, 640,   800, 33,  2,  10, 480,   525,  25000};
struct video_timing video_720x480_60Hz     ={16, 62,  60, 720,   858,  9,  6,  30, 480,   525,  27000};
struct video_timing video_1024x600_60Hz   = {160, 20, 140,1024,  1344,  12,  3,  20,  600, 635  , 55000};
struct video_timing video_1280x720_60Hz    ={110,40, 220,1280,  1650,  5,  5,  20, 720,   750,  74250};
struct video_timing video_1280x720_30Hz    = {110,40, 220,1280,  1650,  5,  5,  20, 720,   750,  37125};
struct video_timing video_1366x768_60Hz    ={26, 110,110,1366,  1592,  13, 6,  13, 768,   800,  73000};  //81000
struct video_timing video_1280x1024_60Hz   ={100,100,208,1280,  1688,  5,  5,  32, 1024, 1066, 107960};
struct video_timing video_1920x1080_25Hz   = {88, 44, 148,1920,  2200,  4,  5,  36, 1080, 1125,  74250};
struct video_timing video_1920x1080_30Hz   ={88, 44, 148,1920,  2200,  4,  5,  36, 1080, 1125,  74250};
struct video_timing video_1920x1080_60Hz   ={88, 44, 148,1920,  2200,  4,  5,  36, 1080, 1125, 148500};
//struct video_timing video_1920x1080_60Hz   ={65, 10, 65,1920,  2060,  20,  5,  20, 1080, 1125, 148500};
struct video_timing video_3840x1080_60Hz   ={176,88, 296,3840,  4400,  4,  5,  36, 1080, 1125, 297000};
struct video_timing video_1920x1200_60Hz   ={48, 32,  80,1920,  2080,  3,  6,  26, 1200, 1235, 154000};
struct video_timing video_3840x2160_30Hz   ={176,88, 296,3840,  4400,  8,  10, 72, 2160, 2250, 297000};
struct video_timing video_3840x2160_60Hz   ={176,88, 296,3840,  4400,  8,  10, 72, 2160, 2250, 594000};
struct video_timing video_1920x720_60Hz    ={148, 44, 88,1920,  2200,  28,  5, 12,  720, 765, 88000};  

/* i2c function*/
static int i2c_master_reg8_send(const struct i2c_client *client, const char reg, const char *buf, int count, int scl_rate)
{
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msg;
	int ret;
	char *tx_buf = (char *)kzalloc(count + 1, GFP_KERNEL);
	if(!tx_buf)
		return -ENOMEM;
	tx_buf[0] = reg;
	memcpy(tx_buf+1, buf, count); 

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = count + 1;
	msg.buf = (char *)tx_buf;
	//msg.scl_rate = scl_rate;

	ret = i2c_transfer(adap, &msg, 1);
	kfree(tx_buf);
	//printk("---%s----ret=%d--count=%d---\n",__func__,ret,count);
	return (ret == 1) ? count : ret;

}

static int i2c_master_reg8_recv(const struct i2c_client *client, const char reg, char *buf, int count, int scl_rate)
{
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msgs[2];
	int ret;
	char reg_buf = reg;
	
	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;
	//msgs[0].scl_rate = scl_rate;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len = count;
	msgs[1].buf = (char *)buf;
	//msgs[1].scl_rate = scl_rate;

	ret = i2c_transfer(adap, msgs, 2);
	//printk("---%s----ret=%d--count=%d---\n",__func__,ret,count);
	return (ret == 2)? count : ret;
}

static uint8_t HDMI_ReadI2C_Byte(uint8_t RegAddr)
{
	uint8_t  ucp_data=0;
	
	i2c_master_reg8_recv(this_client, RegAddr, &ucp_data, 1, I2C_SPEED);	
	return ucp_data;
}

//static bool HDMI_ReadI2C_ByteN(uint8_t RegAddr,uint8_t *ucp_data,uint8_t N)
//{
//	bool flag;
//	
//	flag = i2c_master_reg8_recv(this_client, RegAddr, ucp_data, N, I2C_SPEED);	
//	return flag;
//}

static bool HDMI_WriteI2C_Byte(uint8_t RegAddr, uint8_t d)
{
	bool flag;
	
	flag=i2c_master_reg8_send(this_client, RegAddr, &d, 1, I2C_SPEED);	
	return flag;
}

//static bool HDMI_WriteI2C_ByteN(uint8_t RegAddr, uint8_t *d,uint8_t N)
//{
//	bool flag;
//	
//	flag=i2c_master_reg8_send(this_client, RegAddr, d, N, I2C_SPEED);	
//	return flag;
//}

static void printdec_u32(u32 u32_dec)
{
}

static int LT9211_ChipID(void)
{
	HDMI_WriteI2C_Byte(0xff,0x81);//register bank
	printk("\r\nLT9211 Chip ID:%x,",HDMI_ReadI2C_Byte(0x00));
  	printk("%x, ",HDMI_ReadI2C_Byte(0x01));
	printk("%x, ",HDMI_ReadI2C_Byte(0x02));
	if (HDMI_ReadI2C_Byte(0x00)==0)
		return -1;
	return 0;	
}

static void LT9211_SystemInt(void)
{
	  /* system clock init */		   
	  HDMI_WriteI2C_Byte(0xff,0x82);
	  HDMI_WriteI2C_Byte(0x01,0x18);
	
		HDMI_WriteI2C_Byte(0xff,0x86);
		HDMI_WriteI2C_Byte(0x06,0x61); 	
		HDMI_WriteI2C_Byte(0x07,0xa8); //fm for sys_clk
	  
	  HDMI_WriteI2C_Byte(0xff,0x87); //
	  HDMI_WriteI2C_Byte(0x14,0x08); //default value
	  HDMI_WriteI2C_Byte(0x15,0x00); //default value
	  HDMI_WriteI2C_Byte(0x18,0x0f);
	  HDMI_WriteI2C_Byte(0x22,0x08); //default value
	  HDMI_WriteI2C_Byte(0x23,0x00); //default value
	  HDMI_WriteI2C_Byte(0x26,0x0f); 
}

static void LT9211_MipiRxPhy(void)
{
#if 1//yelsin for WYZN CUSTOMER 2Lane 9211 mipi2RGB
		HDMI_WriteI2C_Byte(0xff,0xd0);
		HDMI_WriteI2C_Byte(0x00,MIPI_LANE_CNT);	// 0: 4 Lane / 1: 1 Lane / 2 : 2 Lane / 3: 3 Lane
#endif

	/* Mipi rx phy */
		HDMI_WriteI2C_Byte(0xff,0x82);
		HDMI_WriteI2C_Byte(0x02,0x44); //port A mipi rx enable
	/*port A/B input 8205/0a bit6_4:EQ current setting*/
	  HDMI_WriteI2C_Byte(0x05,0x36); //port A CK lane swap  0x32--0x36 for WYZN Glassbit2- Port A mipi/lvds rx s2p input clk select: 1 = From outer path.
    HDMI_WriteI2C_Byte(0x0d,0x26); //bit6_4:Port B Mipi/lvds rx abs refer current  0x26 0x76
		HDMI_WriteI2C_Byte(0x17,0x0c);
		HDMI_WriteI2C_Byte(0x1d,0x0c);
	
		HDMI_WriteI2C_Byte(0x0a,0x81);//eq control for LIEXIN  horizon line display issue 0xf7->0x80
		HDMI_WriteI2C_Byte(0x0b,0x00);//eq control  0x77->0x00
	#ifdef _Mipi_PortA_ 
	  /*port a*/
	  HDMI_WriteI2C_Byte(0x07,0x9f); //port clk enable  
	  HDMI_WriteI2C_Byte(0x08,0xfc); //port lprx enable
	#endif  
	#ifdef _Mipi_PortB_	
	  /*port a*/
	  HDMI_WriteI2C_Byte(0x07,0x9f); //port clk enable  
	  HDMI_WriteI2C_Byte(0x08,0xfc); //port lprx enable	
	  /*port b*/
	  HDMI_WriteI2C_Byte(0x0f,0x9F); //port clk enable
	  HDMI_WriteI2C_Byte(0x10,0xfc); //port lprx enable
		HDMI_WriteI2C_Byte(0x04,0xa1);
	#endif
	  /*port diff swap*/
	  HDMI_WriteI2C_Byte(0x09,0x01); //port a diff swap
	  HDMI_WriteI2C_Byte(0x11,0x01); //port b diff swap	
		
	  /*port lane swap*/
	  HDMI_WriteI2C_Byte(0xff,0x86);		
	  HDMI_WriteI2C_Byte(0x33,0x1b); //port a lane swap	1b:no swap	
		HDMI_WriteI2C_Byte(0x34,0x1b); //port b lane swap 1b:no swap
		
#ifdef CSI_INPUTDEBUG
		HDMI_WriteI2C_Byte(0xff,0xd0);
		HDMI_WriteI2C_Byte(0x04,0x10);	//bit4-enable CSI mode
		HDMI_WriteI2C_Byte(0x21,0xc6);		
#endif 		
}

static void LT9211_SetVideoTiming(struct video_timing *video_format)
{
	msleep(100);
	HDMI_WriteI2C_Byte(0xff,0xd0);
	HDMI_WriteI2C_Byte(0x0d,(u8)(video_format->vtotal>>8)); //vtotal[15:8]
	HDMI_WriteI2C_Byte(0x0e,(u8)(video_format->vtotal)); //vtotal[7:0]
	HDMI_WriteI2C_Byte(0x0f,(u8)(video_format->vact>>8)); //vactive[15:8]
	HDMI_WriteI2C_Byte(0x10,(u8)(video_format->vact)); //vactive[7:0]
	HDMI_WriteI2C_Byte(0x15,(u8)(video_format->vs)); //vs[7:0]
	HDMI_WriteI2C_Byte(0x17,(u8)(video_format->vfp>>8)); //vfp[15:8]
	HDMI_WriteI2C_Byte(0x18,(u8)(video_format->vfp)); //vfp[7:0]	

	HDMI_WriteI2C_Byte(0x11,(u8)(video_format->htotal>>8)); //htotal[15:8]
	HDMI_WriteI2C_Byte(0x12,(u8)(video_format->htotal)); //htotal[7:0]
	HDMI_WriteI2C_Byte(0x13,(u8)(video_format->hact>>8)); //hactive[15:8]
	HDMI_WriteI2C_Byte(0x14,(u8)(video_format->hact)); //hactive[7:0]
	HDMI_WriteI2C_Byte(0x16,(u8)(video_format->hs)); //hs[7:0]
	HDMI_WriteI2C_Byte(0x19,(u8)(video_format->hfp>>8)); //hfp[15:8]
	HDMI_WriteI2C_Byte(0x1a,(u8)(video_format->hfp)); //hfp[7:0]	
}

static void LT9211_DesscPll_mipi(struct video_timing *video_format)
{
	u32 pclk;
	u8 pll_lock_flag;
	u8 i;
	u8 pll_post_div;
	u8 pcr_m;
	pclk = video_format->pclk_khz; 
	printk("\r\n LT9211_DesscPll: set rx pll = %ud", pclk);

	HDMI_WriteI2C_Byte(0xff,0x82);
	HDMI_WriteI2C_Byte(0x2d,0x48); 

	if(pclk > 80000)
		{
	   	 HDMI_WriteI2C_Byte(0x35,0x81);
		 pll_post_div = 0x01;
		}
		else if(pclk > 20000)
		{
		 HDMI_WriteI2C_Byte(0x35,0x82);
		 pll_post_div = 0x02;
		}
		else
		{
		 HDMI_WriteI2C_Byte(0x35,0x83); 
		 pll_post_div = 0x04;
		}

	pcr_m = (u8)((pclk*4*pll_post_div)/25000);
	printk("\r\n LT9211_DesscPll: set rx pll pcr_m = 0x%cx", pcr_m);
	
	HDMI_WriteI2C_Byte(0xff,0xd0); 	
	HDMI_WriteI2C_Byte(0x2d,0x40); //M_up_limit
	HDMI_WriteI2C_Byte(0x31,0x10); //M_low_limit
	HDMI_WriteI2C_Byte(0x26,pcr_m|0x80);

	HDMI_WriteI2C_Byte(0xff,0x81); //dessc pll sw rst
	HDMI_WriteI2C_Byte(0x20,0xef);
	HDMI_WriteI2C_Byte(0x20,0xff);

	/* pll lock status */
	for(i = 0; i < 6 ; i++)
	{   
			HDMI_WriteI2C_Byte(0xff,0x81);	
			HDMI_WriteI2C_Byte(0x11,0xfb); /* pll lock logic reset */
			HDMI_WriteI2C_Byte(0x11,0xff);
			
			HDMI_WriteI2C_Byte(0xff,0x87);
			pll_lock_flag = HDMI_ReadI2C_Byte(0x04);
			if(pll_lock_flag & 0x01)
			{
				printk("\r\n LT9211_DesscPll: dessc pll locked");
				break;
			}
			else
			{
				HDMI_WriteI2C_Byte(0xff,0x81); //dessc pll sw rst
				HDMI_WriteI2C_Byte(0x20,0xef);
				HDMI_WriteI2C_Byte(0x20,0xff);
				printk("\r\n LT9211_DesscPll: dessc pll unlocked,sw reset");
			}
	}
}

static void LT9211_RXCSC(void)
{ 
	     HDMI_WriteI2C_Byte(0xff,0xf9);
  if( LT9211_OutPutModde == OUTPUT_RGB888 )
	{
				if( Video_Input_Mode == INPUT_RGB888 )
		 {
			 HDMI_WriteI2C_Byte(0x86,0x00);
			 HDMI_WriteI2C_Byte(0x87,0x00);
		 }
		 else if ( Video_Input_Mode == INPUT_YCbCr444 )
		 {
			 HDMI_WriteI2C_Byte(0x86,0x0f);
			 HDMI_WriteI2C_Byte(0x87,0x00);
		 }
		 else if ( Video_Input_Mode == INPUT_YCbCr422_16BIT )
			{
			 HDMI_WriteI2C_Byte(0x86,0x00);
			 HDMI_WriteI2C_Byte(0x87,0x03);				
			}
	}
	else if( (LT9211_OutPutModde == OUTPUT_BT656_8BIT) || (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) || (LT9211_OutPutModde ==OUTPUT_YCbCr422_16BIT) )
	{  
		 if( Video_Input_Mode == INPUT_RGB888 )
		 {
			 HDMI_WriteI2C_Byte(0x86,0x0f);
			 HDMI_WriteI2C_Byte(0x87,0x30);
		 }
		 else if ( Video_Input_Mode == INPUT_YCbCr444 )
		 {
			 HDMI_WriteI2C_Byte(0x86,0x00);
			 HDMI_WriteI2C_Byte(0x87,0x30);
		 }
		 else if ( Video_Input_Mode == INPUT_YCbCr422_16BIT )
			{
			 HDMI_WriteI2C_Byte(0x86,0x00);
			 HDMI_WriteI2C_Byte(0x87,0x00);				
			}	 
	}
	else if( LT9211_OutPutModde == OUTPUT_YCbCr444 )
	{
		if( Video_Input_Mode == INPUT_RGB888 )
		 {
			 HDMI_WriteI2C_Byte(0x86,0x0f);
			 HDMI_WriteI2C_Byte(0x87,0x00);
		 }
		 else if ( Video_Input_Mode == INPUT_YCbCr444 )
		 {
			 HDMI_WriteI2C_Byte(0x86,0x00);
			 HDMI_WriteI2C_Byte(0x87,0x00);
		 }
		 else if ( Video_Input_Mode == INPUT_YCbCr422_16BIT )
			{
			 HDMI_WriteI2C_Byte(0x86,0x00);
			 HDMI_WriteI2C_Byte(0x87,0x03);				
			}
	}
}
static void LT9211_MipiRxDigital(void)
{	   
    HDMI_WriteI2C_Byte(0xff,0x86);
#ifdef _Mipi_PortA_ 	
    HDMI_WriteI2C_Byte(0x30,0x85); //mipirx HL swap	 	
#endif

#ifdef _Mipi_PortB_	
   HDMI_WriteI2C_Byte(0x30,0x8f); //mipirx HL swap
#endif
	 HDMI_WriteI2C_Byte(0xff,0xD8);
#ifdef _Mipi_PortA_ 	
	HDMI_WriteI2C_Byte(0x16,0x00); //mipirx HL swap	  bit7- 0:portAinput	
#endif

#ifdef _Mipi_PortB_	
   HDMI_WriteI2C_Byte(0x16,0x80); //mipirx HL swap bit7- portBinput
#endif	
	
	  HDMI_WriteI2C_Byte(0xff,0xd0);	
    HDMI_WriteI2C_Byte(0x43,0x12); //rpta mode enable,ensure da_mlrx_lptx_en=0
	
		HDMI_WriteI2C_Byte(0x02,MIPI_SETTLE_VALUE); //mipi rx controller	
#if 0 //forWTWD yelsin0520	
		  HDMI_WriteI2C_Byte(0x0c,0x40);		//MIPI read delay setting
	  HDMI_WriteI2C_Byte(0x1B,0x00);	//PCR de mode delay.[15:8]
	  HDMI_WriteI2C_Byte(0x1c,0x40);	//PCR de mode delay.[7:0]
	  HDMI_WriteI2C_Byte(0x24,0x70);	
	  HDMI_WriteI2C_Byte(0x25,0x40);
 #endif			  
}


static void LT9211_MipiPcr(void)
{	
	u8 loopx;
	u8 pcr_m;
	
    HDMI_WriteI2C_Byte(0xff,0xd0); 	
    HDMI_WriteI2C_Byte(0x0c,0x60);  //fifo position
	HDMI_WriteI2C_Byte(0x1c,0x60);  //fifo position
	HDMI_WriteI2C_Byte(0x24,0x70);  //pcr mode( de hs vs)
			
	HDMI_WriteI2C_Byte(0x2d,0x30); //M up limit
	HDMI_WriteI2C_Byte(0x31,0x02); //M down limit

	/*stage1 hs mode*/
	HDMI_WriteI2C_Byte(0x25,0xf0);  //line limit
	HDMI_WriteI2C_Byte(0x2a,0x30);  //step in limit
	HDMI_WriteI2C_Byte(0x21,0x4f);  //hs_step
	HDMI_WriteI2C_Byte(0x22,0x00); 

	/*stage2 hs mode*/
	HDMI_WriteI2C_Byte(0x1e,0x01);  //RGD_DIFF_SND[7:4],RGD_DIFF_FST[3:0]
	HDMI_WriteI2C_Byte(0x23,0x80);  //hs_step
    /*stage2 de mode*/
	HDMI_WriteI2C_Byte(0x0a,0x02); //de adjust pre line
	HDMI_WriteI2C_Byte(0x38,0x02); //de_threshold 1
	HDMI_WriteI2C_Byte(0x39,0x04); //de_threshold 2
	HDMI_WriteI2C_Byte(0x3a,0x08); //de_threshold 3
	HDMI_WriteI2C_Byte(0x3b,0x10); //de_threshold 4
		
	HDMI_WriteI2C_Byte(0x3f,0x04); //de_step 1
	HDMI_WriteI2C_Byte(0x40,0x08); //de_step 2
	HDMI_WriteI2C_Byte(0x41,0x10); //de_step 3
	HDMI_WriteI2C_Byte(0x42,0x20); //de_step 4

	HDMI_WriteI2C_Byte(0x2b,0xa0); //stable out
	//msleep(100);
    HDMI_WriteI2C_Byte(0xff,0xd0);   //enable HW pcr_m
	pcr_m = HDMI_ReadI2C_Byte(0x26);
	pcr_m &= 0x7f;
	HDMI_WriteI2C_Byte(0x26,pcr_m);
	HDMI_WriteI2C_Byte(0x27,0x0f);

	HDMI_WriteI2C_Byte(0xff,0x81);  //pcr reset
	HDMI_WriteI2C_Byte(0x20,0xbf); // mipi portB div issue
	HDMI_WriteI2C_Byte(0x20,0xff);
	msleep(5);
	HDMI_WriteI2C_Byte(0x0B,0x6F);
	HDMI_WriteI2C_Byte(0x0B,0xFF);
	

	 	msleep(800);//800->120
		 {for(loopx = 0; loopx < 10; loopx++) //Check pcr_stable 10
		   {
			msleep(200);
			HDMI_WriteI2C_Byte(0xff,0xd0);
			if(HDMI_ReadI2C_Byte(0x87)&0x08)
			 {
				printk("\r\nLT9211 pcr stable");
				break;
		   }
			else
			 {
				printk("\r\nLT9211 pcr unstable!!!!");
			 }
	  	}
  	}
	
	HDMI_WriteI2C_Byte(0xff,0xd0);
	printk("LT9211 pcr_stable_M=%x\n",(HDMI_ReadI2C_Byte(0x94)&0x7F));
}


static void LT9211_Txpll(void)
{
	  u8 loopx;
 if( LT9211_OutPutModde == OUTPUT_BT656_8BIT )
	 {
	  HDMI_WriteI2C_Byte(0xff,0x82);
	  HDMI_WriteI2C_Byte(0x2d,0x40);
	  HDMI_WriteI2C_Byte(0x30,0x50);
	  HDMI_WriteI2C_Byte(0x33,0x55);
	 }
 else if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde == OUTPUT_LVDS_1_PORT) || (LT9211_OutPutModde == OUTPUT_RGB888) || (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) )
  {
		HDMI_WriteI2C_Byte(0xff,0x82);
		HDMI_WriteI2C_Byte(0x36,0x01); //b7:txpll_pd
		 if( LT9211_OutPutModde == OUTPUT_LVDS_1_PORT )
		 {
			 HDMI_WriteI2C_Byte(0x37,0x29);
		 }
		 else
		 {
	  HDMI_WriteI2C_Byte(0x37,0x2a);
		 }
		HDMI_WriteI2C_Byte(0x38,0x06);
		HDMI_WriteI2C_Byte(0x39,0x30);
		HDMI_WriteI2C_Byte(0x3a,0x8e);
		HDMI_WriteI2C_Byte(0xff,0x87);
		HDMI_WriteI2C_Byte(0x37,0x14);
		HDMI_WriteI2C_Byte(0x13,0x00);
		HDMI_WriteI2C_Byte(0x13,0x80);
		msleep(100);
		for(loopx = 0; loopx < 10; loopx++) //Check Tx PLL cal
		{
			
	    HDMI_WriteI2C_Byte(0xff,0x87);			
			if(HDMI_ReadI2C_Byte(0x1f)& 0x80)
			{
				if(HDMI_ReadI2C_Byte(0x20)& 0x80)
				{
					printk("\r\nLT9211 tx pll lock");
				}
				else
				{
				 printk("\r\nLT9211 tx pll unlocked");
        }					
				printk("\r\nLT9211 tx pll cal done");
				break;
		  }
			else
			{
				printk("\r\nLT9211 tx pll unlocked");
			}
		}
  } 
		 printk("\r\n system success");	 	
}

static void LT9211_TxPhy(void)
{		
	  HDMI_WriteI2C_Byte(0xff,0x82);
	if( (LT9211_OutPutModde == OUTPUT_RGB888) || (LT9211_OutPutModde ==OUTPUT_BT656_8BIT) || (LT9211_OutPutModde ==OUTPUT_BT1120_16BIT) )
	 {
  	HDMI_WriteI2C_Byte(0x62,0x01); //ttl output enable
	  HDMI_WriteI2C_Byte(0x6b,0xff);
	 }
	else if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde ==OUTPUT_LVDS_1_PORT) )
	{
		 /* dual-port lvds tx phy */	
  	HDMI_WriteI2C_Byte(0x62,0x00); //ttl output disable
		if(LT9211_OutPutModde == OUTPUT_LVDS_2_PORT)
		{
		  HDMI_WriteI2C_Byte(0x3b,0xb8);
		}
		else
		{
			HDMI_WriteI2C_Byte(0x3b,0x38);
		}
	 // HDMI_WriteI2C_Byte(0x3b,0xb8); //dual port lvds enable	
		HDMI_WriteI2C_Byte(0x3e,0x92); 
		HDMI_WriteI2C_Byte(0x3f,0x48); 	
		HDMI_WriteI2C_Byte(0x40,0x31); 		
		HDMI_WriteI2C_Byte(0x43,0x80); 		
		HDMI_WriteI2C_Byte(0x44,0x00);
		HDMI_WriteI2C_Byte(0x45,0x00); 		
		HDMI_WriteI2C_Byte(0x49,0x00);
		HDMI_WriteI2C_Byte(0x4a,0x01);
		HDMI_WriteI2C_Byte(0x4e,0x00);		
		HDMI_WriteI2C_Byte(0x4f,0x00);
		HDMI_WriteI2C_Byte(0x50,0x00);
		HDMI_WriteI2C_Byte(0x53,0x00);
		HDMI_WriteI2C_Byte(0x54,0x01);
		HDMI_WriteI2C_Byte(0xff,0x81);
		HDMI_WriteI2C_Byte(0x20,0x7b); 
		HDMI_WriteI2C_Byte(0x20,0xff); //mlrx mltx calib reset
	}
}

/*InvertRGB_HSVSPoarity, and only for TTL output
*/
static void LT9211_InvertRGB_HSVSPoarity(void)
{
	u8 Reg8263_Value, RegD020_Value;

	printk("\rLT9211 invert HS VS Poarity"); 
	HDMI_WriteI2C_Byte(0xff,0x82);
	Reg8263_Value = HDMI_ReadI2C_Byte(0x63);		
	printk("\r\n reg0x8263= %x", Reg8263_Value);//0xff
	
	HDMI_WriteI2C_Byte(0xff,0xd0);
	RegD020_Value = HDMI_ReadI2C_Byte(0x20);		
	printk("\r\n reg0xD020= %x", RegD020_Value);//0x07

	 // HDMI_WriteI2C_Byte(0xff,0x82);
   // HDMI_WriteI2C_Byte(0x63,0xfb); //0x8263 Bit2 =0 disable DE output MODE
	
	 //to invert HS VS Poarity to Negative in REG 0xD020 bit7-6 to 1, Not bit5-DE
    HDMI_WriteI2C_Byte(0xff,0xD0);
    HDMI_WriteI2C_Byte(0x20, 0xC8); //0x08
}	

static void LT9211_TxDigital(void)
{
    printk("\r\nLT9211 OUTPUT_MODE: ");
	if( LT9211_OutPutModde == OUTPUT_RGB888 )
	 {
		printk("\rLT9211 set to OUTPUT_RGB888");
    HDMI_WriteI2C_Byte(0xff,0x85);
    HDMI_WriteI2C_Byte(0x88,0x50);
		HDMI_WriteI2C_Byte(0x60,0x00);
		HDMI_WriteI2C_Byte(0x6d,0x07);//0x07
		HDMI_WriteI2C_Byte(0x6E,0x00);
		HDMI_WriteI2C_Byte(0xff,0x81);
		HDMI_WriteI2C_Byte(0x36,0xc0); //bit7:ttltx_pixclk_en;bit6:ttltx_BT_clk_en
	 }
	else if( LT9211_OutPutModde == OUTPUT_BT656_8BIT )
	 {
		printk("\rLT9211 set to OUTPUT_BT656_8BIT");
	 	HDMI_WriteI2C_Byte(0xff,0x85);
    HDMI_WriteI2C_Byte(0x88,0x40);
        HDMI_WriteI2C_Byte(0x60,0x34); // bit5_4 BT TX module hsync/vs polarity bit2-Output 8bit BT data enable
        HDMI_WriteI2C_Byte(0x6d,0x00);//0x08 YC SWAP BIT[2:0]=000-outputBGR
		HDMI_WriteI2C_Byte(0x6e,0x07);//0x07-low 16BIT ; 0x06-High 8
		
		HDMI_WriteI2C_Byte(0xff,0x81);
		HDMI_WriteI2C_Byte(0x0d,0xfd);
		HDMI_WriteI2C_Byte(0x0d,0xff);
		HDMI_WriteI2C_Byte(0xff,0x81);
		HDMI_WriteI2C_Byte(0x36,0xc0); //bit7:ttltx_pixclk_en;bit6:ttltx_BT_clk_en
	 }
	else if( LT9211_OutPutModde == OUTPUT_BT1120_16BIT )
	{ 
    printk("\rLT9211 set to OUTPUT_BT1120_16BIT");		
		HDMI_WriteI2C_Byte(0xff,0x85);
    HDMI_WriteI2C_Byte(0x88,0x40);
		HDMI_WriteI2C_Byte(0x60,0x33); //output 16 bit BT1120
		HDMI_WriteI2C_Byte(0x6d,0x08);//0x08 YC SWAP
		HDMI_WriteI2C_Byte(0x6e,0x06);//HIGH 16BIT   0x07-BT low 16bit ;0x06-High 16bit BT
		
		HDMI_WriteI2C_Byte(0xff,0x81);
		HDMI_WriteI2C_Byte(0x0d,0xfd);
		HDMI_WriteI2C_Byte(0x0d,0xff);		
	}
	else if( (LT9211_OutPutModde == OUTPUT_LVDS_2_PORT) || (LT9211_OutPutModde == OUTPUT_LVDS_1_PORT) ) 
	{
		printk("\rLT9211 set to OUTPUT_LVDS");
		HDMI_WriteI2C_Byte(0xff,0x85); /* lvds tx controller */
		HDMI_WriteI2C_Byte(0x59,0x50); 	//bit4-LVDSTX Display color depth set 1-8bit, 0-6bit; 
		HDMI_WriteI2C_Byte(0x5a,0xaa); 
		HDMI_WriteI2C_Byte(0x5b,0xaa);
		if( LT9211_OutPutModde == OUTPUT_LVDS_2_PORT )
		{
			HDMI_WriteI2C_Byte(0x5c,0x01);	//lvdstx port sel 01:dual;00:single
		}
		else
		{
			HDMI_WriteI2C_Byte(0x5c,0x00);
		}
		HDMI_WriteI2C_Byte(0x88,0x50);	
		HDMI_WriteI2C_Byte(0xa1,0x77); 	
		HDMI_WriteI2C_Byte(0xff,0x86);	
		HDMI_WriteI2C_Byte(0x40,0x40); //tx_src_sel
		/*port src sel*/
		HDMI_WriteI2C_Byte(0x41,0x34);	
		HDMI_WriteI2C_Byte(0x42,0x10);
		HDMI_WriteI2C_Byte(0x43,0x23); //pt0_tx_src_sel
		HDMI_WriteI2C_Byte(0x44,0x41);
		HDMI_WriteI2C_Byte(0x45,0x02); //pt1_tx_src_scl
		
#if 1//PORT_OUT_SWAP
			printk("\r\nSAWP reg8646 = 0x%x,",HDMI_ReadI2C_Byte(0x46));
		//HDMI_WriteI2C_Byte(0x46,0x40); //pt0/1_tx_src_scl yelsin
#endif 		

#ifdef lvds_format_JEIDA
    HDMI_WriteI2C_Byte(0xff,0x85);
		HDMI_WriteI2C_Byte(0x59,0xd0); 	
		HDMI_WriteI2C_Byte(0xff,0xd8);
		HDMI_WriteI2C_Byte(0x11,0x40);
#endif	
	}  		
}

static void LT9211_ClockCheckDebug(void)
{
#ifdef _uart_debug_
	u32 fm_value;

	HDMI_WriteI2C_Byte(0xff,0x86);
	HDMI_WriteI2C_Byte(0x00,0x0a);
	msleep(300);
    fm_value = 0;
	fm_value = (HDMI_ReadI2C_Byte(0x08) &(0x0f));
  fm_value = (fm_value<<8) ;
	fm_value = fm_value + HDMI_ReadI2C_Byte(0x09);
	fm_value = (fm_value<<8) ;
	fm_value = fm_value + HDMI_ReadI2C_Byte(0x0a);
	printk("\r\ndessc pixel clock: ");
	printdec_u32(fm_value);
#endif
}

static void LT9211_VideoCheckDebug(void)
{
#ifdef _uart_debug_
	u8 sync_polarity;

	HDMI_WriteI2C_Byte(0xff,0x86);
	sync_polarity = HDMI_ReadI2C_Byte(0x70);
	vs = HDMI_ReadI2C_Byte(0x71);

	hs = HDMI_ReadI2C_Byte(0x72);
  hs = (hs<<8) + HDMI_ReadI2C_Byte(0x73);
	
	vbp = HDMI_ReadI2C_Byte(0x74);
  vfp = HDMI_ReadI2C_Byte(0x75);

	hbp = HDMI_ReadI2C_Byte(0x76);
	hbp = (hbp<<8) + HDMI_ReadI2C_Byte(0x77);

	hfp = HDMI_ReadI2C_Byte(0x78);
	hfp = (hfp<<8) + HDMI_ReadI2C_Byte(0x79);

	vtotal = HDMI_ReadI2C_Byte(0x7A);
	vtotal = (vtotal<<8) + HDMI_ReadI2C_Byte(0x7B);

	htotal = HDMI_ReadI2C_Byte(0x7C);
	htotal = (htotal<<8) + HDMI_ReadI2C_Byte(0x7D);

	vact = HDMI_ReadI2C_Byte(0x7E);
	vact = (vact<<8)+ HDMI_ReadI2C_Byte(0x7F);

	hact = HDMI_ReadI2C_Byte(0x80);
	hact = (hact<<8) + HDMI_ReadI2C_Byte(0x81);

	printk("\r\nsync_polarity = %x", sync_polarity);

  printk("\r\nhfp, hs, hbp, hact, htotal = ");
	printdec_u32(hfp);
	printdec_u32(hs);
	printdec_u32(hbp);
	printdec_u32(hact);
	printdec_u32(htotal);

	printk("\r\nvfp, vs, vbp, vact, vtotal = ");
	printdec_u32(vfp);
	printdec_u32(vs);
	printdec_u32(vbp);
	printdec_u32(vact);
	printdec_u32(vtotal);
#endif
}
static void LT9211_BT_Set(void)
{
	 uint16_t tmp_data;
	if( (LT9211_OutPutModde == OUTPUT_BT1120_16BIT) || (LT9211_OutPutModde == OUTPUT_BT656_8BIT) )
	{
		tmp_data = hs+hbp;
		HDMI_WriteI2C_Byte(0xff,0x85);
		HDMI_WriteI2C_Byte(0x61,(u8)(tmp_data>>8));
		HDMI_WriteI2C_Byte(0x62,(u8)tmp_data);
		HDMI_WriteI2C_Byte(0x63,(u8)(hact>>8));
		HDMI_WriteI2C_Byte(0x64,(u8)hact);
		HDMI_WriteI2C_Byte(0x65,(u8)(htotal>>8));
		HDMI_WriteI2C_Byte(0x66,(u8)htotal);		
		tmp_data = vs+vbp;
		HDMI_WriteI2C_Byte(0x67,(u8)tmp_data);
		HDMI_WriteI2C_Byte(0x68,0x00);
		HDMI_WriteI2C_Byte(0x69,(u8)(vact>>8));
		HDMI_WriteI2C_Byte(0x6a,(u8)vact);
		HDMI_WriteI2C_Byte(0x6b,(u8)(vtotal>>8));
		HDMI_WriteI2C_Byte(0x6c,(u8)vtotal);		
	}
}

static void LT9211_TimingSet(void)
{
  uint16_t hact ;	
	uint16_t vact ;	
	u8 fmt ;		
	u8 pa_lpn = 0;
	
	msleep(500);//500-->100
	HDMI_WriteI2C_Byte(0xff,0xd0);
	hact = (HDMI_ReadI2C_Byte(0x82)<<8) + HDMI_ReadI2C_Byte(0x83) ;
	hact = hact/3;
	fmt = (HDMI_ReadI2C_Byte(0x84) &0x0f);
	vact = (HDMI_ReadI2C_Byte(0x85)<<8) +HDMI_ReadI2C_Byte(0x86);
	pa_lpn = HDMI_ReadI2C_Byte(0x9c);
	printdec_u32(hact);
	printdec_u32(vact);		
	printk("\r\nhact = %x", hact);
	printk("\r\nvact = %x", vact);
	printk("\r\nfmt = %x", fmt);
	printk("\r\npa_lpn = %x", pa_lpn);	

	msleep(100);
 if ((hact == video_1280x720_60Hz.hact ) &&( vact == video_1280x720_60Hz.vact ))
   {
		 pVideoFormat = &video_1280x720_60Hz;
	 }
	 else if ((hact == video_640x400_60Hz.hact ) &&( vact == video_640x400_60Hz.vact ))
   {
		 pVideoFormat = &video_640x400_60Hz;
   } 
 else if ((hact == video_1366x768_60Hz.hact ) &&( vact == video_1366x768_60Hz.vact ))
   {
		 pVideoFormat = &video_1366x768_60Hz;
   }
 else if ((hact == video_1280x1024_60Hz.hact ) &&( vact == video_1280x1024_60Hz.vact ))
   {
		 pVideoFormat = &video_1280x1024_60Hz;
   }
 else if ((hact == video_1920x1080_60Hz.hact ) &&( vact == video_1920x1080_60Hz.vact ))
   {
		 pVideoFormat = &video_1920x1080_60Hz;
   }
 else if ((hact == video_1920x1200_60Hz.hact ) &&( vact == video_1920x1200_60Hz.vact ))
   {
		 pVideoFormat = &video_1920x1200_60Hz;
   }
 else 
   {
	   pVideoFormat = 0;//&video_1920x1080_60Hz;
		 printk("\r\nvideo_none");
   }
	 
	 LT9211_SetVideoTiming(pVideoFormat);
	 	LT9211_DesscPll_mipi(pVideoFormat);
}

#if LT9211_OUTPUT_PATTERN
static void LT9211_Pattern(struct video_timing *video_format)
{
       u32 pclk_khz;
	u8 dessc_pll_post_div;
	u32 pcr_m, pcr_k;

       pclk_khz = video_format->pclk_khz;     

       HDMI_WriteI2C_Byte(0xff,0xf9);
	HDMI_WriteI2C_Byte(0x3e,0x80);  
 
       HDMI_WriteI2C_Byte(0xff,0x85);
	HDMI_WriteI2C_Byte(0x88,0xc0);   //0x90:TTL RX-->Mipi TX  ; 0xd0:lvds RX->MipiTX  0xc0:Chip Video pattern gen->Lvd TX

       HDMI_WriteI2C_Byte(0xa1,0x64); 
       HDMI_WriteI2C_Byte(0xa2,0xff); 

	HDMI_WriteI2C_Byte(0xa3,(u8)((video_format->hs+video_format->hbp)/256));
	HDMI_WriteI2C_Byte(0xa4,(u8)((video_format->hs+video_format->hbp)%256));//h_start

	HDMI_WriteI2C_Byte(0xa5,(u8)((video_format->vs+video_format->vbp)%256));//v_start

   	HDMI_WriteI2C_Byte(0xa6,(u8)(video_format->hact/256));
	HDMI_WriteI2C_Byte(0xa7,(u8)(video_format->hact%256)); //hactive

	HDMI_WriteI2C_Byte(0xa8,(u8)(video_format->vact/256));
	HDMI_WriteI2C_Byte(0xa9,(u8)(video_format->vact%256));  //vactive

   	HDMI_WriteI2C_Byte(0xaa,(u8)(video_format->htotal/256));
	HDMI_WriteI2C_Byte(0xab,(u8)(video_format->htotal%256));//htotal

   	HDMI_WriteI2C_Byte(0xac,(u8)(video_format->vtotal/256));
	HDMI_WriteI2C_Byte(0xad,(u8)(video_format->vtotal%256));//vtotal

   	HDMI_WriteI2C_Byte(0xae,(u8)(video_format->hs/256)); 
	HDMI_WriteI2C_Byte(0xaf,(u8)(video_format->hs%256));   //hsa

       HDMI_WriteI2C_Byte(0xb0,(u8)(video_format->vs%256));    //vsa

       //dessc pll to generate pixel clk
	HDMI_WriteI2C_Byte(0xff,0x82); //dessc pll
	HDMI_WriteI2C_Byte(0x2d,0x48); //pll ref select xtal 

	if(pclk_khz < 44000)
	{
	  	HDMI_WriteI2C_Byte(0x35,0x83);
		dessc_pll_post_div = 16;
	}

	else if(pclk_khz < 88000)
	{
	  	HDMI_WriteI2C_Byte(0x35,0x82);
		dessc_pll_post_div = 8;
	}

	else if(pclk_khz < 176000)
	{
	  	HDMI_WriteI2C_Byte(0x35,0x81);
		dessc_pll_post_div = 4;
	}

	else if(pclk_khz < 352000)
	{
	  	HDMI_WriteI2C_Byte(0x35,0x80);
		dessc_pll_post_div = 0;
	}

	pcr_m = (pclk_khz * dessc_pll_post_div) /25;
	pcr_k = pcr_m%1000;
	pcr_m = pcr_m/1000;

	pcr_k <<= 14; 

	//pixel clk
 	HDMI_WriteI2C_Byte(0xff,0xd0); //pcr
	HDMI_WriteI2C_Byte(0x2d,0x7f);
	HDMI_WriteI2C_Byte(0x31,0x00);

	HDMI_WriteI2C_Byte(0x26,0x80|((u8)pcr_m));
	HDMI_WriteI2C_Byte(0x27,(u8)((pcr_k>>16)&0xff)); //K
	HDMI_WriteI2C_Byte(0x28,(u8)((pcr_k>>8)&0xff)); //K
	HDMI_WriteI2C_Byte(0x29,(u8)(pcr_k&0xff)); //K
}
#endif
	
	
static void LT9211_Config(void)
{
	LT9211_SystemInt();
#if !LT9211_OUTPUT_PATTERN
	LT9211_MipiRxPhy();
	LT9211_MipiRxDigital(); 
	LT9211_TimingSet();
 	LT9211_MipiPcr();
#endif
	
	LT9211_TxDigital();
	LT9211_TxPhy();
#if LT9211_OUTPUT_PATTERN
	LT9211_Pattern(&video_1920x1080_60Hz);
#endif
  if (0)//yelsin	if( LT9211_OutPutModde == OUTPUT_RGB888 )
	 {
    	LT9211_InvertRGB_HSVSPoarity();//only for TTL output	 
	 }	
	msleep(10);
	LT9211_Txpll();
	LT9211_RXCSC();
	LT9211_ClockCheckDebug();
	LT9211_VideoCheckDebug();
  	LT9211_BT_Set();
}

static void LT9211_Reset(void)
{
    gpio_set_value(reset_gpio, 0);
    msleep(500);
    gpio_set_value(reset_gpio, 1);
    msleep(100);    
}


static void LT9211_LedPwd(void)
{
	gpiod_direction_output(ledpwd_gpio, 0);
	msleep(100);
	gpiod_direction_output(ledpwd_gpio, 1);
}

static int LT9211_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct device_node *np = client->dev.of_node;
	
	printk("%s:line=%d -- lt9211 \n",__FUNCTION__,__LINE__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		ret = -ENODEV;
		goto err_to_dev_reg;
	}	
	this_client = client;

	// lt9211 mux a pin
	reset_gpio = of_get_named_gpio_flags(np, "reset_gpio", 0, NULL);
	if (!gpio_is_valid(reset_gpio)) {
		dev_err(&client->dev, "MODEM POWER: invalid reset_gpio: %d\n", reset_gpio);
		goto err_to_dev_reg;
	}

	ret = devm_gpio_request(&client->dev, reset_gpio, "lt9211_reset_gpio");
	if (ret < 0) {
		dev_err(&client->dev, "Failed to request gpio %d with ret:%d\n", reset_gpio, ret);
		goto err_to_dev_reg;
	}
	
	//control lvds backlight
	ledpwd_gpio = devm_gpiod_get_optional(&client->dev, "ledpwd", GPIOD_ASIS);
	if (IS_ERR(ledpwd_gpio)) {
		ret = PTR_ERR(ledpwd_gpio);
		if (ret != -EPROBE_DEFER)	
		dev_err(&client->dev, "failed to get reset GPIO: %d\n", ret);
		goto err_to_dev_reg;
	}

	LT9211_Reset();
	if (LT9211_ChipID()<0){
		dev_err(&client->dev, "Failed to read chip 0\n");
		goto err_to_dev_reg;
	}
    LT9211_Config();
	LT9211_LedPwd();
	printk("%s:line=%d -- lt9211 init ok!\n",__FUNCTION__,__LINE__);

	return 0;
	
err_to_dev_reg:
	return ret;
}

static int LT9211_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id LT9211_id[] = {
	{ LT9211_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, LT9211_id);

static struct of_device_id LT9211_dt_ids[] = {
	{ .compatible = "mipi2lvds,lt9211" },
	{},
};

static struct i2c_driver LT9211_driver = {
	.driver 	= {
		.name	= LT9211_I2C_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(LT9211_dt_ids),
	},
	.probe 		= LT9211_probe,
	.remove 	= LT9211_remove,
	.id_table	= LT9211_id,
};

static int __init LT9211_init(void)
{
	pr_info("LT9211 driver: init\n");
	return i2c_add_driver(&LT9211_driver);
}

static void __exit LT9211_exit(void)
{
	pr_info("LT9211 driver: exit\n");
	i2c_del_driver(&LT9211_driver);
}

module_init(LT9211_init);
module_exit(LT9211_exit);
