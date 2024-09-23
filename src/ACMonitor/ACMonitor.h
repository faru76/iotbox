#ifndef AC_MONITOR_H
#define AC_MONITOR_H

#include "ConfigManager.h"
#include "ControlModule.h"
#include "CommModule.h"
#include "LvJSON.h"
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

class ACMonitor {
public:
    ACMonitor(const ConfigManager& configManager, ControlModule& controlModule, CommModule& commModule);
    void loop(uint64_t now_ms);

private:
    const ConfigManager& configManager;
    ControlModule& controlModule;
    CommModule& commModule;

    struct ACStatus {
        int phase;
        bool red_state;
        bool green_state;
    };

    std::vector<ACStatus> acStatus;

    void checkACStatus(const ConfigManager::AC_in_config& acConfig);
    void resetACStatus();
    std::string generateJSON();
};

#endif // AC_MONITOR_H