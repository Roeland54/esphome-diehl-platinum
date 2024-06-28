#include "diehl.h"
#include <cstring>
#include "esphome/core/log.h"

namespace esphome {
namespace diehl {

unsigned char tx_msg[11];
unsigned char rx_msg[30];
int current_day_of_year = 0;
int last_valid_index = 0;
bool end_of_list = false;
bool finding_last_index = true;
static const char *const TAG = "diehl";

int crc16tab[] = { 0, 4129, 8258, 12387, 16516, 
20645, 24774, 28903, 33032, 37161, 41290, 45419, 49548, 
53677, 57806, 61935, 4657, 528, 12915, 8786, 21173, 
17044, 29431, 25302, 37689, 33560, 45947, 41818, 54205, 
50076, 62463, 58334, 9314, 13379, 1056, 5121, 25830, 
29895, 17572, 21637, 42346, 46411, 34088, 38153, 58862, 
62927, 50604, 54669, 13907, 9842, 5649, 1584, 30423, 
26358, 22165, 18100, 46939, 42874, 38681, 34616, 63455, 
59390, 55197, 51132, 18628, 22757, 26758, 30887, 2112, 
6241, 10242, 14371, 51660, 55789, 59790, 63919, 35144, 
39273, 43274, 47403, 23285, 19156, 31415, 27286, 6769, 
2640, 14899, 10770, 56317, 52188, 64447, 60318, 39801, 
35672, 47931, 43802, 27814, 31879, 19684, 23749, 11298, 
15363, 3168, 7233, 60846, 64911, 52716, 56781, 44330, 
48395, 36200, 40265, 32407, 28342, 24277, 20212, 15891, 
11826, 7761, 3696, 65439, 61374, 57309, 53244, 48923, 
44858, 40793, 36728, 37256, 33193, 45514, 41451, 53516, 
49453, 61774, 57711, 4224, 161, 12482, 8419, 20484, 
16421, 28742, 24679, 33721, 37784, 41979, 46042, 49981, 
54044, 58239, 62302, 689, 4752, 8947, 13010, 16949, 
21012, 25207, 29270, 46570, 42443, 38312, 34185, 62830, 
58703, 54572, 50445, 13538, 9411, 5280, 1153, 29798, 
25671, 21540, 17413, 42971, 47098, 34713, 38840, 59231, 
63358, 50973, 55100, 9939, 14066, 1681, 5808, 26199, 
30326, 17941, 22068, 55628, 51565, 63758, 59695, 39368, 
35305, 47498, 43435, 22596, 18533, 30726, 26663, 6336, 
2273, 14466, 10403, 52093, 56156, 60223, 64286, 35833, 
39896, 43963, 48026, 19061, 23124, 27191, 31254, 2801, 
6864, 10931, 14994, 64814, 60687, 56684, 52557, 48554, 
44427, 40424, 36297, 31782, 27655, 23652, 19525, 15522, 
11395, 7392, 3265, 61215, 65342, 53085, 57212, 44955, 
49082, 36825, 40952, 28183, 32310, 20053, 24180, 11923, 
16050, 3793, 7920 };

void DiehlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "diehl:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with diehl inverter failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Energy", this->day_energy_sensor_);

  this->check_uart_settings(19200);
}

float DiehlComponent::get_setup_priority() const { return setup_priority::DATA; }

void DiehlComponent::setup() {
  ESP_LOGI(TAG, "setup called");

  tx_msg[0] = -59;
  tx_msg[1] = 19;                   
  tx_msg[2] = 3;           
  tx_msg[3] = 0;           
  tx_msg[4] = 0;           
  tx_msg[5] = -61;    

  calc_checksum(6);
  this->write_array(tx_msg, 8);
  clear_uart_rx_buffer_();

  tx_msg[0] = 45;
  tx_msg[1] = 19;
  tx_msg[2] = 6;
  tx_msg[3] = 18;

  ESP_LOGI(TAG, "setup done");
}

void DiehlComponent::update() {
  if (finding_last_index){
    return;
  }
  ESP_LOGI(TAG, "update called");
  update_time();

  end_of_list = false;
  for (int i = (last_valid_index + 1); !end_of_list; i++) {
    fetch_data(i);
  }

	float power = 0;
  if ((rx_msg[17] * 256 + rx_msg[18]) != 0) {
    power = (rx_msg[17]*256+rx_msg[18])/10.0;
  }

	float day_energy = rx_msg[19]*256*256*256+rx_msg[20]*256*256+rx_msg[21]*256+rx_msg[22];

  this->power_sensor_->publish_state(power);
  if (day_energy != 0.0){
    this->day_energy_sensor_->publish_state(day_energy);
  }
}

