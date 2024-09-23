#include "CommModule.h"

// Constructor to initialize the MQTT server with the provided address
CommModule::CommModule(const std::string& address)
    : mqttServer(address.c_str()) // Initialize LvMQTTServer with the address
{
    std::cout << "CommModule initialized on address: " << address << std::endl;
}

// Method to publish a message to a specified topic
void CommModule::publish(const std::string& topic, const std::string& payload)
{
    if (mqttServer.put(topic, payload) != 0)
    {
        std::cerr << "Error publishing to topic: " << topic << std::endl;
    }
    else
    {
        std::cout << "Published to topic: " << topic << "\n" <<"Payload: " << payload << std::endl;
    }
}

// Method to subscribe to a specific topic
void CommModule::subscribe(const std::string& topic)
{
    mqttServer.interest_topic_add(topic);
    std::cout << "Subscribed to topic: " << topic << std::endl;
}

// Method to process MQTT events
void CommModule::loop(uint64_t now_ms)
{
    mqttServer.loop(now_ms);
}
