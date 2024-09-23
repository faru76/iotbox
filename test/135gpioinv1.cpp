#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <linux/gpio.h>
#include <iostream>

// type gpiodetect & gpioinfo to check
// gpiochip0 [GPIOA]
// gpiochip1 [GPIOB]
// gpiochip2 [GPIOC]
// gpiochip3 [GPIOD]
// gpiochip4 [GPIOE]
// gpiochip5 [GPIOF]
// gpiochip6 [GPIOG]
// gpiochip7 [GPIOH]
// gpiochip8 [GPIOI]
// gpiochip9 [GPIOZ]

// GPIO_1   PZ0 /
// GPIO_2   PA4     Need to solder resistor on MYIR
// GPIO_3   PA5     Need to solder resistor on MYIR
// GPIO_4   PZ3
// GPIO_5   PZ1 /
// GPIO_6   PZ2
// GPIO_7   PZ6 /
// GPIO_8   PZ7

// Available GPIO
// PA4
// PF13
// PF14
// PH7
// PH6
// PA5
// PF11
// PF12 (kernel used)

// gpiochip9 [GPIOZ]
// gpiochip0 [GPIOA]
// GPIOs: PZ0, PZ1, PZ2, PZ3, PZ6, PZ7 (gpiochip9)
//        PA4, PA5 (gpiochip0)

struct gpiohandle_request PZ_dcin;

//     struct gpiohandle_request {
//     __u32 lineoffsets[GPIOHANDLES_MAX];
//     __u32 flags;
//     __u8 default_values[GPIOHANDLES_MAX];
//     char consumer_label[GPIO_MAX_NAME_SIZE];
//     __u32 lines;
//     int fd;
// };

struct gpiohandle_data PZ_get_data;

//     struct gpiohandle_data {
//     __u8 values[GPIOHANDLES_MAX];
// };


int Setup_GPIO()
{
	int PZ_fd;
	// Set GPIO_Z as input

    PZ_fd = open("/dev/gpiochip9", 0);
    if (PZ_fd < 0) {
        perror("Failed to open gpiochip9\n");
        return -1;
    }

    printf("%d\n", PZ_fd);

    PZ_dcin.flags = GPIOHANDLE_REQUEST_INPUT;
    strcpy(PZ_dcin.consumer_label, "button_gpio");
    memset(PZ_dcin.default_values, 0, sizeof(PZ_dcin.default_values));
    PZ_dcin.lines  = 3;
    PZ_dcin.lineoffsets[0] = 0;
    PZ_dcin.lineoffsets[1] = 1;
    PZ_dcin.lineoffsets[2] = 6;


    if(ioctl(PZ_fd, GPIO_GET_LINEHANDLE_IOCTL, &PZ_dcin) < 0)
    {
        perror("Failed to set Input PZ\n");
        close(PZ_fd);
        return -1;
    }

    return 1;
}

void loop()
{
    if(ioctl(PZ_dcin.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &PZ_get_data) < 0)
        perror("Error Getting GPIO_Z");

	for (int i = 0; i < 3; ++i)
    {
        printf("PZ %d\n", PZ_get_data.values[i]);
    }

    printf("\n");
}


int main() {

	int s = Setup_GPIO();
    printf("%d", s);
    while (1) {
        system("clear");
    	loop();
    }

    return 0;
}