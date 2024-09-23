#include "CameraManager.h"

uint64_t getcurrenttime_ms(){
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    //the function returns the current time in milliseconds, which looks like this: 1634020000000
}

//Constructor
CameraManager::CameraManager(const ConfigManager& configManager, ControlModule& controlModule, CommModule& commModule)
    : configManager(configManager), controlModule(controlModule), commModule(commModule) {
    // Initialize cameraStatus with the camera configurations
    for (const auto& camConfig : configManager.getCameraConfigs()) {
        CameraStatus camStatus;
        camStatus.ip = camConfig.ip_address;
        camStatus.isAlive = false;
        camStatus.deadCount = 0;
        camStatus.lastHeartbeat = 0;

        for (const auto& demand : camConfig.demands) {
            DemandStatus demandStatus;
            demandStatus.demandId = demand.detect_loop;
            demandStatus.isFound = false;
            demandStatus.isHandled = false;
            demandStatus.lastHandledTime = 0;
            demandStatus.previousCount = 2147483647; 
            camStatus.demandStatus.push_back(demandStatus);
        }

        cameraStatus.push_back(camStatus);
    }

    // print the whole cameraStatus
    for (const auto& camStatus : cameraStatus) {
        std::cout << "Camera IP: " << camStatus.ip << std::endl;
        std::cout << "Camera isAlive: " << camStatus.isAlive << std::endl;
        std::cout << "Camera deadCount: " << camStatus.deadCount << std::endl;
        std::cout << "Camera lastHeartbeat: " << camStatus.lastHeartbeat << std::endl;

        for (const auto& demandStatus : camStatus.demandStatus) {
            std::cout << "Demand ID: " << demandStatus.demandId << std::endl;
            std::cout << "Demand isFound: " << demandStatus.isFound << std::endl;
            std::cout << "Demand isHandled: " << demandStatus.isHandled << std::endl;
            std::cout << "Demand lastHandledTime: " << demandStatus.lastHandledTime << std::endl;
            std::cout << "Demand previousCount: " << demandStatus.previousCount << std::endl;
        }
    }
}

//Method to check the demand for the camera
void CameraManager::checkDemand(const ConfigManager::CameraConfig& camConfig) {
    LvRestfulClient client;
    std::string url = "http://" + camConfig.ip_address + "/api/v1/count";
    unsigned long request_id;

    // Start the HTTP GET request
    if (client.put(url, LvRestfulClient::METHOD_GET, "", 1000, &request_id) != 0) {
        std::cerr << "Failed to start HTTP GET request." << std::endl;
        client.clear_all_conn();
        return;
    }

    // Record the start time for the timeout mechanism
    uint64_t start_time = mg_millis();

    // Poll for the response with a timeout
    while (true) {
        // Check if the timeout has been exceeded
        if (mg_millis() - start_time > 500) {  // 500 milliseconds timeout
            std::cerr << "Timeout waiting for response from camera: " << camConfig.ip_address << std::endl;
            client.clear_all_conn();
            return;
        }

        client.loop(mg_millis());
        std::string responseBody;
        LvRestfulClient::METHOD method;
        int statusCode;

        while (client.get(url, method, responseBody, statusCode, &request_id) == 0) {
            if (statusCode != 200) {
                std::cerr << "HTTP request failed with status code: " << statusCode << std::endl;
                client.clear_all_conn();
                return;
            }

            std::cout << "Camera found for IP: " << camConfig.ip_address << std::endl;

            // Parse the JSON response
            LvJSON json;
            if (json.Parse(responseBody.c_str()).HasParseError()) {
                std::cerr << "Failed to parse JSON response." << std::endl;
                return;
            }

            std::cout << "Processing the JSON response..." << std::endl;
            for (const auto& data : json["data"].GetArray()) {
                int id = data["id"].GetInt();

                if (id != 1 && id != 2) {
                    continue;
                }

                std::cout << "Processing demand ID " << id << std::endl;

                int frameCount = data["frame_count"][0].GetInt();
                auto countConfig = configManager.getDemandCountConfig(camConfig.ip_address, id);
                int CurrentCount = json["data"][countConfig.count_loop - 1]["accumulate_count"][0].GetInt();

                std::cout << "Frame count is at ID " << id << " with value " << std::dec << frameCount << std::endl;
                std::cout << "Count loop is at ID " << countConfig.count_loop << " with value " << std::dec << CurrentCount << std::endl;

                if (cameraStatus.size() == 0) {
                    std::cerr << "Camera status not initialized." << std::endl;
                    return;
                }

                for (auto& camStatus : cameraStatus) {
                    if (camStatus.ip == camConfig.ip_address) {
                        camStatus.isAlive = true;
                        //camStatus.deadCount = 0;
                        camStatus.lastHeartbeat = getcurrenttime_ms();

                        for (auto& demandStatus : camStatus.demandStatus) {
                            if (demandStatus.demandId != id) {
                                continue;
                            }

                            demandStatus.isFound = true;

                            //skip if the demand is already handled
                            if (demandStatus.isHandled) {
                                std::cout << "Demand already handled for demand ID " << id << std::endl;
                                continue;
                            }

                            if (demandStatus.previousCount == CurrentCount) {
                                std::cout << "Demand already processed for count loop ID " << countConfig.count_loop << std::endl;
                                continue;
                            }

                            std::cout << "Storing the current count for count loop ID " << countConfig.count_loop << std::endl;
                            demandStatus.previousCount = CurrentCount;

                            if (frameCount > 0) {
                                std::cout << "Car detected for demand ID: " << id << std::endl;
                                auto gpioConfig = configManager.getDemandGpioConfig(camConfig.ip_address, id);
                                controlModule.handleDemand(id, frameCount, gpioConfig.gpio_type, gpioConfig.gpio_pin);
                                demandStatus.isHandled = true;
                                demandStatus.lastHandledTime = getcurrenttime_ms();
                                std::cout << "Recorded the last handled time for demand ID: " << id << std::endl;
                            } else {
                                std::cout << "No car detected for demand ID: " << id << std::endl;
                                continue;
                            }
                        }
                    }
                }
            }
            client.clear_all_conn();
            return;
        }
    }
}

