#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <linux/i2c-dev.h>
#include "LvRestfulClient.h"
#include "LvJSON.h"

#define I2C_DEV "/dev/i2c-0" // The device file for I2C bus
#define MCP23017_ADDR1 0x20  // I2C address of MCP23017
#define MCP23017_ADDR2 0x21  
#define MCP23017_ADDR3 0x22  
#define IODIRA_REG 0x00     // I/O direction register for Port A
#define GPIOA_REG  0x12     // GPIO register for Port A
#define IODIRB_REG 0x01     // I/O direction register for Port B
#define GPIOB_REG  0x13     // GPIO register for Port B

//OUT1  GPB0
//OUT2  GPB1
//OUT3  GPB2
//OUT4  GPB3

struct i2c_msg {
    unsigned short addr;
    unsigned short flags;
    #define I2C_M_RD 0x0001
    unsigned short len;
    unsigned char *buf;
};

void setPortDirection(int fd, unsigned char port, unsigned char direction, unsigned char address) {
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[2];

    if (port == 'A') buf[0] = IODIRA_REG;
    else if (port == 'B') buf[0] = IODIRB_REG;
    else exit(1);

    buf[1] = direction;

    msg.addr = address;
    msg.flags = 0;
    msg.len = 2;
    msg.buf = buf;

    data.msgs = &msg;
    data.nmsgs = 1;

    if (ioctl(fd, I2C_RDWR, &data) < 0) {
        perror("ioctl error");
        exit(1);
    }

    std::cout << "Set Port " << port << " direction to " << std::hex << static_cast<int>(direction) 
              << " on MCP23017 with address 0x" << std::hex << static_cast<int>(address) << std::endl;
}

void writePort(int fd, unsigned char port, unsigned char value, unsigned char address) {
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[2];

    if (port == 'A') buf[0] = GPIOA_REG;
    else if (port == 'B') buf[0] = GPIOB_REG;
    else exit(1);

    buf[1] = value;

    msg.addr = address;
    msg.flags = 0;
    msg.len = 2;
    msg.buf = buf;

    data.msgs = &msg;
    data.nmsgs = 1;

    if (ioctl(fd, I2C_RDWR, &data) < 0) {
        perror("ioctl error");
        exit(1);
    }

    std::cout << "Wrote value " << std::hex << static_cast<int>(value) << " to Port " << port 
              << " on MCP23017 with address 0x" << std::hex << static_cast<int>(address) << std::endl;
}

unsigned char readPort(int fd, unsigned char port, unsigned char address) {
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[1];
    unsigned char read_buf[1];

    if (port == 'A') buf[0] = GPIOA_REG;
    else if (port == 'B') buf[0] = GPIOB_REG;
    else exit(1);

    msgs[0].addr = address;
    msgs[0].flags = 0; //write
    msgs[0].len = 1;
    msgs[0].buf = buf; //which register to read

    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD; //read
    msgs[1].len = 1;
    msgs[1].buf = read_buf; //contains the value of the register

    data.msgs = msgs;
    data.nmsgs = 2;

    if (ioctl(fd, I2C_RDWR, &data) < 0) {
        perror("ioctl error");
        exit(1);
    }

    std::cout << "Read value 0x" << std::hex << static_cast<int>(read_buf[0]) << " from Port " << port 
              << " on MCP23017 with address 0x" << std::hex << static_cast<int>(address) << std::endl;

    return read_buf[0];
}

bool fetchVehicleCount(LvJSON &json) {
    LvRestfulClient client;
    unsigned long request_id;
    
    // Start an HTTP GET request
    if (client.put("http://192.168.1.103/api/v1/count", LvRestfulClient::METHOD_GET, "", 1000, &request_id) != 0) {
        std::cerr << "Failed to start HTTP request." << std::endl;
        return false;
    }

    // Polling for response
    while (true) {
        client.loop(mg_millis());

        std::string url;
        LvRestfulClient::METHOD method;
        std::string body;
        int status_code;

        if (client.get(url, method, body, status_code, &request_id) == 0) {
            // Parse the JSON response
            json.Parse(body.c_str());
            if (json.HasParseError()) {
                std::cerr << "Failed to parse JSON response." << std::endl;
                return false;
            }
            std::cout << "Fetched vehicle count successfully." << std::endl;
            return true;
        }
    }
}

int main() {
    int fd;

    fd = open(I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("open error");
        exit(1);
    }

    // Set Port A as input and Port B as output, 0 is output, 1 is input
    setPortDirection(fd, 'A', 0xFF, MCP23017_ADDR1);  // Set all 8 pins on Port A as input, 0xFF = 0b11111111
    setPortDirection(fd, 'B', 0x00, MCP23017_ADDR1);  // Set all 8 pins on Port B as output, 0x00 = 0b00000000
    setPortDirection(fd, 'A', 0xFF, MCP23017_ADDR2);  // Set all 8 pins on Port A as input, 0xFF = 0b11111111
    setPortDirection(fd, 'B', 0x00, MCP23017_ADDR2);  // Set all 8 pins on Port B as output, 0x00 = 0b00000000

    while (1) {
        // Fetch vehicle count
        LvJSON json;
        if (fetchVehicleCount(json)) {
            int frame_count = json["data"][0]["frame_count"][0].GetInt();
            std::cout << "Vehicle count: " << frame_count << std::endl;

            // Light up LEDs based on frame count
            if (frame_count > 0) {
                writePort(fd, 'B', 0x0F, MCP23017_ADDR1);  // Turn on LEDs on MCP23017_ADDR1
                writePort(fd, 'B', 0x0F, MCP23017_ADDR2);  // Turn on LEDs on MCP23017_ADDR2
                std::cout << "LEDs turned on due to vehicle presence." << std::endl;
            } else {
                writePort(fd, 'B', 0x00, MCP23017_ADDR1);  // Turn off LEDs on MCP23017_ADDR1
                writePort(fd, 'B', 0x00, MCP23017_ADDR2);  // Turn off LEDs on MCP23017_ADDR2
                std::cout << "LEDs turned off due to no vehicle presence." << std::endl;
            }
        } else {
            std::cerr << "Failed to fetch vehicle count." << std::endl;
        }

        // Read the input state of port A
        unsigned char portAState1 = readPort(fd, 'A', MCP23017_ADDR1);
        unsigned char portAState2 = readPort(fd, 'A', MCP23017_ADDR2);

        // Display the input state of each pin in port A
        std::cout << "Input state of MCP23017_ADDR1 Port A: 0x" << std::hex << static_cast<int>(portAState1) << std::endl;
        std::cout << "Input state of MCP23017_ADDR2 Port A: 0x" << std::hex << static_cast<int>(portAState2) << std::endl;

        sleep(1);  // Polling interval
    }

    close(fd);
    return 0;
}
