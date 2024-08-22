/*
 * GPIO based serio bus driver for bit banging the PS/2 protocol
 *
 * Author: Danilo Krummrich <danilokrummrich@dk-develop.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/serio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/property.h>
#include <linux/of.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/irqdomain.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/of_irq.h>
#include <linux/sched/clock.h>

#include "gsensor.h"



#define DRIVER_NAME		"ps2-gpio"

#define PS2_MODE_RX		0
#define PS2_MODE_TX		1

#define PS2_START_BIT		0
#define PS2_DATA_BIT0		1
#define PS2_DATA_BIT1		2
#define PS2_DATA_BIT2		3
#define PS2_DATA_BIT3		4
#define PS2_DATA_BIT4		5
#define PS2_DATA_BIT5		6
#define PS2_DATA_BIT6		7
#define PS2_DATA_BIT7		8
#define PS2_PARITY_BIT		9
#define PS2_STOP_BIT		10
#define PS2_TX_TIMEOUT		11
#define PS2_ACK_BIT		12

#define PS2_DEV_RET_ACK		0xfa
#define PS2_DEV_RET_NACK	0xfe

#define PS2_CMD_RESEND		0xfe

#define  FRAME_SIZE 4
#define PULL_ROD_MAX_VALUE 65

//两帧之间 最小的 间隔
#define  MIN_INTERVAL_OF_FRAME ((2000) * 1000) //unit: ns

//#define  MAX_INTERVAL_OF_FRAME (MIN_INTERVAL_OF_FRAME + 5000 * 1000) //unit: ns

// 帧内 两字节 之间 最长间隔
#define  MAX_INTR_INTERVAL_IN_BYTE (620 * 1000)  //unit: ns


struct ps2_gpio_data {
	struct device *dev;
//	struct serio *serio;
	unsigned char mode;
	struct gpio_desc *gpio_clk;
	struct gpio_desc *gpio_data;
	int irq;
	unsigned char rx_cnt;
	unsigned char rx_byte;
	unsigned char tx_byte;

	struct mutex tx_mutex;
	//struct delayed_work tx_work;
	struct work_struct byte_work;
	
	int prob_flag ; //0 : in process 1, complete
	
	unsigned int find_frame_head ; // 0, no find， 1，find 
	unsigned int recv_bytes ;
	u8 frame_data[FRAME_SIZE];
	
	struct input_dev *input_dev;
	
	void * gipo2_base;
};


static void byte_work_fn(struct work_struct *work)
{
	
	u8 data[FRAME_SIZE+1];
	struct ps2_gpio_data *drvdata = container_of(work,
						    struct ps2_gpio_data,
						    byte_work);
				
	memcpy(data,  drvdata->frame_data, FRAME_SIZE);  //这里可能需要临界区
	//printk(KERN_DEBUG "ps2: %02x (%u %u %u) \n", data[0], data[1], data[2], data[3]);
	
	if(drvdata->input_dev) //data  check
	{
		u8 value = data[0];
		pr_debug("++distan: %d\n", value);
		
		//report distance
		input_report_abs(drvdata->input_dev, ABS_DISTANCE, value);
		input_sync(drvdata->input_dev);
		
		//report gsensor
		sensor_report_value(drvdata->input_dev, &data[1]);
	}		
	

}


static int test_;
static u64  old_jiffies = 0;

static irqreturn_t ps2_gpio_irq_rx(int irq, void *dev_id)
{
	unsigned char byte, cnt;
	int data;
	struct ps2_gpio_data *drvdata = dev_id;
	u64 from_last, cur = local_clock();
	
	if(drvdata->prob_flag == 0) //wait prob complete
	{
		return IRQ_HANDLED;
	}
	
	from_last = (cur - old_jiffies);
	old_jiffies = cur;

	if(drvdata->find_frame_head == 0 && from_last < MIN_INTERVAL_OF_FRAME)
	{
		return IRQ_HANDLED;
	}

	if(drvdata->find_frame_head == 0 && from_last > MIN_INTERVAL_OF_FRAME)
	{
		//printk(KERN_DEBUG "interval: %llu\n", from_last/1000/1000);
		drvdata->find_frame_head = 1;
		drvdata->recv_bytes = 0;
		
		drvdata->rx_cnt = 0;
		
		from_last = 0; //avoid that first  check error below
	}

	byte = drvdata->rx_byte;
	cnt = drvdata->rx_cnt;

	if (drvdata->find_frame_head && from_last > MAX_INTR_INTERVAL_IN_BYTE) { // in fact , <= 100us in byte.  < 180us between bytes.
		pr_debug( 
			"RX: timeout, probably we missed an interrupt(%llu)\n", from_last);
		goto err;
	}


	data = gpiod_get_value(drvdata->gpio_data);
	if (unlikely(data < 0)) {
		pr_debug("RX: failed to get data gpio val: %d\n",
			data);
		goto err;
	}

	switch (cnt) {
	case PS2_START_BIT:
		/* start bit should be low */
		if (unlikely(data)) {
			pr_debug("RX: start bit should be low\n");
			goto err;
		}
		break;
	case PS2_DATA_BIT0:
	case PS2_DATA_BIT1:
	case PS2_DATA_BIT2:
	case PS2_DATA_BIT3:
	case PS2_DATA_BIT4:
	case PS2_DATA_BIT5:
	case PS2_DATA_BIT6:
	case PS2_DATA_BIT7:
		/* processing data bits */
		if (data)
			byte |= (data << (cnt - 1));
		break;
	case PS2_PARITY_BIT:
		/* check odd parity */
		if (!((hweight8(byte) & 1) ^ data)) {
			//rxflags |= SERIO_PARITY;
			pr_debug( "RX: parity error(%x)\n", byte);
			goto err;
		}


	//printk("r %x\n", byte);
		
		//dev_dbg(drvdata->dev, "RX: sending byte 0x%x\n", byte);
		test_ = byte;
		break;
	case PS2_STOP_BIT:
		/* stop bit should be high */
		if (unlikely(!data)) {
			pr_debug( "RX: stop bit should be high\n");
			goto err;
		}

		drvdata->frame_data[drvdata->recv_bytes] = byte;
		drvdata->recv_bytes++;
		if(drvdata->recv_bytes == FRAME_SIZE){
			schedule_work(&drvdata->byte_work);
			drvdata->find_frame_head = 0;
		}
		cnt = byte = 0;
		
