#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <Adafruit_Fingerprint.h>

namespace esphome {
namespace fingerprint_doorbell {

enum class ScanResult { NO_FINGER, MATCH_FOUND, NO_MATCH_FOUND, ERROR };

struct Match {
  ScanResult scan_result = ScanResult::NO_FINGER;
  uint16_t match_id = 0;
  std::string match_name = "unknown";
  uint16_t match_confidence = 0;
  uint8_t return_code = 0;
};

class FingerprintDoorbell : public Component, public uart::UARTDevice {
 public:
  FingerprintDoorbell() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

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

  // Public methods for services
  bool enroll_fingerprint(uint16_t id, const std::string &name);
  bool delete_fingerprint(uint16_t id);
  bool delete_all_fingerprints();
  bool rename_fingerprint(uint16_t id, const std::string &new_name);
  void set_ignore_touch_ring_state(bool state) { ignore_touch_ring_ = state; }
  uint16_t get_enrolled_count();
  std::string get_fingerprint_name(uint16_t id);

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

  // Internal state
  Adafruit_Fingerprint *finger_{nullptr};
  HardwareSerial *hw_serial_{nullptr};
  bool sensor_connected_{false};
  bool last_touch_state_{false};
  std::map<uint16_t, std::string> fingerprint_names_;
  
  uint32_t last_match_time_{0};
  uint32_t last_ring_time_{0};
  uint16_t last_match_id_{0};

  // Internal methods
  bool connect_sensor();
  Match scan_fingerprint();
  void update_touch_state(bool touched);
  bool is_ring_touched();
  void set_led_ring_ready();
  void set_led_ring_error();
  void load_fingerprint_names();
  void save_fingerprint_name(uint16_t id, const std::string &name);
  void delete_fingerprint_name(uint16_t id);
};

}  // namespace fingerprint_doorbell
}  // namespace esphome
