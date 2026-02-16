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
  this->mode_ = Mode::SCAN;
  
  // Setup REST API
  this->setup_web_server();
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
        this->publish_last_action("Sensor connected");
      }
    }
    return;
  }

  // Handle different modes
  if (this->mode_ == Mode::ENROLL) {
    this->process_enrollment();
    return;
  }

  // Cooldown after no-match to keep LED visible
  if (this->last_ring_time_ > 0 && millis() - this->last_ring_time_ < 1000) {
    return;
  } else if (this->last_ring_time_ > 0) {
    // Cooldown just expired, return to ready state
    this->last_ring_time_ = 0;
    this->set_led_ring_ready();
  }
  
  // Cooldown after match to keep LED visible
  if (this->last_match_time_ > 0 && millis() - this->last_match_time_ < 1000) {
    return;
  } else if (this->last_match_time_ > 0) {
    // Cooldown just expired, return to ready state
    this->last_match_time_ = 0;
    this->set_led_ring_ready();
  }

  // Normal scan mode
  Match match = this->scan_fingerprint();

  // Handle match found
  if (match.scan_result == ScanResult::MATCH_FOUND) {
    ESP_LOGI(TAG, "Match: ID=%d, Name=%s, Confidence=%d", 
             match.match_id, match.match_name.c_str(), match.match_confidence);
    
    // Set match LED here (after scan completes) to ensure it's visible
    this->set_led_ring_match();
    
    if (this->match_id_sensor_ != nullptr)
      this->match_id_sensor_->publish_state(match.match_id);
    if (this->confidence_sensor_ != nullptr)
      this->confidence_sensor_->publish_state(match.match_confidence);
    if (this->match_name_sensor_ != nullptr)
      this->match_name_sensor_->publish_state(match.match_name);
    
    this->publish_last_action("Match: " + match.match_name);
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
    
    // Show no match LED
    this->set_led_ring_no_match();
    
    if (this->ring_sensor_ != nullptr)
      this->ring_sensor_->publish_state(true);
    if (this->doorbell_pin_ != nullptr)
      this->doorbell_pin_->digital_write(true);
    
    this->publish_last_action("Doorbell ring");
    this->last_ring_time_ = millis();
    
    // Clear doorbell output after 1 second
    this->set_timeout(1000, [this]() {
      if (this->ring_sensor_ != nullptr)
        this->ring_sensor_->publish_state(false);
      if (this->doorbell_pin_ != nullptr)
        this->doorbell_pin_->digital_write(false);
      // LED ready state is handled by cooldown logic in loop()
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
  
  // LED configuration debug
  ESP_LOGCONFIG(TAG, "  LED Ready: color=%d, mode=%d, speed=%d", this->led_ready_.color, this->led_ready_.mode, this->led_ready_.speed);
  ESP_LOGCONFIG(TAG, "  LED Error: color=%d, mode=%d, speed=%d", this->led_error_.color, this->led_error_.mode, this->led_error_.speed);
  ESP_LOGCONFIG(TAG, "  LED Enroll: color=%d, mode=%d, speed=%d", this->led_enroll_.color, this->led_enroll_.mode, this->led_enroll_.speed);
  ESP_LOGCONFIG(TAG, "  LED Match: color=%d, mode=%d, speed=%d", this->led_match_.color, this->led_match_.mode, this->led_match_.speed);
  ESP_LOGCONFIG(TAG, "  LED Scanning: color=%d, mode=%d, speed=%d", this->led_scanning_.color, this->led_scanning_.mode, this->led_scanning_.speed);
  ESP_LOGCONFIG(TAG, "  LED No Match: color=%d, mode=%d, speed=%d", this->led_no_match_.color, this->led_no_match_.mode, this->led_no_match_.speed);
  
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

  // Only initialize serial on first attempt
  if (this->connect_attempts_ == 0) {
    // Explicitly initialize Serial2 with pins for ESP-IDF framework
    // RX=GPIO16, TX=GPIO17 are the default Serial2 pins on ESP32
    mySerial.begin(57600, SERIAL_8N1, 16, 17);
    delay(100);  // Give serial time to initialize
    App.feed_wdt();
    
    // Now tell Adafruit library what baud rate we're using
    this->finger_->begin(57600);
    App.feed_wdt();
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
          // Finger detected and image captured - show scanning LED
          this->set_led_ring_scanning();
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
      // Match found - LED is set in loop() after scan returns
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

// ==================== ENROLLMENT ====================

void FingerprintDoorbell::start_enrollment(uint16_t id, const std::string &name) {
  if (!this->sensor_connected_) {
    ESP_LOGW(TAG, "Cannot enroll: sensor not connected");
    this->publish_enroll_status("Error: Sensor not connected");
    this->publish_last_action("Enroll failed: no sensor");
    return;
  }
  
  if (id < 1 || id > 200) {
    ESP_LOGW(TAG, "Invalid ID %d (must be 1-200)", id);
    this->publish_enroll_status("Error: Invalid ID (1-200)");
    this->publish_last_action("Enroll failed: invalid ID");
    return;
  }
  
  ESP_LOGI(TAG, "Starting enrollment for ID %d, name: %s", id, name.c_str());
  
  this->mode_ = Mode::ENROLL;
  this->enroll_step_ = EnrollStep::WAITING_FOR_FINGER;
  this->enroll_id_ = id;
  this->enroll_name_ = name;
  this->enroll_sample_ = 1;
  this->enroll_timeout_ = millis() + 60000;  // 60 second timeout
  
  this->set_led_ring_enroll();
  this->publish_enroll_status("Place finger (1/5)");
  this->publish_last_action("Enrollment started for ID " + std::to_string(id));
}

void FingerprintDoorbell::cancel_enrollment() {
  if (this->mode_ != Mode::ENROLL) {
    return;
  }
  
  ESP_LOGI(TAG, "Enrollment cancelled");
  this->mode_ = Mode::SCAN;
  this->enroll_step_ = EnrollStep::IDLE;
  this->set_led_ring_ready();
  this->publish_enroll_status("Cancelled");
  this->publish_last_action("Enrollment cancelled");
}

void FingerprintDoorbell::process_enrollment() {
  // Check timeout
  if (millis() > this->enroll_timeout_) {
    ESP_LOGW(TAG, "Enrollment timeout");
    this->mode_ = Mode::SCAN;
    this->enroll_step_ = EnrollStep::IDLE;
    this->set_led_ring_ready();
    this->publish_enroll_status("Timeout");
    this->publish_last_action("Enrollment timeout");
    return;
  }

  uint8_t result;
  
  switch (this->enroll_step_) {
    case EnrollStep::WAITING_FOR_FINGER:
      result = this->finger_->getImage();
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "Image captured for sample %d", this->enroll_sample_);
        this->enroll_step_ = EnrollStep::CONVERTING;
      } else if (result != FINGERPRINT_NOFINGER) {
        ESP_LOGW(TAG, "Error capturing image: %d", result);
      }
      break;
      
    case EnrollStep::CONVERTING:
      result = this->finger_->image2Tz(this->enroll_sample_);
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "Image converted for sample %d", this->enroll_sample_);
        
        if (this->enroll_sample_ >= 5) {
          // All 5 samples collected, create model immediately
          // Don't send LED commands before createModel - it can corrupt buffer state
          this->publish_enroll_status("Creating model...");
          result = this->finger_->createModel();
          if (result == FINGERPRINT_OK) {
            ESP_LOGI(TAG, "Model created successfully");
            this->enroll_step_ = EnrollStep::STORING;
            this->publish_enroll_status("Storing...");
          } else if (result == FINGERPRINT_ENROLLMISMATCH) {
            ESP_LOGW(TAG, "Fingerprints did not match");
            this->mode_ = Mode::SCAN;
            this->enroll_step_ = EnrollStep::IDLE;
            this->set_led_ring_error();
            this->publish_enroll_status("Error: prints don't match");
            this->publish_last_action("Enrollment failed: mismatch");
            this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
          } else {
            ESP_LOGW(TAG, "Error creating model: %d", result);
            this->mode_ = Mode::SCAN;
            this->enroll_step_ = EnrollStep::IDLE;
            this->set_led_ring_error();
            this->publish_enroll_status("Error creating model");
            this->publish_last_action("Enrollment failed");
            this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
          }
        } else {
          // Need more samples - show LED feedback for successful scan
          this->set_led_ring_match();
          this->enroll_step_ = EnrollStep::WAITING_REMOVE;
          this->publish_enroll_status("Remove finger");
        }
      } else {
        ESP_LOGW(TAG, "Error converting image: %d", result);
        this->publish_enroll_status("Error, try again");
        this->enroll_step_ = EnrollStep::WAITING_FOR_FINGER;
        this->set_led_ring_enroll();
      }
      break;
      
    case EnrollStep::WAITING_REMOVE:
      result = this->finger_->getImage();
      if (result == FINGERPRINT_NOFINGER) {
        this->enroll_sample_++;
        ESP_LOGI(TAG, "Ready for sample %d", this->enroll_sample_);
        this->enroll_step_ = EnrollStep::WAITING_FOR_FINGER;
        this->set_led_ring_enroll();
        this->publish_enroll_status("Place finger (" + std::to_string(this->enroll_sample_) + "/5)");
      }
      break;
      
    case EnrollStep::CREATING_MODEL:
      result = this->finger_->createModel();
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "Model created successfully");
        this->enroll_step_ = EnrollStep::STORING;
        this->publish_enroll_status("Storing...");
      } else if (result == FINGERPRINT_ENROLLMISMATCH) {
        ESP_LOGW(TAG, "Fingerprints did not match");
        this->mode_ = Mode::SCAN;
        this->enroll_step_ = EnrollStep::IDLE;
        this->set_led_ring_error();
        this->publish_enroll_status("Error: prints don't match");
        this->publish_last_action("Enrollment failed: mismatch");
        this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
      } else {
        ESP_LOGW(TAG, "Error creating model: %d", result);
        this->mode_ = Mode::SCAN;
        this->enroll_step_ = EnrollStep::IDLE;
        this->set_led_ring_error();
        this->publish_enroll_status("Error creating model");
        this->publish_last_action("Enrollment failed");
        this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
      }
      break;
      
    case EnrollStep::STORING:
      result = this->finger_->storeModel(this->enroll_id_);
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "Fingerprint stored at ID %d", this->enroll_id_);
        this->save_fingerprint_name(this->enroll_id_, this->enroll_name_);
        this->finger_->getTemplateCount();
        
        // Show success LED and wait for finger to be removed
        this->enroll_step_ = EnrollStep::DONE;
        this->set_led_ring_match();  // Purple glow for success
        this->publish_enroll_status("Success! Remove finger");
        this->publish_last_action("Enrolled: " + this->enroll_name_ + " (ID " + std::to_string(this->enroll_id_) + ")");
      } else {
        ESP_LOGW(TAG, "Error storing model: %d", result);
        this->mode_ = Mode::SCAN;
        this->enroll_step_ = EnrollStep::IDLE;
        this->set_led_ring_error();
        this->publish_enroll_status("Error storing");
        this->publish_last_action("Enrollment failed: storage error");
        this->set_timeout(2000, [this]() { this->set_led_ring_ready(); });
      }
      break;
    
    case EnrollStep::DONE:
      // Wait for finger to be removed before returning to scan mode
      result = this->finger_->getImage();
      if (result == FINGERPRINT_NOFINGER) {
        ESP_LOGI(TAG, "Enrollment complete, finger removed");
        this->mode_ = Mode::SCAN;
        this->enroll_step_ = EnrollStep::IDLE;
        this->set_led_ring_ready();
        this->publish_enroll_status("Complete");
      }
      break;
      
    default:
      break;
  }
}

