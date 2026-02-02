/*
 * Azure IoT Hub Sample for MXChip AZ3166
 * Using Pure MQTT (No Azure SDK)
 * 
 * This sample demonstrates:
 * - Device-to-Cloud (D2C) telemetry
 * - Cloud-to-Device (C2D) messages
 * - Device Twin (get, update reported, receive desired)
 * 
 * Configuration: Edit mqtt_config.h
 */

#include <Arduino.h>
#include "AZ3166WiFi.h"
#include "HTS221Sensor.h"
#include "OledDisplay.h"

// Azure IoT MQTT library
#include "azure_iot_mqtt.h"
#include "config.h"

// Azure LED pin (directly next to the WiFi LED on the board)
#define LED_AZURE   LED_BUILTIN

// ===== APPLICATION STATE =====
static bool hasWifi = false;
static int messageCount = 0;
static unsigned long lastTelemetryTime = 0;

// Sensors
static DevI2C* i2c;
static HTS221Sensor* tempHumiditySensor;

// ===== APPLICATION CALLBACKS =====

// Called when a C2D message is received
void onC2DMessage(const char* topic, const char* payload, unsigned int length)
{
    Serial.println("App: C2D message received!");
    Serial.print("  Content: ");
    Serial.println(payload);
    
    // Display on OLED
    Screen.clean();
    Screen.print(0, "C2D Message:");
    Screen.print(1, payload);
    
    // TODO: Add your C2D message handling here
    // Example: Parse JSON commands, trigger actions, etc.
}

// Called when desired properties are updated
void onDesiredProperties(const char* payload, int version)
{
    Serial.println("App: Desired properties updated!");
    Serial.print("  Version: ");
    Serial.println(version);
    Serial.print("  Payload: ");
    Serial.println(payload);
    
    // Display on OLED
    Screen.clean();
    Screen.print(0, "Twin Update!");
    Screen.print(1, "Version:");
    char versionStr[16];
    snprintf(versionStr, sizeof(versionStr), "%d", version);
    Screen.print(2, versionStr);
    
    // TODO: Parse JSON and apply property changes
    // Example: Update telemetry interval, LED state, etc.
    
    // Acknowledge by reporting back the same values
    // This confirms the device received and applied the changes
    // Example: azureIoTUpdateReportedProperties("{\"ledState\":true}");
}

// Called when full twin is received
void onTwinReceived(const char* payload)
{
    Serial.println("App: Full Device Twin received!");
    Serial.println(payload);
    
    Screen.clean();
    Screen.print(0, "Twin Received");
    Screen.print(1, "See Serial");
    
    // TODO: Parse the twin JSON to get initial state
    // The twin contains both "desired" and "reported" sections
}

// ===== WIFI INITIALIZATION =====
void initWiFi()
{
    Screen.print(0, "Connecting WiFi");
    Screen.print(1, WIFI_SSID);
    
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    if (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) == WL_CONNECTED)
    {
        hasWifi = true;
        IPAddress ip = WiFi.localIP();
        
        Serial.print("WiFi connected! IP: ");
        Serial.println(ip);
        
        Screen.print(0, "WiFi Connected");
        Screen.print(1, ip.get_address());
    }
    else
    {
        hasWifi = false;
        Serial.println("WiFi connection failed!");
        Screen.print(0, "WiFi Failed!");
    }
}

// ===== SENSOR INITIALIZATION =====
void initSensors()
{
    Serial.println("Initializing sensors...");
    
    i2c = new DevI2C(D14, D15);
    tempHumiditySensor = new HTS221Sensor(*i2c);
    tempHumiditySensor->init(NULL);
    tempHumiditySensor->enable();
    
    Serial.println("Sensors initialized.");
}

