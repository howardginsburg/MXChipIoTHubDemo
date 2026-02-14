# MXChip AZ3166 Azure IoT Hub Demo

A pure MQTT implementation for connecting the MXChip AZ3166 IoT DevKit to Azure IoT Hub. No Azure SDK required - just PubSubClient and direct MQTT communication.

## Features

- **Device-to-Cloud (D2C)**: Send telemetry from all onboard sensors (temperature, humidity, pressure, accelerometer, gyroscope, magnetometer) via SensorManager
- **Cloud-to-Device (C2D)**: Receive messages from IoT Hub
- **Device Twin**: Get/update reported properties, receive desired property changes
- **Visual Status**: Azure LED, User LED, and RGB LED indicate WiFi and MQTT connection status
- **OLED Display**: Shows connection status and sensor readings
- **DeviceConfig**: WiFi credentials and IoT Hub connection string stored in EEPROM, configurable via serial CLI

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- MXChip AZ3166 IoT DevKit
- Azure IoT Hub with a registered device

## Quick Start

### 1. Clone the repository

### 2. Configure the device

#### Option A: Web Interface (Recommended)

1. Hold **Button B** while pressing the **Reset** button to enter AP mode
2. Connect your computer to the WiFi network displayed on the OLED screen
3. Open a browser and navigate to `http://192.168.0.1/`
4. Enter your WiFi credentials and IoT Hub connection string
5. Save and reset the device

#### Option B: Serial CLI

1. Connect via serial terminal (115200 baud)
2. Press **Enter** to access the CLI
3. Run the following commands:

```
set_wifi <ssid> <password>
set_az_iothub <connection_string>
exit
```

The connection string can be found in Azure Portal: IoT Hub → Devices → Your Device → Primary Connection String.

### 3. Build and upload

```bash
pio run --target upload --target monitor
```

Or use the PlatformIO IDE buttons in VS Code.

## Project Structure

```
├── platformio.ini            # Build configuration and connection profile
src/
├── main.cpp              # Application code (callbacks, telemetry, setup/loop)
├── config.h              # Timing configuration (telemetry interval)
├── config.sample.txt     # Template configuration
├── azure_iot_mqtt.h      # Azure IoT MQTT library header
└── azure_iot_mqtt.cpp    # Azure IoT MQTT library implementation
```

## Configuration

| Setting | Location | Description |
|---------|----------|-------------|
| Connection profile | `platformio.ini` build_flags | `-DCONNECTION_PROFILE=PROFILE_IOTHUB_SAS` |
| WiFi credentials | EEPROM (via web interface or serial CLI) | Web: AP mode at `192.168.0.1` / CLI: `set_wifi <ssid> <password>` |
| IoT Hub connection string | EEPROM (via web interface or serial CLI) | Web: AP mode at `192.168.0.1` / CLI: `set_az_iothub <connection_string>` |
| Telemetry interval | `config.h` | `TELEMETRY_INTERVAL` (default 10000ms) |
| Protocol settings | `azure_iot_mqtt.h` | API version, port, SAS duration, root certificate |

## Telemetry Data

All onboard sensors are read via the framework's `SensorManager` and sent as JSON:

```json
{
  "temperature": 25.30,
  "humidity": 45.20,
  "pressure": 1013.25,
  "accelerometer": { "x": 10, "y": -5, "z": 980 },
  "gyroscope": { "x": 100, "y": -200, "z": 50 },
  "magnetometer": { "x": 300, "y": -100, "z": 500 }
}
```

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
| Azure LED ON | Connected to IoT Hub via MQTT |
| Azure LED OFF | Not connected to IoT Hub |
| User LED ON | WiFi and MQTT both connected |
| User LED OFF | WiFi or MQTT disconnected |
| RGB LED Red | WiFi not connected |
| RGB LED Yellow | WiFi connected, MQTT not connected |
| RGB LED Off | Fully connected |

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