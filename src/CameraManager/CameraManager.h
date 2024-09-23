#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "ConfigManager.h"
#include "ControlModule.h"
#include "CommModule.h"
#include "LvRestfulClient.h"
#include "LvJSON.h"
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <map>

class CameraManager {
public:
    CameraManager(const ConfigManager& configManager, ControlModule& controlModule, CommModule& commModule);
    void loop(uint64_t now_ms);
    
private:
    const ConfigManager& configManager;
    ControlModule& controlModule;
    CommModule& commModule;

    struct DemandStatus{
        int demandId;
        bool isFound;
        bool isHandled;
        uint64_t lastHandledTime;
        int previousCount;
    };

    struct CameraStatus{
        std::string ip;
        bool isAlive;
        int deadCount;
        uint64_t lastHeartbeat;
        std::vector<DemandStatus> demandStatus;
    };

    std::vector<CameraStatus> cameraStatus;
    void checkDemand(const ConfigManager::CameraConfig& camConfig);
    void checkHeartbeat(const ConfigManager::CameraConfig& camConfig);
    std::string generateAliveStatusJSON();
    void publishAliveStatus();
    void resetDemandStatus();
};

#endif // CAMERA_MANAGER_H
