/*
 * Azure IoT Hub MQTT Library - Implementation
 * Pure MQTT implementation for Azure IoT Hub
 */

#include "azure_iot_mqtt.h"
#include "config.h"

// mbedTLS for SAS token generation
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "SystemTime.h"

// ===== INTERNAL STATE =====
static WiFiClientSecure wifiClient;
static PubSubClient mqttClient(wifiClient);

static bool isInitialized = false;
static bool isConnected = false;

// Parsed connection string values
static char iotHubHostname[128];
static char deviceId[64];
static char deviceKey[64];
static char sasToken[512];

// MQTT topics
static char telemetryTopic[128];
static char c2dTopic[128];
static char mqttUsername[256];

// Device Twin state
static int twinRequestId = 0;
static bool twinGetPending = false;

// Application callbacks
static C2DMessageCallback c2dCallback = NULL;
static DesiredPropertiesCallback desiredPropsCallback = NULL;
static TwinReceivedCallback twinReceivedCallback = NULL;

// ===== INTERNAL FUNCTIONS =====

// URL encode a string
static void urlEncode(const char* input, char* output, size_t outputSize)
{
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < outputSize - 4; i++)
    {
        char c = input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            output[j++] = c;
        }
        else
        {
            snprintf(&output[j], 4, "%%%02X", (unsigned char)c);
            j += 3;
        }
    }
    output[j] = '\0';
}

// Parse the connection string into components
static bool parseConnectionString()
{
    Serial.println("[AzureIoT] Parsing connection string...");
    
    const char* connStr = IOT_CONNECTION_STRING;
    
    // Parse HostName
    const char* hostStart = strstr(connStr, "HostName=");
    if (hostStart == NULL)
    {
        Serial.println("[AzureIoT] Error: HostName not found!");
        return false;
    }
    hostStart += 9;
    const char* hostEnd = strchr(hostStart, ';');
    if (hostEnd == NULL) hostEnd = connStr + strlen(connStr);
    size_t hostLen = hostEnd - hostStart;
    if (hostLen >= sizeof(iotHubHostname))
    {
        Serial.println("[AzureIoT] Error: HostName too long!");
        return false;
    }
    strncpy(iotHubHostname, hostStart, hostLen);
    iotHubHostname[hostLen] = '\0';
    
    // Parse DeviceId
    const char* deviceStart = strstr(connStr, "DeviceId=");
    if (deviceStart == NULL)
    {
        Serial.println("[AzureIoT] Error: DeviceId not found!");
        return false;
    }
    deviceStart += 9;
    const char* deviceEnd = strchr(deviceStart, ';');
    if (deviceEnd == NULL) deviceEnd = connStr + strlen(connStr);
    size_t deviceLen = deviceEnd - deviceStart;
    if (deviceLen >= sizeof(deviceId))
    {
        Serial.println("[AzureIoT] Error: DeviceId too long!");
        return false;
    }
    strncpy(deviceId, deviceStart, deviceLen);
    deviceId[deviceLen] = '\0';
    
    // Parse SharedAccessKey
    const char* keyStart = strstr(connStr, "SharedAccessKey=");
    if (keyStart == NULL)
    {
        Serial.println("[AzureIoT] Error: SharedAccessKey not found!");
        return false;
    }
    keyStart += 16;
    const char* keyEnd = strchr(keyStart, ';');
    if (keyEnd == NULL) keyEnd = connStr + strlen(connStr);
    size_t keyLen = keyEnd - keyStart;
    if (keyLen >= sizeof(deviceKey))
    {
        Serial.println("[AzureIoT] Error: SharedAccessKey too long!");
        return false;
    }
    strncpy(deviceKey, keyStart, keyLen);
    deviceKey[keyLen] = '\0';
    
    Serial.println("[AzureIoT] Connection string parsed:");
    Serial.print("  HostName: ");
    Serial.println(iotHubHostname);
    Serial.print("  DeviceId: ");
    Serial.println(deviceId);
    
    return true;
}

