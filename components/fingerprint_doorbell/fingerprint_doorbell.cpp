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

  // Initialize hardware serial for fingerprint sensor
  // ESPHome UART component handles the actual serial, we wrap it with Adafruit library
  this->hw_serial_ = &Serial2;
  this->hw_serial_->begin(57600);
  
  this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
  
  // Don't connect in setup() - do it in loop() to avoid blocking watchdog
  this->sensor_connected_ = false;
  
  ESP_LOGI(TAG, "Fingerprint sensor will connect in first loop iteration");
}

void FingerprintDoorbell::loop() {
  // Try to connect sensor (throttle to once per 5 seconds)
  if (!this->sensor_connected_) {
    uint32_t now = millis();
    if (now - this->last_connect_attempt_ >= 5000) {
      this->last_connect_attempt_ = now;
      this->sensor_connected_ = this->connect_sensor();
      if (this->sensor_connected_) {
        ESP_LOGI(TAG, "Fingerprint sensor connected successfully");
        this->load_fingerprint_names();
        this->set_led_ring_ready();
      }
    }
    return;
  }

  // Scan for fingerprints
  Match match = this->scan_fingerprint();

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
  ESP_LOGI(TAG, "Connecting to fingerprint sensor...");
  
  if (this->finger_->verifyPassword()) {
    ESP_LOGI(TAG, "Found fingerprint sensor!");
  } else {
    // Don't wait - just fail and retry on next loop
    ESP_LOGW(TAG, "Fingerprint sensor not responding, will retry...");
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
  match.scan_result = ScanResult::ERROR;

  if (!this->sensor_connected_) {
    return match;
  }

  // Check touch ring
  bool ring_touched = false;
  if (!this->ignore_touch_ring_) {
    if (this->is_ring_touched()) {
      ring_touched = true;
    }
    
    if (ring_touched || this->last_touch_state_) {
      this->update_touch_state(true);
    } else {
      this->update_touch_state(false);
      match.scan_result = ScanResult::NO_FINGER;
      return match;
    }
  }

  // Attempt to scan
  bool do_another_scan = true;
  int scan_pass = 0;
  
  while (do_another_scan) {
    do_another_scan = false;
    scan_pass++;

    // Get image
    uint8_t result = this->finger_->getImage();
    
    if (result == FINGERPRINT_OK) {
      // Image captured successfully
    } else if (result == FINGERPRINT_NOFINGER) {
      if (ring_touched && scan_pass < 15) {
        // Ring touched but no finger yet, keep trying
        delay(50);
        do_another_scan = true;
        continue;
      } else {
        if (ring_touched) {
          // Ring was touched but no finger detected after retries
          match.scan_result = ScanResult::NO_MATCH_FOUND;
          return match;
        } else {
          match.scan_result = ScanResult::NO_FINGER;
          this->update_touch_state(false);
          return match;
        }
      }
    } else {
      // Error getting image
      match.scan_result = ScanResult::ERROR;
      return match;
    }

    // Convert image to features
    result = this->finger_->image2Tz();
    if (result != FINGERPRINT_OK) {
      if (result == FINGERPRINT_IMAGEMESS) {
        ESP_LOGW(TAG, "Image too messy");
      }
      match.scan_result = ScanResult::ERROR;
      return match;
    }

    this->update_touch_state(true);

    // Search for match
    result = this->finger_->fingerSearch();
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
      
      return match;
      
    } else if (result == FINGERPRINT_NOTFOUND) {
      ESP_LOGD(TAG, "No match found (scan #%d of 5)", scan_pass);
      match.scan_result = ScanResult::NO_MATCH_FOUND;
      
      if (scan_pass < 5) {
        do_another_scan = true;  // Try up to 5 times
      }
    } else {
      match.scan_result = ScanResult::ERROR;
      return match;
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
