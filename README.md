# MXChip AZ3166 Azure IoT Hub Demo

A pure MQTT implementation for connecting the MXChip AZ3166 IoT DevKit to Azure IoT Hub. No Azure SDK required - uses the framework's built-in AzureIoT library for direct MQTT communication.

## Features

- **Multiple Connection Profiles**: IoT Hub SAS, DPS Symmetric Key (individual & group enrollment), DPS X.509 Certificate
- **Device-to-Cloud (D2C)**: Send telemetry from all onboard sensors (temperature, humidity, pressure, accelerometer, gyroscope, magnetometer) via SensorManager
- **Cloud-to-Device (C2D)**: Receive messages and device twin updates from IoT Hub
- **Visual Status**: LED indicators for WiFi and MQTT connection status; OLED display for readings
- **DeviceConfig**: All connection settings stored in EEPROM, configurable via web interface or serial CLI

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- MXChip AZ3166 IoT DevKit
- Azure IoT Hub with a registered device, or Azure DPS enrollment

## Connection Profiles

Select a connection profile at build time via PlatformIO environments:

| Profile | Environment | Build Flag | Auth Method | Settings |
|---------|-------------|------------|-------------|----------|
| IoT Hub SAS | `iothub_sas` | `PROFILE_IOTHUB_SAS` | SAS token | WiFi, IoT Hub connection string |
| IoT Hub X.509 | `iothub_cert` | `PROFILE_IOTHUB_CERT` | X.509 certificate | WiFi, IoT Hub connection string, device certificate |
| DPS Symmetric Key | `dps_sas` | `PROFILE_DPS_SAS` | Individual symmetric key | WiFi, DPS endpoint, Scope ID, Registration ID, symmetric key |
| DPS Group SAS | `dps_sas_group` | `PROFILE_DPS_SAS_GROUP` | Group symmetric key | WiFi, DPS endpoint, Scope ID, Registration ID, group master key |
| DPS X.509 | `dps_cert` | `PROFILE_DPS_CERT` | X.509 certificate | WiFi, DPS endpoint, Scope ID, Registration ID, device certificate |

Profile selection is compile-time via `-DCONNECTION_PROFILE=PROFILE_X` in `platformio.ini`. The build flag is global, so `#if CONNECTION_PROFILE` guards work in both project and framework code.

## Quick Start

### 1. Clone the repository

### 2. Build and upload

```bash
# Pick your profile and build + upload + open serial monitor
pio run -e iothub_sas --target upload --target monitor
pio run -e iothub_cert --target upload --target monitor
pio run -e dps_sas --target upload --target monitor
pio run -e dps_sas_group --target upload --target monitor
pio run -e dps_cert --target upload --target monitor
```

Or use the PlatformIO IDE buttons in VS Code (select the environment from the status bar).

### 3. Configure the device

#### Web Interface (Recommended)

The web interface is the preferred way to configure the device. It automatically shows only the settings relevant to the active profile.

