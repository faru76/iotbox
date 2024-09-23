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

#define I2C_DEV "/dev/i2c-0"// The device file for I2C bus
#define MCP23017_ADDR1 0x20  // I2C address of MCP23017
#define MCP23017_ADDR2 0x21  
#define MCP23017_ADDR3 0x22  
#define IODIRA_REG 0x00     // I/O direction register for Port A
#define GPIOA_REG  0x12     // GPIO register for Port A
#define IODIRB_REG 0x01     // I/O direction register for Port B
#define GPIOB_REG  0x13     // GPIO register for Port B
#define MCP23017_I2C_RDWR 0x0707

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

// struct i2c_rdwr_ioctl_data {
//     struct i2c_msg *msgs;
//     int nmsgs;
// };

//setportdirection that can select the address of the MCP23017
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
}
//example call of setPortDirection
//setPortDirection(fd, 'A', 0xFF, MCP23017_ADDR1);

//writeport that can select the address of the MCP23017
void writePort(int fd, unsigned char port, unsigned char value, unsigned char address) {
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[2];

    if (port == 'A') buf[0] = GPIOA_REG;
    else if (port == 'B') buf[0] = GPIOB_REG;
    else exit(1);

    std::cout << "Writing to port: " << port << ", Value: " << static_cast<int>(value) << ", Address: " << std::hex << static_cast<int>(address) << std::endl;	
	std::cout << "Buffer0: " << static_cast<int>(buf[0]) << ", Buffer1: " << static_cast<int>(buf[1]) << std::endl;

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
}
//example call of writePort
//writePort(fd, 'B', 0x0F, MCP23017_ADDR1);

//readport that can select the address of the MCP23017
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

    return read_buf[0];
}
//example call of readPort
//unsigned char portAState = readPort(fd, 'A', MCP23017_ADDR1);

int main() {
    int fd;

    fd = open(I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("open error");
        exit(1);
    }

    // // Set the I2C slave address for all subsequent I2C device transfers
    // if (ioctl(fd, I2C_SLAVE, MCP23017_ADDR1) < 0) {
    //     perror("ioctl error");
    //     exit(1);
    // }

    // Set Port A as input and Port B as output, 0 is output, 1 is input
    setPortDirection(fd, 'A', 0xFF, MCP23017_ADDR1);  // Set all 8 pins on Port A as input, 0xFF = 0b11111111
    setPortDirection(fd, 'B', 0x00, MCP23017_ADDR1);  // Set all 8 pins on Port B as output, 0x00 = 0b00000000
    // setPortDirection(fd, 'A', 0xFF, MCP23017_ADDR2);  // Set all 8 pins on Port A as input, 0xFF = 0b11111111
    // setPortDirection(fd, 'B', 0x00, MCP23017_ADDR2);  // Set all 8 pins on Port B as output, 0x00 = 0b00000000

    while (1) {
        // //Write to Port B
        // writePort(fd, 'B', 0x0F); // 0x0F = 0b00001111
        // sleep(1);
        // writePort(fd, 'B', 0x07); // 0x07 = 0b00000111
        // sleep(1);
        // writePort(fd, 'B', 0x03); // 0x03 = 0b00000011
        // sleep(1);
        // writePort(fd, 'B', 0x01); // 0x01 = 0b00000001
        // sleep(1);

        //Blink the LED connected to pin 0 and 1 of Port B for both MCP23017_ADDR1 and MCP23017_ADDR2
        writePort(fd, 'B', 0x01, MCP23017_ADDR1); // Set pin 0 of Port B to high
        //writePort(fd, 'B', 0x01, MCP23017_ADDR2); // Set pin 0 of Port B to high
        sleep(1);
        writePort(fd, 'B', 0x00, MCP23017_ADDR1); // Set pin 0 of Port B to low
        //writePort(fd, 'B', 0x00, MCP23017_ADDR2); // Set pin 0 of Port B to low
        sleep(1);
        writePort(fd, 'B', 0x02, MCP23017_ADDR1); // Set pin 1 of Port B to high
        //writePort(fd, 'B', 0x02, MCP23017_ADDR2); // Set pin 1 of Port B to high
        sleep(1);
        writePort(fd, 'B', 0x00, MCP23017_ADDR1); // Set pin 1 of Port B to low
        //writePort(fd, 'B', 0x00, MCP23017_ADDR2); // Set pin 1 of Port B to low
        sleep(1);

        //Read the input state of port A
        // unsigned char portAState1 = readPort(fd, 'A', MCP23017_ADDR1);
        // unsigned char portAState2 = readPort(fd, 'A', MCP23017_ADDR2);

        // //Display the input state of each pin in port A
        // std::cout << "Input state of MCP23017_ADDR1 Port A: 0x" << std::hex << static_cast<int>(portAState1) << std::endl;
        // std::cout << "Input state of MCP23017_ADDR2 Port A: 0x" << std::hex << static_cast<int>(portAState2) << std::endl;
    }

    close(fd);

    return 0;
}
