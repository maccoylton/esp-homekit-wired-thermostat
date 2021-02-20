#ifndef TUYA_MCU
#define TUYA_MCU

#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#define MAX_BUFFER_LENGTH 256
#define TUYA_MCU_HEADER_SIZE 7

extern bool sendWifiStateMsg;
extern uint8_t uart_port;
extern bool    gotHeartbeat;
extern bool    gotProdKey;
extern bool    gotWifiMode;
extern bool    timeAvailable;
extern uint8_t mcu_init_stage;
extern uint8_t msg[]; /* used for recevie */
extern uint8_t payload[];
extern uint8_t messageToSend[]; /* used for send */



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

typedef enum
{
    MSG_CMD_HEARTBEAT = 0x00,
    MSG_CMD_QUERY_PROD_INFO = 0x01,
    MSG_CMD_QUERY_WIFI_MODE = 0x02,
    MSG_CMD_REPORT_WIFI_STATUS = 0x03,
    MSG_CMD_RESET_WIFI_SWITCH_NET_CFG = 0x04,
    MSG_CMD_RESET_WIFI_SELECT_NET_CFG = 0x05,
    MSG_CMD_DP_CMD = 0x06, // DP = data point
    MSG_CMD_DP_STATUS = 0x07,
    MSG_CMD_QUERY_DEVICE_STATUS = 0x08, // request all DPs
    MSG_CMD_START_OTA_UPGRADE = 0x0a,
    MSG_CMD_TRANSMIT_OTA_PACKAGE = 0x0b,
    MSG_CMD_OBTAIN_GMT_TIME = 0x0c,
    MSG_CMD_OBTAIN_LOCAL_TIME = 0x1c,
    MSG_CMD_TEST_WIFI = 0x0e,
    MSG_CMD_ONTAIN_MEMORY= 0x0f,
} TUYAMessageCmd_t;



typedef enum
{
    WIFI_STATE_SMART_CONFIG = 0x00,
    WIFI_STATE_AP_CONFIG = 0x01,
    WIFI_STATE_CONNECT_FAILED = 0x02,
    WIFI_STATE_CONNECTED = 0x03 ,
    WIFI_STATE_CONNECTED_WITH_INTERNET = 0x04,
    WIFI_STATE_LOW_POWER = 0x05,
    WIFI_STATE_SMART_CONFIG_AND_AP = 06,
} WifiState_t;


typedef enum
{
    WIFI_MODE_COOPERATIVE_PROCESSING,
    WIFI_MODE_WIFI_PROCESSING
} WifiMode_t;


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

void tuya_mcu_print_message(uint8_t msg[], bool valid);
/* print the contents of the buffer */

void tuya_mcu_updateWifiState();
/* update the wifi status */

void tuya_mcu_sendHeartbeat();
/* send a heartbeat to the MCU */

long long tuya_mcu_get_millis() ;

void tuya_mcu_setWifiState(WifiState_t newState);

bool tuya_mcu_getTime(int dayOfWeek, int hour, int minutes);

bool getTimeAvailable();
/* returns true of time is available, false otherwise*/

bool tuya_mcu_msg_buffer_addbyte(uint8_t byte, uint8_t msg[]);

void tuya_mcu_processRx();

void tuya_mcu_process_message(uint8_t msg[]);

void tuya_mcu_sendTime(bool timeAvailable);


WifiMode_t wifiMode;

#endif
