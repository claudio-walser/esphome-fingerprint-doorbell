#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <map>
#include <Adafruit_Fingerprint.h>

namespace esphome {
namespace fingerprint_doorbell {

enum class ScanResult { NO_FINGER, MATCH_FOUND, NO_MATCH_FOUND, ERROR };
enum class Mode { SCAN, ENROLL, IDLE };
enum class EnrollStep { IDLE, WAITING_FOR_FINGER, CAPTURING, CONVERTING, WAITING_REMOVE, CREATING_MODEL, STORING, DONE, ERROR };

struct Match {
  ScanResult scan_result = ScanResult::NO_FINGER;
  uint16_t match_id = 0;
  std::string match_name = "unknown";
  uint16_t match_confidence = 0;
  uint8_t return_code = 0;
};

class FingerprintDoorbell : public Component {
 public:
  FingerprintDoorbell() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  // Configuration setters
  void set_touch_pin(GPIOPin *pin) { touch_pin_ = pin; }
  void set_doorbell_pin(GPIOPin *pin) { doorbell_pin_ = pin; }
  void set_ignore_touch_ring(bool ignore) { ignore_touch_ring_ = ignore; }

  // Sensor setters
  void set_match_id_sensor(sensor::Sensor *sensor) { match_id_sensor_ = sensor; }
  void set_confidence_sensor(sensor::Sensor *sensor) { confidence_sensor_ = sensor; }
  void set_match_name_sensor(text_sensor::TextSensor *sensor) { match_name_sensor_ = sensor; }
  void set_ring_sensor(binary_sensor::BinarySensor *sensor) { ring_sensor_ = sensor; }
  void set_finger_sensor(binary_sensor::BinarySensor *sensor) { finger_sensor_ = sensor; }
  void set_enroll_status_sensor(text_sensor::TextSensor *sensor) { enroll_status_sensor_ = sensor; }
  void set_last_action_sensor(text_sensor::TextSensor *sensor) { last_action_sensor_ = sensor; }

  // Public methods for HA services and REST API
  void start_enrollment(uint16_t id, const std::string &name);
  void cancel_enrollment();
  bool delete_fingerprint(uint16_t id);
  bool delete_all_fingerprints();
  bool rename_fingerprint(uint16_t id, const std::string &new_name);
  void set_ignore_touch_ring_state(bool state) { ignore_touch_ring_ = state; }
  uint16_t get_enrolled_count();
  std::string get_fingerprint_name(uint16_t id);
  std::string get_fingerprint_list_json();
  bool is_enrolling() { return mode_ == Mode::ENROLL; }
  bool is_sensor_connected() { return sensor_connected_; }

 protected:
  GPIOPin *touch_pin_{nullptr};
  GPIOPin *doorbell_pin_{nullptr};
  bool ignore_touch_ring_{false};
  bool last_ignore_touch_ring_{false};

  // Sensors
  sensor::Sensor *match_id_sensor_{nullptr};
  sensor::Sensor *confidence_sensor_{nullptr};
  text_sensor::TextSensor *match_name_sensor_{nullptr};
  binary_sensor::BinarySensor *ring_sensor_{nullptr};
  binary_sensor::BinarySensor *finger_sensor_{nullptr};
  text_sensor::TextSensor *enroll_status_sensor_{nullptr};
  text_sensor::TextSensor *last_action_sensor_{nullptr};

  // Internal state
  Adafruit_Fingerprint *finger_{nullptr};
  HardwareSerial *hw_serial_{nullptr};
  bool sensor_connected_{false};
  bool last_touch_state_{false};
  std::map<uint16_t, std::string> fingerprint_names_;
  
  uint32_t last_match_time_{0};
  uint32_t last_ring_time_{0};
  uint16_t last_match_id_{0};
  uint32_t last_connect_attempt_{0};
  uint8_t connect_attempts_{0};

  // Enrollment state machine
  Mode mode_{Mode::SCAN};
  EnrollStep enroll_step_{EnrollStep::IDLE};
  uint16_t enroll_id_{0};
  std::string enroll_name_;
  uint8_t enroll_sample_{0};
  uint32_t enroll_timeout_{0};

  // Internal methods
  bool connect_sensor();
  Match scan_fingerprint();
  void process_enrollment();
  void update_touch_state(bool touched);
  bool is_ring_touched();
  void set_led_ring_ready();
  void set_led_ring_error();
  void set_led_ring_enroll();
  void load_fingerprint_names();
  void save_fingerprint_name(uint16_t id, const std::string &name);
  void delete_fingerprint_name(uint16_t id);
  void publish_enroll_status(const std::string &status);
  void publish_last_action(const std::string &action);
  void setup_web_server();
};

// ==================== AUTOMATION ACTIONS ====================

template<typename... Ts>
class EnrollAction : public Action<Ts...>, public Parented<FingerprintDoorbell> {
 public:
  TEMPLATABLE_VALUE(uint16_t, finger_id)
  TEMPLATABLE_VALUE(std::string, name)

  void play(Ts... x) override {
    this->parent_->start_enrollment(this->finger_id_.value(x...), this->name_.value(x...));
  }
};

template<typename... Ts>
class CancelEnrollAction : public Action<Ts...>, public Parented<FingerprintDoorbell> {
 public:
  void play(Ts... x) override {
    this->parent_->cancel_enrollment();
  }
};

template<typename... Ts>
class DeleteAction : public Action<Ts...>, public Parented<FingerprintDoorbell> {
 public:
  TEMPLATABLE_VALUE(uint16_t, finger_id)

  void play(Ts... x) override {
    this->parent_->delete_fingerprint(this->finger_id_.value(x...));
  }
};

template<typename... Ts>
class DeleteAllAction : public Action<Ts...>, public Parented<FingerprintDoorbell> {
 public:
  void play(Ts... x) override {
    this->parent_->delete_all_fingerprints();
  }
};

template<typename... Ts>
class RenameAction : public Action<Ts...>, public Parented<FingerprintDoorbell> {
 public:
  TEMPLATABLE_VALUE(uint16_t, finger_id)
  TEMPLATABLE_VALUE(std::string, name)

  void play(Ts... x) override {
    this->parent_->rename_fingerprint(this->finger_id_.value(x...), this->name_.value(x...));
  }
};

}  // namespace fingerprint_doorbell
}  // namespace esphome
