# Migration Checklist - Original → ESPHome

## ⚠️ Important Notes Before Migration

- **Fingerprints will NOT be preserved** - The NVS storage format is different
- **You will need to re-enroll all fingerprints** after migration
- **MQTT is replaced** by Home Assistant API (simpler but different)
- **Web UI is removed** - All management via Home Assistant

## 📋 Pre-Migration Checklist

- [ ] Document all enrolled fingerprints (IDs and names)
- [ ] Take photos of current wiring for reference
- [ ] Backup your current firmware (optional but recommended)
- [ ] Install ESPHome (Home Assistant add-on or standalone)
- [ ] Have Home Assistant running and accessible

## 🔧 Migration Steps

### Step 1: Prepare ESPHome Environment

1. **Install ESPHome** (if not already installed)
   - Via Home Assistant: Settings → Add-ons → ESPHome
   - Standalone: `pip install esphome`

2. **Copy package files to ESPHome directory:**
   ```bash
   # If using Home Assistant add-on
   cp -r esphome-package/components /config/esphome/
   cp esphome-package/fingerprint-doorbell.yaml /config/esphome/packages/
   
   # If using standalone ESPHome
   cp -r esphome-package/components ~/.esphome/
   cp esphome-package/fingerprint-doorbell.yaml ~/.esphome/packages/
   ```

### Step 2: Create Device Configuration

1. **Copy example config:**
   ```bash
   cp esphome-package/example-config.yaml /config/esphome/fingerprint-doorbell.yaml
   ```

2. **Edit the configuration:**
   - Update `name` and `friendly_name`
   - Add WiFi credentials to `secrets.yaml`
   - Generate API encryption key: `esphome fingerprint-doorbell.yaml config`
   - Add OTA password

3. **Validate configuration:**
   ```bash
   esphome config fingerprint-doorbell.yaml
   ```

### Step 3: Flash ESP32

**First Time (USB Required):**
```bash
esphome run fingerprint-doorbell.yaml
```

Select your USB port when prompted.

**Future Updates (OTA):**
```bash
esphome run fingerprint-doorbell.yaml --device fingerprint-doorbell.local
```

### Step 4: Verify in Home Assistant

1. Go to Settings → Devices & Services
2. ESPHome integration should show your new device
3. Click "Configure" and enter the API encryption key
4. Device should appear with all sensors

### Step 5: Re-enroll Fingerprints

For each fingerprint you had enrolled:

1. Open Home Assistant → Developer Tools → Services
2. Select `esphome.fingerprint_doorbell_enroll_fingerprint`
3. Enter:
   ```yaml
   finger_id: 1
   finger_name: "John"
   ```
4. Place finger on sensor 5 times when LED flashes purple
5. Repeat for all fingerprints

### Step 6: Set Up Automations

Replace your old MQTT automations with new ones:

**Old MQTT way:**
```yaml
trigger:
  platform: mqtt
  topic: "fingerprintDoorbell/matchId"
```

**New ESPHome way:**
```yaml
trigger:
  platform: state
  entity_id: sensor.fingerprint_match_id
```

See `home-assistant-examples.yaml` for complete examples.

## 🔄 Feature Mapping

| Original Feature | ESPHome Equivalent |
|------------------|-------------------|
| MQTT Topic `matchId` | Entity `sensor.fingerprint_match_id` |
| MQTT Topic `matchName` | Entity `text_sensor.fingerprint_match_name` |
| MQTT Topic `matchConfidence` | Entity `sensor.fingerprint_confidence` |
| MQTT Topic `ring` | Entity `binary_sensor.doorbell_ring` |
| MQTT Topic `ignoreTouchRing` | Service `set_ignore_touch_ring` |
| Web UI Enrollment | Service `enroll_fingerprint` |
| Web UI Delete | Service `delete_fingerprint` |
| Web UI Rename | Service `rename_fingerprint` |
| Web UI Settings | ESPHome YAML config |
| Web OTA Update | ESPHome Dashboard OTA |
| NTP Server | `time.homeassistant` component |
| WiFi Config AP | ESPHome fallback AP |

## 🧪 Testing Checklist

After migration, verify:

- [ ] Device shows up in Home Assistant
- [ ] All sensors are updating
- [ ] Fingerprint enrollment works
- [ ] Known fingerprints are recognized
- [ ] Unknown fingers trigger doorbell
- [ ] LED ring shows correct colors
- [ ] Services work from Home Assistant
- [ ] OTA updates work
- [ ] Logs are accessible

## 🐛 Common Migration Issues

### Device Won't Connect to WiFi
**Solution:** Check `secrets.yaml` WiFi credentials are correct

### Sensor Not Found
**Solution:** 
- Verify wiring hasn't changed
- Check ESPHome logs: `esphome logs fingerprint-doorbell.yaml`
- Ensure sensor has power and is grounded

### Services Not Appearing
**Solution:**
- Restart Home Assistant
- Check ESPHome device is "Online" in HA
- Verify API encryption key is correct

### Enrollment Fails
**Solution:**
- Check ESPHome logs for errors
- Ensure finger is clean and dry
- Try increasing timeout in component if needed

### Old MQTT Topics Still Publishing
**Solution:**
- Ensure old firmware is not running
- Clear MQTT retained messages
- Restart MQTT broker if needed

## 📝 Configuration Comparison

### Old PlatformIO (`platformio.ini`)
```ini
[env:esp32doit-devkit-v1]
platform = espressif32@3.5.0
board = esp32doit-devkit-v1
framework = arduino
lib_deps = 
    ESP Async WebServer
    AsyncElegantOTA
    PubSubClient
    Adafruit Fingerprint Sensor Library
```

### New ESPHome (`fingerprint-doorbell.yaml`)
```yaml
esphome:
  name: fingerprint-doorbell
  platform: ESP32
  board: esp32doit-devkit-v1
  libraries:
    - "adafruit/Adafruit Fingerprint Sensor Library@^2.1.0"

packages:
  fingerprint_doorbell: !include packages/fingerprint-doorbell.yaml
```

## 🎯 Post-Migration Benefits

After successful migration, you gain:

✅ **Simpler Configuration** - YAML instead of web UI + code  
✅ **Better Integration** - Native HA entities vs MQTT  
✅ **Easier Updates** - ESPHome dashboard vs manual upload  
✅ **Better Logging** - Real-time logs in ESPHome  
✅ **Auto-Discovery** - No manual MQTT entity creation  
✅ **Service Calls** - Manage fingerprints from HA UI  
✅ **Version Control** - Config in YAML (Git-friendly)  

## 🆘 Need Help?

If you encounter issues during migration:

1. Check ESPHome logs: `esphome logs fingerprint-doorbell.yaml`
2. Check Home Assistant logs: Settings → System → Logs
3. Review the troubleshooting section in README.md
4. Open an issue on GitHub with logs and config

## ⏮️ Rolling Back

If you need to go back to the original firmware:

1. Flash original firmware via PlatformIO
2. Re-configure via web UI at http://fingerprintdoorbell
3. Re-enroll fingerprints
4. Re-configure MQTT settings

Your original firmware is still available in the `src/` directory.

---

**Ready to migrate?** Start with Step 1 above! 🚀
