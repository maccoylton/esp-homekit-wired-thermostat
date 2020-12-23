#ifndef TUYA_THERMOSTAT
#define TUYA_THERMOSTAT

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <FreeRTOS.h>
#include <task.h>
#include <tuya_mcu.h>


typedef enum
{
CHANGE_TYPE_CURRENT_SCHEDULE_PERIOD,
    CHANGE_TYPE_POWER = 0x01,
    CHANGE_TYPE_SETPOINT_TEMP = 0x02,
    CHANGE_TYPE_INTERNAL_TEMP = 0x03,
    CHANGE_TYPE_MODE = 0x04,
    CHANGE_TYPE_ECONOMY = 0x05,
    CHANGE_TYPE_LOCK = 0x06,
    CHANGE_TYPE_SCHEDULE = 0x65,
    CHANGE_TYPE_EXTERNAL_TEMP = 0x66,
} TUYA_Thermostat_change_type_t;

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
    MSG_CMD_OBTAIN_LOCAL_TIME = 0x1c,
    MSG_CMD_TEST_WIFI = 0x0e,
} TUYAMessageCmd_t;

typedef enum
{
    WIFI_MODE_COOPERATIVE_PROCESSING,
    WIFI_MODE_WIFI_PROCESSING
} WifiMode_t;

typedef enum
{
    WIFI_STATE_SMART_CONFIG,
    WIFI_STATE_AP_CONFIG,
    WIFI_STATE_CONNECT_FAILED,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_CONNECTED_WITH_INTERNET,
    WIFI_STATE_LOW_POWER
} WifiState_t;

typedef enum
{
    MODE_SCHEDULE,
    MODE_MANUAL,
} thermostat_mode_t;

typedef struct
{
    uint8_t magic1;
    uint8_t magic2;
    uint8_t version;
    uint8_t cmd;
} tuya_message_t;


typedef struct
{
    int hour;
    int minute;
    float temperature;
} SchedulePeriod_t;


extern bool powerOn;
extern float setPointTemp;
extern float internalTemp;
extern float externalTemp;
extern thermostat_mode_t mode;
extern bool economyOn;
extern bool locked;
extern uint8_t schedule[54];
WifiState_t wifiState;
WifiMode_t wifiMode;

extern  uint8_t uart_port;



//tuya_thermostat my_thermostat;

void tuya_thermostat_handleDPStatusMsg(uint8_t msg[]);
void tuya_thermostat_processTx(bool timeAvailable);
void tuya_thermostat_sendTime(bool timeAvailable);
float tuya_thermostat_getScheduleSetPointTemperature(int day, int period);
void tuya_thermostat_getSchedulePeriod(int day, int period, SchedulePeriod_t* p);
void tuya_thermostat_updateWifiState();
void tuya_thermostat_sendHeartbeat();
void tuya_thermostat_loop(void *args);
void tuya_thermostat_setPower( bool on, bool updateMCU);
void tuya_thermostat_emitChange(TUYA_Thermostat_change_type_t cmd);
void tuya_thermostat_setSetPointTemp( float temp, bool updateMCU);


#endif
