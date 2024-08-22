#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/input.h>

#include "gsensor.h"



static int sensor_convert_data(char high_byte, char low_byte)
{
    s64 result;
	int devid = MMA8451_DEVID;
	//int precision = sensor->ops->precision;
	switch (devid) {
		case MMA8451_DEVID:	
			//swap(high_byte,low_byte);
			result = ((int)high_byte << (MMA8451_PRECISION-8)) 
					| ((int)low_byte >> (16-MMA8451_PRECISION));
			if (result < MMA8451_BOUNDARY)
				result = result * MMA8451_GRAVITY_STEP;
			else
				result = ~(((~result & (0x7fff >> (16 - MMA8451_PRECISION))) + 1)
					* MMA8451_GRAVITY_STEP) + 1;
			break;

		case MMA8452_DEVID:			
			swap(high_byte,low_byte);
			result = ((int)high_byte << (MMA8452_PRECISION-8)) 
					| ((int)low_byte >> (16-MMA8452_PRECISION));
			if (result < MMA8452_BOUNDARY)
				result = result * MMA8452_GRAVITY_STEP;
			else
				result = ~(((~result & (0x7fff >> (16 - MMA8452_PRECISION))) + 1)
					* MMA8452_GRAVITY_STEP) + 1;
			break;
			
		case MMA8453_DEVID:
			swap(high_byte,low_byte);
			result = ((int)high_byte << (MMA8453_PRECISION-8)) 
					| ((int)low_byte >> (16-MMA8453_PRECISION));
			if (result < MMA8453_BOUNDARY)
				result = result * MMA8453_GRAVITY_STEP;
			else
				result = ~(((~result & (0x7fff >> (16 - MMA8453_PRECISION))) + 1)
					* MMA8453_GRAVITY_STEP) + 1;
			break;

		case MMA8653_DEVID:
			swap(high_byte,low_byte);
			result = ((int)high_byte << (MMA8653_PRECISION-8)) 
					| ((int)low_byte >> (16-MMA8653_PRECISION));
			if (result < MMA8653_BOUNDARY)
				result = result * MMA8653_GRAVITY_STEP;
			else
				result = ~(((~result & (0x7fff >> (16 - MMA8653_PRECISION))) + 1)
					* MMA8653_GRAVITY_STEP) + 1;
			break;

		default:
			printk(KERN_ERR "%s: devid wasn't set correctly\n",__func__);
			return -EFAULT;
    }

    return (int)result;
}

static int gsensor_report_value(struct input_dev * input_dev,  int x, int y, int z)
{
	
		/* Report acceleration sensor information */
		input_report_abs(input_dev, ABS_X, x);
		input_report_abs(input_dev, ABS_Y, y);
		input_report_abs(input_dev, ABS_Z, z);
		input_sync(input_dev);
	
	return 0;
}

int sensor_report_value(struct input_dev * input_dev, char* buffer)
{
	int x,y,z;
	int xs,ys,zs;
	int orientation[9] = {1,0, 0, 0, 1, 0, 0, 0, 1};


	//this gsensor need 6 bytes buffer
	x = sensor_convert_data(buffer[0], 0);	//buffer[1]:high bit 
	y = sensor_convert_data(buffer[1], 0);
	z = sensor_convert_data(buffer[2], 0);		

	xs = (orientation[0])*x + (orientation[1])*y + (orientation[2])*z;
	ys = (orientation[3])*x + (orientation[4])*y + (orientation[5])*z; 
	zs = (orientation[6])*x + (orientation[7])*y + (orientation[8])*z;

	gsensor_report_value(input_dev, xs, ys, zs);
	
	pr_debug("--gsensor: %d, %d, %d\n", xs, ys, zs);

	
	return 1;
}
