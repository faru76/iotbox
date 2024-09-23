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

// Available GPIOs
// PA4, PF13, PF14, PH7, PH6, PA5, PF11, PF12 (kernel used)

// gpiochip2 [GPIOC]
// GPIOs: PC13 (gpiochip2)

struct gpio_v2_line_request req;
//     __u32 offsets[GPIO_V2_LINES_MAX];
//     char consumer[GPIO_MAX_NAME_SIZE];
//     struct gpio_v2_line_config config;
//     __u32 num_lines;
//     __u32 event_buffer_size;
//     __u32 padding[5];
//     __s32 fd;

struct gpio_v2_line_values values;
//     __aligned_u64 bits[GPIO_V2_LINES_MAX];
//     __aligned_u64 mask;

int Setup_GPIO()
{
    int PC_fd;

    // Open GPIO chip
    PC_fd = open("/dev/gpiochip2", O_RDWR);
    if (PC_fd < 0) {
        perror("Failed to open gpiochip2");
        return -1;
    }

    // Initialize the line request
    memset(&req, 0, sizeof(req));
    req.offsets[0] = 13; // PC13
    strcpy(req.consumer, "out"); // Consumer name
    req.num_lines = 1;
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;

    // Request the line
    if (ioctl(PC_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        perror("Error obtaining GPIO_C");
        close(PC_fd);
        return -1;
    }

    return req.fd;
}

void loop()
{
    // Initialize the values structure
    memset(&values, 0, sizeof(values));
    values.mask = 1ULL << 0; // Bit 0 in this request corresponds to PC13
    values.bits = 1; // Set PC13 to high

    // Set the line value
    if (ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        perror("Error setting value GPIO_C");
    } else {
        printf("PC13 is set to high\n");
    }
}

int main()
{
    int PC_fd = Setup_GPIO();
    if (PC_fd < 0) {
        return -1;
    }

    while (1) {
        loop();
        sleep(1);
    }

    close(PC_fd); // Close the GPIO chip file descriptor
    return 0;
}
