# MXChip AZ3166 Azure IoT Hub Demo

A pure MQTT implementation for connecting the MXChip AZ3166 IoT DevKit to Azure IoT Hub. No Azure SDK required - just PubSubClient and direct MQTT communication.

## Features

- **Device-to-Cloud (D2C)**: Send telemetry data (temperature, humidity) to IoT Hub
- **Cloud-to-Device (C2D)**: Receive messages from IoT Hub
- **Device Twin**: Get/update reported properties, receive desired property changes
- **Visual Status**: Azure LED indicates MQTT connection status
- **OLED Display**: Shows connection status and sensor readings

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- MXChip AZ3166 IoT DevKit
- Azure IoT Hub with a registered device

## Quick Start

### 1. Clone the repository

### 2. Configure credentials

```bash
cp src/config.sample.txt src/config.h
```

Edit `src/config.h` with your settings:

```cpp
#define WIFI_SSID           "your-wifi-ssid"
#define WIFI_PASSWORD       "your-wifi-password"
#define IOT_CONNECTION_STRING "HostName=your-hub.azure-devices.net;DeviceId=your-device;SharedAccessKey=your-key"
```

### 3. Build and upload

```bash
pio run --target upload --target monitor
```

Or use the PlatformIO IDE buttons in VS Code.

## Project Structure

```
src/
├── main.cpp              # Application code (callbacks, telemetry, setup/loop)
├── config.h              # Your configuration (git-ignored)
├── config.sample.txt     # Template configuration
├── azure_iot_mqtt.h      # Azure IoT MQTT library header
└── azure_iot_mqtt.cpp    # Azure IoT MQTT library implementation
```

## Configuration

| File | Purpose |
|------|---------|
| `config.h` | WiFi credentials, IoT Hub connection string, telemetry interval |
| `azure_iot_mqtt.h` | Protocol settings (API version, port, SAS duration), root certificate |

## Azure CLI Commands

### Send C2D Message

```bash
az iot device c2d-message send \
  --hub-name YOUR_HUB \
  --device-id YOUR_DEVICE \
  --data "Hello from cloud!"
```

### Update Desired Properties

```bash
az iot hub device-twin update \
  --hub-name YOUR_HUB \
  --device-id YOUR_DEVICE \
  --desired '{"ledState": true, "telemetryInterval": 5}'
```

### View Device Twin

```bash
az iot hub device-twin show \
  --hub-name YOUR_HUB \
  --device-id YOUR_DEVICE
```

## API Reference

### Initialization

```cpp
azureIoTInit();                    // Initialize library (after WiFi connected)
azureIoTConnect();                 // Connect to IoT Hub
azureIoTIsConnected();             // Check connection status
azureIoTLoop();                    // Process messages (call in loop())
```

### Callbacks

```cpp
azureIoTSetC2DCallback(callback);              // C2D messages
azureIoTSetDesiredPropertiesCallback(callback); // Desired property updates
azureIoTSetTwinReceivedCallback(callback);      // Full twin received
```

### Telemetry & Twin

```cpp
azureIoTSendTelemetry(payload, properties);     // Send D2C telemetry
azureIoTRequestTwin();                          // Request full twin
azureIoTUpdateReportedProperties(json);         // Update reported properties
```

### Accessors

```cpp
azureIoTGetDeviceId();    // Get device ID from connection string
azureIoTGetHostname();    // Get IoT Hub hostname from connection string
```

## MQTT Topics Reference

| Feature | Topic Pattern |
|---------|---------------|
| D2C Telemetry | `devices/{deviceId}/messages/events/{properties}` |
| C2D Messages | `devices/{deviceId}/messages/devicebound/#` |
| Twin GET | `$iothub/twin/GET/?$rid={rid}` |
| Twin Response | `$iothub/twin/res/{status}/?$rid={rid}` |
| Twin PATCH | `$iothub/twin/PATCH/properties/reported/?$rid={rid}` |
| Desired Updates | `$iothub/twin/PATCH/properties/desired/#` |

## LED Indicators

| LED | Meaning |
|-----|---------|
| Azure LED ON | Connected to IoT Hub |
| Azure LED OFF | Disconnected |
| WiFi LED | Managed by firmware |

## Troubleshooting

### TLS Connection Failed
- Check WiFi connection
- Verify IoT Hub hostname in connection string
- Ensure device is registered in IoT Hub

### MQTT Connection Failed (state -1)
- Check SAS token expiry
- Verify SharedAccessKey is correct
- Check if NTP time sync succeeded

### No Telemetry in IoT Hub
- Verify connection string includes correct DeviceId
- Check Serial Monitor for error messages
- Use Azure CLI to monitor: `az iot hub monitor-events --hub-name YOUR_HUB`

## License

MIT