//printk("%s: read ok(%x)%lu\n", __func__, test_, in_irq());
		goto end; /* success */
	default:
		dev_err(drvdata->dev, "RX: got out of sync with the device\n");
		goto err;
	}

	cnt++;
	goto end; /* success */

err:
	cnt = 0;
	byte = 0;
	drvdata->find_frame_head = 0; //find next frame
	drvdata->recv_bytes = 0;
	
end:
	drvdata->rx_cnt = cnt;
	drvdata->rx_byte = byte;
	return IRQ_HANDLED;
}




static int ps2_gpio_get_props(struct device *dev,
				 struct ps2_gpio_data *drvdata)
{
	drvdata->gpio_data = devm_gpiod_get(dev, "data", GPIOD_IN);
	if (IS_ERR(drvdata->gpio_data)) {
		dev_err(dev, "failed to request data gpio: %ld",
			PTR_ERR(drvdata->gpio_data));
		return PTR_ERR(drvdata->gpio_data);
	}

	drvdata->gpio_clk = devm_gpiod_get(dev, "clk", GPIOD_IN);
	if (IS_ERR(drvdata->gpio_clk)) {
		dev_err(dev, "failed to request clock gpio: %ld",
			PTR_ERR(drvdata->gpio_clk));
		return PTR_ERR(drvdata->gpio_clk);
	}

	// close data output
	gpiod_direction_output(drvdata->gpio_data, 0);

	return 0;
}

#define ENABLE_BIND_INTERRUPT 

