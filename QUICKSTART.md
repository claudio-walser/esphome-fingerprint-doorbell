# ESPHome Package - Quick Start Guide

This directory contains everything you need to run your Fingerprint Doorbell with ESPHome and Home Assistant.

## 📁 What's Inside

```
esphome-package/
├── components/                          # Custom ESPHome component
│   └── fingerprint_doorbell/
│       ├── __init__.py                  # Component registration
│       ├── fingerprint_doorbell.h       # C++ header
│       ├── fingerprint_doorbell.cpp     # Core logic
│       ├── sensor.py                    # Numeric sensors
│       ├── text_sensor.py               # Text sensors
│       └── binary_sensor.py             # Binary sensors
│
├── fingerprint-doorbell.yaml            # Main package config (like everythingsmarthome)
├── example-config.yaml                  # Your device config template
├── example-secrets.yaml                 # Secrets template
├── home-assistant-examples.yaml         # HA automation examples
│
├── README.md                            # Full documentation
├── MIGRATION.md                         # Migration guide from original
└── QUICKSTART.md                        # This file
```

## 🚀 5-Minute Setup

### 1. Copy Files to ESPHome

**If using Home Assistant ESPHome add-on:**
```bash
cd /config/esphome
mkdir -p packages components
cp -r /path/to/esphome-package/components/fingerprint_doorbell components/
cp /path/to/esphome-package/fingerprint-doorbell.yaml packages/
```

**If using standalone ESPHome:**
```bash
cd ~/.esphome
mkdir -p packages components
cp -r /path/to/esphome-package/components/fingerprint_doorbell components/
cp /path/to/esphome-package/fingerprint-doorbell.yaml packages/
```

### 2. Create Your Device Config

Create `fingerprint-doorbell.yaml` in your ESPHome config directory:

```yaml
substitutions:
  name: fingerprint-doorbell
  friendly_name: "Front Door"

packages:
  fingerprint_doorbell: !include packages/fingerprint-doorbell.yaml

esphome:
  name: ${name}
  platform: ESP32
  board: esp32doit-devkit-v1

wifi:
  ssid: "YourWiFiSSID"
  password: "YourWiFiPassword"
  ap:
    ssid: "${friendly_name} Fallback"
    password: "12345678"

captive_portal:

api:
  encryption:
    key: "generate-this-key"  # Run: esphome wizard fingerprint-doorbell.yaml

ota:
  password: "your-ota-password"

logger:

web_server:
  port: 80
```

### 3. Flash Your ESP32

**First time (USB cable required):**
```bash
esphome run fingerprint-doorbell.yaml
```

**Future updates (over WiFi):**
```bash
esphome run fingerprint-doorbell.yaml --device fingerprint-doorbell.local
```

### 4. Add to Home Assistant

1. Go to **Settings** → **Devices & Services**
2. ESPHome integration should auto-discover your device
3. Click **Configure** and enter your API encryption key
4. Done! All sensors and services are now available

### 5. Enroll Your First Fingerprint

In Home Assistant:
1. Go to **Developer Tools** → **Services**
2. Select `esphome.fingerprint_doorbell_enroll_fingerprint`
3. Enter:
   ```yaml
   finger_id: 1
   finger_name: "Your Name"
   ```
4. Click **Call Service**
5. Place your finger on the sensor 5 times when the LED flashes purple

## 📊 Available Entities

After setup, you'll have these entities in Home Assistant:

### Sensors
- `sensor.fingerprint_match_id` - ID of matched finger (-1 = no match)
- `sensor.fingerprint_confidence` - Match confidence (0-400)

### Text Sensors
- `text_sensor.fingerprint_match_name` - Name of matched person

### Binary Sensors
- `binary_sensor.doorbell_ring` - Doorbell event (unknown finger)
- `binary_sensor.finger_detected` - Is someone touching the sensor?

## 🛠️ Available Services

- `enroll_fingerprint` - Add new fingerprint
- `delete_fingerprint` - Remove fingerprint
- `delete_all_fingerprints` - Clear all fingerprints
- `rename_fingerprint` - Change fingerprint name
- `set_ignore_touch_ring` - Enable/disable rain protection

## 📱 Example Automation

Auto-unlock door when recognized:

```yaml
automation:
  - alias: "Auto Unlock Front Door"
    trigger:
      - platform: state
        entity_id: sensor.fingerprint_match_id
    condition:
      - condition: numeric_state
        entity_id: sensor.fingerprint_match_id
        above: 0
    action:
      - service: lock.unlock
        target:
          entity_id: lock.front_door
```

## 🔧 Hardware Wiring

| ESP32 Pin | Sensor Pin | Description |
|-----------|------------|-------------|
| GPIO16    | RX         | UART RX     |
| GPIO17    | TX         | UART TX     |
| GPIO5     | Touch      | Touch ring  |
| 3.3V      | VCC        | Power       |
| GND       | GND        | Ground      |
| GPIO19    | -          | Doorbell output (optional) |

⚠️ **Important:** Ground the metal sensor housing to prevent ESD resets!

## 📚 More Information

- **Full documentation:** See `README.md`
- **Migration guide:** See `MIGRATION.md` (if coming from original project)
- **HA examples:** See `home-assistant-examples.yaml`

## ❓ Troubleshooting

**Sensor not found?**
- Check wiring (RX/TX connections)
- Verify sensor has power
- Check logs: `esphome logs fingerprint-doorbell.yaml`

**Device won't connect to WiFi?**
- Device creates fallback AP: Connect to "Front Door Fallback" / password "12345678"
- Update WiFi credentials via captive portal

**Services not appearing in HA?**
- Restart Home Assistant
- Check device is online in ESPHome integration

## 🎯 Next Steps

1. ✅ Set up basic configuration (you just did this!)
2. Enroll family members' fingerprints
3. Create automations for auto-unlock
4. Set up doorbell notifications
5. Add rain protection automation (if outdoor installation)

---

**Need help?** Check `README.md` for detailed documentation or open an issue on GitHub!

**Enjoy your ESPHome-powered Fingerprint Doorbell! 🎉**
