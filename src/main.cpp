#include "ConfigManager.h"
#include "CameraManager.h"
#include "ControlModule.h"
#include "CommModule.h"
#include "ACMonitor.h"
#include "DCinput.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <unistd.h>

#define MCP23017_ADDR1 0x20
#define MCP23017_ADDR2 0x21

uint64_t get_current_time_ms(){
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int main() {
    std::cout << "------------ Starting the program ------------" << std::endl;
    ConfigManager configManager("config.json");
    std::cout << "----- Configuration loaded successfully -----" << std::endl;

    std::cout << "-------- Starting the control module --------" << std::endl;
    ControlModule controlModule("/dev/i2c-0", MCP23017_ADDR1, MCP23017_ADDR2, configManager);
    std::cout << "-------- Control module initialized ---------" << std::endl;

    std::cout << "-------- Starting the communication module --------" << std::endl;
    // CommModule commModule(configManager.getCommModuleAddress());
    CommModule commModule("0.0.0.0:1883");
    std::cout << "-------- Communication module initialized ---------" << std::endl;

    std::cout << "-------- Starting the DC input module --------" << std::endl;
    DCInput dcInput(configManager, controlModule, commModule);
    std::cout << "-------- DC input module initialized ---------" << std::endl;

    std::cout << "-------- Starting the AC monitor module --------" << std::endl;
    ACMonitor acMonitor(configManager, controlModule, commModule);
    std::cout << "-------- AC monitor module initialized ---------" << std::endl;
    
    std::cout << "-------- Starting the camera manager --------" << std::endl;
    CameraManager cameraManager(configManager, controlModule, commModule);
    std::cout << "-------- Camera manager initialized ---------" << std::endl;

    std::cout << "---------- Starting the main loop -----------" << std::endl;
    while (true) {
        uint64_t now_ms = get_current_time_ms();
        std::cout << "-------- Entering camera manager loop -------" << std::endl;
        cameraManager.loop(now_ms);

        //AC Monitor loop
        std::cout << "-------- Entering AC monitor loop -------" << std::endl;
        acMonitor.loop(now_ms);

        //DC Input loop
        std::cout << "-------- Entering DC input loop -------" << std::endl;
        dcInput.loop(now_ms);

        //CommModule loop to handle incoming messages
        std::cout << "-------- Entering communication module loop -------" << std::endl;
        commModule.loop(now_ms);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