// ==================== DELETE / RENAME ====================

bool FingerprintDoorbell::delete_fingerprint(uint16_t id) {
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
    this->publish_last_action("Delete failed: no sensor");
    return false;
  }
  
  if (this->finger_->deleteModel(id) == FINGERPRINT_OK) {
    std::string name = this->get_fingerprint_name(id);
    this->delete_fingerprint_name(id);
    this->finger_->getTemplateCount();
    ESP_LOGI(TAG, "Deleted fingerprint ID %d", id);
    this->publish_last_action("Deleted: " + name + " (ID " + std::to_string(id) + ")");
    return true;
  }
  this->publish_last_action("Delete failed for ID " + std::to_string(id));
  return false;
}

bool FingerprintDoorbell::delete_all_fingerprints() {
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
    this->publish_last_action("Delete all failed: no sensor");
    return false;
  }
  
  if (this->finger_->emptyDatabase() == FINGERPRINT_OK) {
    // Collect IDs first to avoid modifying map while iterating
    std::vector<uint16_t> ids_to_delete;
    for (auto const& pair : this->fingerprint_names_) {
      ids_to_delete.push_back(pair.first);
    }
    // Now delete each name from preferences
    for (uint16_t id : ids_to_delete) {
      this->delete_fingerprint_name(id);
    }
    this->fingerprint_names_.clear();
    this->finger_->getTemplateCount();
    ESP_LOGI(TAG, "Deleted all fingerprints");
    this->publish_last_action("Deleted all fingerprints");
    return true;
  }
  this->publish_last_action("Delete all failed");
  return false;
}

