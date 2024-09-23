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

// GPIO_1   PE0 /
// GPIO_2   PA4     Need to solder resistor on MYIR
// GPIO_3   PA5     Need to solder resistor on MYIR
// GPIO_4   PE3
// GPIO_5   PE1 /
// GPIO_6   PE2
// GPIO_7   PE6 /
// GPIO_8   PE7

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
// GPIOs: PE0, PE1, PE2, PE3, PE6, PE7 (gpiochip9)
//        PA4, PA5 (gpiochip0)

struct gpiohandle_request PE_out;

//     struct gpiohandle_request {
//     __u32 lineoffsets[GPIOHANDLES_MAX];
//     __u32 flags;
//     __u8 default_values[GPIOHANDLES_MAX];
//     char consumer_label[GPIO_MAX_NAME_SIZE];
//     __u32 lines;
//     int fd;
// };

struct gpiohandle_data PE_set_data;

//     struct gpiohandle_data {
//     __u8 values[GPIOHANDLES_MAX];
// };


int Setup_GPIO()
{
	int PE_fd;
	// Set GPIO_E as Output

    PE_fd = open("/dev/gpiochip2", 0);
    if (PE_fd < 0) {
        perror("Failed to open gpiochip4\n");
        return -1;
    }

    printf("%d\n", PE_fd);

    PE_out.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(PE_out.consumer_label, "out");
    //memcpy(PE_out.default_values, &data, sizeof(PE_out.default_values))
    memset(PE_out.default_values, 0, sizeof(PE_out.default_values));
    PE_out.lines  = 1;
    PE_out.lineoffsets[0] = 13;
    PE_out.default_values[0] = 1;


    if(ioctl(PE_fd, GPIO_GET_LINEHANDLE_IOCTL, &PE_out) < 0){
        perror("Failed to set Output PE\n");
        close(PE_fd);
        return -1;
    }

    return 1;
}

void loop()
{
    PE_set_data.values[0] = 1; //set PE9 to high

    if(ioctl(PE_out.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &PE_set_data) < 0)
        perror("Error Getting GPIO_Z");

    printf("PE9 is set to high\n");
}


int main() {

	int s = Setup_GPIO();
    printf("%d\n", s);
    if(s<0){
        return -1;
    }

    while (1) {
    	loop();
        sleep(1);
    }

    return 0;
} 