#ifndef GSENSOR_H__
#define GSENSOR_H__


#define FREAD_MASK				0 /* enabled(1<<1) only if reading MSB 8bits*/
#define MMA845X_RANGE		(16384 * 2)

/* mma8451 */
#define MMA8451_PRECISION       14
#define MMA8451_BOUNDARY        (0x1 << (MMA8451_PRECISION - 1))
#define MMA8451_GRAVITY_STEP    (MMA845X_RANGE / MMA8451_BOUNDARY)

/* mma8452 */
#define MMA8452_PRECISION       12
#define MMA8452_BOUNDARY        (0x1 << (MMA8452_PRECISION - 1))
#define MMA8452_GRAVITY_STEP    (MMA845X_RANGE / MMA8452_BOUNDARY)

/* mma8453 */
#define MMA8453_PRECISION       10
#define MMA8453_BOUNDARY        (0x1 << (MMA8453_PRECISION - 1))
#define MMA8453_GRAVITY_STEP    (MMA845X_RANGE / MMA8453_BOUNDARY)

/* mma8653 */
#define MMA8653_PRECISION       10
#define MMA8653_BOUNDARY        (0x1 << (MMA8653_PRECISION - 1))
#define MMA8653_GRAVITY_STEP    (MMA845X_RANGE / MMA8653_BOUNDARY)


#define MMA8451_DEVID		0x1a
#define MMA8452_DEVID		0x2a
#define MMA8453_DEVID		0x3a
#define MMA8653_DEVID		0x5a


#define MMA8451_ENABLE		1

int sensor_report_value(struct input_dev * input_dev, char* buffer);


#endif