//Method to check the heartbeat for the camera, if last heartbeat is greater than 1000ms, the camera is considered dead, and the gpio pin is toggled to low
void CameraManager::checkHeartbeat(const ConfigManager::CameraConfig& camConfig) {
    for (auto& camStatus : cameraStatus) {
        if (camStatus.ip != camConfig.ip_address) {
            continue;
        }

        uint64_t now_ms = getcurrenttime_ms();
        if (now_ms - camStatus.lastHeartbeat > 1000) {
            camStatus.deadCount++;
            if (camStatus.deadCount > 2) {
                camStatus.isAlive = false;
                //toggle gpio pin that represents the camera status to low using handleHeartbeat method
                controlModule.handleHeartbeat(camStatus.ip, false);
                std::cout << "Camera: " << camConfig.ip_address << " is dead." << std::endl;
            }
        } else {
            camStatus.deadCount = 0;
            controlModule.handleHeartbeat(camStatus.ip, true);
        }
    }
}

// Method to generate JSON output for the camera isAlive status
std::string CameraManager::generateAliveStatusJSON() {
    LvJSON doc;
    auto& allocator = doc.GetAllocator();

    doc.SetObject();

    LvJSON::Value cameraStatusArray(rapidjson::kArrayType);
    for (const auto& camStatus : cameraStatus) {
        LvJSON::Value cameraStatusObj(rapidjson::kObjectType);

        // Properly set the string using SetString with the allocator
        rapidjson::Value ipValue;
        ipValue.SetString(camStatus.ip.c_str(), allocator); // Use SetString for dynamic strings
        cameraStatusObj.AddMember("ip", ipValue, allocator);

        // Add the isAlive boolean value
        cameraStatusObj.AddMember("isAlive", camStatus.isAlive, allocator);

        cameraStatusArray.PushBack(cameraStatusObj, allocator);
    }

    doc.AddMember("cameraStatus", cameraStatusArray, allocator);

    // Create a StringBuffer and Writer for generating the final JSON string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::string jsonString = buffer.GetString();

    // Return the JSON string
    return jsonString;
}

//Method to publish the JSON output to the MQTT server
void CameraManager::publishAliveStatus() {
    std::string jsonOutput = generateAliveStatusJSON();
    commModule.publish("Camera_status", jsonOutput);
}

// New method to reset the demand status
// If the demand is handled and the lastHandledTime is greater than the hold time, the demand is reset
void CameraManager::resetDemandStatus() {
    for (auto& camStatus : cameraStatus) {
        for (auto& demandStatus : camStatus.demandStatus) {
            uint64_t now_ms = getcurrenttime_ms();
            int hold_time = configManager.getDemandHoldTime(demandStatus.demandId);

            if (demandStatus.isHandled && now_ms - demandStatus.lastHandledTime > hold_time) {
                std::cout << "Demand: " << demandStatus.demandId << " hold time exceeded." << std::endl;
                auto gpioConfig = configManager.getDemandGpioConfig(camStatus.ip, demandStatus.demandId);
                controlModule.resetDemand(demandStatus.demandId);
                demandStatus.isHandled = false;
            }
        }
    }
}

//New method to loop through the camera configurations, checking, resetting the demand and checking the heartbeat
void CameraManager::loop(uint64_t now_ms) {
    for (const auto& camConfig : configManager.getCameraConfigs()) {
        std::cout << "--------------------------------------------------------------------------------" << std::endl;
        std::cout << "Searching for camera: " << camConfig.ip_address << std::endl;
        checkDemand(camConfig);
        checkHeartbeat(camConfig);
        publishAliveStatus();
        resetDemandStatus();
        usleep(500000);
    }
}