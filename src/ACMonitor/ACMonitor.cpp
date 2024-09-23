#include "ACMonitor.h"

// Constructor
ACMonitor::ACMonitor(const ConfigManager& configManager, ControlModule& controlModule, CommModule& commModule) 
    : configManager(configManager), controlModule(controlModule), commModule(commModule) {
    // Initialize acStatus with the AC configurations
    for (const auto& acConfig : configManager.getACconfigs()) {
        ACStatus acStat;
        acStat.phase = acConfig.phase;
        acStat.red_state = 0;
        acStat.green_state = 0;
        acStatus.push_back(acStat);
    }
}

// Method to monitor the AC status
void ACMonitor::checkACStatus(const ConfigManager::AC_in_config& acConfig) {
    // Loop through the AC status vector
    for (auto& acStat : acStatus) {
        if (acStat.phase == acConfig.phase) {
            // Get the GPIO pin configuration for red and green lights
            auto red_gpio_pin = configManager.getACConfig(acStat.phase).red_gpio_pin;
            auto green_gpio_pin = configManager.getACConfig(acStat.phase).green_gpio_pin;

            // Check the current pin status using readACStatus method from ControlModule
            bool current_red_state = controlModule.readACStatus(red_gpio_pin);
            bool current_green_state = controlModule.readACStatus(green_gpio_pin);

            // If the value is true, it means the light is off
            if (current_red_state) {
                acStat.red_state = 0;
            }else{
                acStat.red_state = 1;
            }

            if (current_green_state) {
                acStat.green_state = 0;
            }else{
                acStat.green_state = 1;
            }

            return; // Phase found and processed, exit the loop
        }
    }
    // If the phase is not found, throw an error
    std::cerr << "AC phase not found: " << acConfig.phase << std::endl;
}

// Method to reset the AC status
void ACMonitor::resetACStatus() {
    // Loop through the AC status vector
    for (auto& acStat : acStatus) {
        acStat.red_state = 0;
        acStat.green_state = 0;
    }
}

// Method to generate JSON output for the AC status
std::string ACMonitor::generateJSON() {
    LvJSON doc;
    auto& allocator = doc.GetAllocator();

    // Initialize the root as an object
    doc.SetObject();

    // Create an array for the AC status
    LvJSON::Value acStatusArray(rapidjson::kArrayType);
    for (const auto& acStat : acStatus) {
        LvJSON::Value acStatObj(rapidjson::kObjectType);
        acStatObj.AddMember("phase", acStat.phase, allocator);
        acStatObj.AddMember("red_state", acStat.red_state, allocator);
        acStatObj.AddMember("green_state", acStat.green_state, allocator);
        acStatusArray.PushBack(acStatObj, allocator);
    }
    doc.AddMember("AC_status", acStatusArray, allocator);

    // Convert the JSON document to a string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::string jsonString = buffer.GetString();
    return jsonString;

    //the JSON output will be in the format:
    // {
    //     "AC_status": [
    //         {
    //             "phase": 1,
    //             "red_state": 0,
    //             "green_state": 0
    //         },
    //         {
    //             "phase": 2,
    //             "red_state": 0,
    //             "green_state": 0
    //         }
    //     ]
    // }
}

// Method to loop through the AC status
void ACMonitor::loop(uint64_t now_ms) {
    // Loop through the AC configurations
    for (const auto& acConfig : configManager.getACconfigs()) {
        checkACStatus(acConfig);
    }

    // Generate JSON output for the AC status
    std::string jsonOutput = generateJSON();
    
    // Publish the JSON output to the MQTT server
    commModule.publish("AC_status", jsonOutput);

    // Reset the AC status
    resetACStatus();
}
            