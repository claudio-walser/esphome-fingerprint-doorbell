# Pin Troubleshooting for Error 0x01

## Current Issue

Error code `0x01` = `FINGERPRINT_PACKETRECIEVEERR` means:
- UART is working
- Data is being transmitted
- **But packets are corrupted or garbled**

## Most Common Causes (in order)

### 1. TX/RX Wires Swapped ⚡ MOST LIKELY

**Symptom**: Consistent 0x01 error on every attempt

**Current Test**: Latest code uses **RX=GPIO17, TX=GPIO16** (swapped from default)

**What to try**:
1. Rebuild with latest code (already swapped)
2. If still fails, physically swap the wires:
   - Move wire from GPIO16 to GPIO17
   - Move wire from GPIO17 to GPIO16

### 2. Wrong Default Pins for Your Board

The Wemos D1 Mini32 **might use different Serial2 pins** than standard ESP32.

**Pin configurations to try** (edit `fingerprint_doorbell.cpp` line 30):

```cpp
// Current (swapped):
this->hw_serial_->begin(57600, SERIAL_8N1, 17, 16);  // RX=17, TX=16

// Try standard:
this->hw_serial_->begin(57600, SERIAL_8N1, 16, 17);  // RX=16, TX=17

// Try alternate UART pins:
this->hw_serial_->begin(57600, SERIAL_8N1, 13, 15);  // RX=13, TX=15
this->hw_serial_->begin(57600, SERIAL_8N1, 25, 26);  // RX=25, TX=26
this->hw_serial_->begin(57600, SERIAL_8N1, 32, 33);  // RX=32, TX=33
```

### 3. Baud Rate Mismatch (less likely)

R503 sensors sometimes ship configured to different baud rates.

**Baud rates to try**:
```cpp
this->hw_serial_->begin(9600, SERIAL_8N1, 17, 16);    // Try 9600
this->hw_serial_->begin(19200, SERIAL_8N1, 17, 16);   // Try 19200
this->hw_serial_->begin(115200, SERIAL_8N1, 17, 16);  // Try 115200
```

### 4. Sensor Needs Reset

Some R503 sensors need power cycle after flashing new firmware.

**What to try**:
1. Unplug ESP32 power completely
2. Wait 10 seconds
3. Plug back in
4. Watch boot logs

## Quick Diagnostic: Flash Original Firmware

Since the original PlatformIO code worked once, flashing it will confirm:
- Wiring is correct
- Sensor is functional
- Pin configuration works

### Option A: Install PlatformIO and Flash

```bash
# Install PlatformIO
pip install platformio

# Flash original firmware
cd /var/home/claudio/Development/FingerprintDoorbellOriginal/legacy-src
pio run -t upload --upload-port /dev/ttyUSB0

# Watch serial output
pio device monitor -p /dev/ttyUSB0 -b 115200
```

Look for:
```
Adafruit finger detect test
Found fingerprint sensor!
```

### Option B: Use ESPHome to Flash Different Pin Configs

Edit `components/fingerprint_doorbell/fingerprint_doorbell.cpp` line 30:

**Test 1: Standard pins**
```cpp
this->hw_serial_->begin(57600, SERIAL_8N1, 16, 17);
```

**Test 2: Swapped pins** (current)
```cpp
this->hw_serial_->begin(57600, SERIAL_8N1, 17, 16);
```

**Test 3: Alternate GPIO**
```cpp
this->hw_serial_->begin(57600, SERIAL_8N1, 25, 26);
```

After each change:
1. Commit and push to GitHub
2. Clean build in ESPHome
3. Flash wirelessly
4. Check logs for connection success

## What Each Error Code Means

From Adafruit_Fingerprint.h:

- `0x00` = `FINGERPRINT_OK` ✅ Working!
- **`0x01` = `FINGERPRINT_PACKETRECIEVEERR`** ⚠️ Communication garbled (TX/RX swap or wrong pins)
- `0x02` = `FINGERPRINT_NOFINGER` - No finger detected (normal)
- `0x03` = `FINGERPRINT_IMAGEFAIL` - Failed to capture image
- `0xFF` = Timeout (no response at all - sensor not powered or wrong pins)

## Checking Your Wiring

**Physical verification needed:**

1. **Power**:
   - R503 VCC → ESP32 3.3V or 5V (check sensor datasheet)
   - R503 GND → ESP32 GND
   - Sensor LED should be ON when powered

2. **Data** (this is what we're debugging):
   - R503 TX (yellow wire?) → ESP32 RX (currently GPIO17)
   - R503 RX (white wire?) → ESP32 TX (currently GPIO16)

3. **Common mistakes**:
   - TX→TX and RX→RX (should be crossed: TX→RX)
   - Using 5V for data pins (ESP32 is 3.3V - can damage it)
   - Loose connections

## Next Steps

1. **Quick test**: Rebuild with current swapped-pin code
2. **If still fails**: Try physically swapping the two data wires
3. **If still fails**: Install PlatformIO and flash original firmware to verify hardware
4. **If original works**: Compare exact pin configuration and replicate in ESPHome

## Automated Pin Scanner (Future Enhancement)

Could create a diagnostic mode that tries all pin combinations:

```cpp
int rx_pins[] = {16, 17, 25, 26, 32, 33};
int tx_pins[] = {16, 17, 25, 26, 32, 33};

for (int rx : rx_pins) {
  for (int tx : tx_pins) {
    if (rx == tx) continue;
    Serial2.begin(57600, SERIAL_8N1, rx, tx);
    delay(100);
    if (finger.verifyPassword() == FINGERPRINT_OK) {
      ESP_LOGI(TAG, "SUCCESS! RX=%d, TX=%d", rx, tx);
      return;
    }
    Serial2.end();
  }
}
```

This would automate finding the correct pins but takes ~2 minutes to scan all combinations.
