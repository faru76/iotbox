#include "DCinput.h"

// Constructor
DCInput::DCInput(const ConfigManager& configManager, ControlModule& controlModule, CommModule& commModule) 
    : configManager(configManager), controlModule(controlModule), commModule(commModule) {
    // Initialize the DCStatus vector
    for (const auto& dcConfig : configManager.getDCConfigs()) {
        DCStatus dc;
        dc.push_button = dcConfig.push_button;
        dc.isPressed = false;
        dcStatus.push_back(dc);
    }
}

// Method to check the DC status
void DCInput::checkDCStatus(const ConfigManager::DC_in_config& dcConfig) {
    // Loop through the DC status vector
    for (auto& dc : dcStatus) {
        if (dc.push_button == dcConfig.push_button) {
            // Get the GPIO pin configuration
            auto gpio_pin = configManager.getDCConfig(dc.push_button).gpio_pin;

            // Read the current status of the push button
            bool current_status = controlModule.readDCStatus(gpio_pin);

            // If the value is true, it means the push button is not pressed
            if (current_status) {
                dc.isPressed = false;
            }
            else {
                dc.isPressed = true;
            }

            return; // Push button found and processed, exit the loop
        }
    }
    // If the push button is not found, throw an error
    std::cerr << "Error: Push button not found: " << dcConfig.push_button << std::endl;
}

// Method to reset the DC status
void DCInput::resetDCStatus() {
    // Loop through the DC status vector
    for (auto& dc : dcStatus) {
        dc.isPressed = false;
    }
}

// Method to generate JSON output for the DC status
std::string DCInput::generateJSON() {
    LvJSON doc;
    auto& allocator = doc.GetAllocator();

    // Initialize the root as an object
    doc.SetObject();

    // Create an array for the DC status
    LvJSON::Value dcStatusArray(rapidjson::kArrayType);
    for (const auto& dc : dcStatus) {
        LvJSON::Value dcObj(rapidjson::kObjectType);
        dcObj.AddMember("push_button", dc.push_button, allocator);
        dcObj.AddMember("isPressed", dc.isPressed, allocator);
        dcStatusArray.PushBack(dcObj, allocator);
    }
    doc.AddMember("DC_status", dcStatusArray, allocator);

    // Convert the JSON document to a string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::string jsonString = buffer.GetString();
    return jsonString;

    // The JSON output will be in the format:
    // {
    //     "DC_status": [
    //         {
    //             "push_button": 1,
    //             "isPressed": false
    //         },
    //         {
    //             "push_button": 2,
    //             "isPressed": true
    //         }
    //     ]
    // }
}

// Method to loop through the DC status
void DCInput::loop(uint64_t now_ms) {
    // Loop through the DC configurations
    for (const auto& dcConfig : configManager.getDCConfigs()) {
        checkDCStatus(dcConfig);
    }

    // Generate JSON output for the DC status
    std::string jsonOutput = generateJSON();

    // Publish the JSON output to the MQTT server
    commModule.publish("DC_status", jsonOutput);

    // Reset the DC status
    resetDCStatus();
}