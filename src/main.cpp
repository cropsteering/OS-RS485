/**
 * @file main.cpp
 * @author Jamie Howse (r4wknet@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-08-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/** Include libraries here */
#include <Arduino.h>
#include <ArduinoRS485.h>
#include <vector>
#include <MQTT.h>
#include <logger.h>
#include <Preferences.h>

/** MQTT Lib */
MQTT mqtt_lib;
/** Logger Lib */
LOGGER logger_lib;
/** Preferences instance */
Preferences flash_storage;
/** RS485 reply message que */
std::vector<int> reply_que;
/** RS485 send que */
std::vector<char*> send_que;
/** Read time interval */
uint64_t delay_time;
/** Set sensor baud rate */
uint32_t baud_rate = 4800;
/** Is RS485 busy */
bool busy = false;
/** Sensor read count */
uint8_t sensor_count = 0;
/** Turn on/off debug output */
#define DEBUG 1

/** Forward declaration */
void rs485_send();
void rs485_read();
void R_LOG(String chan, String data);

/**
 * @brief Setup firmware
 * 
 */
void setup()
{
    /** Setup WisBlock pins/io slots and serial */
    pinMode(WB_IO2, OUTPUT);
    digitalWrite(WB_IO2, HIGH); 
    delay(500);

    time_t timeout = millis();
    Serial.begin(115200);
    while (!Serial)
    {
        if ((millis() - timeout) < 5000)
        {
            delay(100);
        } else {
            break;
        }
    }

    /** Initialize flash storage */
    R_LOG("FLASH", "Starting flash storage");
    flash_storage.begin("RS485", false);
    delay_time = flash_storage.getULong64("period", 15000000);
    R_LOG("FLASH", "Read: Delay time " + String(delay_time));
    CSV = flash_storage.getBool("csv", true);
    R_LOG("FLASH", "Read: CSV " + String(CSV));
    use_sd = flash_storage.getBool("sd", false);
    R_LOG("FLASH", "Read: SD " + String(use_sd));
    gmtoffset_sec = flash_storage.getInt("gmt", -12600);
    R_LOG("FLASH", "Read: GMT " + String(gmtoffset_sec));
    daylightoffset_sec = flash_storage.getUInt("dst", 3600);
    R_LOG("FLASH", "Read: DST " + String(daylightoffset_sec));

    /** 
     * Join WiFi and connect to MQTT 
     * Setup logger
     * 
     */
    mqtt_lib.mqtt_setup();
    logger_lib.logger_setup();

    /** 
     * Setup RS485
     * TODO: Make BAUD rate configurable
     *       via MQTT
     */
    R_LOG("RS485", "Starting bus " + String(baud_rate));
    RS485.begin(baud_rate);
    RS485.receive();
}

/**
 * @brief Main loop
 * 
 */
void loop() 
{
    /** Loop our MQTT lib */
    mqtt_lib.mqtt_loop();

    if(RS485.available())
    {
        reply_que.push_back(RS485.read());
    }

    static uint32_t last_time;
    if ((micros() - last_time) >= delay_time && !busy)
    {
        last_time = micros();
        rs485_send();
    }
}

/**
 * @brief Send messages to sensors
 * 
 */
void rs485_send()
{
    busy = true;
    char read_temp[] = {0x01, 0x04, 0x00, 0x01, 0x00, 0x01, 0x60, 0x0a};
    char read_humi[] = {0x01, 0x04, 0x00, 0x02, 0x00, 0x01, 0x90, 0x0A};
    char read_thcs[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xCB};
    //send_que.push_back(read_temp);
    //send_que.push_back(read_humi);
    send_que.push_back(read_thcs);

    size_t size = send_que.size();

    if(sensor_count != size)
    {
        RS485.beginTransmission();
        RS485.write(send_que[sensor_count++], 8);
        RS485.endTransmission();
        delay(250);
        rs485_read();
    } else {
        sensor_count = 0;
        RS485.beginTransmission();
        RS485.write(send_que[sensor_count++], 8);
        RS485.endTransmission();
        delay(250);
        rs485_read();
    }

    send_que.clear();
    busy = false;
}

/**
 * @brief Read reply of sensors
 * 
 */
void rs485_read()
{
    size_t que_size = reply_que.size();
    if(que_size > 0)
    {
        uint8_t addr = reply_que[0];
        uint8_t num_bytes = reply_que[2];
        std::vector<uint8_t> temp;
        String sensor_data;
        if(num_bytes < 2)
        {
            temp.push_back(reply_que[3]);
        } else if (num_bytes == 2) {
            temp.push_back(reply_que[3]);
            temp.push_back(reply_que[4]);
        } else if (num_bytes > 2) {
            size_t size = num_bytes;
            for(int x = 0; x < size/2; x++)
            {
              temp.push_back(reply_que[3+(x+x)]);
              temp.push_back(reply_que[4+(x+x)]);
            }
        }

        size_t data_size = temp.size();
        if(data_size < 2)
        {
            sensor_data += String(temp[0]);
        } else {
            for(int y = 0; y < data_size/2; y++)
            {
              uint8_t high_b = temp[y+y];
              uint8_t low_b = temp[(y+y)+1];
              uint16_t result = (high_b << 8) | low_b;
              float resultf = result / 10.0;
              if(y == (data_size/2)-1)
              {
                  sensor_data += String(resultf);
              } else {
                  sensor_data += String(resultf) + ", ";
              }
            }
        }
        /** Send MQTT here */
        R_LOG("RS485", sensor_data);
        mqtt_lib.mqtt_publish(String(addr), sensor_data);
        logger_lib.write_sd(sensor_data);

        reply_que.clear();
    }
}

/**
 * @brief Save key:value data to flash
 * 
 * @param key char
 * @param value uint32_t
 * @param restart restart SDI-12 sensor lookup
 */
void flash_32(const char* key, int32_t value, bool restart)
{
    flash_storage.putInt(key, value);
    R_LOG("FLASH", "Write: " + String(key) + "/" + String(value));
    if(restart) { }
}

/**
 * @brief Save key:value data to flash
 * 
 * @param key char
 * @param value uint32_t
 * @param restart restart SDI-12 sensor lookup
 */
void flash_32u(const char* key, uint32_t value, bool restart)
{
    flash_storage.putUInt(key, value);
    R_LOG("FLASH", "Write: " + String(key) + "/" + String(value));
    if(restart) { }
}

/**
 * @brief Save key:value data to flash
 * 
 * @param key char
 * @param value uint64_t
 * @param restart restart SDI-12 sensor lookup
 */
void flash_64u(const char* key, uint64_t value, bool restart)
{
    flash_storage.putULong64(key, value);
    R_LOG("FLASH", "Write: " + String(key) + "/" + String(value));
    if(restart) { }
}

/**
 * @brief Save key:value data to flash
 * 
 * @param key char
 * @param value bool
 * @param restart restart SDI-12 sensor lookup
 */
void flash_bool(const char* key, bool value, bool restart)
{
    flash_storage.putBool(key, value);
    R_LOG("FLASH", "Write: " + String(key) + "/" + String(value));
    if(restart) { }
}

/**
 * @brief 
 * 
 * @param chan Output channel
 * @param data String to output
 */
void R_LOG(String chan, String data)
{
    #if DEBUG
    String disp = "["+chan+"] " + data;
    Serial.println(disp);
    #endif
}
