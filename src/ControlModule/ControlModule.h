#ifndef CONTROL_MODULE_H
#define CONTROL_MODULE_H

#include <string>
#include <vector>
#include <linux/gpio.h>
#include <linux/i2c-dev.h>
#include "ConfigManager.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>
#include <iostream>
#include <bitset>
#include <chrono>
#include <map>
#include <cstring> // std::strcpy
#include <cerrno> // errno
#include <cstdio> // perror

#define ON_BOARD_GPIO 2
#define I2C_MCP23017_0x20 0
#define I2C_MCP23017_0x21 1

#define I2C_DEV "/dev/i2c-0"// The device file for I2C bus
#define MCP23017_ADDR1 0x20  // I2C address of MCP23017
#define MCP23017_ADDR2 0x21  
#define MCP23017_ADDR3 0x22  
#define IODIRA_REG 0x00     // I/O direction register for Port A
#define GPIOA_REG  0x12     // GPIO register for Port A
#define IODIRB_REG 0x01     // I/O direction register for Port B
#define GPIOB_REG  0x13     // GPIO register for Port B
#define MCP23017_I2C_RDWR 0x0707

struct i2c_msg {
    unsigned short addr;
    unsigned short flags;
#define I2C_M_RD 0x0001
    unsigned short len;
    unsigned char *buf;
}; 

class ControlModule {
public:
    ControlModule(const std::string& i2cDevice, int mcpAddress1, int mcpAddress2, const ConfigManager& configManager);
    ~ControlModule();

    void handleDemand(int demandId, int frameCount, int gpioType, int gpioPin);
    void resetDemand(int demandId);
    void handleHeartbeat(const std::string& ip, bool isAlive);
    bool readACStatus(int pin);
    bool readDCStatus(int pin);
    const ConfigManager &getConfigManager() const { return configManager; }

private:
    int i2cFile;
    int gpioFile;
    int mcpAddress1;
    int mcpAddress2;
    ConfigManager configManager;
	
    struct gpiohandle_request gpioRequestInput;
    struct gpiohandle_request gpioRequestOutput;
    struct gpiohandle_data gpioDataInput;
    struct gpiohandle_data gpioDataOutput;

    unsigned char portValue;
    unsigned char cachedPortAValue;
    unsigned char cachedPortBValue;

    unsigned long long lastCacheUpdateTime;
    unsigned long cacheExpireDuration;

    void writeGPIO(const char* gpiochip, int line_offset, int value);
    bool readGPIO(const char* gpiochip, int line_offset);

    void refreshPortValues();
    void setPortDirection(int fd, unsigned char port, unsigned char direction, unsigned char address);
    void writePort(unsigned char port, unsigned char value, unsigned char address);
    unsigned char readPort(unsigned char port, unsigned char address);
};

#endif // CONTROL_MODULE_H
