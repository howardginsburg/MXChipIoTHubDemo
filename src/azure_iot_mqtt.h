/*
 * Azure IoT Hub MQTT Library
 * Pure MQTT implementation for Azure IoT Hub
 * 
 * Handles:
 * - Connection string loading from EEPROM (via DeviceConfig)
 * - SAS token generation
 * - MQTT connection management
 * - Device Twin operations
 * - Message routing
 * 
 * Configuration is loaded from EEPROM using the DeviceConfig API.
 * Use the serial CLI to configure WiFi and IoT Hub connection string.
 */

#ifndef AZURE_IOT_MQTT_H
#define AZURE_IOT_MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "AZ3166WiFi.h"

// ===== AZURE IOT HUB PROTOCOL SETTINGS =====
// Azure IoT Hub API version
#define IOT_HUB_API_VERSION "2021-04-12"

// MQTT port for Azure IoT Hub (TLS)
#define MQTT_PORT           8883

// SAS token validity duration (24 hours in seconds)
#define SAS_TOKEN_DURATION  86400

// ===== AZURE IOT HUB ROOT CERTIFICATE =====
// DigiCert Global Root G2 - Azure IoT Hub root certificate
// Valid until: January 15, 2038
static const char AZURE_IOT_ROOT_CA[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
"1Yl9PMCcit652T4Vs5rHh5zhQVrBdPZBp9NOZGerGm5HaDgcqQ3L2jTPNsONq6vL\n"
"HOgszJEzY5d2LO7D+VQ8qf9w1fUfx4ztcdL0Y5Bx7ey/ZL/OB0d9m0K5SH5Rp4gf\n"
"qyeHeSnYLJwHJG/NPawNl/WPtjplVp2B8l4hy2aVpv8XNNP/9KlIjN8C4yKp9hsj\n"
"p+mD9LKuGCBiIIXBu7K2UVT/yWJmM6g9jZJDLf3uXMiPcOq6BNFuPaH7t7bP3MxW\n"
"3WF5+VGPYtM8k+8W3dKhpGnlB8KdvO7ItGp4PysVIxbGNfyXFCy4h6PTY7NxJVma\n"
"lJM=\n"
"-----END CERTIFICATE-----\n";

// ===== CALLBACK TYPES =====
// Application registers these to receive messages

// Called when a C2D message is received
// topic: full MQTT topic (contains message properties)
// payload: message content
// length: payload length
typedef void (*C2DMessageCallback)(const char* topic, const char* payload, unsigned int length);

// Called when desired properties are updated
// payload: JSON with desired property changes
// version: desired properties version
typedef void (*DesiredPropertiesCallback)(const char* payload, int version);

// Called when full twin is received (response to GET)
// payload: full twin JSON document
typedef void (*TwinReceivedCallback)(const char* payload);

// ===== INITIALIZATION =====

// Initialize the Azure IoT MQTT library
// Must be called after WiFi is connected
// Returns true on success
bool azureIoTInit();

// Connect to Azure IoT Hub via MQTT
// Returns true if connected successfully
bool azureIoTConnect();

// Check if connected to IoT Hub
bool azureIoTIsConnected();

// Must be called in loop() to process MQTT messages
void azureIoTLoop();

// ===== CALLBACKS =====

// Register callback for C2D messages
void azureIoTSetC2DCallback(C2DMessageCallback callback);

// Register callback for desired property updates
void azureIoTSetDesiredPropertiesCallback(DesiredPropertiesCallback callback);

// Register callback for twin GET response
void azureIoTSetTwinReceivedCallback(TwinReceivedCallback callback);

// ===== TELEMETRY (D2C) =====

// Send telemetry message
// payload: JSON string to send
// properties: optional URL-encoded properties (e.g., "prop1=value1&prop2=value2"), can be NULL
// Returns true on success
bool azureIoTSendTelemetry(const char* payload, const char* properties);

// ===== DEVICE TWIN =====

// Request full device twin (response via TwinReceivedCallback)
void azureIoTRequestTwin();

// Update reported properties
// jsonPayload: JSON object with properties to update (e.g., "{\"prop\":\"value\"}")
void azureIoTUpdateReportedProperties(const char* jsonPayload);

// ===== ACCESSORS =====

// Get the device ID (parsed from connection string)
const char* azureIoTGetDeviceId();

// Get the IoT Hub hostname (parsed from connection string)
const char* azureIoTGetHostname();

#endif // AZURE_IOT_MQTT_H