// Generate SAS token
static bool generateSasToken(uint32_t expiryTimeSeconds)
{
    Serial.println("[AzureIoT] Generating SAS token...");
    
    // Build resource URI
    char resourceUri[256];
    snprintf(resourceUri, sizeof(resourceUri), "%s/devices/%s", iotHubHostname, deviceId);
    
    // URL-encode the resource URI
    char encodedUri[256];
    urlEncode(resourceUri, encodedUri, sizeof(encodedUri));
    
    // Build signature string
    char signatureString[512];
    snprintf(signatureString, sizeof(signatureString), "%s\n%lu", encodedUri, (unsigned long)expiryTimeSeconds);
    
    // Decode the base64-encoded device key
    unsigned char decodedKey[64];
    size_t decodedKeyLen = 0;
    int ret = mbedtls_base64_decode(decodedKey, sizeof(decodedKey), &decodedKeyLen,
                                     (const unsigned char*)deviceKey, strlen(deviceKey));
    if (ret != 0)
    {
        Serial.print("[AzureIoT] Failed to decode device key! Error: ");
        Serial.println(ret);
        return false;
    }
    
    // Compute HMAC-SHA256
    unsigned char hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    ret = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    if (ret != 0)
    {
        Serial.println("[AzureIoT] Failed to setup HMAC!");
        mbedtls_md_free(&ctx);
        return false;
    }
    
    mbedtls_md_hmac_starts(&ctx, decodedKey, decodedKeyLen);
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)signatureString, strlen(signatureString));
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);
    
    // Base64-encode the HMAC result
    unsigned char base64Signature[64];
    size_t base64Len = 0;
    ret = mbedtls_base64_encode(base64Signature, sizeof(base64Signature), &base64Len,
                                 hmacResult, sizeof(hmacResult));
    if (ret != 0)
    {
        Serial.println("[AzureIoT] Failed to base64 encode signature!");
        return false;
    }
    base64Signature[base64Len] = '\0';
    
    // URL-encode the signature
    char encodedSignature[128];
    urlEncode((const char*)base64Signature, encodedSignature, sizeof(encodedSignature));
    
    // Build the final SAS token
    snprintf(sasToken, sizeof(sasToken),
        "SharedAccessSignature sr=%s&sig=%s&se=%lu",
        encodedUri, encodedSignature, (unsigned long)expiryTimeSeconds);
    
    Serial.println("[AzureIoT] SAS token generated successfully");
    return true;
}

// Internal MQTT callback - routes messages to application callbacks
static void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    // Copy payload to null-terminated string
    char messageContent[1024];
    unsigned int copyLength = (length < sizeof(messageContent) - 1) ? length : sizeof(messageContent) - 1;
    memcpy(messageContent, payload, copyLength);
    messageContent[copyLength] = '\0';
    
    Serial.println();
    Serial.println("[AzureIoT] ======================================");
    Serial.print("[AzureIoT] Message on: ");
    Serial.println(topic);
    Serial.print("[AzureIoT] Payload (");
    Serial.print(length);
    Serial.println(" bytes)");
    Serial.println("[AzureIoT] ======================================");
    
    // Route: C2D messages
    if (strstr(topic, "/messages/devicebound/") != NULL)
    {
        Serial.println("[AzureIoT] -> C2D Message");
        if (c2dCallback != NULL)
        {
            c2dCallback(topic, messageContent, length);
        }
    }
    // Route: Device Twin Response
    else if (strncmp(topic, "$iothub/twin/res/", 17) == 0)
    {
        // Parse status code
        int status = atoi(topic + 17);
        Serial.print("[AzureIoT] -> Twin Response, status: ");
        Serial.println(status);
        
        if (status == 200 && twinGetPending)
        {
            twinGetPending = false;
            Serial.println("[AzureIoT] Full Device Twin received");
            if (twinReceivedCallback != NULL)
            {
                twinReceivedCallback(messageContent);
            }
        }
        else if (status == 204)
        {
            Serial.println("[AzureIoT] Reported properties accepted");
        }
        else if (status != 200)
        {
            Serial.print("[AzureIoT] Twin operation failed: ");
            Serial.println(status);
        }
    }
    // Route: Desired Property Update
    else if (strncmp(topic, "$iothub/twin/PATCH/properties/desired/", 38) == 0)
    {
        // Parse version
        int version = 0;
        const char* versionStart = strstr(topic, "$version=");
        if (versionStart)
        {
            version = atoi(versionStart + 9);
        }
        
        Serial.print("[AzureIoT] -> Desired Properties, version: ");
        Serial.println(version);
        
        if (desiredPropsCallback != NULL)
        {
            desiredPropsCallback(messageContent, version);
        }
    }
    else
    {
        Serial.println("[AzureIoT] -> Unknown message type");
    }
}

