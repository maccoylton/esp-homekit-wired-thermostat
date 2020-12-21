#ifndef TUYA_MCU
#define YUYA_MCU

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>

static const uint8_t MAX_BUFFER_LENGTH = 128;
static const uint8_t TUYA_MCU_HEADER_SIZE = 7;


enum message_indexes
{
    E_MAGIC1 = 0,
    E_MAGIC2 = 1,
    E_VERSION = 2,
    E_CMD = 3,
    E_PAYLOAD_LENGTH_HI = 4,
    E_PAYLOAD_LENGTH_LO = 5,
    E_PAYLOAD = 6
};

uint8_t tuya_mcu_get_msg_length(uint8_t msg[]);
/* calculate the message legnth form the heaer length + the payload length */

uint8_t tuya_mcu_calc_checksum(uint8_t msg[]);
/* calculate the checksum for a message, returns the checksum */

void tuya_mcu_set_checksum(uint8_t msg[]);
/* populates the message with the checksum */


bool tuya_mcu_message_is_valid(uint8_t msg[]);
/* returns treue if header magic bytes are set correctly and calculated checksum matches message contents */

uint8_t tuya_mcu_get_command(uint8_t msg[]);
/* returns the command contained in message */

uint8_t tuya_mcu_get_payload_length(uint8_t msg[]);
/* returns the legth of the payload contained in msg */

void tuya_mcu_set_payload_length(uint8_t msg[], uint8_t payload_length);
/* set the leggth of the payload in message  */

void tuya_mcu_send_message(uint8_t cmd, uint8_t payload[], uint8_t payload_length);
/* screate a message for cmnd and add the data payload  */

void tuya_mcu_send_cmd(uint8_t cmd);
/* create a message and send the cmnd */

bool serial_available();

int serial_read ();

uint8_t serial_write (const uint8_t* ptr, uint8_t len);

void tuya_mcu_set_payload (uint8_t msg[], uint8_t payload[], uint8_t payload_length);
/* add the payload to the message */

uint8_t tuya_mcu_get_payload(uint8_t msg[], uint8_t payload[]);
/* copy the payload from the message to payload and return the length of the payload */

uint8_t tuya_mcu_get_version(uint8_t msg[]);


#endif
