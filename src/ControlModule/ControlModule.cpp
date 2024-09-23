#include "ControlModule.h"

ControlModule::ControlModule(const std::string& i2cDevice, int mcpAddress1, int mcpAddress2, const ConfigManager& configManager)
    : mcpAddress1(mcpAddress1), mcpAddress2(mcpAddress2), configManager(configManager), cacheExpireDuration(250), cachedPortAValue(0x00), cachedPortBValue(0x00), lastCacheUpdateTime(0) {
    
    // Open the I2C device file
    i2cFile = open(i2cDevice.c_str(), O_RDWR);
    if (i2cFile < 0) {
        perror("Failed to open I2C device");
        exit(1);
    }

    // MCP23017 Initialization, note that GPA7 and GPB7 cannot be used as input, so this part needs to be modified later
    // set all pins of port A as input except for GPA7
    setPortDirection(i2cFile, 'A', 0b11111111, mcpAddress1);
    // set all pins of port B as output except for GPB0
    setPortDirection(i2cFile, 'B', 0b00000000, mcpAddress1);
    // Initialization to set all GPIO pins related to demands to LOW
    portValue = 0b00000000;
    writePort('A', portValue, mcpAddress1);
    writePort('B', portValue, mcpAddress1);
    std::cout << "Control Module Initialized." << std::endl;
}

ControlModule::~ControlModule() {
    close(i2cFile);
}

//handle demand method
void ControlModule::handleDemand(int demandId, int frameCount, int gpioType, int gpioPin) {
    std::cout << "Handling demand: " << demandId << " with frame count: " << frameCount << std::endl;
    //set the bit of portValue based on the gpioPin, while keeping the other bits the same
    portValue |= (1 << gpioPin);
    //write the portValue to the MCP23017
    writePort('B', portValue, (gpioType == I2C_MCP23017_0x20) ? mcpAddress1 : mcpAddress2);
    
    std::cout << "Written to port: " << portValue << std::endl;
    std::cout << "Demand handled." << std::endl;
    return;
}

//reset demand method
void ControlModule::resetDemand(int demandId) {
    // Clear the demand, set the bit to low
    portValue &= ~(1 << demandId-1);
    // Write the portValue to the MCP23017
    writePort('B', portValue, mcpAddress1);
    std::cout << "Demand: " << demandId << " reset." << std::endl;
}

//handle heartbeat method
void ControlModule::handleHeartbeat(const std::string& ip, bool isAlive) {
    // Get the heartbeat GPIO config from the config manager based on the IP
    auto gpioConfig = configManager.getStatusGpioConfig(ip);

    // Determine the corresponding GPIO pin based on the index returned by config manager
    const char* gpiochip;
    int line_offset;

    switch (gpioConfig.gpio_pin) {
        case 0:
            gpiochip = "/dev/gpiochip2"; // PC13
            line_offset = 13;
            break;
        case 1:
            gpiochip = "/dev/gpiochip6"; // PG8
            line_offset = 8;
            break;
        case 2:
            gpiochip = "/dev/gpiochip8"; // PI2
            line_offset = 2;
            break;
        case 3:
            gpiochip = "/dev/gpiochip8"; // PI7
            line_offset = 7;
            break;
        default:
            std::cerr << "Invalid GPIO pin for heartbeat in config" << std::endl;
            return;
    }

    // Write the corresponding GPIO value (HIGH = alive, LOW = dead)
    writeGPIO(gpiochip, line_offset, isAlive ? 0 : 1); // Inverted logic, HIGH = 0, LOW = 1, active low circuit

    std::cout << "Heartbeat status updated for camera " << ip << ": " 
              << (isAlive ? "Alive (HIGH)" : "Dead (LOW)") << std::endl;
}

//method to refresh cached port values from the MCP23017 if the cache has expired
void ControlModule::refreshPortValues() {
    // Get the current time in milliseconds
    unsigned long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // Check if the cache has expired
    if (now - lastCacheUpdateTime > cacheExpireDuration) {
        // Read the port values from the MCP23017
        cachedPortAValue = readPort('A', mcpAddress1);
        cachedPortBValue = readPort('B', mcpAddress1);
        // Update the last cache update time
        lastCacheUpdateTime = now;
    }
}

// Method to read AC status from port A of MCP23017, take the pin as input and return the status
bool ControlModule::readACStatus(int pin) {
    refreshPortValues();
    return (cachedPortAValue >> pin) & 1; // Return the status of the pin as a boolean, 1 = HIGH, 0 = LOW
}

