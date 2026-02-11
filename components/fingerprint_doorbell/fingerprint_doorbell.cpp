#include "fingerprint_doorbell.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace fingerprint_doorbell {

static const char *const TAG = "fingerprint_doorbell";

void FingerprintDoorbell::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Fingerprint Doorbell...");

  // Setup pins
  if (this->touch_pin_ != nullptr) {
    this->touch_pin_->setup();
    this->touch_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);
  }

  if (this->doorbell_pin_ != nullptr) {
    this->doorbell_pin_->setup();
    this->doorbell_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->doorbell_pin_->digital_write(false);
  }

  // AUTO-SCAN MODE: Try to find correct TX/RX pins automatically
  ESP_LOGI(TAG, "Starting automatic pin detection for R503 sensor...");
  
  // Common ESP32 pin combinations for UART
  const int rx_candidates[] = {16, 17, 25, 26, 32, 33};
  const int tx_candidates[] = {16, 17, 25, 26, 32, 33};
  
  bool found = false;
  
  for (int rx : rx_candidates) {
    for (int tx : tx_candidates) {
      if (rx == tx) continue;  // Can't use same pin for RX and TX
      
      ESP_LOGD(TAG, "Testing RX=GPIO%d, TX=GPIO%d...", rx, tx);
      
      // Try this pin combination
      Serial2.begin(57600, SERIAL_8N1, rx, tx);
      delay(100);  // Give time to initialize
      
      // Create temporary Adafruit_Fingerprint object
      Adafruit_Fingerprint test_finger(&Serial2);
      
      // Try to verify password
      uint8_t result = test_finger.verifyPassword();
      
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "✓ SUCCESS! Found working pins: RX=GPIO%d, TX=GPIO%d", rx, tx);
        this->hw_serial_ = &Serial2;
        this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
        found = true;
        break;
      } else {
        ESP_LOGV(TAG, "  Failed with error 0x%02X", result);
        Serial2.end();  // Clean up for next attempt
        delay(50);
      }
    }
    if (found) break;
  }
  
  if (!found) {
    ESP_LOGE(TAG, "✗ Could not find working pin combination!");
    ESP_LOGE(TAG, "Please check: 1) Sensor power 2) Wiring 3) Sensor functionality");
    // Initialize anyway with default pins for manual troubleshooting
    Serial2.begin(57600, SERIAL_8N1, 16, 17);
    this->hw_serial_ = &Serial2;
    this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
  }
  
  this->sensor_connected_ = false;
  ESP_LOGI(TAG, "Fingerprint sensor will connect in first loop iteration");
}

