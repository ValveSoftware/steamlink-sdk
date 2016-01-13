#ifndef __GALOIS_GPIO_HEADER__
#define __GALOIS_GPIO_HEADER__

#define GPIO_IOCTL_READ			0x1111
#define GPIO_IOCTL_WRITE		0x1112
#define GPIO_IOCTL_SET			0x1113
#define GPIO_IOCTL_GET			0x1114
#define GPIO_IOCTL_LOCK			0x1115
#define GPIO_IOCTL_UNLOCK		0x1116
#define GPIO_IOCTL_INIT_IRQ		0x2222
#define GPIO_IOCTL_EXIT_IRQ		0x2223
#define GPIO_IOCTL_ENABLE_IRQ	0x2224
#define GPIO_IOCTL_DISABLE_IRQ	0x2225
#define GPIO_IOCTL_CLEAR_IRQ	0x2226
#define GPIO_IOCTL_READ_IRQ		0x2227

#define SM_GPIO_IOCTL_READ			0x3333
#define SM_GPIO_IOCTL_WRITE			0x3334
#define SM_GPIO_IOCTL_SET			0x3335
#define SM_GPIO_IOCTL_GET			0x3336
#define SM_GPIO_IOCTL_LOCK			0x3337
#define SM_GPIO_IOCTL_UNLOCK			0x3338
#define SM_GPIO_IOCTL_INIT_IRQ		0x4444
#define SM_GPIO_IOCTL_EXIT_IRQ		0x4445
#define SM_GPIO_IOCTL_ENABLE_IRQ	0x4446
#define SM_GPIO_IOCTL_DISABLE_IRQ	0x4447
#define SM_GPIO_IOCTL_CLEAR_IRQ		0x4448
#define SM_GPIO_IOCTL_READ_IRQ		0x4449

#define GPIO_MODE_DATA_IN		1
#define GPIO_MODE_DATA_OUT		2
#define GPIO_MODE_IRQ_LOWLEVEL	3
#define GPIO_MODE_IRQ_HIGHLEVEL	4
#define GPIO_MODE_IRQ_RISEEDGE	5
#define GPIO_MODE_IRQ_FALLEDGE	6

typedef struct galois_gpio_data {
	int port;	/* port number: for SoC, 0~31; for SM, 0~11 */
	int mode;	/* port mode: data(in/out) or irq(level/edge) */
	uint data;	/* port data: 1 or 0, only valid when mode is data(in/out) */
} galois_gpio_data_t;

#define SM_GPIO_PORT_NUM	16
#define GPIO_PORT_NUM		32
#define IOMAPPER_REG_NUM	12
typedef struct galois_gpio_reg {
	unsigned int port_dr, port_ddr, port_ctl;
	int gpio_mode[GPIO_PORT_NUM];
} galois_gpio_reg_t;

#endif