bool FingerprintDoorbell::rename_fingerprint(uint16_t id, const std::string &new_name) {
  std::string old_name = this->get_fingerprint_name(id);
  this->save_fingerprint_name(id, new_name);
  ESP_LOGI(TAG, "Renamed ID %d from '%s' to '%s'", id, old_name.c_str(), new_name.c_str());
  this->publish_last_action("Renamed ID " + std::to_string(id) + " to " + new_name);
  return true;
}

// ==================== TEMPLATE TRANSFER ====================

// Define FINGERPRINT_DOWNLOAD command (not in Arduino library)
#define FINGERPRINT_DOWNLOAD 0x09

bool FingerprintDoorbell::get_template(uint16_t id, std::vector<uint8_t> &template_data) {
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
    ESP_LOGW(TAG, "Cannot get template: sensor not connected");
    return false;
  }
  
  // Temporarily pause scanning to avoid conflicts
  Mode previous_mode = this->mode_;
  this->mode_ = Mode::IDLE;
  delay(200);  // Give sensor time to settle
  
  // Flush any leftover data in serial buffer
  while (mySerial.available()) {
    mySerial.read();
  }
  
  // Load template from flash into character buffer 1
  uint8_t result = this->finger_->loadModel(id);
  if (result != FINGERPRINT_OK) {
    ESP_LOGW(TAG, "Failed to load template %d: error %d", id, result);
    this->mode_ = previous_mode;
    return false;
  }
  
  // Flush again after loadModel response
  delay(50);
  while (mySerial.available()) {
    mySerial.read();
  }
  
  // Start template transfer from sensor
  result = this->finger_->getModel();
  if (result != FINGERPRINT_OK) {
    ESP_LOGW(TAG, "Failed to start template transfer: error %d", result);
    this->mode_ = previous_mode;
    return false;
  }
  
  // Read raw bytes from serial - template is 512 bytes
  // Sensor sends 128-byte data packets (based on sensor's packet_len setting)
  // Each packet: 9 bytes header + 128 bytes data + 2 bytes checksum = 139 bytes
  // 4 packets needed for 512 bytes = 556 total bytes
  // Packet structure: 2-byte start (EF01), 4-byte addr, 1-byte type, 2-byte length, N-byte data, 2-byte checksum
  const int PACKET_DATA_SIZE = 128;
  const int PACKET_OVERHEAD = 11;  // 9 header + 2 checksum
  const int PACKET_SIZE = PACKET_DATA_SIZE + PACKET_OVERHEAD;  // 139 bytes
  const int NUM_PACKETS = 4;
  const int TOTAL_BYTES = PACKET_SIZE * NUM_PACKETS;  // 556 bytes
  
  uint8_t raw_data[600];
  memset(raw_data, 0xff, sizeof(raw_data));
  
  uint32_t start_time = millis();
  const uint32_t TIMEOUT_MS = 5000;
  int idx = 0;
  
  while (idx < TOTAL_BYTES && (millis() - start_time < TIMEOUT_MS)) {
    if (mySerial.available()) {
      raw_data[idx++] = mySerial.read();
    }
  }
  
  ESP_LOGD(TAG, "Read %d raw bytes from sensor (expected %d)", idx, TOTAL_BYTES);
  
  // Log first packet header for debugging
  if (idx >= 12) {
    ESP_LOGI(TAG, "Download PKT1 header: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4], raw_data[5],
             raw_data[6], raw_data[7], raw_data[8], raw_data[9], raw_data[10], raw_data[11]);
  }
  
  if (idx < TOTAL_BYTES) {
    ESP_LOGW(TAG, "Timeout reading template data, only got %d bytes (expected %d)", idx, TOTAL_BYTES);
    this->mode_ = previous_mode;
    return false;
  }
  
  // Extract template data from 4 packets of 128 bytes each
  template_data.clear();
  template_data.reserve(512);
  
  for (int pkt = 0; pkt < NUM_PACKETS; pkt++) {
    int pkt_start = pkt * PACKET_SIZE;
    int data_start = pkt_start + 9;  // Skip 9-byte header
    
    // Verify packet header
    if (raw_data[pkt_start] != 0xEF || raw_data[pkt_start + 1] != 0x01) {
      ESP_LOGW(TAG, "Invalid packet %d header at byte %d: %02X %02X", 
               pkt + 1, pkt_start, raw_data[pkt_start], raw_data[pkt_start + 1]);
    }
    
    // Extract 128 bytes of data from this packet
    for (int i = 0; i < PACKET_DATA_SIZE; i++) {
      template_data.push_back(raw_data[data_start + i]);
    }
  }
  
  // Debug: verify no packet headers in template data
  ESP_LOGI(TAG, "Template bytes 126-137: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           template_data[126], template_data[127], template_data[128], template_data[129],
           template_data[130], template_data[131], template_data[132], template_data[133],
           template_data[134], template_data[135], template_data[136], template_data[137]);
  
  ESP_LOGI(TAG, "Downloaded template %d: %d bytes", id, template_data.size());
  this->mode_ = previous_mode;
  return true;
}

