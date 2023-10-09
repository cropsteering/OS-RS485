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
std::vector<std::array<uint8_t, 8>> send_que;
/** Read time interval */
uint64_t delay_time;
/** Set sensor baud rate */
uint32_t baud_rate;
/** Is RS485 busy */
bool busy = false;
/** Sensor read count */
uint8_t sensor_count;
/** Number of read messages */
uint8_t read_num;
/** Retry one time message? */
bool onetime_retry;
/** One time message */
uint8_t onetime_msg[8];
/** Turn on/off debug output */
#define DEBUG 1

/** Forward declaration */
void rs485_send();
void rs485_read(bool mqtt_send);
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
    baud_rate = flash_storage.getUInt("baud", 4800);
    R_LOG("FLASH", "Read: Baud rate " + String(baud_rate));
    read_num = flash_storage.getUInt("rnum", 0);
    R_LOG("FLASH", "Read: Read number " + String(read_num));
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

    for(int x = 0; x < read_num; x++)
    {
        uint8_t temp[8];
        std::array<uint8_t, 8> temp_array;
        String msg_name = "msg" + String(x+1);
        flash_storage.getBytes(msg_name.c_str(), &temp, sizeof(temp));
        for(int y = 0; y < 8; y++)
        {
            temp_array[y] = temp[y];
        }
        R_LOG("FLASH", "Read: MSG " + msg_name);
        send_que.push_back(temp_array);
    }

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
 * These messages are repeated every X
 * 
 */
void rs485_send()
{
    busy = true;
    size_t size = send_que.size();
    if(size > 0) 
    {
        R_LOG("RS485", "Sending RS485 message");
        if(sensor_count != size)
        {
            uint8_t temp_array[8];
            for(int x = 0; x < 8; x++)
            {
                temp_array[x] = send_que[sensor_count][x];
            }
            RS485.beginTransmission();
            RS485.write(temp_array, 8);
            RS485.endTransmission();
            delay(250);
            rs485_read(true);
            sensor_count++;
        } else {
            sensor_count = 0;
            uint8_t temp_array[8];
            for(int x = 0; x < 8; x++)
            {
                temp_array[x] = send_que[sensor_count][x];
            }
            RS485.beginTransmission();
            RS485.write(temp_array, 8);
            RS485.endTransmission();
            delay(250);
            rs485_read(true);
            sensor_count++;
        }
    }
    busy = false;
    if(onetime_retry) { send_onetime(onetime_msg); }
}

/**
 * @brief Send RS485 message one time
 * Used for things like config settings
 * Nothing sent to logger or MQTT
 * 
 * @param value 
 */
void send_onetime(uint8_t value[8])
{
    if(!busy)
    {
        R_LOG("RS485", "Sending one time message");
        RS485.beginTransmission();
        RS485.write(value, 8);
        RS485.endTransmission();
        onetime_retry = false;
        delay(250);
        rs485_read(false);
    } else {
        R_LOG("RS485", "Busy, caching one time message");
        for(int x = 0; x < 8; x++)
        {
            onetime_msg[x] = value[x];
        }
        onetime_retry = true;
    }
}

/**
 * @brief Read reply of sensors
 * 
 */
void rs485_read(bool mqtt_send)
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
        if(mqtt_send)
        {
            mqtt_lib.mqtt_publish(String(addr), sensor_data);
            logger_lib.write_sd(sensor_data);
        }

        reply_que.clear();
    }
}

/**
 * @brief Save key:value data to flash
 * 
 * @param key char
 * @param value uint32_t
 * @param restart unused
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
 * @param restart unused
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
 * @param restart unused
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
 * @param restart unused
 */
void flash_bool(const char* key, bool value, bool restart)
{
    flash_storage.putBool(key, value);
    R_LOG("FLASH", "Write: " + String(key) + "/" + String(value));
    if(restart) { }
}

/**
 * @brief Save key:value data to flash
 * 
 * @param key char
 * @param value bool
 * @param restart unused
 */
void flash_bytes(const char* key, uint8_t value[8], bool restart)
{
    flash_storage.putBytes(key, value, 8);
    R_LOG("FLASH", "Write: " + String(key));
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
