#include "ConfigManager.h"

//Constructor
ConfigManager::ConfigManager(const std::string& configFile) : configFilePath(configFile) {
    if(!loadConfig()) {
        std::cerr << "Failed to load configuration file." << std::endl;
    }

    std::cout << "ConfigManager object created with config file: " << configFile << std::endl;

    // print the whole CameraConfig
    for (const auto& camConfig : cameraConfigs) {
        std::cout << "Camera IP: " << camConfig.ip_address << std::endl;
        std::cout << "Camera status GPIO type: " << camConfig.status_gpio_type << std::endl;
        std::cout << "Camera status GPIO pin: " << camConfig.status_gpio_pin << std::endl;

        for (const auto& demand : camConfig.demands) {
            std::cout << "Demand detect loop: " << demand.detect_loop << std::endl;
            std::cout << "Demand count loop: " << demand.count_loop << std::endl;
            std::cout << "Demand GPIO type: " << demand.gpio_type << std::endl;
            std::cout << "Demand GPIO pin: " << demand.gpio_pin << std::endl;
            std::cout << "Demand hold time: " << demand.hold_time << std::endl;
        }
    }
}

//Destructor
ConfigManager::~ConfigManager() {
    std::cout << "ConfigManager object destroyed." << std::endl;
}

// Load configurations, cameraconfig is an array of objects, while camconfig is an object
bool ConfigManager::loadConfig() {
    std::ifstream ifs(configFilePath);
    if (!ifs.is_open()) {
        std::cerr << "Config file not found. Creating default config file." << std::endl;
        createDefaultConfig();
        ifs.open(configFilePath);
        if (!ifs.is_open()) {
            std::cerr << "Failed to create and open default config file." << std::endl;
            return false;
        }
    }

    std::string configData((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    ifs.close();

    LvJSON doc;
    if (doc.Parse(configData.c_str()).HasParseError()) {
        std::cerr << "JSON parse error in configuration file." << std::endl;
        return false;
    }

    if (!validateConfig(doc)) {
        std::cerr << "Configuration validation failed." << std::endl;
        return false;
    }

    LvJSON::checkType(doc, "gpio_type", LvJSON::Object);
    const LvJSON::Value& gpioType = doc["gpio_type"];
    for (LvJSON::Value::ConstMemberIterator itr = gpioType.MemberBegin(); itr != gpioType.MemberEnd(); ++itr) {
        gpioTypeMap[itr->name.GetString()] = itr->value.GetInt();
    }

    LvJSON::checkType(doc, "camera_config", LvJSON::Array);
    const LvJSON::Value& cameras = doc["camera_config"];
    for (LvJSON::SizeType i = 0; i < cameras.Size(); i++) {
        CameraConfig camConfig;
        LvJSON::checkType(cameras[i], "ip_address", LvJSON::String);
        camConfig.ip_address = cameras[i]["ip_address"].GetString();

        LvJSON::checkType(cameras[i], "status_gpio_type", LvJSON::Int);
        camConfig.status_gpio_type = cameras[i]["status_gpio_type"].GetInt();

        LvJSON::checkType(cameras[i], "status_gpio_pin", LvJSON::Int);
        camConfig.status_gpio_pin = cameras[i]["status_gpio_pin"].GetInt();

        LvJSON::checkType(cameras[i], "Demand", LvJSON::Array);
        const LvJSON::Value& demands = cameras[i]["Demand"];
        for (LvJSON::SizeType j = 0; j < demands.Size(); j++) {
            Demand demand;

            LvJSON::checkType(demands[j], "detect_loop", LvJSON::Int);
            demand.detect_loop = demands[j]["detect_loop"].GetInt();

            LvJSON::checkType(demands[j], "count_loop", LvJSON::Int);
            demand.count_loop = demands[j]["count_loop"].GetInt();

            LvJSON::checkType(demands[j], "gpio_type", LvJSON::Int);
            demand.gpio_type = demands[j]["gpio_type"].GetInt();

            LvJSON::checkType(demands[j], "gpio_pin", LvJSON::Int);
            demand.gpio_pin = demands[j]["gpio_pin"].GetInt();

            LvJSON::checkType(demands[j], "hold_time", LvJSON::Int);
            demand.hold_time = demands[j]["hold_time"].GetInt();

            camConfig.demands.push_back(demand);
        }
        cameraConfigs.push_back(camConfig);
    }

    LvJSON::checkType(doc, "AC_in_config", LvJSON::Array);
    const LvJSON::Value& acconfigs = doc["AC_in_config"];
    for (LvJSON::SizeType i = 0; i < acconfigs.Size(); i++) {
        AC_in_config acConfig;
        LvJSON::checkType(acconfigs[i], "phase", LvJSON::Int);
        acConfig.phase = acconfigs[i]["phase"].GetInt();

        LvJSON::checkType(acconfigs[i], "red_gpio_pin", LvJSON::Int);
        acConfig.red_gpio_pin = acconfigs[i]["red_gpio_pin"].GetInt();

        LvJSON::checkType(acconfigs[i], "green_gpio_pin", LvJSON::Int);
        acConfig.green_gpio_pin = acconfigs[i]["green_gpio_pin"].GetInt();

        acConfigs.push_back(acConfig);
    }

    LvJSON::checkType(doc, "DC_in_config", LvJSON::Array);
    const LvJSON::Value& dcconfigs = doc["DC_in_config"];
    for (LvJSON::SizeType i = 0; i < dcconfigs.Size(); i++) {
        DC_in_config dcConfig;
        LvJSON::checkType(dcconfigs[i], "push_button", LvJSON::Int);
        dcConfig.push_button = dcconfigs[i]["push_button"].GetInt();

        LvJSON::checkType(dcconfigs[i], "gpio_pin", LvJSON::Int);
        dcConfig.gpio_pin = dcconfigs[i]["gpio_pin"].GetInt();

        dcConfigs.push_back(dcConfig);
    }

    return true;
}

//method to return camera configs
const std::vector<ConfigManager::CameraConfig>& ConfigManager::getCameraConfigs() const {
    return cameraConfigs;
}

//method to return demand GPIO config, specifically the gpio type and pin
ConfigManager::GpioConfig ConfigManager::getDemandGpioConfig(const std::string& ip, int detect_loop) const {
    std::cout << "Getting demand GPIO config for IP: " << ip << " and virtual loop ID: " << detect_loop << std::endl;
    for (const auto& camConfig : cameraConfigs) { 
        if (camConfig.ip_address == ip) {
            for (const auto& demand : camConfig.demands) { 
                if (demand.detect_loop == detect_loop) {
                    std::cout << "Demand found for virtual loop ID: " << detect_loop << std::endl;
                    std::cout << "The GPIO type is: " << demand.gpio_type << " and the GPIO pin is: " << demand.gpio_pin << std::endl;
                    return {demand.gpio_type, demand.gpio_pin};
                }
            }
            std::cerr << "Demand not found for virtual loop ID: " << detect_loop << std::endl;
            return {};
        }
    }
    std::cerr << "Demand GPIO config not found for IP: " << ip << std::endl;
    return {};
}

//method to return the GPIO type config for a specific type
ConfigManager::GpioConfig ConfigManager::getGpioTypeConfig(const std::string& type) {
    GpioConfig config;
    if (gpioTypeMap.find(type) != gpioTypeMap.end()) {
        config.gpio_type = gpioTypeMap[type];
    }
    return config;
}

//method to return status GPIO config, specifically the gpio type and pin
ConfigManager::GpioConfig ConfigManager::getStatusGpioConfig(const std::string& ip) const {
    for (const auto& camConfig : cameraConfigs) {
        if (camConfig.ip_address == ip) {
            return {camConfig.status_gpio_pin};
        }
    }
    std::cerr << "Camera not found for IP: " << ip << std::endl;
    return {};
}

//method to return camera config for a specific IP address
ConfigManager::CameraConfig ConfigManager::getCameraConfig(const std::string& ip) const {
    for (const auto& camConfig : cameraConfigs) {
        if (camConfig.ip_address == ip) {
            return camConfig;
        }
    }
    std::cerr << "Camera config not found for IP: " << ip << std::endl;
    return {};
}

//method to return the hold time for a demand
int ConfigManager::getDemandHoldTime(int detect_loop) const {
    for (const auto& camConfig : cameraConfigs) {
        for (const auto& demand : camConfig.demands) {
            if (demand.detect_loop == detect_loop) {
                return demand.hold_time;
            }
        }
    }
    std::cerr << "Hold time not found for virtual loop ID: " << detect_loop << std::endl;
    return -1;
}

//method to return the count loop for a demand
ConfigManager::Demand ConfigManager::getDemandCountConfig(const std::string& ip, int detect_loop) const {
    for (const auto& camConfig : cameraConfigs) {
        if (camConfig.ip_address == ip) {
            for (const auto& demand : camConfig.demands) {
                if (demand.detect_loop == detect_loop) {
                    std::cout << "Count loop for virtual loop ID: " << detect_loop << " is: " << demand.count_loop << std::endl;
                    return demand;
                }
            }
            std::cerr << "Count loop not found for virtual loop ID: " << detect_loop << std::endl;
            return {};
        }
    }
    std::cerr << "Camera not found for IP: " << ip << std::endl;
    return {};
}

//Get AC configs
const std::vector<ConfigManager::AC_in_config>& ConfigManager::getACconfigs() const {
    return acConfigs;
}

//method to return AC config for a specific phase
ConfigManager::AC_in_config ConfigManager::getACConfig(int phase) const {
    for (const auto& acConfig : acConfigs) {
        if (acConfig.phase == phase) {
            return acConfig;
        }
    }
    std::cerr << "AC config not found for phase: " << phase << std::endl;
    return {};
}

//Get DC configs
const std::vector<ConfigManager::DC_in_config>& ConfigManager::getDCConfigs() const {
    return dcConfigs;
}

//method to return DC config for a specific push button
ConfigManager::DC_in_config ConfigManager::getDCConfig(int push_button) const {
    for (const auto& dcConfig : dcConfigs) {
        if (dcConfig.push_button == push_button) {
            return dcConfig;
        }
    }
    std::cerr << "DC config not found for push button: " << push_button << std::endl;
    return {};
}

//method to validate the configuration, checking the types of the values
bool ConfigManager::validateConfig(const LvJSON& doc) {
    try {
        // Validate gpio_type
        LvJSON::checkType(doc, "gpio_type", LvJSON::Object);

        // Validate camera_config
        LvJSON::checkType(doc, "camera_config", LvJSON::Array);

        const LvJSON::Value& cameras = doc["camera_config"];
        for (LvJSON::SizeType i = 0; i < cameras.Size(); i++) {
            LvJSON::checkType(cameras[i], "ip_address", LvJSON::String);
            LvJSON::checkType(cameras[i], "status_gpio_type", LvJSON::Int);
            LvJSON::checkType(cameras[i], "status_gpio_pin", LvJSON::Int);
            LvJSON::checkType(cameras[i], "Demand", LvJSON::Array);

            const LvJSON::Value& demands = cameras[i]["Demand"];
            for (LvJSON::SizeType j = 0; j < demands.Size(); j++) {
                LvJSON::checkType(demands[j], "detect_loop", LvJSON::Int);
                LvJSON::checkType(demands[j], "count_loop", LvJSON::Int);
                LvJSON::checkType(demands[j], "gpio_type", LvJSON::Int);
                LvJSON::checkType(demands[j], "gpio_pin", LvJSON::Int);
                LvJSON::checkType(demands[j], "hold_time", LvJSON::Int);
            }
        }

        // Validate AC_in_config
        LvJSON::checkType(doc, "AC_in_config", LvJSON::Array);
        const LvJSON::Value& acconfigs = doc["AC_in_config"];
        for (LvJSON::SizeType i = 0; i < acconfigs.Size(); i++) {
            LvJSON::checkType(acconfigs[i], "phase", LvJSON::Int);
            LvJSON::checkType(acconfigs[i], "red_gpio_pin", LvJSON::Int);
            LvJSON::checkType(acconfigs[i], "green_gpio_pin", LvJSON::Int);
        }

        // Validate DC_in_config
        LvJSON::checkType(doc, "DC_in_config", LvJSON::Array);
        const LvJSON::Value& dcconfigs = doc["DC_in_config"];
        for (LvJSON::SizeType i = 0; i < dcconfigs.Size(); i++) {
            LvJSON::checkType(dcconfigs[i], "push_button", LvJSON::Int);
            LvJSON::checkType(dcconfigs[i], "gpio_pin", LvJSON::Int);
        }
    } catch (const std::string& err) {
        std::cerr << "Validation error: " << err << std::endl;
        return false;
    }

    return true;
}

//method to check if an IP address is valid, using regex
bool ConfigManager::isValidIPAddress(const std::string& ip) {
    std::regex ipPattern(
        R"((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))");
    std::smatch match;
    if (std::regex_match(ip, match, ipPattern)) {
        for (int i = 1; i <= 4; ++i) {
            int octet = std::stoi(match[i].str());
            if (octet < 0 || octet > 255) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// Method to create a default configuration file
void ConfigManager::createDefaultConfig(const std::string& configFilePath) {
    LvJSON doc;
    auto& allocator = doc.GetAllocator();

    // Initialize the root as an object
    doc.SetObject();

    // Create gpio_type section
    LvJSON::Value gpioType(rapidjson::kObjectType);
    gpioType.AddMember("on_board_gpio", 2, allocator);
    gpioType.AddMember("io_expander_20", 0, allocator);
    gpioType.AddMember("io_expander_21", 1, allocator);
    doc.AddMember("gpio_type", gpioType, allocator);

    // Create camera_config section
    LvJSON::Value cameraConfig(rapidjson::kArrayType);
    for (int i = 1; i <= 4; i++) {
        LvJSON::Value cam(rapidjson::kObjectType);

        // Adding IP address using std::string and allocator
        std::string ipAddress = "192.168.10." + std::to_string(i);
        rapidjson::Value ipValue;
        ipValue.SetString(ipAddress.c_str(), allocator);
        cam.AddMember("ip_address", ipValue, allocator);
        
        // Add default status values
        cam.AddMember("status_gpio_type", 2, allocator);
        cam.AddMember("status_gpio_pin", 0, allocator);

        // Add default demand settings
        LvJSON::Value demands(rapidjson::kArrayType);
        for (int j = 1; j <= 2; j++) {
            LvJSON::Value demand(rapidjson::kObjectType);
            demand.AddMember("detect_loop", j, allocator);
            demand.AddMember("count_loop", 3, allocator);
            demand.AddMember("gpio_type", 0, allocator);
            demand.AddMember("gpio_pin", 0, allocator);
            demand.AddMember("hold_time", 5000, allocator);
            demands.PushBack(demand, allocator);
        }
        cam.AddMember("Demand", demands, allocator);
        cameraConfig.PushBack(cam, allocator);
    }
    doc.AddMember("camera_config", cameraConfig, allocator);

    // Create AC_in_config section
    LvJSON::Value acConfig(rapidjson::kArrayType);
    for (int i = 1; i <= 4; i++) {
        LvJSON::Value ac(rapidjson::kObjectType);
        ac.AddMember("phase", i, allocator);
        ac.AddMember("red_gpio_pin", (i - 1) * 2, allocator);
        ac.AddMember("green_gpio_pin", (i - 1) * 2 + 1, allocator);
        acConfig.PushBack(ac, allocator);
    }
    doc.AddMember("AC_in_config", acConfig, allocator);

    // Create DC_in_config section
    LvJSON::Value dcConfig(rapidjson::kArrayType);
    for (int i = 1; i <= 4; i++) {
        LvJSON::Value dc(rapidjson::kObjectType);
        dc.AddMember("push_button", i, allocator);
        dc.AddMember("gpio_pin", i - 1, allocator);
        dcConfig.PushBack(dc, allocator);
    }
    doc.AddMember("DC_in_config", dcConfig, allocator);

    // Write the default configuration to a file
    std::ofstream ofs(configFilePath);
    if (ofs.is_open()) {
        ofs << doc.stringify_pretty();  // Assuming LvJSON has this method
        ofs.close();
        std::cout << "Default configuration file created: " << configFilePath << std::endl;
    } else {
        std::cerr << "Failed to create default configuration file: " << configFilePath << std::endl;
    }
}