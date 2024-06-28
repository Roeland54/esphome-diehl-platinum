#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace diehl {

class DiehlComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_time(time::RealTimeClock *time) { time_ = time; }
  time::RealTimeClock *get_time() const { return time_; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_day_energy_sensor(sensor::Sensor *sensor) { this->day_energy_sensor_ = sensor; }

  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;
  void setup() override;
  void update() override;

 protected:
  time::RealTimeClock *time_;

  // Sensors
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *day_energy_sensor_{nullptr};

  // Methods
  void update_time();
  void clear_uart_rx_buffer_();
  void calc_checksum(int len);
  void fetch_data(int index);
  int convert_to_seconds(int hour, int min, int sec);
  void find_last_index_based_on_time();
};

}  // namespace diehl
}  // namespace esphome
