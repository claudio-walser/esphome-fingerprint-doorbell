# ESPHome Library Dependencies

The ESPHome component requires the Adafruit Fingerprint Sensor Library.

## Automatic Installation

ESPHome should automatically download the required library when you include this in your configuration:

```yaml
esphome:
  name: fingerprint-doorbell
  platform: ESP32
  board: esp32doit-devkit-v1
  libraries:
    - "adafruit/Adafruit Fingerprint Sensor Library@^2.1.0"
```

## If You Get Compilation Errors

If ESPHome can't find the library, you may need to add it to your `platformio_options`:

```yaml
esphome:
  name: fingerprint-doorbell
  platform: ESP32
  board: esp32doit-devkit-v1
  platformio_options:
    lib_deps:
      - adafruit/Adafruit Fingerprint Sensor Library@^2.1.0
```

## Manual Library Installation

If automatic installation fails, you can manually install the library:

1. Open your ESPHome configuration directory
2. Create a `lib/` folder if it doesn't exist
3. Download the library from: https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library
4. Extract to `lib/Adafruit_Fingerprint_Sensor_Library/`

## Library Version

- **Tested with:** v2.1.0
- **Minimum version:** v2.0.0
- **Maximum version:** v2.x.x (should work with any 2.x version)

## Dependencies

The Adafruit Fingerprint library has no additional dependencies beyond Arduino core libraries.