1. Hold **Button B** while pressing the **Reset** button to enter AP mode
2. Connect your computer to the WiFi network displayed on the OLED screen
3. Open a browser and navigate to `http://192.168.0.1/`
4. Enter your WiFi credentials and connection settings (see the [Settings](#connection-profiles) column in the profiles table)
5. Click **Save Configuration** and reset the device

#### Serial CLI (Alternative)

You can also configure the device via the serial console (115200 baud). Press **Enter** to access the CLI, then use the commands below.

| Command | Description | Profiles |
|---------|-------------|----------|
| `set_wifi <ssid> <password>` | WiFi credentials | All |
| `set_az_iothub <connection_string>` | IoT Hub connection string | `iothub_sas`, `iothub_cert` |
| `set_devicecert <pem>` | Device certificate + private key (PEM) | `iothub_cert`, `dps_cert` |
| `set_dps_endpoint <endpoint>` | DPS global endpoint | DPS profiles |
| `set_scopeid <scope_id>` | DPS ID Scope | DPS profiles |
| `set_regid <registration_id>` | DPS registration ID | DPS profiles |
| `set_symkey <key>` | Symmetric key (individual or group master key) | `dps_sas`, `dps_sas_group` |

Type `exit` to leave the CLI and reboot.

> **Tip**: For the DPS group profile (`dps_sas_group`), the symmetric key should be the **group enrollment master key** from Azure Portal → DPS → Enrollment Groups → your group → Primary Key. The device derives its own key at runtime.

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

```bash
# Send a Cloud-to-Device message
az iot device c2d-message send \
  --hub-name YOUR_HUB --device-id YOUR_DEVICE --data "Hello from cloud!"

# Update desired properties
az iot hub device-twin update \
  --hub-name YOUR_HUB --device-id YOUR_DEVICE \
  --desired '{"ledState": true, "telemetryInterval": 5}'

# View device twin
az iot hub device-twin show \
  --hub-name YOUR_HUB --device-id YOUR_DEVICE

# Monitor telemetry
az iot hub monitor-events --hub-name YOUR_HUB
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
azureIoTGetDeviceId();    // Get device ID
azureIoTGetHostname();    // Get IoT Hub hostname
```

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

| Symptom | Things to check |
|---------|----------------|
| TLS connection failed | WiFi connection, IoT Hub hostname, device registration |
| MQTT state -1 | SAS token expiry, SharedAccessKey, NTP time sync |
| MQTT state 5 (unauthorized) | Key buffer truncation — ensure firmware is up to date |
| No telemetry in IoT Hub | DeviceId in connection string, Serial Monitor for errors |

## Architecture

### Project Structure

```
platformio.ini              # Build environments and shared settings
src/
└── main.cpp                # Application code (callbacks, telemetry, setup/loop)
```

The project contains only application code. All Azure IoT logic lives in the framework's AzureIoT library.

### Framework Library: AzureIoT

Located at `libraries/AzureIoT/src/` in the [framework](https://github.com/howardginsburg/framework-arduinostm32mxchip):

| Module | Purpose |
|--------|---------|
| `AzureIoTHub.h/.cpp` | Public API — init, connect, telemetry, twin, callbacks. Profile-specific init via `#if CONNECTION_PROFILE` guards. |
| `AzureIoTDPS.h/.cpp` | DPS registration state machine over MQTT. Creates a temporary PubSubClient, connects to DPS, polls for assignment. |
| `AzureIoTCrypto.h/.cpp` | SAS token generation, HMAC-SHA256, URL encoding, group key derivation. Uses mbedtls. |
| `AzureIoTConfig.h` | Protocol constants and DigiCert Global Root G2 CA certificate. |

PubSubClient (Nick O'Leary, MIT) is bundled in the framework at `libraries/PubSubClient/src/` — no external `lib_deps` needed.

### Init Flow by Profile

**IoT Hub SAS** — Load connection string → parse HostName/DeviceId/SharedAccessKey → NTP sync → generate SAS token → connect

**IoT Hub X.509** — Load connection string → parse HostName/DeviceId → load device cert + key → configure mTLS → connect (no password)

**DPS SAS Individual** — Load DPS settings + symmetric key → NTP sync → generate DPS SAS token → register with DPS → get assigned hub → generate IoT Hub SAS token → connect

**DPS SAS Group** — Load DPS settings + group master key → derive device key via HMAC-SHA256 → NTP sync → generate DPS SAS token → register with DPS → generate IoT Hub SAS token → connect

**DPS X.509** — Load DPS settings + device cert → configure mTLS → register with DPS (no SAS token) → get assigned hub → connect with mTLS

### Design Decisions

- **Framework library over project files**: Azure IoT logic is reusable across projects; projects only contain application code
- **Compile-time profile selection** (`#if CONNECTION_PROFILE`): Saves RAM on constrained device (256KB) by only allocating buffers for the active profile
- **MQTT for DPS** over HTTP REST: Reuses PubSubClient, avoids writing a raw HTTP client
- **Minimal JSON parsing** (strstr-based): No JSON library dependency; sufficient for DPS responses
- **Temporary PubSubClient for DPS**: Keeps DPS state isolated from the IoT Hub connection

## License

MIT