# 🧪 Testing Guide - Fingerprint Doorbell ESPHome Package

## Choose Your Testing Method

### ✅ Option 1: GitHub Testing (RECOMMENDED for containers)
Test directly from GitHub - perfect for ESPHome in Docker/Podman containers

### Option 2: Local Testing
For development or when you have local ESPHome installation

---

## Option 1: GitHub Testing (Start Here!)

### Step 1: Create ESPHome Configuration

In your ESPHome dashboard, create a new device configuration file:

```yaml
substitutions:
  name: fingerprint-doorbell
  friendly_name: "Front Door Fingerprint"

# GitHub package reference
packages:
  fingerprint_doorbell: github://claudio-walser/esphome-fingerprint-doorbell/fingerprint-doorbell.yaml@main

esphome:
  name: ${name}
  friendly_name: ${friendly_name}
  platformio_options:
    platform: espressif32@6.4.0

esp32:
  board: esp32doit-devkit-v1
  framework:
    type: arduino

# WiFi configuration
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  
  # Fallback hotspot if WiFi fails
  ap:
    ssid: "${friendly_name} Fallback"
    password: "12345678"

captive_portal:

# Enable Home Assistant API
api:
  encryption:
    key: !secret api_encryption_key

# Enable OTA updates
ota:
  - platform: esphome
    password: !secret ota_password

# Optional: Web server for diagnostics
web_server:
  port: 80
```

### Step 2: Configure Secrets

Edit your `secrets.yaml` file:
```yaml
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"
api_encryption_key: "generate_via_esphome_dashboard"
ota_password: "your_ota_password"
```

### Step 3: Validate Configuration

1. Click **"Validate"** in ESPHome dashboard
2. ESPHome will download the package from GitHub automatically
3. Check for compilation errors

### Step 4: First Flash (USB Required)

1. Connect ESP32 via USB to your server/machine
2. Click **"Install"** → **"Plug into this computer"**
3. Wait for compilation (~5-10 minutes first time)
4. After success, future updates are wireless!

### Step 5: Verify in Home Assistant

1. Settings → Devices & Services
2. ESPHome should auto-discover the device
3. Configure with API encryption key
4. Verify 5 entities appear:
   - sensor.fingerprint_match_id
   - sensor.fingerprint_confidence
   - text_sensor.fingerprint_match_name
   - binary_sensor.doorbell_ring
   - binary_sensor.finger_detected

### Step 6: Test & Enroll

See testing steps in "Common Testing Steps" section below.

---

## Option 2: Local Testing

### Step 1: Install to ESPHome Directory

**Automatic installation (recommended):**
```bash
cd esphome-package
./install-local.sh
```

**Manual installation:**
```bash
# If using Home Assistant ESPHome add-on:
cp -r esphome-package/components/fingerprint_doorbell /config/esphome/components/
cp esphome-package/fingerprint-doorbell.yaml /config/esphome/packages/
cp esphome-package/example-config.yaml /config/esphome/fingerprint-doorbell.yaml

# If using standalone ESPHome:
cp -r esphome-package/components/fingerprint_doorbell ~/.esphome/components/
cp esphome-package/fingerprint-doorbell.yaml ~/.esphome/packages/
cp esphome-package/example-config.yaml ~/.esphome/fingerprint-doorbell.yaml
```

### Step 2: Edit Configuration

Open your ESPHome dashboard and edit `fingerprint-doorbell.yaml`:

```yaml
substitutions:
  name: fingerprint-doorbell
  friendly_name: "Front Door Fingerprint"

# Local package reference (note the path!)
packages:
  fingerprint_doorbell: !include packages/fingerprint-doorbell.yaml

esphome:
  name: ${name}
  platform: ESP32
  board: esp32doit-devkit-v1

# Your WiFi credentials
wifi:
  ssid: "YourWiFiSSID"
  password: "YourWiFiPassword"
  
  # Fallback hotspot if WiFi fails
  ap:
    ssid: "${friendly_name} Fallback"
    password: "12345678"

captive_portal:

# Home Assistant API
api:
  encryption:
    key: "GENERATE_THIS_KEY"  # Generate with: esphome wizard

# OTA updates
ota:
  password: "your-ota-password"

# Logging
logger:
  level: DEBUG

# Optional: Web server for diagnostics
web_server:
  port: 80
```

