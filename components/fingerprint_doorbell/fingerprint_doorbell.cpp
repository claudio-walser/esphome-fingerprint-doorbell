#include "fingerprint_doorbell.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace fingerprint_doorbell {

static const char *const TAG = "fingerprint_doorbell";

// Use Serial2 exactly like the original working code
#define mySerial Serial2

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

  // Initialize exactly like original: pass Serial2 pointer to Adafruit library
  this->hw_serial_ = &mySerial;
  this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
  
  ESP_LOGI(TAG, "Using Serial2 with default pins (RX=GPIO16, TX=GPIO17)");
  this->sensor_connected_ = false;
}

void FingerprintDoorbell::loop() {
  // Connect sensor if not connected (throttled to every 5 seconds)
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
    ESP_LOGI(TAG, "Match: ID=%d, Name=%s, Confidence=%d", 
             match.match_id, match.match_name.c_str(), match.match_confidence);
    
    if (this->match_id_sensor_ != nullptr)
      this->match_id_sensor_->publish_state(match.match_id);
    if (this->confidence_sensor_ != nullptr)
      this->confidence_sensor_->publish_state(match.match_confidence);
    if (this->match_name_sensor_ != nullptr)
      this->match_name_sensor_->publish_state(match.match_name);
    
    this->last_match_time_ = millis();
    this->last_match_id_ = match.match_id;
    
    // Clear after 3 seconds
    this->set_timeout(3000, [this]() {
      if (this->match_id_sensor_ != nullptr)
        this->match_id_sensor_->publish_state(-1);
      if (this->match_name_sensor_ != nullptr)
        this->match_name_sensor_->publish_state("");
      if (this->confidence_sensor_ != nullptr)
        this->confidence_sensor_->publish_state(0);
    });
  }
  // Handle no match (doorbell ring)
  else if (match.scan_result == ScanResult::NO_MATCH_FOUND) {
    ESP_LOGI(TAG, "No match - doorbell ring!");
    
    if (this->ring_sensor_ != nullptr)
      this->ring_sensor_->publish_state(true);
    if (this->doorbell_pin_ != nullptr)
      this->doorbell_pin_->digital_write(true);
    
    this->last_ring_time_ = millis();
    
    // Clear after 1 second
    this->set_timeout(1000, [this]() {
      if (this->ring_sensor_ != nullptr)
        this->ring_sensor_->publish_state(false);
      if (this->doorbell_pin_ != nullptr)
        this->doorbell_pin_->digital_write(false);
    });
  }

  // Update finger detection sensor
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
  ESP_LOGI(TAG, "Connecting to fingerprint sensor (attempt %d)...", this->connect_attempts_ + 1);
  
  if (this->finger_ == nullptr) {
    ESP_LOGE(TAG, "Fingerprint object not initialized!");
    return false;
  }

  // Only call begin() on first attempt
  if (this->connect_attempts_ == 0) {
    this->finger_->begin(57600);
    App.feed_wdt();  // Feed watchdog
  }
  
  this->connect_attempts_++;
  
  // Try to verify password (non-blocking - just one attempt per call)
  if (this->finger_->verifyPassword()) {
    ESP_LOGI(TAG, "Found fingerprint sensor!");
    
    // Startup LED signal
    this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 0);

    // Read sensor parameters
    ESP_LOGI(TAG, "Reading sensor parameters");
    this->finger_->getParameters();
    ESP_LOGI(TAG, "Status: 0x%02X, Capacity: %d, Security: %d", 
             this->finger_->status_reg, this->finger_->capacity, this->finger_->security_level);

    this->finger_->getTemplateCount();
    ESP_LOGI(TAG, "Sensor contains %d templates", this->finger_->templateCount);
    
    this->connect_attempts_ = 0;  // Reset for future reconnects
    return true;
  }
  
  // Allow up to 10 attempts (5 seconds interval * 10 = 50 seconds total)
  if (this->connect_attempts_ >= 10) {
    ESP_LOGE(TAG, "Did not find fingerprint sensor after %d attempts", this->connect_attempts_);
    this->connect_attempts_ = 0;  // Reset for future retries
  }
  
  return false;
}

