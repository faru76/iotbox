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
#include <thread>
#include <chrono>

struct gpiohandle_request handle_req;
struct gpiohandle_data handle_data;

// Define the GPIO pins to blink
// const char* gpiochips[] = { "/dev/gpiochip2", "/dev/gpiochip6", "/dev/gpiochip8" };
// const int line_offsets[] = { 13, 8, 2, 7 };  // Assuming these are the line offsets for PC13, PG8, PI2, and PI7

int Setup_GPIO(int chip_fd, int line_offset) {
    // Set GPIO line as Output
    handle_req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(handle_req.consumer_label, "out");
    memset(handle_req.default_values, 0, sizeof(handle_req.default_values));
    handle_req.lines = 1;
    handle_req.lineoffsets[0] = line_offset;
    handle_req.default_values[0] = 1;

    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &handle_req) < 0) {
        perror("Failed to set Output");
        return -1;
    }

    return 0;
}

void Test_GPIO(int line_offset) {
    // Set the GPIO line to high
    handle_data.values[0] = 1;

    if (ioctl(handle_req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &handle_data) < 0)
        perror("Error setting GPIO line to high");
    else
        printf("GPIO line %d set to high\n", line_offset);

    // Delay for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Set the GPIO line to low
    handle_data.values[0] = 0;

    if (ioctl(handle_req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &handle_data) < 0)
        perror("Error setting GPIO line to low");
    else
        printf("GPIO line %d set to low\n", line_offset);

    // Delay for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    printf("\n"); // Add a space after each pin test
}

int main() {
    const char* gpiochips[] = {
        "/dev/gpiochip2", "/dev/gpiochip6", "/dev/gpiochip8", "/dev/gpiochip8"
    };

    const int line_offsets[] = { 13, 8, 2, 7 };

    while (1) {
        int chip_fd;
        for (int i = 0; i < sizeof(gpiochips) / sizeof(gpiochips[0]); ++i) {
            chip_fd = open(gpiochips[i], O_RDONLY); 
            if (chip_fd < 0) {
                perror("Failed to open gpiochip");
                continue;
            }

            printf("Testing line %d on chip %s\n", line_offsets[i], gpiochips[i]);
            if (Setup_GPIO(chip_fd, line_offsets[i]) == 0) {
                Test_GPIO(line_offsets[i]);
                close(handle_req.fd); // Close the handle after each test
            }

            close(chip_fd); // Close the chip file descriptor
            sleep(2);
        }
    }

    return 0;
}