// ===== PUBLIC API IMPLEMENTATION =====

bool azureIoTInit()
{
    Serial.println("[AzureIoT] Initializing...");
    
    // Parse connection string
    if (!parseConnectionString())
    {
        return false;
    }
    
    // Sync time for SAS token
    Serial.println("[AzureIoT] Syncing time via NTP...");
    SyncTime();
    
    uint32_t expiryTime;
    if (IsTimeSynced() == 0)
    {
        time_t epochTime = time(NULL);
        Serial.print("[AzureIoT] Time synced, epoch: ");
        Serial.println((unsigned long)epochTime);
        expiryTime = (uint32_t)epochTime + SAS_TOKEN_DURATION;
    }
    else
    {
        Serial.println("[AzureIoT] NTP failed, using fallback expiry");
        expiryTime = 1738540800; // Feb 3, 2026
    }
    
    // Generate SAS token
    if (!generateSasToken(expiryTime))
    {
        return false;
    }
    
    // Build MQTT username
    snprintf(mqttUsername, sizeof(mqttUsername), 
        "%s/%s/?api-version=%s", 
        iotHubHostname, deviceId, IOT_HUB_API_VERSION);
    
    // Build topics
    snprintf(telemetryTopic, sizeof(telemetryTopic), 
        "devices/%s/messages/events/", deviceId);
    snprintf(c2dTopic, sizeof(c2dTopic), 
        "devices/%s/messages/devicebound/#", deviceId);
    
    Serial.println("[AzureIoT] Configuration:");
    Serial.print("  Username: ");
    Serial.println(mqttUsername);
    Serial.print("  D2C Topic: ");
    Serial.println(telemetryTopic);
    
    // Configure TLS
    Serial.println("[AzureIoT] Configuring TLS...");
    wifiClient.setCACert(AZURE_IOT_ROOT_CA);
    
    // Test TLS connection
    Serial.println("[AzureIoT] Testing TLS connection...");
    if (wifiClient.connect(iotHubHostname, MQTT_PORT))
    {
        Serial.println("[AzureIoT] TLS test successful");
        wifiClient.stop();
        delay(500);
    }
    else
    {
        Serial.println("[AzureIoT] TLS failed, trying insecure...");
        wifiClient.setInsecure();
        if (wifiClient.connect(iotHubHostname, MQTT_PORT))
        {
            Serial.println("[AzureIoT] TLS insecure test successful");
            wifiClient.stop();
            delay(500);
        }
        else
        {
            Serial.println("[AzureIoT] TLS connection failed!");
            return false;
        }
    }
    
    isInitialized = true;
    Serial.println("[AzureIoT] Initialization complete");
    return true;
}