bool FingerprintDoorbell::upload_template(uint16_t id, const std::string &name, const std::vector<uint8_t> &template_data) {
  if (!this->sensor_connected_ || this->finger_ == nullptr) {
    ESP_LOGW(TAG, "Cannot upload template: sensor not connected");
    return false;
  }
  
  if (template_data.size() != 512) {
    ESP_LOGW(TAG, "Invalid template size: %d bytes (expected 512)", template_data.size());
    return false;
  }
  
  // Temporarily pause scanning to avoid conflicts
  Mode previous_mode = this->mode_;
  this->mode_ = Mode::IDLE;
  delay(200);  // Give sensor time to settle
  
  // Flush any leftover data in serial buffer
  while (mySerial.available()) {
    mySerial.read();
  }
  
  // Use sensor's configured packet length (128 bytes for most R503 sensors)
  // The FPM library uses this approach for uploads even though downloads come in 256-byte packets
  const uint16_t packet_len = 128;
  ESP_LOGI(TAG, "Uploading template to ID %d (%d bytes, using %d-byte packets)", id, template_data.size(), packet_len);
  
  // Log first 16 bytes of template for debugging
  ESP_LOGI(TAG, "Template data first 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           template_data[0], template_data[1], template_data[2], template_data[3],
           template_data[4], template_data[5], template_data[6], template_data[7],
           template_data[8], template_data[9], template_data[10], template_data[11],
           template_data[12], template_data[13], template_data[14], template_data[15]);
  
  // Send download command (0x09) - tells sensor to receive template into buffer 1
  uint8_t cmd_data[] = {FINGERPRINT_DOWNLOAD, 0x01};  // Download to char buffer 1
  Adafruit_Fingerprint_Packet cmd_packet(FINGERPRINT_COMMANDPACKET, sizeof(cmd_data), cmd_data);
  this->finger_->writeStructuredPacket(cmd_packet);
  
  // Read acknowledgment
  Adafruit_Fingerprint_Packet ack_packet(FINGERPRINT_ACKPACKET, 0, nullptr);
  if (this->finger_->getStructuredPacket(&ack_packet, 1000) != FINGERPRINT_OK) {
    ESP_LOGW(TAG, "No acknowledgment for download command");
    this->mode_ = previous_mode;
    return false;
  }
  
  if (ack_packet.type != FINGERPRINT_ACKPACKET || ack_packet.data[0] != FINGERPRINT_OK) {
    ESP_LOGW(TAG, "Download command failed: type=0x%02X, code=0x%02X", ack_packet.type, ack_packet.data[0]);
    this->mode_ = previous_mode;
    return false;
  }
  
  ESP_LOGD(TAG, "Sensor ready to receive template data");
  
  // Add small delay after ACK before sending data
  delay(50);
  
  // Flush any bytes that might have arrived
  int flushed = 0;
  while (mySerial.available()) {
    mySerial.read();
    flushed++;
  }
  if (flushed > 0) {
    ESP_LOGD(TAG, "Flushed %d extra bytes before sending data", flushed);
  }
  
  // Send template data in packets matching sensor's configured packet length
  // Packet format: 0xEF01 (2) + addr (4) + type (1) + length (2) + data (N) + checksum (2)
  const uint32_t addr = 0xFFFFFFFF;
  size_t total_size = template_data.size();
  size_t written = 0;
  int pkt_num = 0;
  int num_packets = (total_size + packet_len - 1) / packet_len;  // Round up
  
  // Allocate packet buffer: header(9) + data(packet_len) + checksum(2)
  std::vector<uint8_t> packet(9 + packet_len + 2);
  
  while (written < total_size) {
    size_t remaining = total_size - written;
    size_t chunk_size = (remaining < packet_len) ? remaining : packet_len;
    bool is_last = (written + chunk_size >= total_size);
    pkt_num++;
    
    // Header: 0xEF01
    packet[0] = 0xEF;
    packet[1] = 0x01;
    
    // Address (4 bytes, big-endian)
    packet[2] = (addr >> 24) & 0xFF;
    packet[3] = (addr >> 16) & 0xFF;
    packet[4] = (addr >> 8) & 0xFF;
    packet[5] = addr & 0xFF;
    
    // Packet type: 0x02 for data, 0x08 for end data
    packet[6] = is_last ? FINGERPRINT_ENDDATAPACKET : FINGERPRINT_DATAPACKET;
    
    // Length: data bytes + 2 checksum bytes
    uint16_t pkt_len_field = chunk_size + 2;
    packet[7] = (pkt_len_field >> 8) & 0xFF;
    packet[8] = pkt_len_field & 0xFF;
    
    // Data
    for (size_t i = 0; i < chunk_size; i++) {
      packet[9 + i] = template_data[written + i];
    }
    
    // Checksum: sum of type + length_high + length_low + data
    uint16_t checksum = packet[6] + packet[7] + packet[8];
    for (size_t i = 0; i < chunk_size; i++) {
      checksum += packet[9 + i];
    }
    packet[9 + chunk_size] = (checksum >> 8) & 0xFF;
    packet[9 + chunk_size + 1] = checksum & 0xFF;
    
    // Send raw packet
    size_t total_pkt_len = 9 + chunk_size + 2;
    mySerial.write(packet.data(), total_pkt_len);
    mySerial.flush();
    
    // Log full packet header for debugging (first 12 bytes)
    ESP_LOGI(TAG, "Upload PKT%d header: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (total=%d)",
             pkt_num, packet[0], packet[1], packet[2], packet[3], packet[4], packet[5],
             packet[6], packet[7], packet[8], packet[9], packet[10], packet[11], (int)total_pkt_len);
    
    written += chunk_size;
    delay(20);  // Small delay between packets
  }
  
  // Wait for sensor to process the data
  delay(100);
  
  // Check if sensor sent any response after receiving data
  // Some sensors send an ACK after the end data packet
  int avail = mySerial.available();
  if (avail > 0) {
    ESP_LOGD(TAG, "Sensor sent %d bytes after data transfer", avail);
    uint8_t resp[20];
    int resp_len = 0;
    while (mySerial.available() && resp_len < 20) {
      resp[resp_len++] = mySerial.read();
    }
    // Log first few bytes
    if (resp_len >= 10) {
      ESP_LOGD(TAG, "Response: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
               resp[0], resp[1], resp[2], resp[3], resp[4], resp[5], resp[6], resp[7], resp[8], resp[9]);
      // Check if it's an ACK packet with error code
      if (resp[0] == 0xEF && resp[1] == 0x01 && resp[6] == 0x07) {
        // ACK packet, byte 9 is the confirmation code
        ESP_LOGD(TAG, "Received ACK with code: 0x%02X", resp[9]);
        if (resp[9] != 0x00) {
          ESP_LOGW(TAG, "Sensor rejected template data with error: 0x%02X", resp[9]);
          this->mode_ = previous_mode;
          return false;
        }
      }
    }
  } else {
    ESP_LOGD(TAG, "No response from sensor after data transfer");
  }
  
  // Wait longer for sensor to fully process the template data
  delay(500);
  
  // First, try to delete any existing template at this ID (just in case)
  ESP_LOGD(TAG, "Deleting any existing template at ID %d before storing", id);
  uint8_t del_result = this->finger_->deleteModel(id);
  ESP_LOGD(TAG, "deleteModel(%d) returned: %d", id, del_result);
  delay(100);
  
  ESP_LOGD(TAG, "Attempting to store template to flash at ID %d", id);
  
  // Store the template from buffer to flash
  uint8_t result = this->finger_->storeModel(id);
  ESP_LOGI(TAG, "storeModel(%d) returned: %d", id, result);
  
  // If still failing, try next available ID
  if (result != FINGERPRINT_OK) {
    this->finger_->getTemplateCount();
    uint16_t next_id = this->finger_->templateCount + 1;
    ESP_LOGW(TAG, "Failed to store at ID %d, trying next free ID %d...", id, next_id);
    result = this->finger_->storeModel(next_id);
    ESP_LOGI(TAG, "storeModel(%d) returned: %d", next_id, result);
    if (result == FINGERPRINT_OK) {
      id = next_id;  // Use the ID that worked
    }
  }
  
  if (result != FINGERPRINT_OK) {
    ESP_LOGW(TAG, "Failed to store template at ID %d: error %d", id, result);
    
    // Try to get more info - attempt to read back the buffer
    ESP_LOGD(TAG, "Trying getModel() to check if buffer has data...");
    uint8_t check = this->finger_->getModel();
    ESP_LOGD(TAG, "getModel() returned: %d", check);
    
    this->mode_ = previous_mode;
    return false;
  }
  
  // Save the name
  this->save_fingerprint_name(id, name);
  this->finger_->getTemplateCount();
  
  ESP_LOGI(TAG, "Template uploaded and stored at ID %d with name '%s'", id, name.c_str());
  this->publish_last_action("Imported: " + name + " (ID " + std::to_string(id) + ")");
  
  this->mode_ = previous_mode;
  return true;
}