void FingerprintDoorbell::loop() {
  ESP_LOGD(TAG, ">>> loop() START sensor_connected=%d", this->sensor_connected_);
  
  // Try to connect sensor (throttle to once per 5 seconds)
  if (!this->sensor_connected_) {
    ESP_LOGD(TAG, ">>> Not connected, checking throttle");
    uint32_t now = millis();
    if (now - this->last_connect_attempt_ >= 5000) {
      ESP_LOGD(TAG, ">>> Attempting connection");
      this->last_connect_attempt_ = now;
      this->sensor_connected_ = this->connect_sensor();
      if (this->sensor_connected_) {
        ESP_LOGI(TAG, "Fingerprint sensor connected successfully");
        this->load_fingerprint_names();
        ESP_LOGD(TAG, ">>> After load_fingerprint_names");
        // TODO: LEDcontrol blocks for 10+ seconds - needs to be fixed
        // this->set_led_ring_ready();
        ESP_LOGD(TAG, ">>> Skipped set_led_ring_ready (blocks watchdog)");
        yield();  // Feed watchdog after sensor operations
        ESP_LOGD(TAG, ">>> After yield, falling through to scan");
      } else {
        ESP_LOGD(TAG, ">>> Connection failed, returning");
        return;  // Only return if connection failed
      }
    } else {
      ESP_LOGD(TAG, ">>> Throttling, returning");
      return;  // Only return if not time to retry yet
    }
  }

  // Scan for fingerprints - TEMPORARILY DISABLED TO ALLOW BOOT
  ESP_LOGD(TAG, ">>> Fingerprint scanning disabled to allow boot");
  yield();  // Feed watchdog
  return;  // Skip scanning entirely
  
  #if 0 // Temporarily disabled
  ESP_LOGD(TAG, ">>> About to scan");
  yield();  // Feed watchdog
  ESP_LOGD(TAG, ">>> Calling scan_fingerprint()");
  Match match = this->scan_fingerprint();
  ESP_LOGD(TAG, ">>> scan_fingerprint() returned");

  // Handle match found
  if (match.scan_result == ScanResult::MATCH_FOUND) {
    ESP_LOGI(TAG, "Match found: ID=%d, Name=%s, Confidence=%d", 
             match.match_id, match.match_name.c_str(), match.match_confidence);
    
    if (this->match_id_sensor_ != nullptr) {
      this->match_id_sensor_->publish_state(match.match_id);
    }
    if (this->confidence_sensor_ != nullptr) {
      this->confidence_sensor_->publish_state(match.match_confidence);
    }
    if (this->match_name_sensor_ != nullptr) {
      this->match_name_sensor_->publish_state(match.match_name);
    }
    
    this->last_match_time_ = millis();
    this->last_match_id_ = match.match_id;
    
    // Clear after 3 seconds
    this->set_timeout(3000, [this]() {
      if (this->match_id_sensor_ != nullptr) {
        this->match_id_sensor_->publish_state(-1);
      }
      if (this->match_name_sensor_ != nullptr) {
        this->match_name_sensor_->publish_state("");
      }
      if (this->confidence_sensor_ != nullptr) {
        this->confidence_sensor_->publish_state(0);
      }
    });
  }
  
  // Handle no match (doorbell ring)
  else if (match.scan_result == ScanResult::NO_MATCH_FOUND) {
    ESP_LOGI(TAG, "No match found - doorbell ring!");
    
    if (this->ring_sensor_ != nullptr) {
      this->ring_sensor_->publish_state(true);
    }
    
    // Trigger doorbell output pin
    if (this->doorbell_pin_ != nullptr) {
      this->doorbell_pin_->digital_write(true);
    }
    
    this->last_ring_time_ = millis();
    
    // Clear ring state after 1 second
    this->set_timeout(1000, [this]() {
      if (this->ring_sensor_ != nullptr) {
        this->ring_sensor_->publish_state(false);
      }
      if (this->doorbell_pin_ != nullptr) {
        this->doorbell_pin_->digital_write(false);
      }
    });
  }

  // Update finger detection binary sensor
  if (this->finger_sensor_ != nullptr) {
    bool finger_present = (match.scan_result != ScanResult::NO_FINGER);
    this->finger_sensor_->publish_state(finger_present);
  }
  #endif // Temporarily disabled
}

void FingerprintDoorbell::dump_config() {
  ESP_LOGCONFIG(TAG, "Fingerprint Doorbell:");
  LOG_PIN("  Touch Pin: ", this->touch_pin_);
  LOG_PIN("  Doorbell Pin: ", this->doorbell_pin_);
  ESP_LOGCONFIG(TAG, "  Ignore Touch Ring: %s", YESNO(this->ignore_touch_ring_));
  ESP_LOGCONFIG(TAG, "  Sensor Connected: %s", YESNO(this->sensor_connected_));
  
  if (this->sensor_connected_ && this->finger_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Sensor Capacity: %d", this->finger_->capacity);
    ESP_LOGCONFIG(TAG, "  Enrolled Templates: %d", this->finger_->templateCount);
  }
}

