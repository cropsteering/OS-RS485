/**
 * @file MQTT.h
 * @author Jamie Howse (r4wknet@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __MQTT_H__
#define __MQTT_H__

#include <map>
#include <vector>

/**
 * @brief MQTT Lib
 * 
 */
class MQTT
{
    public:
    void mqtt_setup();
    void mqtt_loop();
    void mqtt_publish(String addr, String data);
};

/** Overloads for config */
extern uint64_t delay_time;
extern bool CSV;
extern bool give_up;
extern bool use_sd;
extern std::vector<std::array<uint8_t, 8>> send_que;
extern uint8_t read_num;
void chng_addr(String addr_old, String addr_new);
void flash_32(const char* key, int32_t value, bool restart);
void flash_32u(const char* key, uint32_t value, bool restart);
void flash_64u(const char* key, uint64_t value, bool restart);
void flash_bool(const char* key, bool value, bool restart);
void flash_bytes(const char* key, uint8_t value[8], bool restart);
void send_onetime(uint8_t value[8]);

#endif