// Method to read DC status from port A of MCP23017, take the pin as input and return the status
bool ControlModule::readDCStatus(int pin) {
    refreshPortValues();
    return (cachedPortAValue >> pin) & 1; // Return the status of the pin as a boolean, 1 = HIGH, 0 = LOW
}

// Method to write to an onboard GPIO pin (combined setup and write)
void ControlModule::writeGPIO(const char* gpiochip, int line_offset, int value) {
    //open the GPIO device file
    gpioFile = open(gpiochip, O_WRONLY);
    if (gpioFile < 0) {
        perror("Failed to open GPIO device");
        return;
    }

    // Set GPIO as output
    gpioRequestOutput.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(gpioRequestOutput.consumer_label, "control_gpio");
    memset(gpioRequestOutput.default_values, 0, sizeof(gpioRequestOutput.default_values));
    gpioRequestOutput.lines = 1;
    gpioRequestOutput.lineoffsets[0] = line_offset;
    gpioRequestOutput.default_values[0] = 0;  // Initial value LOW

    if (ioctl(gpioFile, GPIO_GET_LINEHANDLE_IOCTL, &gpioRequestOutput) < 0) {
        perror("Failed to set GPIO as output");
        return;
    }

    // Set the GPIO pin to the desired value (high or low)
    gpioDataOutput.values[0] = value;
    if (ioctl(gpioRequestOutput.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &gpioDataOutput) < 0) {
        perror("Error setting GPIO value");
    } else {
        printf("GPIO set to %d on line %d\n", value, line_offset);
    }

    // Clean up
    close(gpioRequestOutput.fd);  // Close the handle
    close(gpioFile);  // Close the GPIO device file

    return;
}

// Method to read from an onboard GPIO pin (combined setup and read)
bool ControlModule::readGPIO(const char* gpiochip, int line_offset) {
    // Open the GPIO device file
    gpioFile = open(gpiochip, O_RDONLY);
    if (gpioFile < 0) {
        perror("Failed to open GPIO device");
        return false;
    }

    // Set GPIO as input
    gpioRequestInput.flags = GPIOHANDLE_REQUEST_INPUT;
    gpioRequestInput.lines = 1;
    gpioRequestInput.lineoffsets[0] = line_offset;
    strcpy(gpioRequestInput.consumer_label, "control_gpio_input");

    if (ioctl(gpioFile, GPIO_GET_LINEHANDLE_IOCTL, &gpioRequestInput) < 0) {
        perror("Failed to set GPIO as input");
        return false;
    }

    // Read the GPIO value
    if (ioctl(gpioRequestInput.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &gpioDataInput) < 0) {
        perror("Error reading GPIO input");
        close(gpioRequestInput.fd);
        return false;
    }

    bool value = gpioDataInput.values[0];  // Get the GPIO value

    // Clean up
    close(gpioRequestInput.fd);  // Close the handle
    close(gpioFile);  // Close the GPIO device file

    return value;
}

// Method to set the direction of a port (A or B) on the MCP23017
void ControlModule::setPortDirection(int fd, unsigned char port, unsigned char direction, unsigned char address) {
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[2];

    buf[0] = (port == 'A') ? IODIRA_REG : IODIRB_REG;
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

// Method to write to a port (A or B) on the MCP23017
void ControlModule::writePort(unsigned char port, unsigned char value, unsigned char address) {
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[2];

    buf[0] = (port == 'A') ? GPIOA_REG : GPIOB_REG;
    buf[1] = value;

    std::cout << "Writing to port: " << port << ", Value: " << std::bitset<8>(value) << ", Address: " << std::hex << static_cast<int>(address) << std::endl;
    
    msg.addr = address;
    msg.flags = 0;
    msg.len = 2;
    msg.buf = buf;

    data.msgs = &msg;
    data.nmsgs = 1;

    if (ioctl(i2cFile, I2C_RDWR, &data) < 0) {
        perror("ioctl error");
        exit(1);
    }
}

// Method to read from a port (A or B) on the MCP23017
unsigned char ControlModule::readPort(unsigned char port, unsigned char address) {
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data data;
    unsigned char buf[1];
    unsigned char read_buf[1];

    buf[0] = (port == 'A') ? GPIOA_REG : GPIOB_REG;

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = buf;

    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].buf = read_buf;

    data.msgs = msgs;
    data.nmsgs = 2;

    if (ioctl(i2cFile, I2C_RDWR, &data) < 0) {
        perror("ioctl error");
        exit(1);
    }

    return read_buf[0];
}