bool FingerprintDoorbell::connect_sensor() {
  ESP_LOGI(TAG, "Connecting to fingerprint sensor (attempt %d)...", this->connect_retry_count_ + 1);
  
  // Try to verify password
  uint8_t result = this->finger_->verifyPassword();
  
  if (result == FINGERPRINT_OK) {
    ESP_LOGI(TAG, "Found fingerprint sensor!");
    this->connect_retry_count_ = 0;  // Reset counter on success
  } else {
    ESP_LOGW(TAG, "Fingerprint sensor not responding (error code: 0x%02X)", result);
    
    // Original code waits longer on first attempt, then retries
    // After 2-3 attempts with 5-second spacing, sensor should respond if working
    this->connect_retry_count_++;
    
    if (this->connect_retry_count_ >= 3) {
      ESP_LOGW(TAG, "Sensor still not responding after %d attempts - check wiring and power", this->connect_retry_count_);
    }
    
    return false;
  }

  // Flash LED to indicate connection
  this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 0);

  // Get sensor parameters
  this->finger_->getParameters();
  ESP_LOGI(TAG, "Sensor capacity: %d", this->finger_->capacity);
  ESP_LOGI(TAG, "Security level: %d", this->finger_->security_level);
  
  this->finger_->getTemplateCount();
  ESP_LOGI(TAG, "Sensor contains %d templates", this->finger_->templateCount);

  return true;
}

Match FingerprintDoorbell::scan_fingerprint() {
  Match match;
  match.scan_result = ScanResult::NO_FINGER;

  if (!this->sensor_connected_) {
    match.scan_result = ScanResult::ERROR;
    return match;
  }

  // Check touch ring
  bool ring_touched = false;
  if (!this->ignore_touch_ring_) {
    if (this->is_ring_touched()) {
      ring_touched = true;
    }
    
    if (!ring_touched && !this->last_touch_state_) {
      this->update_touch_state(false);
      match.scan_result = ScanResult::NO_FINGER;
      this->scan_state_ = ScanState::IDLE;
      return match;
    }
  }

  // State machine for non-blocking scanning
  switch (this->scan_state_) {
    case ScanState::IDLE: {
      // Start new scan
      this->scan_pass_ = 0;
      this->imaging_attempt_ = 0;
      this->scan_start_time_ = millis();
      this->ring_touched_at_start_ = ring_touched;
      this->scan_state_ = ScanState::WAITING_FOR_FINGER;
      this->update_touch_state(true);
      match.scan_result = ScanResult::NO_FINGER;
      return match;
    }
    
    case ScanState::WAITING_FOR_FINGER: {
      // Timeout check (5 seconds max)
      if (millis() - this->scan_start_time_ > 5000) {
        ESP_LOGW(TAG, "Scan timeout");
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        match.scan_result = ScanResult::NO_MATCH_FOUND;
        return match;
      }
      
      // Non-blocking image capture attempt
      yield();
      uint8_t result = this->finger_->getImage();
      yield();
      
      if (result == FINGERPRINT_OK) {
        // Image captured, move to converting
        this->scan_state_ = ScanState::CONVERTING;
        match.scan_result = ScanResult::NO_FINGER;
        return match;
      } else if (result == FINGERPRINT_NOFINGER) {
        // No finger yet
        this->imaging_attempt_++;
        if (this->ring_touched_at_start_ && this->imaging_attempt_ >= 15) {
          // Ring touched but no finger after max attempts
          this->scan_state_ = ScanState::IDLE;
          this->update_touch_state(false);
          match.scan_result = ScanResult::NO_MATCH_FOUND;
          return match;
        }
        match.scan_result = ScanResult::NO_FINGER;
        return match;
      } else {
        // Error getting image
        ESP_LOGW(TAG, "Error getting image: 0x%02X", result);
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        match.scan_result = ScanResult::ERROR;
        return match;
      }
    }
    
    case ScanState::CONVERTING: {
      // Convert image to features
      yield();
      uint8_t result = this->finger_->image2Tz();
      yield();
      if (result != FINGERPRINT_OK) {
        if (result == FINGERPRINT_IMAGEMESS) {
          ESP_LOGW(TAG, "Image too messy");
        }
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        match.scan_result = ScanResult::ERROR;
        return match;
      }
      
      this->update_touch_state(true);
      this->scan_state_ = ScanState::SEARCHING;
      match.scan_result = ScanResult::NO_FINGER;
      return match;
    }
    
    case ScanState::SEARCHING: {
      // Search for match
      yield();
      uint8_t result = this->finger_->fingerSearch();
      yield();
      
      if (result == FINGERPRINT_OK) {
        // Match found!
        this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
        
        match.scan_result = ScanResult::MATCH_FOUND;
        match.match_id = this->finger_->fingerID;
        match.match_confidence = this->finger_->confidence;
        
        // Get name from storage
        if (this->fingerprint_names_.count(match.match_id) > 0) {
          match.match_name = this->fingerprint_names_[match.match_id];
        } else {
          match.match_name = "ID " + std::to_string(match.match_id);
        }
        
        this->scan_state_ = ScanState::IDLE;
        return match;
        
      } else if (result == FINGERPRINT_NOTFOUND) {
        this->scan_pass_++;
        ESP_LOGD(TAG, "No match found (scan #%d of 5)", this->scan_pass_);
        
        if (this->scan_pass_ < 5) {
          // Try again - reset to waiting for finger
          this->imaging_attempt_ = 0;
          this->scan_state_ = ScanState::WAITING_FOR_FINGER;
          match.scan_result = ScanResult::NO_FINGER;
          return match;
        } else {
          // Max attempts reached
          this->scan_state_ = ScanState::IDLE;
          this->update_touch_state(false);
          match.scan_result = ScanResult::NO_MATCH_FOUND;
          return match;
        }
      } else {
        // Search error
        ESP_LOGW(TAG, "Search error: 0x%02X", result);
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        match.scan_result = ScanResult::ERROR;
        return match;
      }
    }
  }

  return match;
}

