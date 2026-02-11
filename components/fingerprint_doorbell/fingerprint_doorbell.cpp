#include "fingerprint_doorbell.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace fingerprint_doorbell {

static const char *const TAG = "fingerprint_doorbell";

// Define mySerial as Serial2 - exactly like original code
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

<<<<<<< Updated upstream
  // Initialize Serial2 with configured pins
  ESP_LOGI(TAG, "Initializing fingerprint sensor on RX=GPIO%d, TX=GPIO%d", 
           this->sensor_rx_pin_, this->sensor_tx_pin_);
  Serial2.begin(57600, SERIAL_8N1, this->sensor_rx_pin_, this->sensor_tx_pin_);
  delay(50);  // Brief delay for serial init
  
  this->hw_serial_ = &Serial2;
  this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
  
  this->sensor_connected_ = false;
  this->last_connect_attempt_ = 0;  // Allow immediate first attempt
}

void FingerprintDoorbell::loop() {
  // Try to connect sensor if not connected (throttled)
=======
  // Initialize Serial2 pointer and Adafruit_Fingerprint - exactly like original
  this->hw_serial_ = &mySerial;
  this->finger_ = new Adafruit_Fingerprint(this->hw_serial_);
  
  ESP_LOGI(TAG, "Using Serial2 with default pins (RX=GPIO16, TX=GPIO17)");
  ESP_LOGI(TAG, "Sensor will connect in first loop iteration");
  
  this->sensor_connected_ = false;
}

void FingerprintDoorbell::loop() {
  // Try to connect sensor (throttle to once per 5 seconds)
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
=======
      return;
    } else {
      return;
>>>>>>> Stashed changes
    }
    return;
  }

<<<<<<< Updated upstream
  // Process enrollment if active
  if (this->enroll_state_ != EnrollState::IDLE) {
    this->process_enrollment();
    return;
  }

  // Normal scan mode
  this->do_scan();
=======
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
>>>>>>> Stashed changes
}

void FingerprintDoorbell::dump_config() {
  ESP_LOGCONFIG(TAG, "Fingerprint Doorbell:");
  ESP_LOGCONFIG(TAG, "  Sensor RX Pin: GPIO%d", this->sensor_rx_pin_);
  ESP_LOGCONFIG(TAG, "  Sensor TX Pin: GPIO%d", this->sensor_tx_pin_);
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
  
<<<<<<< Updated upstream
  uint8_t result = this->finger_->verifyPassword();
  
  if (result != FINGERPRINT_OK) {
    this->connect_retry_count_++;
    if (this->connect_retry_count_ >= 3) {
      ESP_LOGW(TAG, "Sensor not responding after %d attempts (error: 0x%02X)", 
               this->connect_retry_count_, result);
    }
    return false;
  }
  
  ESP_LOGI(TAG, "Found fingerprint sensor!");
  this->connect_retry_count_ = 0;
  
  // Brief LED flash to indicate connection
  this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 3);
  
  // Get sensor parameters
  this->finger_->getParameters();
  ESP_LOGI(TAG, "Sensor capacity: %d", this->finger_->capacity);
  
=======
  if (this->finger_ == nullptr) {
    ESP_LOGE(TAG, "Fingerprint object not initialized!");
    return false;
  }

  // Call begin() - library handles Serial2 initialization with default pins
  // This is the key difference - let the library do it!
  this->finger_->begin(57600);
  delay(50);
  
  // First verification attempt
  if (this->finger_->verifyPassword()) {
    ESP_LOGI(TAG, "Found fingerprint sensor!");
  } else {
    // Wait longer for sensor to start (especially after OTA)
    delay(5000);
    if (this->finger_->verifyPassword()) {
      ESP_LOGI(TAG, "Found fingerprint sensor!");
    } else {
      ESP_LOGE(TAG, "Did not find fingerprint sensor :(");
      return false;
    }
  }

  // Startup LED signal
  this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 0);

  // Read sensor parameters
  ESP_LOGI(TAG, "Reading sensor parameters");
  this->finger_->getParameters();
  ESP_LOGI(TAG, "Status: 0x%02X", this->finger_->status_reg);
  ESP_LOGI(TAG, "Sys ID: 0x%02X", this->finger_->system_id);
  ESP_LOGI(TAG, "Capacity: %d", this->finger_->capacity);
  ESP_LOGI(TAG, "Security level: %d", this->finger_->security_level);
  ESP_LOGI(TAG, "Device address: 0x%08X", this->finger_->device_addr);
  ESP_LOGI(TAG, "Packet len: %d", this->finger_->packet_len);
  ESP_LOGI(TAG, "Baud rate: %d", this->finger_->baud_rate);