void DiehlComponent::loop() {
  if (finding_last_index){
    if (!time_->now().is_valid()){
      return;
    }

    update_time();

    ESP_LOGI(TAG, "finding last index");
    ESP_LOGI(TAG, "finding last index: %i", last_valid_index);
    fetch_data(last_valid_index + 1);

    find_last_index_based_on_time();
  }
}

void DiehlComponent::find_last_index_based_on_time() {
  int hour = rx_msg[4];
  int minute = rx_msg[5];
  int second = rx_msg[6];

  if (hour == 0 && minute == 0 && second == 0){
    return;
  }

  ESP_LOGI(TAG, "received timestamp: %i:%i:%i", hour, minute, second);

  ESPTime time = time_->now();
  int current_time_seconds = convert_to_seconds(time.hour, time.minute, time.second);
  int timestamp_seconds = convert_to_seconds(hour, minute, second);

  int indexes = (current_time_seconds - timestamp_seconds) / 31;

  ESP_LOGI(TAG, "index diff: %i", indexes);

  if (0 > indexes){
    return;
  }

  last_valid_index = last_valid_index + indexes - 6;
  ESP_LOGI(TAG, "new last valid index: %i", last_valid_index);
  finding_last_index = false;
}

void DiehlComponent::update_time() {
  ESPTime time = time_->now();

  if(current_day_of_year != time.day_of_year)
  {
    last_valid_index = 0; //reset the index

    tx_msg[4] = (time.year - 2000) & 0xFF;
    tx_msg[5] = time.month;
    tx_msg[6] = time.day_of_month;

    ESP_LOGI(TAG, "current date: %d/%d/%d", tx_msg[4], tx_msg[5], tx_msg[6]);

    current_day_of_year = time.day_of_year;
  }
}

void DiehlComponent::fetch_data(int index) { 
  ESP_LOGI(TAG, "fetching data for index: %i", index);

  unsigned char diH = (unsigned char)((index & 0xFF00) >> 8);
  unsigned char diL = (unsigned char)(index & 0xFF);
  tx_msg[7] = diH;
  tx_msg[8] = diL;

  calc_checksum(9);
  clear_uart_rx_buffer_();
  this->write_array(tx_msg, 11);

  unsigned char buffer[30] = {0};
  int buffer_len = 0;
  int data;
  int timeout = 50;  // ms

  // Read the data from the UART
  while (timeout > 0) {
    if (this->available()) {
      data = this->read();
      if (data > -1){
        buffer[buffer_len] = (uint8_t) data;
        buffer_len++;
      }
    } else {
      delay(1);
      timeout--;
    }
  }

  // Create a string to hold the formatted output
  char log_buffer[5 * 30 + 1]; // Each byte needs 2 chars, plus a space, plus a null terminator

  // Fill the string with formatted byte values
  char *ptr = log_buffer;
  for (int i = 0; i < 30; i++) {
      ptr += sprintf(ptr, "%d ", buffer[i]);
  }
  // Log the entire buffer at once
  ESP_LOGI(TAG, "response: %s", log_buffer);

  if (buffer[0] == 132 || buffer[0] == 0){
    end_of_list = true;
    finding_last_index = false;
    return;
  }

  last_valid_index = index;
  memcpy(rx_msg, buffer, 30);
}

void DiehlComponent::clear_uart_rx_buffer_() {
  uint8_t tmp;
  while (this->available()) {
    this->read_byte(&tmp);
  }
}

void DiehlComponent::calc_checksum(int len) {
	int crc = 65535; 
	//unsigned char rValue[2];

	for (int RS_index = 0; RS_index < len; RS_index++) {
		int dataInt = tx_msg[RS_index] & 0xFF;
		int rIndex = dataInt ^ (crc & 0xFFFF) >> 8;
		rIndex &= 65535;
		crc = crc16tab[rIndex] ^ (crc & 0xFFFF) << 8;
		crc &= 65535;
	}
  crc &= 65535;
	tx_msg[len] = (crc & 0xFF00) >> 8;
	tx_msg[len + 1] = (crc & 0xFF);
}

int DiehlComponent::convert_to_seconds(int hour, int minute, int second) {
  return hour * 3600 + minute * 60 + second;
}
}  // namespace diehl
}  // namespace esphome