// ===== SEND TELEMETRY =====
void sendTelemetry()
{
    if (!azureIoTIsConnected())
    {
        return;
    }
    
    // Read sensor data
    float temperature = 0;
    float humidity = 0;
    
    tempHumiditySensor->getTemperature(&temperature);
    tempHumiditySensor->getHumidity(&humidity);
    
    // Build JSON payload
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"deviceId\":\"%s\",\"messageId\":%d,\"temperature\":%.2f,\"humidity\":%.2f}",
        azureIoTGetDeviceId(), messageCount++, temperature, humidity);
    
    Serial.print("Sending telemetry: ");
    Serial.println(payload);
    
    // Update display
    char tempStr[32];
    char humidStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.1f C", temperature);
    snprintf(humidStr, sizeof(humidStr), "Humidity: %.1f%%", humidity);
    
    Screen.clean();
    Screen.print(0, "Sending Data...");
    Screen.print(1, tempStr);
    Screen.print(2, humidStr);
    
    // Build message properties (optional)
    const char* props = (temperature > 30) ? "temperatureAlert=true" : NULL;
    
    // Send telemetry
    if (azureIoTSendTelemetry(payload, props))
    {
        Screen.print(3, "Sent OK");
    }
    else
    {
        Screen.print(3, "Send Failed!");
    }
}

// ===== SETUP =====
void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Azure IoT Hub Demo - MXChip AZ3166");
    Serial.println("  Pure MQTT (No Azure SDK)");
    Serial.println("========================================");
    Serial.println();
    
    // Initialize OLED
    Screen.init();
    Screen.print(0, "Azure IoT Demo");
    Screen.print(1, "Initializing...");
    
    // Initialize Azure LED (off until connected)
    pinMode(LED_AZURE, OUTPUT);
    digitalWrite(LED_AZURE, LOW);
    
    // Initialize WiFi
    initWiFi();
    if (!hasWifi)
    {
        Serial.println("Setup failed: No WiFi");
        return;
    }
    delay(1000);
    
    // Initialize sensors
    initSensors();
    delay(500);
    
    // Initialize Azure IoT
    Screen.print(2, "Init IoT Hub...");
    if (!azureIoTInit())
    {
        Serial.println("Setup failed: IoT init failed");
        Screen.print(2, "IoT Init Failed!");
        return;
    }
    
    // Register callbacks
    azureIoTSetC2DCallback(onC2DMessage);
    azureIoTSetDesiredPropertiesCallback(onDesiredProperties);
    azureIoTSetTwinReceivedCallback(onTwinReceived);
    
    // Connect to IoT Hub
    Screen.print(2, "Connecting...");
    if (!azureIoTConnect())
    {
        Serial.println("Setup failed: IoT connection failed");
        Screen.print(2, "Connect Failed!");
        digitalWrite(LED_AZURE, LOW);
        return;
    }
    
    // Connected - turn on Azure LED
    digitalWrite(LED_AZURE, HIGH);
    
    // Setup complete
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Setup complete!");
    Serial.println("  - D2C: Telemetry every 10 sec");
    Serial.println("  - C2D: Listening for messages");
    Serial.println("  - Twin: Enabled");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Azure CLI commands:");
    Serial.println("  C2D: az iot device c2d-message send --hub-name YOUR_HUB --device-id YOUR_DEVICE --data \"Hello!\"");
    Serial.println("  Twin: az iot hub device-twin update --hub-name YOUR_HUB --device-id YOUR_DEVICE --desired '{\"prop\":true}'");
    Serial.println();
    
    Screen.clean();
    Screen.print(0, "Ready!");
    Screen.print(1, "Sending data...");
    
    // Request initial twin
    azureIoTRequestTwin();
    
    // Report initial state
    char reportedJson[128];
    snprintf(reportedJson, sizeof(reportedJson),
        "{\"firmwareVersion\":\"1.0.0\",\"telemetryInterval\":%d,\"deviceStarted\":true}",
        TELEMETRY_INTERVAL / 1000);
    azureIoTUpdateReportedProperties(reportedJson);
    
    lastTelemetryTime = millis();
}

// ===== MAIN LOOP =====
void loop()
{
    // Process Azure IoT messages
    azureIoTLoop();
    
    // Update Azure LED based on connection status
    digitalWrite(LED_AZURE, azureIoTIsConnected() ? HIGH : LOW);
    
    // Send telemetry at regular intervals
    if (azureIoTIsConnected())
    {
        unsigned long now = millis();
        if (now - lastTelemetryTime >= TELEMETRY_INTERVAL)
        {
            sendTelemetry();
            lastTelemetryTime = now;
        }
    }
    
    delay(100);
}
