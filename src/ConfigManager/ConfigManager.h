#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "LvJSON.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <regex>

class ConfigManager {
public:
    // Nested structures for various configurations
    struct GpioConfig {
        int gpio_type;
        int gpio_pin;
    };

    struct Demand {
        int detect_loop;
        int count_loop;
        int gpio_type;
        int gpio_pin;
        int hold_time;
    };

    struct CameraConfig {
        std::string ip_address;
        int status_gpio_type;
        int status_gpio_pin;
        std::vector<Demand> demands;
    };

    struct AC_in_config {
        int phase;
        int red_gpio_pin;
        int green_gpio_pin;
    };

    struct DC_in_config {
        int push_button;
        int gpio_pin;
    };

    // Constructor and Destructor
    explicit ConfigManager(const std::string& configFile);
    ~ConfigManager();

    // Public methods
    const std::vector<CameraConfig>& getCameraConfigs() const;
    
    GpioConfig getDemandGpioConfig(const std::string& ip, int detect_loop) const;
    GpioConfig getGpioTypeConfig(const std::string& type);
    GpioConfig getStatusGpioConfig(const std::string& ip) const;
    
    CameraConfig getCameraConfig(const std::string& ip) const;
    int getDemandHoldTime(int detect_loop) const;
    Demand getDemandCountConfig(const std::string& ip, int detect_loop) const;

    const std::vector<AC_in_config>& getACconfigs() const;
    AC_in_config getACConfig(int phase) const;
    const std::vector<DC_in_config>& getDCConfigs() const;
    DC_in_config getDCConfig(int push_button) const;

private:
    // Private member variables
    std::string configFilePath;
    std::map<std::string, int> gpioTypeMap;
    std::vector<CameraConfig> cameraConfigs;
    std::vector<AC_in_config> acConfigs;
    std::vector<DC_in_config> dcConfigs;

    // Private methods
    bool loadConfig();
    bool validateConfig(const LvJSON& doc);
    bool isValidIPAddress(const std::string& ip);
    void createDefaultConfig(const std::string& filepath = "");
};

#endif // CONFIG_MANAGER_H