### Step 3: Generate API Encryption Key

In ESPHome dashboard, click **"Install"** → **"Manual download"** → ESPHome will generate the key for you.

Or via command line:
```bash
esphome wizard fingerprint-doorbell.yaml
```

### Step 4: Validate Configuration

In ESPHome dashboard, click **"Validate"** to check for errors.

Or via command line:
```bash
esphome config fingerprint-doorbell.yaml
```

### Step 5: Compile and Flash

**First time (USB cable required):**
1. Connect ESP32 via USB
2. Click **"Install"** in ESPHome dashboard
3. Select **"Plug into this computer"**
4. Choose the USB port
5. Wait for compilation and upload (~5 minutes first time)

**Or via command line:**
```bash
esphome run fingerprint-doorbell.yaml
```

### Step 6: Verify in Home Assistant

1. Go to **Settings** → **Devices & Services**
2. ESPHome integration should show a new device discovered
3. Click **"Configure"**
4. Enter the API encryption key
5. Device appears with all sensors!

### Step 7: Test Basic Functionality

1. Check logs in ESPHome dashboard - should show "Fingerprint sensor connected"
2. Place finger on sensor - LED should flash red
3. Check Home Assistant - `binary_sensor.finger_detected` should turn ON

### Step 8: Enroll First Fingerprint

In Home Assistant:
1. Go to **Developer Tools** → **Services**
2. Select `esphome.fingerprint_doorbell_enroll_fingerprint`
3. Enter:
   ```yaml
   finger_id: 1
   finger_name: "Test User"
   ```
4. Click **"Call Service"**
5. Watch the sensor LED - it will flash purple
6. Place your finger on the sensor when LED flashes
7. Remove finger when LED turns solid purple
8. Repeat 5 times total

### Step 9: Test Fingerprint Recognition

1. Place enrolled finger on sensor
2. Check Home Assistant entities:
   - `sensor.fingerprint_match_id` should show `1`
   - `text_sensor.fingerprint_match_name` should show `"Test User"`
   - `sensor.fingerprint_confidence` should show a value (higher = better)

### Step 10: Test Doorbell Function

1. Place an **un-enrolled** finger on the sensor
2. `binary_sensor.doorbell_ring` should turn ON for 1 second
3. GPIO19 should trigger (if you have something connected)

---

---

## Common Testing Steps

### Enroll First Fingerprint

In Home Assistant:
1. Developer Tools → Services
2. Select `esphome.fingerprint_doorbell_enroll_fingerprint`
3. Enter:
   ```yaml
   finger_id: 1
   finger_name: "Test User"
   ```
4. Watch LED flash purple
5. Place finger 5 times when prompted

### Test Recognition

1. Place enrolled finger on sensor
2. Verify entities update:
   - match_id = 1
   - match_name = "Test User"
   - confidence > 100

### Test Doorbell

1. Place unknown finger
2. Verify `doorbell_ring` triggers for 1 second

---

## 🐛 Troubleshooting

### "Component fingerprint_doorbell not found"

**Solution:** Check the `external_components` path in `fingerprint-doorbell.yaml`

For GitHub package:
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/claudio-walser/esphome-fingerprint-doorbell
      ref: main
    components: [ fingerprint_doorbell ]
```

For local development:
```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ fingerprint_doorbell ]
```

### "Library Adafruit Fingerprint not found"

**Solution:** Add to your config (should be in package already):
```yaml
esphome:
  libraries:
    - "adafruit/Adafruit Fingerprint Sensor Library@^2.1.0"
