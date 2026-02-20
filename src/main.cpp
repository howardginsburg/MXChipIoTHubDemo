/*
 * Azure IoT Hub Sample for MXChip AZ3166
 * Using Pure MQTT (No Azure SDK)
 * 
 * This sample demonstrates:
 * - Device-to-Cloud (D2C) telemetry (all sensors via SensorManager)
 * - Cloud-to-Device (C2D) messages
 * - Device Twin (get, update reported, receive desired)
 * 
 * Configuration is loaded from EEPROM using DeviceConfig.
 * Sensor data is collected via the SensorManager framework API.
 * Use the serial CLI to configure WiFi and IoT Hub connection string.
 */

#include <Arduino.h>
#include "AZ3166WiFi.h"
#include "OledDisplay.h"
#include "SensorManager.h"
#include "RGB_LED.h"

// Azure IoT library (framework)
#include "AzureIoTHub.h"
#include "DeviceConfig.h"

// Azure LED pin (directly next to the WiFi LED on the board)
#define LED_AZURE   LED_BUILTIN

// ===== APPLICATION STATE =====
static bool hasWifi = false;
static bool hasMqtt = false;
static int messageCount = 0;
static unsigned long lastTelemetryTime = 0;
static RGB_LED rgbLed;

/**
 * Update OLED display
 */
void updateDisplay(const char* line1, const char* line2 = NULL, const char* line3 = NULL)
{
    Screen.clean();
    Screen.print(0, line1);
    if (line2) Screen.print(1, line2);
    if (line3) Screen.print(2, line3);
}

/**
 * Update LEDs based on connection status
 */
void updateLEDs()
{
    digitalWrite(LED_AZURE, hasMqtt ? HIGH : LOW);
    digitalWrite(LED_USER, (hasWifi && hasMqtt) ? HIGH : LOW);
    
    if (!hasWifi)
        rgbLed.setRed();
    else if (!hasMqtt)
        rgbLed.setYellow();
    else
        rgbLed.turnOff();
}

// ===== APPLICATION CALLBACKS =====

// Called when a C2D message is received
void onC2DMessage(const char* topic, const char* payload, unsigned int length)
{
    Serial.println("App: C2D message received!");
    Serial.print("  Content: ");
    Serial.println(payload);
    
    updateDisplay("C2D Message:", payload);
    
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
    
    char versionStr[16];
    snprintf(versionStr, sizeof(versionStr), "%d", version);
    updateDisplay("Twin Update!", "Version:", versionStr);
    
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
    
    updateDisplay("Twin Received", "See Serial");
    
    // TODO: Parse the twin JSON to get initial state
    // The twin contains both "desired" and "reported" sections
}

// ===== WIFI INITIALIZATION =====
void initWiFi()
{
    updateDisplay("Connecting WiFi");
    
    Serial.println("Connecting to WiFi (credentials from EEPROM)...");
    
    // WiFi.begin() with no parameters reads credentials from EEPROM
    if (WiFi.begin() == WL_CONNECTED)
    {
        hasWifi = true;
        IPAddress ip = WiFi.localIP();
        
        Serial.print("WiFi connected! IP: ");
        Serial.println(ip);
        
        updateDisplay("WiFi Connected", ip.get_address());
    }
    else
    {
        hasWifi = false;
        Serial.println("WiFi connection failed!");
        Serial.println("Use the serial CLI to configure:");
        Serial.println("  set_wifi <ssid> <password>");
        updateDisplay("WiFi Failed!", "Use serial CLI");
    }
}

// ===== SEND TELEMETRY =====
void sendTelemetry()
{
    if (!hasMqtt)
    {
        return;
    }
    
    // Build payload: sensor JSON with messageId/deviceId/timestamp prepended
    messageCount++;
    char sensorJson[512];
    if (!Sensors.toJson(sensorJson, sizeof(sensorJson))) return;

    // Get ISO 8601 timestamp
    char timestamp[25];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    // Build final payload with messageId, deviceId, timestamp, and all sensor data
    char payload[700];
    snprintf(payload, sizeof(payload),
        "{\"messageId\":%d,\"deviceId\":\"%s\",\"timestamp\":\"%s\",%s",
        messageCount, azureIoTGetDeviceId(), timestamp, sensorJson + 1);
    
    Serial.print("Sending telemetry: ");
    Serial.println(payload);
    
    // Update display with key values
    float temp = Sensors.getTemperature();
    float hum = Sensors.getHumidity();
    float press = Sensors.getPressure();
    
    char tempStr[32];
    char humidStr[32];
    char pressStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.1f C", temp);
    snprintf(humidStr, sizeof(humidStr), "Humidity: %.1f%%", hum);
    snprintf(pressStr, sizeof(pressStr), "Press: %.1f hPa", press);
    
    updateDisplay(tempStr, humidStr, pressStr);
    
    // Build message properties (optional)
    const char* props = (temp > 30) ? "temperatureAlert=true" : NULL;
    
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
    Serial.print("  Profile: ");
    Serial.println(DeviceConfig_GetProfileName());
    Serial.println("========================================");
    Serial.println();
    
    // Initialize OLED
    Screen.init();
    updateDisplay("Azure IoT Demo", "Initializing...");
    
    // Initialize Azure LED (off until connected)
    pinMode(LED_AZURE, OUTPUT);
    digitalWrite(LED_AZURE, LOW);
    
    // Initialize WiFi (credentials from EEPROM)
    initWiFi();
    if (!hasWifi)
    {
        Serial.println("Setup failed: No WiFi");
        return;
    }
    delay(1000);
    
    // SensorManager is auto-initialized by the framework
    Serial.println("Sensors ready (via SensorManager)");
    
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
        hasMqtt = false;
        updateLEDs();
        return;
    }
    
    hasMqtt = true;
    updateLEDs();
    
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
    
    updateDisplay("Ready!", "Sending data...");
    
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
    
    // Update connection status and LEDs
    hasMqtt = azureIoTIsConnected();
    updateLEDs();
    
    // Send telemetry at regular intervals
    if (hasMqtt)
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