void FingerprintDoorbell::update_touch_state(bool touched) {
  if ((touched != this->last_touch_state_) || (this->ignore_touch_ring_ != this->last_ignore_touch_ring_)) {
    if (touched) {
      this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 0);
    } else {
      this->set_led_ring_ready();
    }
  }
  this->last_touch_state_ = touched;
  this->last_ignore_touch_ring_ = this->ignore_touch_ring_;
}

bool FingerprintDoorbell::is_ring_touched() {
  if (this->touch_pin_ != nullptr) {
    return this->touch_pin_->digital_read();
  }
  return false;
}

void FingerprintDoorbell::set_led_ring_ready() {
  if (this->ignore_touch_ring_) {
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  } else {
    this->finger_->LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE);
  }
}

void FingerprintDoorbell::set_led_ring_error() {
  this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
}

bool FingerprintDoorbell::enroll_fingerprint(uint16_t id, const std::string &name) {
  if (!this->sensor_connected_) {
    ESP_LOGE(TAG, "Sensor not connected");
    return false;
  }

  ESP_LOGI(TAG, "Starting enrollment for ID %d (%s)", id, name.c_str());
  
  // 5-pass enrollment process
  for (int pass = 1; pass <= 5; pass++) {
    ESP_LOGI(TAG, "Place finger (pass %d/5)...", pass);
    
    this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_PURPLE, 0);
    
    // Wait for finger
    int timeout = 0;
    while (this->finger_->getImage() != FINGERPRINT_OK) {
      delay(100);
      timeout++;
      if (timeout > 100) {  // 10 second timeout
        ESP_LOGE(TAG, "Enrollment timeout");
        return false;
      }
    }

    // Convert image
    uint8_t result;
    if (pass <= 2) {
      result = this->finger_->image2Tz(pass);
    } else {
      result = this->finger_->image2Tz(1);
      if (result == FINGERPRINT_OK) {
        result = this->finger_->createModel();
        if (result == FINGERPRINT_OK) {
          result = this->finger_->storeModel(id);
        }
      }
    }

    if (result != FINGERPRINT_OK) {
      ESP_LOGE(TAG, "Enrollment failed at pass %d", pass);
      return false;
    }

    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
    
    ESP_LOGI(TAG, "Pass %d complete. Remove finger.", pass);
    
    // Wait for finger removal
    timeout = 0;
    while (this->finger_->getImage() != FINGERPRINT_NOFINGER) {
      delay(100);
      timeout++;
      if (timeout > 50) break;
    }
  }

  // Save name
  this->save_fingerprint_name(id, name);
  
  ESP_LOGI(TAG, "Enrollment successful for ID %d", id);
  this->set_led_ring_ready();
  
  return true;
}

