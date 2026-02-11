# Fingerprint Doorbell - ESPHome Package

Transform your existing Fingerprint Doorbell project into a native ESPHome component with full Home Assistant integration!

## 🎯 What This Package Does

This ESPHome package converts your Arduino-based Fingerprint Doorbell into a native ESPHome component, providing:

- ✅ **Native Home Assistant Integration** - No MQTT needed, direct API integration
- ✅ **Auto-Discovery** - Sensors automatically appear in Home Assistant
- ✅ **Services** - Enroll, delete, and manage fingerprints from Home Assistant
- ✅ **OTA Updates** - Update firmware wirelessly from ESPHome dashboard
- ✅ **Web Interface** - Built-in web server for diagnostics
- ✅ **Full Feature Parity** - All original features preserved

## 📋 Prerequisites

- **ESPHome** installed (via Home Assistant add-on or standalone)
- **Home Assistant** (for full integration)
- **ESP32** board (esp32doit-devkit-v1 or compatible)
- **Grow R503** fingerprint sensor
- Basic knowledge of YAML configuration

## 🚀 Quick Start

### Method 1: Local Package (Recommended for Development)

1. **Copy the package to your ESPHome config directory:**
   ```bash
   cp -r esphome-package/components ~/.esphome/
   cp esphome-package/fingerprint-doorbell.yaml ~/.esphome/packages/
   ```

2. **Create your device configuration:**
   ```yaml
   # fingerprint-doorbell-front.yaml
   substitutions:
     name: fingerprint-doorbell-front
     friendly_name: "Front Door Fingerprint"

   packages:
     fingerprint_doorbell: !include packages/fingerprint-doorbell.yaml

   esphome:
     name: ${name}
     platform: ESP32
     board: esp32doit-devkit-v1

   wifi:
     ssid: !secret wifi_ssid
     password: !secret wifi_password

   api:
     encryption:
       key: !secret api_encryption_key

   ota:
     password: !secret ota_password
   ```

3. **Compile and upload:**
   ```bash
   esphome run fingerprint-doorbell-front.yaml
   ```

### Method 2: GitHub Package (Once Published)

```yaml
substitutions:
  name: fingerprint-doorbell

packages:
  fingerprint_doorbell: github://YOUR_USERNAME/fingerprint-doorbell-esphome/fingerprint-doorbell.yaml@main

esphome:
  name: ${name}
  platform: ESP32
  board: esp32doit-devkit-v1

# ... rest of config
```

## 🔌 Hardware Wiring

Same as the original project:

| ESP32 Pin | R503 Sensor Pin | Description |
|-----------|-----------------|-------------|
| GPIO16    | RX (Yellow)     | UART RX     |
| GPIO17    | TX (White)      | UART TX     |
| GPIO5     | Touch (Blue)    | Touch Ring  |
| 3.3V      | VCC (Red)       | Power       |
| GND       | GND (Black)     | Ground      |
| GPIO19    | -               | Doorbell Output (optional) |

**Important:** Ground the sensor housing to prevent ESD resets!

## 📊 Exposed Entities

### Sensors
- **Fingerprint Match ID** (`sensor.fingerprint_match_id`)
  - ID of matched fingerprint (1-200)
  - `-1` when no match

- **Fingerprint Confidence** (`sensor.fingerprint_confidence`)
  - Match confidence score (1-400, higher is better)
  - `0` when no match

### Text Sensors
- **Fingerprint Match Name** (`text_sensor.fingerprint_match_name`)
  - Name of matched fingerprint
  - Empty when no match

### Binary Sensors
- **Doorbell Ring** (`binary_sensor.doorbell_ring`)
  - `on` when unknown finger detected (doorbell event)
  - Stays `on` for 1 second

- **Finger Detected** (`binary_sensor.finger_detected`)
  - `on` when any finger is on the sensor
  - `off` when no finger present

## 🛠️ Services

All services are available in Home Assistant Developer Tools → Services:

### `esphome.{device_name}_enroll_fingerprint`
Enroll a new fingerprint.

**Parameters:**
- `finger_id` (int): ID 1-200
- `finger_name` (string): Name for this fingerprint

**Example:**
```yaml
service: esphome.fingerprint_doorbell_front_enroll_fingerprint
data:
  finger_id: 1
  finger_name: "John"
```

**Process:**
1. Call the service
2. Place finger on sensor when LED flashes purple
3. Remove finger when LED turns solid purple
4. Repeat 5 times total
5. Enrollment complete!

### `esphome.{device_name}_delete_fingerprint`
Delete a single fingerprint.

**Parameters:**
- `finger_id` (int): ID to delete

**Example:**
```yaml
service: esphome.fingerprint_doorbell_front_delete_fingerprint
data:
  finger_id: 1
```

### `esphome.{device_name}_delete_all_fingerprints`
Delete all enrolled fingerprints.

**Example:**
```yaml
service: esphome.fingerprint_doorbell_front_delete_all_fingerprints
```

### `esphome.{device_name}_rename_fingerprint`
Rename an existing fingerprint.

**Parameters:**
- `finger_id` (int): ID to rename
- `new_name` (string): New name

**Example:**
```yaml
service: esphome.fingerprint_doorbell_front_rename_fingerprint
data:
  finger_id: 1
  new_name: "John Smith"
```

### `esphome.{device_name}_set_ignore_touch_ring`
Enable/disable touch ring (for rain protection).

**Parameters:**
- `state` (bool): `true` to ignore, `false` to enable

**Example:**
```yaml
service: esphome.fingerprint_doorbell_front_set_ignore_touch_ring
data:
  state: true
```

