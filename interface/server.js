const express = require('express');
const mqtt = require('mqtt');
const http = require('http');
const WebSocket = require('ws');

// Create an Express app
const app = express();

// Serve static files (HTML, CSS, JS) from the 'public' directory
app.use(express.static('public'));

// Create an HTTP server
const server = http.createServer(app);

// Create a WebSocket server
const wss = new WebSocket.Server({ server });

// MQTT broker connection
const mqttClient = mqtt.connect('mqtt://192.168.10.101:1883'); // Change to your broker's address if needed

// Subscribe to topics when connected to the broker
mqttClient.on('connect', () => {
    mqttClient.subscribe('Camera_status');
    mqttClient.subscribe('AC_status');
    mqttClient.subscribe('DC_status');
});

// Forward MQTT messages to all connected WebSocket clients
mqttClient.on('message', (topic, message) => {
    console.log(`MQTT Message received. Topic: ${topic}, Message: ${message.toString()}`);  // Log incoming messages
    const data = { topic, message: message.toString() };
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify(data));
        }
    });
});

// Start the server
server.listen(3000, () => {
    console.log('Server is running on http://localhost:3000');
});
