#ifndef COMM_MODULE_H
#define COMM_MODULE_H

#include "LvMQTTServer.h"
#include <string>
#include <iostream>

class CommModule
{
public:
    // Constructor to initialize the MQTT server
    CommModule(const std::string& address);

    // Method to publish a message to a topic
    void publish(const std::string& topic, const std::string& payload);

    // Method to subscribe to a topic
    void subscribe(const std::string& topic);

    // Method to process MQTT events
    void loop(uint64_t now_ms);

private:
    LvMQTTServer mqttServer;
};

#endif // COMM_MODULE_H