static int ps2_gpio_probe(struct platform_device *pdev)
{
	struct ps2_gpio_data *drvdata;
	struct device *dev = &pdev->dev;
	int error;
	int result;
	
#ifdef ENABLE_BIND_INTERRUPT	
	struct cpumask cpumask;
	int tempirq;	
	tempirq = platform_get_irq(pdev, 0);
	//bind irq to cpu 2
	cpumask_clear(&cpumask);
	cpumask_set_cpu(2, &cpumask); 
	result = irq_set_affinity(tempirq, &cpumask);
	pr_debug("irq_set_affinity = %d\n", result);
	pr_debug("%s: %d irq = %d\n", __func__, __LINE__, tempirq);
#endif	


	drvdata = devm_kzalloc(dev, sizeof(struct ps2_gpio_data), GFP_KERNEL);
	//serio = kzalloc(sizeof(struct serio), GFP_KERNEL);
	if (!drvdata ) {
		error = -ENOMEM;
		goto err_free_serio;
	}


	error = ps2_gpio_get_props(dev, drvdata);
	if (error)
		goto err_free_serio;


	if (gpiod_cansleep(drvdata->gpio_data) ||
	    gpiod_cansleep(drvdata->gpio_clk)) {
		dev_err(dev, "GPIO data or clk are connected via slow bus\n");
		error = -EINVAL;
	}


	drvdata->irq=gpio_to_irq(desc_to_gpio(drvdata->gpio_clk));
	//drvdata->irq=irq_of_parse_and_map(dev->of_node, 0);
	if (drvdata->irq < 0) {
		dev_err(dev, "failed to get irq from platform resource: %d\n",
			drvdata->irq);
		error = drvdata->irq;
		goto err_free_serio;
	}

//printk("%s: %d irq = %d\n", __func__, __LINE__, drvdata->irq);

	old_jiffies = local_clock();
	
	error = devm_request_irq(dev, drvdata->irq, ps2_gpio_irq_rx,
				 IRQF_NO_THREAD | IRQF_TRIGGER_FALLING, DRIVER_NAME, drvdata);
	if (error) {
		dev_err(dev, "failed to request irq %d: %d\n",
			drvdata->irq, error);
		goto err_free_serio;
	}


	drvdata->gipo2_base = ioremap(0xff780000, 0x80000);
	pr_debug("gpio_base = %p\n", drvdata->gipo2_base);
	


	/* Keep irq disabled until serio->open is called. */
	//disable_irq(drvdata->irq);
	


	/* Write can be enabled in platform/dt data, but possibly it will not
	 * work because of the tough timings.
	 */

	drvdata->dev = dev;
	
	//bind to input device
	drvdata->input_dev = devm_input_allocate_device(dev);
	if (!drvdata->input_dev) {
		result = -ENOMEM;
		dev_err(dev,
			"Failed to allocate input device\n");
		goto err_free_serio;
	}
	drvdata->input_dev->name = "pullrod_gsensor";
	set_bit(EV_ABS, drvdata->input_dev->evbit);
	input_set_abs_params(drvdata->input_dev, ABS_DISTANCE, 0, PULL_ROD_MAX_VALUE, 0, 0);

#if 1
	/* x-axis acceleration */
	input_set_abs_params(drvdata->input_dev, ABS_X, -MMA845X_RANGE, MMA845X_RANGE, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(drvdata->input_dev, ABS_Y, -MMA845X_RANGE, MMA845X_RANGE, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(drvdata->input_dev, ABS_Z, -MMA845X_RANGE, MMA845X_RANGE, 0, 0);		
#endif	
	
	drvdata->input_dev->dev.parent = dev;
	result = input_register_device(drvdata->input_dev);
	if (result) {
		dev_err(dev,
			"Unable to register input device %s\n", drvdata->input_dev->name);
		goto err_free_serio;
	}

	

	/* Tx count always starts at 1, as the start bit is sent implicitly by
	 * host-to-device communication initialization.
	 */

	INIT_WORK(&drvdata->byte_work, byte_work_fn);
	mutex_init(&drvdata->tx_mutex);

	platform_set_drvdata(pdev, drvdata);
	
	// open data output
	
	gpiod_direction_input(drvdata->gpio_data);
	drvdata->prob_flag = 1;
	//enable_irq(drvdata->irq);


printk("%s: %d exit.\n", __func__, __LINE__);

	return 0;	/* success */

err_free_serio:
	return error;
}

static int ps2_gpio_remove(struct platform_device *pdev)
{

	struct ps2_gpio_data *drvdata = platform_get_drvdata(pdev);
	
	disable_irq(drvdata->irq);
	
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id ps2_gpio_match[] = {
	{ .compatible = "ps2-gpio", },
	{ },
};
MODULE_DEVICE_TABLE(of, ps2_gpio_match);
#endif

static struct platform_driver ps2_gpio_driver = {
	.probe		= ps2_gpio_probe,
	.remove		= ps2_gpio_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(ps2_gpio_match),
	},
};
module_platform_driver(ps2_gpio_driver);

MODULE_AUTHOR("Danilo Krummrich <danilokrummrich@dk-develop.de>");
MODULE_DESCRIPTION("GPIO PS2 driver");
MODULE_LICENSE("GPL v2");