```

### "Sensor not connecting"

**Check ESPHome logs:**
```bash
esphome logs fingerprint-doorbell.yaml
```

Look for:
- "Found fingerprint sensor!" = Good!
- "Did not find fingerprint sensor" = Check wiring

**Common issues:**
- RX/TX swapped
- Wrong voltage (must be 3.3V)
- Sensor not grounded
- Wrong UART pins in config

### Compilation errors about Serial2

**Solution:** The package uses Serial2 (GPIO16/17). If your board doesn't have it, modify in `fingerprint-doorbell.yaml`:
```yaml
uart:
  id: uart_fingerprint
  tx_pin: GPIO17  # Change if needed
  rx_pin: GPIO16  # Change if needed
  baud_rate: 57600  # DON'T change this!
```

### Services not appearing in Home Assistant

**Solution:**
1. Restart Home Assistant
2. Check device is "Online" in ESPHome integration
3. Check API encryption key matches
4. Check ESPHome logs for connection errors

---

## 📊 Verification Checklist

After installation, verify:

- [ ] ESPHome compiles without errors
- [ ] ESP32 connects to WiFi
- [ ] Device appears in Home Assistant
- [ ] Sensor connects (check logs: "Found fingerprint sensor!")
- [ ] LED ring shows correct colors (blue = ready)
- [ ] All 5 entities appear in HA:
  - [ ] `sensor.fingerprint_match_id`
  - [ ] `sensor.fingerprint_confidence`
  - [ ] `text_sensor.fingerprint_match_name`
  - [ ] `binary_sensor.doorbell_ring`
  - [ ] `binary_sensor.finger_detected`
- [ ] All 5 services available:
  - [ ] `enroll_fingerprint`
  - [ ] `delete_fingerprint`
  - [ ] `delete_all_fingerprints`
  - [ ] `rename_fingerprint`
  - [ ] `set_ignore_touch_ring`
- [ ] Finger detection works (LED flashes red)
- [ ] Enrollment works (5-pass process)
- [ ] Recognition works (match ID/name appear)
- [ ] Doorbell works (unknown finger triggers ring)
- [ ] OTA updates work

---

## 📝 Quick Testing Commands

```bash
# Validate config
esphome config fingerprint-doorbell.yaml

# Compile only (no upload)
esphome compile fingerprint-doorbell.yaml

# Upload via USB
esphome run fingerprint-doorbell.yaml

# Upload via WiFi (after first flash)
esphome upload fingerprint-doorbell.yaml --device fingerprint-doorbell.local

# View logs
esphome logs fingerprint-doorbell.yaml

# View logs via WiFi
esphome logs fingerprint-doorbell.yaml --device fingerprint-doorbell.local

# Clean build files
esphome clean fingerprint-doorbell.yaml
```

---

## 🎯 Recommended Testing Flow

1. ✅ **Local install** → Validate config works
2. ✅ **Compile** → Check for errors
3. ✅ **Flash** → First upload via USB
4. ✅ **Verify in HA** → Device auto-discovered
5. ✅ **Test sensors** → Finger detection works
6. ✅ **Enroll** → Service call works
7. ✅ **Recognition** → Match detection works
8. ✅ **Doorbell** → Unknown finger triggers ring
9. ✅ **OTA** → Wireless update works
10. ✅ **GitHub** → Publish for others (optional)

---

## 💡 Tips

- **Use ESPHome logs heavily** - they show everything happening
- **Start with DEBUG logging** - change to INFO later for production
- **Test one feature at a time** - don't rush
- **Keep USB cable handy** - for recovery if WiFi fails
- **Document your GPIO pins** - if you customize them
- **Backup your config** - before making changes

---

## 🆘 Still Having Issues?

1. Check ESPHome logs: `esphome logs fingerprint-doorbell.yaml`
2. Check Home Assistant logs: Settings → System → Logs
3. Verify wiring matches the diagram in README.md
4. Try the original PlatformIO version to verify hardware works
5. Open an issue with:
   - Your config file (remove secrets!)
   - ESPHome logs
   - Hardware setup details

---

**Ready to test? Start with Option 1 (Local Testing)!** 🚀