Match FingerprintDoorbell::scan_fingerprint() {
  Match match;
  match.scan_result = ScanResult::ERROR;

  if (!this->sensor_connected_)
    return match;

  // Check touch ring first (if not ignored)
  bool ring_touched = false;
  if (!this->ignore_touch_ring_) {
    if (this->is_ring_touched())
      ring_touched = true;
    
    if (ring_touched || this->last_touch_state_) {
      this->update_touch_state(true);
    } else {
      this->update_touch_state(false);
      match.scan_result = ScanResult::NO_FINGER;
      return match;
    }
  }

  // Multi-pass scanning (up to 5 attempts) - exactly like original
  bool do_another_scan = true;
  int scan_pass = 0;
  
  while (do_another_scan) {
    do_another_scan = false;
    scan_pass++;

    // STEP 1: Get Image
    bool do_imaging = true;
    int imaging_pass = 0;
    
    while (do_imaging) {
      do_imaging = false;
      imaging_pass++;
      
      match.return_code = this->finger_->getImage();
      
      switch (match.return_code) {
        case FINGERPRINT_OK:
          break;
          
        case FINGERPRINT_NOFINGER:
        case FINGERPRINT_PACKETRECIEVEERR:
          if (ring_touched) {
            this->update_touch_state(true);
            if (imaging_pass < 15) {
              do_imaging = true;
              break;
            } else {
              match.scan_result = ScanResult::NO_MATCH_FOUND;
              return match;
            }
          } else {
            if (this->ignore_touch_ring_ && scan_pass > 1) {
              match.scan_result = ScanResult::NO_MATCH_FOUND;
            } else {
              match.scan_result = ScanResult::NO_FINGER;
              this->update_touch_state(false);
            }
            return match;
          }
          
        case FINGERPRINT_IMAGEFAIL:
          ESP_LOGW(TAG, "Imaging error");
          this->update_touch_state(true);
          return match;
          
        default:
          ESP_LOGW(TAG, "Unknown error");
          return match;
      }
    }

    // STEP 2: Convert Image
    match.return_code = this->finger_->image2Tz();
    
    switch (match.return_code) {
      case FINGERPRINT_OK:
        this->update_touch_state(true);
        break;
      case FINGERPRINT_IMAGEMESS:
        ESP_LOGW(TAG, "Image too messy");
        return match;
      case FINGERPRINT_PACKETRECIEVEERR:
        ESP_LOGW(TAG, "Communication error");
        return match;
      case FINGERPRINT_FEATUREFAIL:
      case FINGERPRINT_INVALIDIMAGE:
        ESP_LOGW(TAG, "Could not find fingerprint features");
        return match;
      default:
        ESP_LOGW(TAG, "Unknown error");
        return match;
    }

    // STEP 3: Search DB
    match.return_code = this->finger_->fingerSearch();
    
    if (match.return_code == FINGERPRINT_OK) {
      // Match found!
      this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
      
      match.scan_result = ScanResult::MATCH_FOUND;
      match.match_id = this->finger_->fingerID;
      match.match_confidence = this->finger_->confidence;
      
      auto it = this->fingerprint_names_.find(match.match_id);
      if (it != this->fingerprint_names_.end()) {
        match.match_name = it->second;
      } else {
        match.match_name = "unknown";
      }
      
    } else if (match.return_code == FINGERPRINT_PACKETRECIEVEERR) {
      ESP_LOGW(TAG, "Communication error");
      
    } else if (match.return_code == FINGERPRINT_NOTFOUND) {
      ESP_LOGD(TAG, "No match (Scan #%d of 5)", scan_pass);
      match.scan_result = ScanResult::NO_MATCH_FOUND;
      
      if (scan_pass < 5)
        do_another_scan = true;
      
    } else {
      ESP_LOGW(TAG, "Unknown error");
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
  if (this->touch_pin_ == nullptr)
    return false;
  // LOW = touched (capacitive sensor)
  return !this->touch_pin_->digital_read();
}

void FingerprintDoorbell::set_led_ring_ready() {
  if (this->finger_ == nullptr || !this->sensor_connected_)
    return;
  
  if (this->ignore_touch_ring_) {
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE, 0);
  } else {
    this->finger_->LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE, 0);
  }
}

void FingerprintDoorbell::set_led_ring_error() {
  if (this->finger_ != nullptr)
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED, 0);
}

void FingerprintDoorbell::load_fingerprint_names() {
  this->fingerprint_names_.clear();
  
  for (uint16_t i = 1; i <= 200; i++) {
    std::string key = "fp_" + std::to_string(i);
    ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
    
    std::array<char, 32> name_array;
    if (pref.load(&name_array)) {
      std::string name(name_array.data());
      if (!name.empty() && name[0] != '\0' && name != "@empty") {
        this->fingerprint_names_[i] = name;
      }
    }
  }
  
  ESP_LOGI(TAG, "%d fingerprint names loaded", this->fingerprint_names_.size());
}

void FingerprintDoorbell::save_fingerprint_name(uint16_t id, const std::string &name) {
  std::string key = "fp_" + std::to_string(id);
  ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
  
  std::array<char, 32> name_array = {};
  strncpy(name_array.data(), name.c_str(), 31);
  name_array[31] = '\0';
  pref.save(&name_array);
  
  this->fingerprint_names_[id] = name;
}

void FingerprintDoorbell::delete_fingerprint_name(uint16_t id) {
  std::string key = "fp_" + std::to_string(id);
  ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
  
  std::array<char, 32> empty_array = {};
  pref.save(&empty_array);
  
  this->fingerprint_names_.erase(id);
}

bool FingerprintDoorbell::enroll_fingerprint(uint16_t id, const std::string &name) {
  // TODO: Implement enrollment
  ESP_LOGW(TAG, "Enrollment not yet implemented");
  return false;
}

bool FingerprintDoorbell::delete_fingerprint(uint16_t id) {
  if (!this->sensor_connected_ || this->finger_ == nullptr)
    return false;
  
  if (this->finger_->deleteModel(id) == FINGERPRINT_OK) {
    this->delete_fingerprint_name(id);
    ESP_LOGI(TAG, "Deleted fingerprint ID %d", id);
    return true;
  }
  return false;
}

bool FingerprintDoorbell::delete_all_fingerprints() {
  if (!this->sensor_connected_ || this->finger_ == nullptr)
    return false;
  
  if (this->finger_->emptyDatabase() == FINGERPRINT_OK) {
    this->fingerprint_names_.clear();
    ESP_LOGI(TAG, "Deleted all fingerprints");
    return true;
  }
  return false;
}

bool FingerprintDoorbell::rename_fingerprint(uint16_t id, const std::string &new_name) {
  this->save_fingerprint_name(id, new_name);
  ESP_LOGI(TAG, "Renamed ID %d to '%s'", id, new_name.c_str());
  return true;
}

uint16_t FingerprintDoorbell::get_enrolled_count() {
  return this->finger_ != nullptr ? this->finger_->templateCount : 0;
}

std::string FingerprintDoorbell::get_fingerprint_name(uint16_t id) {
  auto it = this->fingerprint_names_.find(id);
  return (it != this->fingerprint_names_.end()) ? it->second : "unknown";
}

}  // namespace fingerprint_doorbell
}  // namespace esphome
