# Sensor Connection Fix - February 11, 2026

## Problem

The R503 fingerprint sensor was not responding to `verifyPassword()` calls, showing repeated connection failures:

```
[I] Connecting to fingerprint sensor...
[W] Fingerprint sensor not responding, will retry...
```

## Root Cause

The issue was a **hardware abstraction conflict**:

1. **ESPHome UART Component** was configured to manage GPIO16/17
2. **Arduino Serial2** (used by Adafruit_Fingerprint library) also uses GPIO16/17
3. Our code assigned `Serial2` without calling `begin()`, assuming ESPHome initialized it
4. **Serial2 needs its own `begin()` call** even if ESPHome configures the hardware

## The Original Working Code

Looking at the original PlatformIO implementation (`legacy-src/src/FingerprintManager.cpp`):

```cpp
#define mySerial Serial2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

bool FingerprintManager::connect() {
    // set the data rate for the sensor serial port
    finger.begin(57600);  // This calls Serial2.begin(57600)
    delay(50);
    if (finger.verifyPassword()) {
        Serial.println("Found fingerprint sensor!");
    }
    // ...
}
```

**Key insight**: The original code **only used Serial2**, not ESPHome's UART component.

## The Fix

### Step 1: Initialize Serial2 Explicitly

In `fingerprint_doorbell.cpp` setup():

```cpp
this->hw_serial_ = &Serial2;
this->hw_serial_->begin(57600, SERIAL_8N1, 16, 17);  // Explicit initialization
delay(50);  // Give sensor time to initialize
this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
```

### Step 2: Remove ESPHome UART Dependency

Since we're using Serial2 directly (like the original code), we don't need ESPHome's UART component:

**Before** (`__init__.py`):
```python
DEPENDENCIES = ["uart"]
FingerprintDoorbell = fingerprint_doorbell_ns.class_(
    "FingerprintDoorbell", cg.Component, uart.UARTDevice
)
```

**After**:
```python
# No UART dependency
FingerprintDoorbell = fingerprint_doorbell_ns.class_(
    "FingerprintDoorbell", cg.Component
)
```

**Before** (`fingerprint-doorbell.yaml`):
```yaml
uart:
  id: uart_fingerprint
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 57600

fingerprint_doorbell:
  uart_id: uart_fingerprint
```

**After**:
```yaml
# No uart section needed - Serial2 uses GPIO16/17 by default
fingerprint_doorbell:
  id: fp_doorbell
```

### Step 3: Add Retry Counter

Added connection attempt tracking to help diagnose wiring issues:

```cpp
uint8_t connect_retry_count_{0};

bool FingerprintDoorbell::connect_sensor() {
  ESP_LOGI(TAG, "Connecting to fingerprint sensor (attempt %d)...", 
           this->connect_retry_count_ + 1);
  
  uint8_t result = this->finger_->verifyPassword();
  
  if (result == FINGERPRINT_OK) {
    ESP_LOGI(TAG, "Found fingerprint sensor!");
    this->connect_retry_count_ = 0;
  } else {
    this->connect_retry_count_++;
    if (this->connect_retry_count_ >= 3) {
      ESP_LOGW(TAG, "Sensor still not responding after %d attempts - check wiring and power", 
               this->connect_retry_count_);
    }
    return false;
  }
  // ...
}
```

## Why This Works

1. **Serial2.begin()** properly initializes the Arduino HAL's Stream object
2. **No UART conflict** - ESPHome UART component removed entirely
3. **Matches original architecture** - uses same approach as working PlatformIO code
4. **Explicit pin configuration** - `begin(57600, SERIAL_8N1, 16, 17)` ensures correct pins
5. **50ms delay** - gives sensor time to initialize after serial port opens

## ESP32 UART Details

ESP32 has 3 hardware UARTs:
- **UART0** (Serial): GPIO1/3 - Used for logging/USB
- **UART1** (Serial1): GPIO9/10 - Often used for flash, avoid
- **UART2** (Serial2): GPIO16/17 - **This is what we use**

Serial2 defaults to GPIO16 (RX) and GPIO17 (TX), which is why the original code didn't need explicit pin configuration.

## Testing the Fix

After deploying this fix, you should see:

```
[I] Connecting to fingerprint sensor (attempt 1)...
[I] Found fingerprint sensor!
[I] Fingerprint sensor connected successfully
[I] Sensor capacity: 200
[I] Security level: 5
```

If the sensor still doesn't connect after 3 attempts, check:

1. **Wiring**: 
   - R503 TX → ESP32 GPIO16
   - R503 RX → ESP32 GPIO17
   - VCC/GND properly connected

2. **Power**: R503 needs stable 3.3V or 5V (check datasheet)

3. **Sensor LED**: Should light up if powered correctly

## Commits

- `8aa8e6f`: Fix sensor initialization - call Serial2.begin() explicitly
- `1d8e387`: Remove ESPHome UART dependency - use Serial2 directly

## References

- Original PlatformIO code: `legacy-src/src/FingerprintManager.cpp`
- Adafruit Fingerprint Library: https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library
- ESP32 UART pins: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html