bool azureIoTConnect()
{
    if (!isInitialized)
    {
        Serial.println("[AzureIoT] Not initialized!");
        return false;
    }
    
    Serial.println("[AzureIoT] Connecting to IoT Hub...");
    
    mqttClient.setServer(iotHubHostname, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);
    
    int retries = 0;
    while (!mqttClient.connected() && retries < 5)
    {
        Serial.print("[AzureIoT] Attempt ");
        Serial.println(retries + 1);
        
        if (mqttClient.connect(deviceId, mqttUsername, sasToken))
        {
            isConnected = true;
            Serial.println("[AzureIoT] Connected!");
            
            // Subscribe to topics
            bool subOk = true;
            subOk &= mqttClient.subscribe(c2dTopic);
            subOk &= mqttClient.subscribe("$iothub/twin/res/#");
            subOk &= mqttClient.subscribe("$iothub/twin/PATCH/properties/desired/#");
            
            if (subOk)
            {
                Serial.println("[AzureIoT] Subscribed to all topics");
            }
            else
            {
                Serial.println("[AzureIoT] Warning: Some subscriptions failed");
            }
            
            return true;
        }
        else
        {
            int state = mqttClient.state();
            Serial.print("[AzureIoT] Failed, state: ");
            Serial.println(state);
            retries++;
            delay(3000);
        }
    }
    
    isConnected = false;
    Serial.println("[AzureIoT] Connection failed after retries");
    return false;
}

bool azureIoTIsConnected()
{
    return isConnected && mqttClient.connected();
}

void azureIoTLoop()
{
    if (!isInitialized) return;
    
    if (!mqttClient.connected())
    {
        isConnected = false;
        Serial.println("[AzureIoT] Disconnected, attempting reconnect...");
        azureIoTConnect();
    }
    
    mqttClient.loop();
}

void azureIoTSetC2DCallback(C2DMessageCallback callback)
{
    c2dCallback = callback;
}

void azureIoTSetDesiredPropertiesCallback(DesiredPropertiesCallback callback)
{
    desiredPropsCallback = callback;
}

void azureIoTSetTwinReceivedCallback(TwinReceivedCallback callback)
{
    twinReceivedCallback = callback;
}

bool azureIoTSendTelemetry(const char* payload, const char* properties)
{
    if (!azureIoTIsConnected())
    {
        Serial.println("[AzureIoT] Cannot send: not connected");
        return false;
    }
    
    // Build topic with optional properties
    char topic[256];
    if (properties != NULL && strlen(properties) > 0)
    {
        snprintf(topic, sizeof(topic), "devices/%s/messages/events/%s", deviceId, properties);
    }
    else
    {
        snprintf(topic, sizeof(topic), "devices/%s/messages/events/", deviceId);
    }
    
    bool success = mqttClient.publish(topic, payload);
    if (success)
    {
        Serial.println("[AzureIoT] Telemetry sent");
    }
    else
    {
        Serial.println("[AzureIoT] Telemetry send failed");
    }
    return success;
}

void azureIoTRequestTwin()
{
    if (!azureIoTIsConnected())
    {
        Serial.println("[AzureIoT] Cannot request twin: not connected");
        return;
    }
    
    char topic[64];
    snprintf(topic, sizeof(topic), "$iothub/twin/GET/?$rid=%d", ++twinRequestId);
    
    twinGetPending = true;
    
    if (mqttClient.publish(topic, ""))
    {
        Serial.println("[AzureIoT] Twin GET request sent");
    }
    else
    {
        Serial.println("[AzureIoT] Twin GET request failed");
        twinGetPending = false;
    }
}

void azureIoTUpdateReportedProperties(const char* jsonPayload)
{
    if (!azureIoTIsConnected())
    {
        Serial.println("[AzureIoT] Cannot update reported: not connected");
        return;
    }
    
    char topic[64];
    snprintf(topic, sizeof(topic), 
        "$iothub/twin/PATCH/properties/reported/?$rid=%d", ++twinRequestId);
    
    if (mqttClient.publish(topic, jsonPayload))
    {
        Serial.println("[AzureIoT] Reported properties sent");
    }
    else
    {
        Serial.println("[AzureIoT] Reported properties send failed");
    }
}

const char* azureIoTGetDeviceId()
{
    return deviceId;
}

const char* azureIoTGetHostname()
{
    return iotHubHostname;
}