## 🎨 LED Ring Indicators

| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Breathing | Ready (touch ring enabled) |
| Blue | Solid | Ready (touch ring disabled) |
| Red | Flashing | Finger detected, scanning |
| Purple | Flashing | Enrollment mode, waiting for finger |
| Purple | Solid | Match found / enrollment pass complete |
| Red | Solid | Error state |

## 🏡 Home Assistant Integration Examples

### Auto-Unlock Door on Match
```yaml
automation:
  - alias: "Front Door Auto Unlock"
    trigger:
      - platform: state
        entity_id: sensor.fingerprint_match_id
    condition:
      - condition: numeric_state
        entity_id: sensor.fingerprint_match_id
        above: 0
      - condition: numeric_state
        entity_id: sensor.fingerprint_confidence
        above: 150
    action:
      - service: lock.unlock
        target:
          entity_id: lock.front_door
      - service: notify.mobile_app
        data:
          message: "Door unlocked for {{ states('text_sensor.fingerprint_match_name') }}"
```

### Doorbell Notification
```yaml
automation:
  - alias: "Doorbell Ring Notification"
    trigger:
      - platform: state
        entity_id: binary_sensor.doorbell_ring
        to: "on"
    action:
      - service: notify.mobile_app
        data:
          message: "Someone is at the front door"
          title: "Doorbell Ring"
```

### Rain Protection
```yaml
automation:
  - alias: "Fingerprint Rain Protection"
    trigger:
      - platform: state
        entity_id: binary_sensor.rain_sensor
    action:
      - service: esphome.fingerprint_doorbell_front_set_ignore_touch_ring
        data:
          state: "{{ is_state('binary_sensor.rain_sensor', 'on') }}"
```

## ⚙️ Configuration Options

### Override Default Pins
```yaml
fingerprint_doorbell:
  touch_pin: GPIO5       # Default
  doorbell_pin: GPIO19   # Default
  ignore_touch_ring: false  # Set true for rain-exposed sensors
```

### Customize UART Pins
```yaml
uart:
  tx_pin: GPIO17  # Default
  rx_pin: GPIO16  # Default
  baud_rate: 57600  # Do not change!
```

### Customize Sensor Names
```yaml
sensor:
  - platform: fingerprint_doorbell
    match_id:
      name: "Custom Match ID Name"
    confidence:
      name: "Custom Confidence Name"
```

## 🔧 Troubleshooting

### Sensor Not Found
- Check wiring (especially RX/TX - they should NOT be swapped)
- Verify 3.3V power supply
- Ensure sensor housing is grounded
- Check ESPHome logs: `esphome logs fingerprint-doorbell-front.yaml`

### False Doorbell Rings in Rain
- Set `ignore_touch_ring: true` in config, OR
- Use Home Assistant automation to dynamically enable rain protection

### Enrollment Fails
- Ensure finger is clean and dry
- Place finger consistently in the same position
- Don't press too hard or too soft
- Wait for LED to turn solid purple before removing finger

### Compilation Errors
- Ensure you have the Adafruit Fingerprint library:
  ```yaml
  esphome:
    libraries:
      - "adafruit/Adafruit Fingerprint Sensor Library@^2.1.0"
  ```

## 📦 What's Included

```
esphome-package/
├── components/
│   └── fingerprint_doorbell/
│       ├── __init__.py              # Component registration
│       ├── fingerprint_doorbell.h   # C++ header
│       ├── fingerprint_doorbell.cpp # Core implementation
│       ├── sensor.py                # Sensor platform
│       ├── text_sensor.py           # Text sensor platform
│       └── binary_sensor.py         # Binary sensor platform
├── fingerprint-doorbell.yaml        # Main package config
├── example-config.yaml              # User config example
├── example-secrets.yaml             # Secrets template
└── home-assistant-examples.yaml     # HA automation examples
```

## 🆚 Comparison: Original vs ESPHome

| Feature | Original (PlatformIO) | ESPHome Package |
|---------|----------------------|-----------------|
| Home Assistant Integration | MQTT | Native API |
| Configuration | Web UI | YAML + HA Services |
| Updates | Manual/Web OTA | ESPHome Dashboard |
| Fingerprint Management | Web UI | HA Services |
| Logging | Web UI | ESPHome Logs |
| Sensor Data | MQTT Topics | HA Entities |
| Setup Complexity | Medium | Easy |
| Customization | C++ Code | YAML Config |

## 🔄 Migration from Original

If you're migrating from the original PlatformIO project:

1. **Fingerprints are NOT preserved** - You'll need to re-enroll
2. **Settings reset** - Reconfigure via ESPHome YAML
3. **MQTT not needed** - Uses Home Assistant API
4. **Web UI replaced** - Use Home Assistant UI + Services

## 📚 Additional Resources

- [ESPHome Documentation](https://esphome.io)
- [Original Project README](../README.md)
- [Grow R503 Datasheet](https://cdn.shopify.com/s/files/1/0551/3656/5159/files/R503_fingerprint_module_user_manual.pdf)
- [Home Assistant ESPHome Integration](https://www.home-assistant.io/integrations/esphome/)

## 🤝 Contributing

Found a bug or have a feature request? Please open an issue or submit a pull request!

## 📄 License

Same license as the original FingerprintDoorbell project.

## 🙏 Credits

- Original FingerprintDoorbell by frickelzeugs
- ESPHome package conversion by [Your Name]
- Inspired by Everything Smart Home's Everything Presence One

---

**Note:** This is an ESPHome reimplementation. For the original PlatformIO version, see the main project README.