// ==================== UTILITIES ====================

uint16_t FingerprintDoorbell::get_enrolled_count() {
  return this->finger_ != nullptr ? this->finger_->templateCount : 0;
}

std::string FingerprintDoorbell::get_fingerprint_name(uint16_t id) {
  auto it = this->fingerprint_names_.find(id);
  return (it != this->fingerprint_names_.end()) ? it->second : "unknown";
}

std::string FingerprintDoorbell::get_fingerprint_list_json() {
  std::string json = "[";
  bool first = true;
  
  if (this->finger_ == nullptr || !this->sensor_connected_) {
    return "[]";
  }
  
  // Use cached fingerprint names instead of scanning all slots
  for (const auto& pair : this->fingerprint_names_) {
    if (!first) json += ",";
    json += "{\"id\":" + std::to_string(pair.first) + ",\"name\":\"" + pair.second + "\"}";
    first = false;
  }
  
  json += "]";
  return json;
}

void FingerprintDoorbell::update_touch_state(bool touched) {
  if ((touched != this->last_touch_state_) || (this->ignore_touch_ring_ != this->last_ignore_touch_ring_)) {
    if (touched) {
      this->set_led_ring_scanning();
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
    // When touch ring is ignored, use solid "on" mode instead of breathing
    this->finger_->LEDcontrol(FINGERPRINT_LED_ON, 0, this->led_ready_.color, 0);
  } else {
    this->finger_->LEDcontrol(this->led_ready_.mode, this->led_ready_.speed, this->led_ready_.color, 0);
  }
}

void FingerprintDoorbell::set_led_ring_error() {
  if (this->finger_ != nullptr)
    this->finger_->LEDcontrol(this->led_error_.mode, this->led_error_.speed, this->led_error_.color, 0);
}

void FingerprintDoorbell::set_led_ring_enroll() {
  if (this->finger_ != nullptr)
    this->finger_->LEDcontrol(this->led_enroll_.mode, this->led_enroll_.speed, this->led_enroll_.color, 0);
}

void FingerprintDoorbell::set_led_ring_match() {
  ESP_LOGD(TAG, "LED match: color=%d, mode=%d, speed=%d", this->led_match_.color, this->led_match_.mode, this->led_match_.speed);
  if (this->finger_ != nullptr)
    this->finger_->LEDcontrol(this->led_match_.mode, this->led_match_.speed, this->led_match_.color, 0);
}

void FingerprintDoorbell::set_led_ring_scanning() {
  ESP_LOGD(TAG, "LED scanning: color=%d, mode=%d, speed=%d", this->led_scanning_.color, this->led_scanning_.mode, this->led_scanning_.speed);
  if (this->finger_ != nullptr)
    this->finger_->LEDcontrol(this->led_scanning_.mode, this->led_scanning_.speed, this->led_scanning_.color, 0);
}

void FingerprintDoorbell::set_led_ring_no_match() {
  ESP_LOGD(TAG, "LED no_match: color=%d, mode=%d, speed=%d", this->led_no_match_.color, this->led_no_match_.mode, this->led_no_match_.speed);
  if (this->finger_ != nullptr)
    this->finger_->LEDcontrol(this->led_no_match_.mode, this->led_no_match_.speed, this->led_no_match_.color, 0);
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

void FingerprintDoorbell::publish_enroll_status(const std::string &status) {
  ESP_LOGI(TAG, "Enroll status: %s", status.c_str());
  if (this->enroll_status_sensor_ != nullptr) {
    this->enroll_status_sensor_->publish_state(status);
  }
}

void FingerprintDoorbell::publish_last_action(const std::string &action) {
  ESP_LOGI(TAG, "Action: %s", action.c_str());
  if (this->last_action_sensor_ != nullptr) {
    this->last_action_sensor_->publish_state(action);
  }
}

// ==================== REST API ====================

class FingerprintRequestHandler : public AsyncWebHandler {
 public:
  FingerprintRequestHandler(FingerprintDoorbell *parent) : parent_(parent) {}
  
  bool canHandle(AsyncWebServerRequest *request) const override {
    std::string url = request->url();
    return url.rfind("/fingerprint/", 0) == 0;  // starts_with equivalent
  }
  
  bool isRequestHandlerTrivial() const override { return false; }
  
  bool check_auth(AsyncWebServerRequest *request) const {
    std::string token = this->parent_->get_api_token();
    if (token.empty()) {
      return true;  // No token configured, allow access
    }
    
    // Check Authorization header: "Bearer <token>"
    auto auth_header = request->get_header("Authorization");
    if (!auth_header.has_value()) {
      return false;
    }
    
    std::string expected = "Bearer " + token;
    return auth_header.value() == expected;
  }
  
  void send_cors_response(AsyncWebServerRequest *request, int code, const char *content_type, const std::string &body) {
    AsyncWebServerResponse *response = request->beginResponse(code, content_type, body.c_str());
    // Note: Access-Control-Allow-Origin is added by ESPHome's web_server component
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    request->send(response);
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    // Handle CORS preflight
    if (request->method() == HTTP_OPTIONS) {
      auto *response = request->beginResponse(200, "text/plain", "");
      response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
      response->addHeader("Access-Control-Max-Age", "86400");
      request->send(response);
      return;
    }

    // Check authorization for all endpoints
    if (!this->check_auth(request)) {
      this->send_cors_response(request, 401, "application/json", "{\"error\":\"Unauthorized\"}");
      return;
    }
    
    std::string url = request->url();
    ESP_LOGD(TAG, "Request: %s %s", request->method() == HTTP_GET ? "GET" : (request->method() == HTTP_POST ? "POST" : "OTHER"), url.c_str());
    
    // GET /fingerprint/list - Get list of enrolled fingerprints
    if (url == "/fingerprint/list" && request->method() == HTTP_GET) {
      std::string json = this->parent_->get_fingerprint_list_json();
      this->send_cors_response(request, 200, "application/json", json);
      return;
    }
    
    // GET /fingerprint/status - Get current status
    if (url == "/fingerprint/status" && request->method() == HTTP_GET) {
      std::string json = "{";
      json += "\"connected\":" + std::string(this->parent_->is_sensor_connected() ? "true" : "false");
      json += ",\"enrolling\":" + std::string(this->parent_->is_enrolling() ? "true" : "false");
      json += ",\"count\":" + std::to_string(this->parent_->get_enrolled_count());
      json += "}";
      this->send_cors_response(request, 200, "application/json", json);
      return;
    }
    
    // POST /fingerprint/enroll?id=X&name=Y - Start enrollment
    if (url == "/fingerprint/enroll" && request->method() == HTTP_POST) {
      if (!request->hasParam("id") || !request->hasParam("name")) {
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"Missing id or name parameter\"}");
        return;
      }
      std::string id_str = request->getParam("id")->value();
      std::string name = request->getParam("name")->value();
      uint16_t id = std::atoi(id_str.c_str());
      
      if (id < 1 || id > 200) {
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"ID must be 1-200\"}");
        return;
      }
      
      this->parent_->start_enrollment(id, name);
      std::string response = "{\"status\":\"enrollment_started\",\"id\":" + std::to_string(id) + ",\"name\":\"" + name + "\"}";
      this->send_cors_response(request, 200, "application/json", response);
      return;
    }
    
    // POST /fingerprint/cancel - Cancel enrollment
    if (url == "/fingerprint/cancel" && request->method() == HTTP_POST) {
      this->parent_->cancel_enrollment();
      this->send_cors_response(request, 200, "application/json", "{\"status\":\"cancelled\"}");
      return;
    }
    
    // POST /fingerprint/delete?id=X - Delete fingerprint
    if (url == "/fingerprint/delete" && request->method() == HTTP_POST) {
      if (!request->hasParam("id")) {
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"Missing id parameter\"}");
        return;
      }
      std::string id_str = request->getParam("id")->value();
      uint16_t id = std::atoi(id_str.c_str());
      
      if (this->parent_->delete_fingerprint(id)) {
        std::string response = "{\"status\":\"deleted\",\"id\":" + std::to_string(id) + "}";
        this->send_cors_response(request, 200, "application/json", response);
      } else {
        this->send_cors_response(request, 500, "application/json", "{\"error\":\"Delete failed\"}");
      }
      return;
    }
    
    // POST /fingerprint/delete_all - Delete all fingerprints
    if (url == "/fingerprint/delete_all" && request->method() == HTTP_POST) {
      if (this->parent_->delete_all_fingerprints()) {
        this->send_cors_response(request, 200, "application/json", "{\"status\":\"all_deleted\"}");
      } else {
        this->send_cors_response(request, 500, "application/json", "{\"error\":\"Delete all failed\"}");
      }
      return;
    }
    
    // POST /fingerprint/rename?id=X&name=Y - Rename fingerprint
    if (url == "/fingerprint/rename" && request->method() == HTTP_POST) {
      if (!request->hasParam("id") || !request->hasParam("name")) {
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"Missing id or name parameter\"}");
        return;
      }
      std::string id_str = request->getParam("id")->value();
      std::string name = request->getParam("name")->value();
      uint16_t id = std::atoi(id_str.c_str());
      
      if (this->parent_->rename_fingerprint(id, name)) {
        std::string response = "{\"status\":\"renamed\",\"id\":" + std::to_string(id) + ",\"name\":\"" + name + "\"}";
        this->send_cors_response(request, 200, "application/json", response);
      } else {
        this->send_cors_response(request, 500, "application/json", "{\"error\":\"Rename failed\"}");
      }
      return;
    }
    
    // GET /fingerprint/template?id=X - Export fingerprint template as base64
    if (url == "/fingerprint/template" && request->method() == HTTP_GET) {
      if (!request->hasParam("id")) {
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"Missing id parameter\"}");
        return;
      }
      std::string id_str = request->getParam("id")->value();
      uint16_t id = std::atoi(id_str.c_str());
      
      std::vector<uint8_t> template_data;
      if (this->parent_->get_template(id, template_data)) {
        // Base64 encode the template data
        std::string base64 = this->base64_encode(template_data);
        std::string name = this->parent_->get_fingerprint_name(id);
        std::string response = "{\"id\":" + std::to_string(id) + ",\"name\":\"" + name + "\",\"template\":\"" + base64 + "\"}";
        this->send_cors_response(request, 200, "application/json", response);
      } else {
        this->send_cors_response(request, 500, "application/json", "{\"error\":\"Failed to export template\"}");
      }
      return;
    }
    
    // POST /fingerprint/template - Import fingerprint template
    // Body: application/x-www-form-urlencoded with id, name, template fields
    if (url == "/fingerprint/template" && request->method() == HTTP_POST) {
      // Use hasArg/arg for POST body parameters (form-urlencoded)
      bool has_id = request->hasArg("id");
      bool has_name = request->hasArg("name");
      bool has_template = request->hasArg("template");
      
      if (!has_id || !has_name || !has_template) {
        ESP_LOGW(TAG, "Import request missing parameters: id=%d name=%d template=%d",
                 has_id, has_name, has_template);
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"Missing id, name, or template parameter\"}");
        return;
      }
      
      std::string id_str = request->arg("id");
      std::string name = request->arg("name");
      std::string template_base64 = request->arg("template");
      uint16_t id = std::atoi(id_str.c_str());
      
      ESP_LOGI(TAG, "Import request: id=%d name='%s' template_len=%d", id, name.c_str(), template_base64.length());
      
      // Decode base64
      std::vector<uint8_t> template_data = this->base64_decode(template_base64);
      
      if (template_data.empty()) {
        ESP_LOGW(TAG, "Failed to decode base64 template (len=%d)", template_base64.length());
        this->send_cors_response(request, 400, "application/json", "{\"error\":\"Invalid base64 template data\"}");
        return;
      }
      
      ESP_LOGI(TAG, "Decoded template: %d bytes", template_data.size());
      
      if (this->parent_->upload_template(id, name, template_data)) {
        std::string response = "{\"status\":\"imported\",\"id\":" + std::to_string(id) + ",\"name\":\"" + name + "\"}";
        this->send_cors_response(request, 200, "application/json", response);
      } else {
        this->send_cors_response(request, 500, "application/json", "{\"error\":\"Failed to import template\"}");
      }
      return;
    }
    
    this->send_cors_response(request, 404, "application/json", "{\"error\":\"Unknown endpoint\"}");
  }
  
  // Base64 encoding
  std::string base64_encode(const std::vector<uint8_t> &data) const {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);
    
    for (size_t i = 0; i < data.size(); i += 3) {
      uint32_t n = static_cast<uint32_t>(data[i]) << 16;
      if (i + 1 < data.size()) n |= static_cast<uint32_t>(data[i + 1]) << 8;
      if (i + 2 < data.size()) n |= static_cast<uint32_t>(data[i + 2]);
      
      result += chars[(n >> 18) & 0x3F];
      result += chars[(n >> 12) & 0x3F];
      result += (i + 1 < data.size()) ? chars[(n >> 6) & 0x3F] : '=';
      result += (i + 2 < data.size()) ? chars[n & 0x3F] : '=';
    }
    return result;
  }
  
  // Base64 decoding
  std::vector<uint8_t> base64_decode(const std::string &input) const {
    static const int decode_table[256] = {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
      52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
      -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
      15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
      -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
      41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    std::vector<uint8_t> result;
    result.reserve(input.size() * 3 / 4);
    
    uint32_t val = 0;
    int bits = 0;
    
    for (char c : input) {
      if (c == '=') break;
      // URL encoding converts '+' to space, so treat space as '+'
      if (c == ' ') c = '+';
      int d = decode_table[static_cast<uint8_t>(c)];
      if (d < 0) continue;
      
      val = (val << 6) | d;
      bits += 6;
      
      if (bits >= 8) {
        bits -= 8;
        result.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
      }
    }
    return result;
  }
  
 protected:
  FingerprintDoorbell *parent_;
};

void FingerprintDoorbell::setup_web_server() {
  auto *base = web_server_base::global_web_server_base;
  if (base == nullptr) {
    ESP_LOGW(TAG, "WebServerBase not found, REST API disabled");
    return;
  }
  
  base->init();
  base->add_handler(new FingerprintRequestHandler(this));
  ESP_LOGI(TAG, "REST API registered at /fingerprint/*");
}

}  // namespace fingerprint_doorbell
}  // namespace esphome
