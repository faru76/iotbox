#ifndef DC_INPUT_H
#define DC_INPUT_H

#include "ConfigManager.h"
#include "ControlModule.h"
#include "CommModule.h"
#include "LvJSON.h"
#include <iostream>
#include <vector>
#include <string>

class DCInput{
public:
    DCInput(const ConfigManager& configManager, ControlModule& controlModule, CommModule& commModule);
    void loop(uint64_t now_ms);
    void checkDCStatus(const ConfigManager::DC_in_config& dcConfig); 

private:
    const ConfigManager& configManager;
    ControlModule& controlModule;
    CommModule& commModule;

    struct DCStatus{
        int push_button;
        bool isPressed;
    };

    std::vector<DCStatus> dcStatus;

    void resetDCStatus();
    std::string generateJSON();
};

#endif // DC_INPUT_H