>>>>>>> Stashed changes
  this->finger_->getTemplateCount();
  ESP_LOGI(TAG, "Sensor contains %d templates", this->finger_->templateCount);

  return true;
}

<<<<<<< Updated upstream
void FingerprintDoorbell::do_scan() {
  // Check touch ring first (if not ignoring)
=======
Match FingerprintDoorbell::scan_fingerprint() {
  Match match;
  match.scan_result = ScanResult::ERROR;

  if (!this->sensor_connected_) {
    return match;
  }

  // Finger detection by capacitive touch ring (if not ignored)
>>>>>>> Stashed changes
  bool ring_touched = false;
  if (!this->ignore_touch_ring_ && this->touch_pin_ != nullptr) {
    ring_touched = this->is_ring_touched();
    
<<<<<<< Updated upstream
    // If neither touched nor was touched before, skip scanning
    if (!ring_touched && !this->last_touch_state_) {
      this->update_touch_state(false);
      if (this->finger_sensor_ != nullptr) {
        this->finger_sensor_->publish_state(false);
      }
      this->scan_state_ = ScanState::IDLE;
      return;
    }
  }

  switch (this->scan_state_) {
    case ScanState::IDLE: {
      // Start new scan
      this->scan_pass_ = 0;
      this->imaging_attempt_ = 0;
      this->scan_start_time_ = millis();
      this->ring_touched_at_start_ = ring_touched;
      this->scan_state_ = ScanState::WAITING_FOR_FINGER;
      this->update_touch_state(true);
      return;
    }
    
    case ScanState::WAITING_FOR_FINGER: {
      // Timeout check (5 seconds max)
      if (millis() - this->scan_start_time_ > 5000) {
        ESP_LOGD(TAG, "Scan timeout");
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        this->publish_ring();
        return;
      }
      
      uint8_t result = this->finger_->getImage();
      
      if (result == FINGERPRINT_OK) {
        this->scan_state_ = ScanState::CONVERTING;
        return;
      } else if (result == FINGERPRINT_NOFINGER) {
        this->imaging_attempt_++;
        if (this->ring_touched_at_start_ && this->imaging_attempt_ >= 15) {
          // Ring touched but no finger after max attempts
          this->scan_state_ = ScanState::IDLE;
          this->update_touch_state(false);
          this->publish_ring();
        }
        return;
      } else {
        ESP_LOGW(TAG, "Error getting image: 0x%02X", result);
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        return;
      }
    }
    
    case ScanState::CONVERTING: {
      uint8_t result = this->finger_->image2Tz();
      if (result != FINGERPRINT_OK) {
        if (result == FINGERPRINT_IMAGEMESS) {
          ESP_LOGD(TAG, "Image too messy");
        }
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        return;
      }
      
      this->update_touch_state(true);
      this->scan_state_ = ScanState::SEARCHING;
      return;
    }
    
    case ScanState::SEARCHING: {
      uint8_t result = this->finger_->fingerSearch();
      
      if (result == FINGERPRINT_OK) {
        // Match found!
        std::string name;
        if (this->fingerprint_names_.count(this->finger_->fingerID) > 0) {
          name = this->fingerprint_names_[this->finger_->fingerID];
        } else {
          name = "ID " + std::to_string(this->finger_->fingerID);
        }
        
        ESP_LOGI(TAG, "Match found: ID=%d, Name=%s, Confidence=%d", 
                 this->finger_->fingerID, name.c_str(), this->finger_->confidence);
        
        this->publish_match(this->finger_->fingerID, this->finger_->confidence, name);
        this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
        
        // Clear match after 3 seconds
        this->set_timeout(3000, [this]() {
          this->clear_match();
          this->set_led_ring_ready();
        });
        
        this->scan_state_ = ScanState::IDLE;
        return;
        
      } else if (result == FINGERPRINT_NOTFOUND) {
        this->scan_pass_++;
        ESP_LOGD(TAG, "No match found (scan #%d of 5)", this->scan_pass_);
        
        if (this->scan_pass_ < 5) {
          // Try again
          this->imaging_attempt_ = 0;
          this->scan_state_ = ScanState::WAITING_FOR_FINGER;
          return;
        } else {
          // Max attempts reached - doorbell ring
          this->publish_ring();
          this->scan_state_ = ScanState::IDLE;
          this->update_touch_state(false);
          return;
        }
      } else {
        ESP_LOGW(TAG, "Search error: 0x%02X", result);
        this->scan_state_ = ScanState::IDLE;
        this->update_touch_state(false);
        return;
=======
    if (ring_touched || this->last_touch_state_) {
      this->update_touch_state(true);
    } else {
      this->update_touch_state(false);
      match.scan_result = ScanResult::NO_FINGER;
      return match;
    }
  }

  // Multi-pass scanning logic (up to 5 attempts)
  bool do_another_scan = true;
  int scan_pass = 0;
  
  while (do_another_scan) {
    do_another_scan = false;
    scan_pass++;

    ///////////////////////////////////////////////////////////
    // STEP 1: Get Image from Sensor
    ///////////////////////////////////////////////////////////
    bool do_imaging = true;
    int imaging_pass = 0;
    
    while (do_imaging) {
      do_imaging = false;
      imaging_pass++;
      
      match.return_code = this->finger_->getImage();
      
      switch (match.return_code) {
        case FINGERPRINT_OK:
          // Image captured successfully
          break;
          
        case FINGERPRINT_NOFINGER:
        case FINGERPRINT_PACKETRECIEVEERR:
          if (ring_touched) {
            // Ring touched but no finger yet - keep trying
            this->update_touch_state(true);
            if (imaging_pass < 15) {
              do_imaging = true;
              break;
            } else {
              // 15 attempts without image - ring event
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

    ///////////////////////////////////////////////////////////
    // STEP 2: Convert Image to feature map
    ///////////////////////////////////////////////////////////
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

    ///////////////////////////////////////////////////////////
    // STEP 3: Search DB for matching features
    ///////////////////////////////////////////////////////////
    match.return_code = this->finger_->fingerSearch();
    
    if (match.return_code == FINGERPRINT_OK) {
      // Match found!
      this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
      
      match.scan_result = ScanResult::MATCH_FOUND;
      match.match_id = this->finger_->fingerID;
      match.match_confidence = this->finger_->confidence;
      
      // Get name from map
      auto it = this->fingerprint_names_.find(match.match_id);
      if (it != this->fingerprint_names_.end()) {
        match.match_name = it->second;
      } else {
        match.match_name = "unknown";
      }
      
    } else if (match.return_code == FINGERPRINT_PACKETRECIEVEERR) {
      ESP_LOGW(TAG, "Communication error");
      
    } else if (match.return_code == FINGERPRINT_NOTFOUND) {
      ESP_LOGI(TAG, "Did not find a match (Scan #%d of 5)", scan_pass);
      match.scan_result = ScanResult::NO_MATCH_FOUND;
      
      if (scan_pass < 5) {
        do_another_scan = true;
>>>>>>> Stashed changes
      }
      
    } else {
      ESP_LOGW(TAG, "Unknown error");
    }
  }
}

<<<<<<< Updated upstream
void FingerprintDoorbell::process_enrollment() {
  uint32_t now = millis();
  
  switch (this->enroll_state_) {
    case EnrollState::WAIT_FINGER_PLACE: {
      // Timeout check (30 seconds)
      if (now - this->enroll_timeout_start_ > 30000) {
        ESP_LOGW(TAG, "Enrollment timeout waiting for finger");
        this->enroll_state_ = EnrollState::FAILED;
        return;
      }
      
      uint8_t result = this->finger_->getImage();
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "Finger detected, capturing pass %d/5", this->enroll_pass_ + 1);
        this->enroll_state_ = EnrollState::CAPTURING;
      }
      return;
    }
    
    case EnrollState::CAPTURING: {
      // Convert image to template
      uint8_t result = this->finger_->image2Tz(this->enroll_pass_ + 1);
      if (result != FINGERPRINT_OK) {
        ESP_LOGW(TAG, "image2Tz failed: 0x%02X", result);
        this->enroll_state_ = EnrollState::FAILED;
        return;
      }
      
      ESP_LOGI(TAG, "Pass %d/5 captured successfully", this->enroll_pass_ + 1);
      this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
      
      this->enroll_pass_++;
      
      if (this->enroll_pass_ >= 5) {
        // All passes done, create model
        this->enroll_state_ = EnrollState::CREATE_MODEL;
      } else {
        // Wait for finger removal
        this->enroll_timeout_start_ = now;
        this->enroll_state_ = EnrollState::WAIT_FINGER_REMOVE;
      }
      return;
    }
    
    case EnrollState::WAIT_FINGER_REMOVE: {
      // Timeout check
      if (now - this->enroll_timeout_start_ > 10000) {
        ESP_LOGW(TAG, "Timeout waiting for finger removal");
        this->enroll_state_ = EnrollState::FAILED;
        return;
      }
      
      uint8_t result = this->finger_->getImage();
      if (result == FINGERPRINT_NOFINGER) {
        ESP_LOGI(TAG, "Finger removed. Place finger again for pass %d/5", this->enroll_pass_ + 1);
        this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_PURPLE, 0);
        this->enroll_timeout_start_ = now;
        this->enroll_state_ = EnrollState::WAIT_FINGER_PLACE;
      }
      return;
    }
    
    case EnrollState::CREATE_MODEL: {
      ESP_LOGI(TAG, "Creating fingerprint model...");
      uint8_t result = this->finger_->createModel();
      if (result != FINGERPRINT_OK) {
        ESP_LOGE(TAG, "createModel failed: 0x%02X", result);
        this->enroll_state_ = EnrollState::FAILED;
        return;
      }
      this->enroll_state_ = EnrollState::STORE_MODEL;
      return;
    }
    
    case EnrollState::STORE_MODEL: {
      ESP_LOGI(TAG, "Storing fingerprint as ID %d...", this->enroll_id_);
      uint8_t result = this->finger_->storeModel(this->enroll_id_);
      if (result != FINGERPRINT_OK) {
        ESP_LOGE(TAG, "storeModel failed: 0x%02X", result);
        this->enroll_state_ = EnrollState::FAILED;
        return;
      }
      
      // Save name
      this->save_fingerprint_name(this->enroll_id_, this->enroll_name_);
      ESP_LOGI(TAG, "Enrollment successful for ID %d (%s)", this->enroll_id_, this->enroll_name_.c_str());
      
      this->enroll_state_ = EnrollState::DONE;
      return;
    }
    
    case EnrollState::DONE: {
      this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
      this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
      this->enroll_state_ = EnrollState::IDLE;
      return;
    }
    
    case EnrollState::FAILED: {
      ESP_LOGE(TAG, "Enrollment failed");
      this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
      this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
      this->enroll_state_ = EnrollState::IDLE;
      return;
    }
    
    case EnrollState::IDLE:
      return;
  }
}

void FingerprintDoorbell::enroll_fingerprint(uint16_t id, const std::string &name) {
  if (!this->sensor_connected_) {
    ESP_LOGE(TAG, "Sensor not connected");
    return;
  }
  
  if (this->enroll_state_ != EnrollState::IDLE) {
    ESP_LOGW(TAG, "Enrollment already in progress");
    return;
  }

  ESP_LOGI(TAG, "Starting enrollment for ID %d (%s)", id, name.c_str());
  
  this->enroll_id_ = id;
  this->enroll_name_ = name;
  this->enroll_pass_ = 0;
  this->enroll_timeout_start_ = millis();
  this->enroll_state_ = EnrollState::WAIT_FINGER_PLACE;
  
  this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_PURPLE, 0);
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
  
  ESP_LOGE(TAG, "Failed to delete fingerprint ID %d: 0x%02X", id, result);
  return false;
}

bool FingerprintDoorbell::delete_all_fingerprints() {
  if (!this->sensor_connected_) {
    return false;
  }

  uint8_t result = this->finger_->emptyDatabase();
  if (result == FINGERPRINT_OK) {
    // Clear all names from NVS
    for (auto &pair : this->fingerprint_names_) {
      this->delete_fingerprint_name(pair.first);
    }
    this->fingerprint_names_.clear();
    
    ESP_LOGI(TAG, "Deleted all fingerprints");
    return true;
  }
  
  ESP_LOGE(TAG, "Failed to delete all fingerprints: 0x%02X", result);
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
  if (this->finger_ == nullptr) return;
  
  if (this->ignore_touch_ring_) {
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  } else {
    this->finger_->LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE);
  }
}

void FingerprintDoorbell::set_led_ring_error() {
  if (this->finger_ != nullptr) {
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
  }
}

void FingerprintDoorbell::publish_match(uint16_t id, uint16_t confidence, const std::string &name) {
  if (this->match_id_sensor_ != nullptr) {
    this->match_id_sensor_->publish_state(id);
  }
  if (this->confidence_sensor_ != nullptr) {
    this->confidence_sensor_->publish_state(confidence);
  }
  if (this->match_name_sensor_ != nullptr) {
    this->match_name_sensor_->publish_state(name);
  }
  if (this->finger_sensor_ != nullptr) {
    this->finger_sensor_->publish_state(true);
  }
  
  this->last_match_time_ = millis();
  this->last_match_id_ = id;
}

void FingerprintDoorbell::publish_ring() {
  ESP_LOGI(TAG, "Doorbell ring!");
  
  if (this->ring_sensor_ != nullptr) {
    this->ring_sensor_->publish_state(true);
  }
  
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

void FingerprintDoorbell::clear_match() {
  if (this->match_id_sensor_ != nullptr) {
    this->match_id_sensor_->publish_state(-1);
  }
  if (this->match_name_sensor_ != nullptr) {
    this->match_name_sensor_->publish_state("");
  }
  if (this->confidence_sensor_ != nullptr) {
    this->confidence_sensor_->publish_state(0);
  }
  if (this->finger_sensor_ != nullptr) {
    this->finger_sensor_->publish_state(false);
  }
}

void FingerprintDoorbell::load_fingerprint_names() {
  ESP_LOGI(TAG, "Loading fingerprint names from preferences...");
  
  // Only load names for IDs that actually exist on the sensor
  // First get the list of stored templates
  this->finger_->getTemplateCount();
  uint16_t count = this->finger_->templateCount;
  
  if (count == 0) {
    ESP_LOGI(TAG, "No fingerprints stored on sensor");
    return;
  }
  
  // Load names for IDs 1-200, but yield periodically
  int loaded = 0;
  for (uint16_t i = 1; i <= 200; i++) {
    if (i % 20 == 0) {
      yield();  // Feed watchdog every 20 iterations
    }
    
=======
void FingerprintDoorbell::update_touch_state(bool touched) {
  if ((touched != this->last_touch_state_) || (this->ignore_touch_ring_ != this->last_ignore_touch_ring_)) {
    if (touched) {
      // Turn touch indicator on (red flashing)
      this->finger_->LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 0);
    } else {
      // Turn touch indicator off (back to ready state)
      this->set_led_ring_ready();
    }
  }
  this->last_touch_state_ = touched;
  this->last_ignore_touch_ring_ = this->ignore_touch_ring_;
}

bool FingerprintDoorbell::is_ring_touched() {
  if (this->touch_pin_ == nullptr) {
    return false;
  }
  
  // LOW = touched (capacitive sensor behavior)
  return !this->touch_pin_->digital_read();
}

void FingerprintDoorbell::set_led_ring_ready() {
  if (this->finger_ == nullptr || !this->sensor_connected_) {
    return;
  }
  
  if (this->ignore_touch_ring_) {
    // Blue solid - touch ring ignored
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE, 0);
  } else {
    // Blue breathing - touch ring active
    this->finger_->LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE, 0);
  }
}

void FingerprintDoorbell::set_led_ring_error() {
  if (this->finger_ == nullptr) {
    return;
  }
  
  this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED, 0);
}

void FingerprintDoorbell::load_fingerprint_names() {
  this->fingerprint_names_.clear();
  
  // Load all fingerprint names from preferences
  // Using fixed-size char array to match ESPHome preferences API
  for (uint16_t i = 1; i <= 200; i++) {
>>>>>>> Stashed changes
    std::string key = "fp_" + std::to_string(i);
    ESPPreferenceObject pref = global_preferences->make_preference<std::array<char, 32>>(fnv1_hash(key.c_str()));
    
    std::array<char, 32> name_array;
    if (pref.load(&name_array)) {
      std::string name(name_array.data());
      if (!name.empty() && name[0] != '\0' && name != "@empty") {
        this->fingerprint_names_[i] = name;
        loaded++;
      }
    }
  }
  
<<<<<<< Updated upstream
  ESP_LOGI(TAG, "Loaded %d fingerprint names", loaded);
=======
  ESP_LOGI(TAG, "%d fingerprints loaded from preferences", this->fingerprint_names_.size());
  
  if (this->finger_ != nullptr && this->fingerprint_names_.size() != this->finger_->templateCount) {
    ESP_LOGW(TAG, "Warning: Fingerprint count mismatch! %d on sensor, %d in preferences",
             this->finger_->templateCount, this->fingerprint_names_.size());
  }
>>>>>>> Stashed changes
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
  strncpy(empty_array.data(), "@empty", 31);
  pref.save(&empty_array);
  
  this->fingerprint_names_.erase(id);
}

// Public service methods
bool FingerprintDoorbell::enroll_fingerprint(uint16_t id, const std::string &name) {
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
    ESP_LOGE(TAG, "Sensor not connected");
    return false;
  }
  
  ESP_LOGI(TAG, "Starting enrollment for ID %d...", id);
  ESP_LOGI(TAG, "Place finger on sensor (5 scans required)");
  
  // TODO: Implement full enrollment logic (5-pass process from original)
  // This is a simplified version
  
  return false;  // Not implemented yet
}

bool FingerprintDoorbell::delete_fingerprint(uint16_t id) {
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
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
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
    return false;
  }
  
  uint8_t result = this->finger_->emptyDatabase();
  if (result == FINGERPRINT_OK) {
    this->fingerprint_names_.clear();
    ESP_LOGI(TAG, "Deleted all fingerprints");
    return true;
  }
  
  return false;
}

bool FingerprintDoorbell::rename_fingerprint(uint16_t id, const std::string &new_name) {
  this->save_fingerprint_name(id, new_name);
  ESP_LOGI(TAG, "Renamed fingerprint ID %d to '%s'", id, new_name.c_str());
  return true;
}

uint16_t FingerprintDoorbell::get_enrolled_count() {
  if (this->finger_ != nullptr) {
    return this->finger_->templateCount;
  }
  return 0;
}

std::string FingerprintDoorbell::get_fingerprint_name(uint16_t id) {
  auto it = this->fingerprint_names_.find(id);
  if (it != this->fingerprint_names_.end()) {
    return it->second;
  }
  return "unknown";
}

}  // namespace fingerprint_doorbell
}  // namespace esphome