bool FingerprintDoorbell::delete_fingerprint(uint16_t id) {
  if (!this->sensor_connected_) {
    return false;
  }

  uint8_t result = this->finger_->deleteModel(id);
  if (result == FINGERPRINT_OK) {
    this->delete_fingerprint_name(id);
    ESP_LOGI(TAG, "Deleted fingerprint ID %d", id);
    return true;
  }
  
  return false;
}

bool FingerprintDoorbell::delete_all_fingerprints() {
  if (!this->sensor_connected_) {
    return false;
  }

  uint8_t result = this->finger_->emptyDatabase();
  if (result == FINGERPRINT_OK) {
    this->fingerprint_names_.clear();
    
    // Clear from NVS
    ESPPreferenceObject pref = global_preferences->make_preference<uint8_t>(fnv1_hash("fp_names"));
    pref.save(&result);  // Just clear it
    
    ESP_LOGI(TAG, "Deleted all fingerprints");
    return true;
  }
  
  return false;
}

bool FingerprintDoorbell::rename_fingerprint(uint16_t id, const std::string &new_name) {
  this->save_fingerprint_name(id, new_name);
  ESP_LOGI(TAG, "Renamed fingerprint ID %d to %s", id, new_name.c_str());
  return true;
}

uint16_t FingerprintDoorbell::get_enrolled_count() {
  if (this->sensor_connected_) {
    this->finger_->getTemplateCount();
    return this->finger_->templateCount;
  }
  return 0;
}

std::string FingerprintDoorbell::get_fingerprint_name(uint16_t id) {
  if (this->fingerprint_names_.count(id) > 0) {
    return this->fingerprint_names_[id];
  }
  return "";
}

void FingerprintDoorbell::load_fingerprint_names() {
  // Load fingerprint names from NVS
  // This is a simplified version - you may want to implement proper serialization
  ESP_LOGI(TAG, "Loading fingerprint names from preferences");
  
  for (uint16_t i = 1; i <= 200; i++) {
    yield();  // Feed watchdog during long loop
    std::string key = "fp_" + std::to_string(i);
    ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
    
    std::array<char, 32> name_array;
    if (pref.load(&name_array)) {
      std::string name(name_array.data());
      if (!name.empty() && name != "@empty") {
        this->fingerprint_names_[i] = name;
      }
    }
  }
  
  ESP_LOGI(TAG, "Loaded %d fingerprint names", this->fingerprint_names_.size());
}

void FingerprintDoorbell::save_fingerprint_name(uint16_t id, const std::string &name) {
  this->fingerprint_names_[id] = name;
  
  std::string key = "fp_" + std::to_string(id);
  ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
  
  std::array<char, 32> name_array{};
  std::copy_n(name.begin(), std::min(name.size(), size_t(31)), name_array.begin());
  
  pref.save(&name_array);
}

void FingerprintDoorbell::delete_fingerprint_name(uint16_t id) {
  this->fingerprint_names_.erase(id);
  
  std::string key = "fp_" + std::to_string(id);
  ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
  
  std::array<char, 32> empty{};
  pref.save(&empty);
}

}  // namespace fingerprint_doorbell
}  // namespace esphome
