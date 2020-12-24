/*
 *
 * Copyright 2019-2020  David B Brown  (@maccoylton)
 * 
 */

#include <espressif/esp_wifi.h>
#include <sys/time.h>
#include <espressif/esp_sta.h>
#include <tuya_thermostat.h>
#include <string.h>

/* external variables */
bool powerOn = false;
float setPointTemp = 20.0f;
float internalTemp = 20.0f;
float externalTemp = 20.0f;
thermostat_mode_t mode = 0;
bool economyOn = false;
bool locked = false;
uint8_t schedule[54] = {0};

/*variables used here */
uint32_t currentByte = 0;
uint32_t lastMS = 0;
uint8_t dataLength = 0;
uint8_t protocolVersion = 0;
bool    resetBuffer = false;
bool    sendWifiStateMsg = false;
bool    gotHeartbeat = false;
bool    gotProdKey = false;
bool    gotWifiMode = false;
bool    canQuery = false;
bool haveSchedule = false;
int scheduleCurrentDay = -1;
int scheduleCurrentPeriod = -1;


bool getTimeAvailable(){
    /* need to implement */
    return false;
}



bool tuya_thermostat_getTime(int dayOfWeek, int hour, int minutes)
{
    printf ("%s: Start\n", __func__);
    bool gotTime = false;
    struct tm* new_time = NULL;
    if (getTimeAvailable())
    {
        struct timezone tz = {0};
        struct timeval tv = {0};
        
        gettimeofday(&tv, &tz);
        time_t tnow = time(NULL);
        
        new_time = localtime(&tnow);
        // sunday = 0, sat = 6
        dayOfWeek = new_time->tm_wday;
        hour = new_time->tm_hour;
        minutes = new_time->tm_min;
        
        gotTime = true;
    }
    printf ("%s: End\n", __func__);

    return gotTime;
}


long long tuya_thermostat_get_millis() {
    printf ("%s: Start\n", __func__);
    struct timeval te;
    gettimeofday(&te, NULL);
    printf ("%s: End\n", __func__);
    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}


void reset()
{
    printf ("%s: Start\n", __func__);
    resetBuffer = true;
    printf ("%s: End\n", __func__);
}


void checkReset()
{
    printf ("%s: Start\n", __func__);
    if (resetBuffer)
    {
        currentByte = 0;
        dataLength = 0;
        resetBuffer = false;
    }
    printf ("%s: End\n", __func__);
}

bool msg_buffer_addbyte(uint8_t byte, uint8_t msg[])
{
    printf ("%s: Start\n", __func__);

    uint32_t newLastMS = tuya_thermostat_get_millis();
    
    if (newLastMS - lastMS > 5)
        reset();
    
    checkReset();
    
    lastMS = newLastMS;
    
    if (0 == currentByte)
    {
        if (0x55 == byte)
        {
            msg[currentByte] = byte;
            ++currentByte;
        }
        
    }
    else if (1 == currentByte)
    {
        if (0xAA == byte)
        {
            msg[currentByte] = byte;
            ++currentByte;
        }
        else
        {
            reset();
        }
    }
    else
    {
        msg[currentByte] = byte;
        
        if (5 == currentByte)
            dataLength = msg[4] * 0x100 + msg[5];
        
        if (currentByte >= 6u && dataLength+6u == currentByte)
        {
            reset();
            return true;
        }
        else
            ++currentByte;
    }
    printf ("%s: End\n", __func__);

    return false;
}


void tuya_thermostat_process_message(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);

    if (!tuya_mcu_message_is_valid(msg))
    {
        /*            logger.addBytes("RX INVALID MSG:", msg_buffer.bytes, msg_buffer.length;
         */
        return;
    }
    /*
     logger.addBytes("RX:", msg_buffer.bytes, msg_buffer.length;
     */
    
    switch(tuya_mcu_get_command(msg))
    {
        case MSG_CMD_HEARTBEAT:
        {
            uint8_t payload[MAX_BUFFER_LENGTH];
            uint8_t payload_length = tuya_mcu_get_payload ( msg, payload);
            if (1 == payload_length)
            {
                if (1 != payload[0])
                    sendWifiStateMsg = true;
                if (!gotHeartbeat)
                {
                    gotHeartbeat = true;
                    protocolVersion = tuya_mcu_get_version(msg);
                    
                    if (!gotProdKey)
                    {
                        tuya_mcu_send_cmd(MSG_CMD_QUERY_PROD_INFO);
                    }
                }
            }
        }
            break;
        case MSG_CMD_QUERY_PROD_INFO:
        {
            if (tuya_mcu_get_payload_length(msg))
            {
                gotProdKey = true;
                
                if (!gotWifiMode)
                {
                    tuya_mcu_send_cmd (MSG_CMD_QUERY_WIFI_MODE);
                    
                }
            }
        }
            break;
        case MSG_CMD_QUERY_WIFI_MODE:
        {
            gotWifiMode = true;
            if (tuya_mcu_get_payload_length(msg) == 2)
            {
                //const uint8_t* payload = tuya_mcu_get_payload(msg);
                //uint8_t wifi_indicator_pin = payload[0];
                //uint8_t reset_pin = payload[1];
                wifiMode = WIFI_MODE_WIFI_PROCESSING;
            }
            else
            {
                wifiMode = WIFI_MODE_COOPERATIVE_PROCESSING;
            }
            /*                logger.addLine("WIFI PROCESSING MODE:" + String(wifiMode));
             */
        }
            break;
        case MSG_CMD_REPORT_WIFI_STATUS:
        {
            canQuery = true;
            tuya_mcu_send_cmd (MSG_CMD_QUERY_DEVICE_STATUS);
        }
            break;
        case MSG_CMD_RESET_WIFI_SWITCH_NET_CFG:
        {
            tuya_mcu_send_cmd (MSG_CMD_RESET_WIFI_SWITCH_NET_CFG);
            
            /* need to sort this                if (wifiConfigCallback)
             {
             tuya_thermostat_setWifiState(WIFI_STATE_SMART_CONFIG);
             wifiConfigCallback();
             }
             */
        }
            break;
        case MSG_CMD_RESET_WIFI_SELECT_NET_CFG:
        {
            // not implemented
            
        }
            break;
        case MSG_CMD_DP_STATUS:
        {
            if (tuya_mcu_get_payload_length(msg))
            {
                tuya_thermostat_handleDPStatusMsg(msg);
            }
        }
            break;
        default:
            // unknown command
            break;
    }
    printf ("%s: Start\n", __func__);

}


void tuya_thermostat_processRx()
{
    printf ("%s: Start\n", __func__);
    bool hasMsg = false;
    if (serial_available())
    {
        uint8_t msg[MAX_BUFFER_LENGTH];
        while (serial_available())
        {
            hasMsg = msg_buffer_addbyte(serial_read(), msg);
            if (hasMsg)
            {
                tuya_thermostat_process_message(msg);
            }
        }
    }
    printf ("%s: End\n", __func__);
}


void tuya_thermostat_loop(void *args)
{
    while (1) {
        tuya_thermostat_processRx();
        tuya_thermostat_processTx(getTimeAvailable());
        
        // update scheduleCurrentDay  and scheduleCurrentPeriod
        if (getTimeAvailable())
        {
            uint32_t timeNow = tuya_thermostat_get_millis();
            uint32_t timeLastScheduleUpdate = timeNow+1;
            
            if (timeNow - timeLastScheduleUpdate > 30000)
            {
                timeLastScheduleUpdate = timeNow;
                int day = 0;
                int hour = 0;
                int mins = 0;
                if (tuya_thermostat_getTime(day, hour, mins))
                {
                    // make monday first day
                    if (day == 0)
                        day = 7;
                    --day;
                    
                    int period = -1;
                    SchedulePeriod_t p;
                    for (int i = 0 ; i < 6; ++i)
                    {
                        tuya_thermostat_getSchedulePeriod(day, i, &p);
                        
                        if (hour < p.hour)
                        {
                            break;
                        }
                        else if (hour == p.hour && mins < p.minute)
                        {
                            break;
                        }
                        else
                        {
                            period = i;
                        }
                    }
                    
                    
                    if (-1 == period) // still on previous day schedule
                    {
                        if (day == 0)
                            day = 7;
                        --day;
                        period = 5;
                    }
                    if (day != scheduleCurrentDay || period != scheduleCurrentPeriod)
                    {
                        scheduleCurrentDay = day;
                        scheduleCurrentPeriod = period;
                        tuya_thermostat_emitChange(CHANGE_TYPE_CURRENT_SCHEDULE_PERIOD);
                    }
                }
            }
        }
        vTaskDelay(10000/ portTICK_PERIOD_MS);
    }
}


int tuya_thermostat_getScheduleDay()
{
    return scheduleCurrentDay;
}

int tuya_thermostat_getScheduleCurrentPeriod()
{
    return scheduleCurrentPeriod;
}


void tuya_thermostat_getSchedulePeriod(int day, int period, SchedulePeriod_t* p)
{
    if (day < 0 || day > 6 || period < 0 || period > 5)
        return;
    
    int i = 0;
    int j = period;
    if (i > 4)
        i = day - 4;
    
    p->minute = schedule[i * 18 + j * 3 + 0];
    p->hour = schedule[i * 18 + j * 3 + 1];
    p->temperature = schedule[i * 18 + j * 3 + 2] / 2.f;
}


float tuya_thermostat_getScheduleSetPointTemperature(int day, int period)
{
    SchedulePeriod_t p;
    tuya_thermostat_getSchedulePeriod(day, period, &p);
    return p.temperature;
}


float tuya_thermostat_getScheduleCurrentPeriodSetPointTemp()
{
    return tuya_thermostat_getScheduleSetPointTemperature(scheduleCurrentDay, scheduleCurrentPeriod );
}


void tuya_thermostat_processTx(bool timeAvailable)
{
    printf ("%s: Start\n", __func__);

    tuya_thermostat_sendHeartbeat();
    tuya_thermostat_updateWifiState();
    tuya_thermostat_sendTime(timeAvailable);
    printf ("%s: End\n", __func__);

}

void tuya_thermostat_sendHeartbeat()
{
    printf ("%s: Start\n", __func__);

    static uint32_t timeLastSend = 0;
    static uint32_t delay = 3000;
    uint32_t timeNow = tuya_thermostat_get_millis();
    
    if (timeNow - timeLastSend > delay)
    {
        timeLastSend = timeNow;
        if (gotHeartbeat)
            delay = 10000;
        else
            delay = 3000;
        
        tuya_mcu_send_cmd(MSG_CMD_HEARTBEAT);
    }
    printf ("%s: End\n", __func__);

}

void tuya_thermostat_setWifiState(WifiState_t newState)
{
    if (wifiState != newState || sendWifiStateMsg)
    {
        wifiState = newState;
        
        sendWifiStateMsg = true;
        
        if (gotWifiMode)
        {
            uint8_t payload[1] = {(uint8_t)wifiState};
            tuya_mcu_send_message (MSG_CMD_REPORT_WIFI_STATUS, payload, 1);
            sendWifiStateMsg = false;
        }
        
    }
    
}

void tuya_thermostat_updateWifiState()
{
    // check once per second for wifi state change
    static uint32_t timeLastSend = 0;
    uint32_t timeNow = tuya_thermostat_get_millis();
    
    if (timeNow - timeLastSend > 1000)
    {
        timeLastSend = timeNow;
        
        WifiState_t newState = WIFI_STATE_CONNECT_FAILED;
        /*	need to fix
         wl_status_t wifiStatus = WiFi.status();
         switch(wifiStatus)
         {
         case WL_NO_SHIELD:
         case WL_IDLE_STATUS:
         case WL_NO_SSID_AVAIL:
         case WL_DISCONNECTED:
         case WL_CONNECTION_LOST:
         case WL_CONNECT_FAILED:
         newState = WIFI_STATE_CONNECT_FAILED;
         break;
         case WL_SCAN_COMPLETED:
         //newWifiState = settings1.getWiFiState();
         break;
         case WL_CONNECTED:
         newState = WIFI_STATE_CONNECTED_WITH_INTERNET;
         break;
         }*/
        uint8_t status ;
        
        status = sdk_wifi_station_get_connect_status();
        
        switch (status)
        {
                
            case STATION_NO_AP_FOUND:
            case STATION_CONNECT_FAIL:
                newState = WIFI_STATE_CONNECT_FAILED;
                break;
            case STATION_GOT_IP:
                newState = WIFI_STATE_CONNECTED_WITH_INTERNET;
                break;
            default:
                newState = WIFI_STATE_CONNECT_FAILED;
                break;
                
        }
        
        tuya_thermostat_setWifiState(newState);
    }
}

void tuya_thermostat_sendTime(bool timeAvailable)
{
    printf ("%s: Start\n", __func__);
    struct tm* new_time = NULL;
    if (timeAvailable)
    {
        struct timezone tz = {0};
        struct timeval tv = {0};
        
        gettimeofday(&tv, &tz);
        time_t tnow = time(NULL);
        
        // localtime / gmtime every second change
        static time_t nexttv = 0;
        if (nexttv < tv.tv_sec && canQuery)
        {
            nexttv = tv.tv_sec + 3600; // update every hour
            new_time = localtime(&tnow);
        }
    }
    
    if (new_time != NULL)
    {
        // weekday: 0 = monday, 6 = sunday
        uint8_t w = (new_time->tm_wday == 0 ? 7 : new_time->tm_wday);
        uint8_t s = new_time->tm_sec;
        uint8_t m = new_time->tm_min;
        uint8_t h = new_time->tm_hour;
        uint8_t d = new_time->tm_mday;
        uint8_t M = new_time->tm_mon  + 1;
        uint8_t y = new_time->tm_year % 100;
        
        uint8_t payload[8] = {01,y,M,d,h,m,s,w};
        tuya_mcu_send_message(MSG_CMD_OBTAIN_LOCAL_TIME, payload, 8);
    }
    printf ("%s: End\n", __func__);
}


void tuya_thermostat_setPower( bool on, bool updateMCU)
{
    printf ("%s: Start\n", __func__);

    if (on != powerOn)
    {
        powerOn = on;
        if (updateMCU)
        {
            // send change to mcu
            uint8_t payload[5] = {0x01, 0x01, 0x00, 0x01, (uint8_t)(powerOn ? 1 : 0)};
            tuya_mcu_send_message(MSG_CMD_DP_CMD, payload, 5);
        } else {
            tuya_thermostat_emitChange(CHANGE_TYPE_POWER);
        }
    }
    printf ("%s: End\n", __func__);

}


void tuya_thermostat_setSetPointTemp( float temp, bool updateMCU)
{
    printf ("%s: Start\n", __func__);

    if (temp != setPointTemp)
    {
        setPointTemp = temp;
        if (updateMCU)
        {
            // send change to mcu
            //RX: 55 aa 01 07 00 08 02 02 00 04 00 00 00 24 3c
            uint8_t payload[8] = {0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, (uint8_t)(setPointTemp*2 + 0.5)};
            tuya_mcu_send_message(MSG_CMD_DP_CMD, payload, 8);
        } else {
            tuya_thermostat_emitChange(CHANGE_TYPE_SETPOINT_TEMP);
        }
    }
    printf ("%s: End\n", __func__);

}

void tuya_thermostat_setInternalTemp( float temp)
{
    printf ("%s: Start\n", __func__);

    if (temp != internalTemp)
    {
        internalTemp = temp;
        tuya_thermostat_emitChange(CHANGE_TYPE_INTERNAL_TEMP);
    }
    printf ("%s: End\n", __func__);

}


void tuya_thermostat_setExternalTemp( float temp)
{
    printf ("%s: Start\n", __func__);

    if (temp != externalTemp)
    {
        externalTemp = temp;
        tuya_thermostat_emitChange(CHANGE_TYPE_EXTERNAL_TEMP);
    }
    printf ("%s: End\n", __func__);

}


void tuya_thermostat_setMode(thermostat_mode_t m, bool updateMCU)
{
    printf ("%s: Start\n", __func__);
    if (m != mode)
    {
        mode = m;
        if (updateMCU)
        {
            // send change to mcu
            uint8_t payload[5] = {0x04, 0x04, 0x00, 0x01, mode};
            tuya_mcu_send_message(MSG_CMD_DP_CMD, payload, 5);
            
        } else {
            tuya_thermostat_emitChange(CHANGE_TYPE_MODE);
        }
    }
    printf ("%s: End\n", __func__);

}


void tuya_thermostat_setEconomy( bool econ, bool updateMCU)
{
    printf ("%s: Start\n", __func__);

    if (econ != economyOn)
    {
        economyOn = econ;
        if(updateMCU)
        {
            // send change to mcu
            uint8_t payload[5] = {0x05, 0x01, 0x00, 0x01,(uint8_t)( economyOn ? 1 : 0)};
            tuya_mcu_send_message(MSG_CMD_DP_CMD, payload, 5);
        } else
        {
            tuya_thermostat_emitChange(CHANGE_TYPE_ECONOMY);
        }
    }
    printf ("%s: End\n", __func__);

}


void tuya_thermostat_setLock(bool lock, bool updateMCU)
{
    printf ("%s: Start\n", __func__);

    if (lock != locked)
    {
        locked = lock;
        if(updateMCU)
        {
            // send change to mcu
            uint8_t payload[5] = {0x06, 0x01, 0x00, 0x01, (uint8_t)(locked ? 1 : 0)};
            tuya_mcu_send_message(MSG_CMD_DP_CMD, payload, 5);
        } else {
            tuya_thermostat_emitChange(CHANGE_TYPE_LOCK);
        }
    }
    printf ("%s: End\n", __func__);
}


void tuya_thermostat_setSchedule(const uint8_t* s, uint8_t length, bool updateMCU)
{
    printf ("%s: Start\n", __func__);

    if (54 != length)
        return;
    
    bool changed = false;
    
    //setSchedule weekday, sat, sun x 6 times
    for (int i = 0; i < length; ++i)
    {
        if (s[i] != schedule[i])
        {
            schedule[i] = s[i];
            changed = true;
        }
    }
    if (changed)
    {
        haveSchedule = true;
        if (updateMCU)
        {
            // send change to mcu
            uint8_t payload[58] = {0x65, 0x00, 0x00, 0x36, 0};
            uint8_t* ptr = payload + 4;
            for (int i = 0; i < 54; ++i)
            {
                ptr[i] = schedule[i];
            }
            tuya_mcu_send_message(MSG_CMD_DP_CMD, payload, 58);
        }
    }
    printf ("%s: End\n", __func__);
}


void tuya_thermostat_handleDPStatusMsg(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);

    uint8_t payload[MAX_BUFFER_LENGTH];
    
    uint8_t payload_length = tuya_mcu_get_payload(msg, payload);
    
    
    switch(payload[0])
    {
        case CHANGE_TYPE_POWER: // power on/off (byte 4)
        {
            printf ("%s: Power\n", __func__);

            //RX: 55 aa 01 07 00 05 01 01 00 01 01 10
            if (5 == payload_length    )
            {
                tuya_thermostat_setPower(1 == payload[4], false);
            }
        }
            break;
        case CHANGE_TYPE_SETPOINT_TEMP: // set point temp
        {
            //RX: 55 aa 01 07 00 08 02 02 00 04 00 00 00 2e 45
            printf ("%s: SETPOINT_TEMP\n", __func__);

            if (8 == payload_length)
            {
                tuya_thermostat_setSetPointTemp(payload[7]/2.0f, false);
            }
            
        }
            break;
        case CHANGE_TYPE_INTERNAL_TEMP: // temperature
        {
            //RX: 55 aa 01 07 00 08 03 02 00 04 00 00 00 24 3c
            printf ("%s: Internal Temp\n", __func__);
            if (8 == payload_length)
            {
                tuya_thermostat_setInternalTemp(payload[7]/2.0f);
            }
        }
            break;
        case CHANGE_TYPE_MODE: // mode (schedule = 0 / manual = 1)
        {
            //RX: 55 aa 01 07 00 05 04 04 00 01 00 15
            printf ("%s: Mode\n", __func__);
            if (5 == payload_length)
            {
                tuya_thermostat_setMode(payload[4] ? MODE_MANUAL : MODE_SCHEDULE, false);
                
            }
            
        }
            break;
        case CHANGE_TYPE_ECONOMY: // economy
        {
            //RX: 55 aa 01 07 00 05 05 01 00 01 00 13
            printf ("%s: Economy\n", __func__);
            if (5 == payload_length)
            {
                tuya_thermostat_setEconomy(1 == payload[4], false);
            }
        }
            break;
        case CHANGE_TYPE_LOCK: // lock
        {
            //RX: 55 aa 01 07 00 05 06 01 00 01 00 14
            //    TX: 55 AA 00 06 00 05 06 01 00 01 01 00
            
            printf ("%s: Lock\n", __func__);

            if (5 == payload_length)
            {
                tuya_thermostat_setLock(1 == payload[4], false);
            }
        }
            break;
        case 0x65: // schedule
        {
            // RX: 55 aa 01 07 00 3a 65
            // 00 00 36 00
            // 06 28 00 08 1e 1e b 1e 1e d 1e 00 11 2c 00 16 1e 00
            // 06 28 00 08 28 1e b 28 1e d 28 00 11 28 00 16 1e 00
            // 06 28 00 08 28 1e b 28 1e d 28 00 11 28 00 16 1e f
            printf ("%s: Schedule\n", __func__);

            if (58  == payload_length)
            {
                tuya_thermostat_setSchedule(payload + 4, tuya_mcu_get_payload_length(msg)-4, false);
            }
        }
            break;
        case CHANGE_TYPE_EXTERNAL_TEMP: // floor temp
        {
            // RX: 55 aa 01 07 00 08 66 02 00 04 00 00 00 00 7b
            printf ("%s: External Temp\n", __func__);
            if (8 == payload_length)
            {
                tuya_thermostat_setExternalTemp(payload[7]/2.0f);
            }
        }
            break;
        case 0x68: // ??
        {
            // RX: 55 aa 01 07 00 05 68 01 00 01 01 77
            printf ("%s: Unknown\n", __func__);
            if (5 == payload_length)
            {
            }
            
        }
            break;
            
    }
    printf ("%s: End\n", __func__);

}




/*
 String toString()
 {
 String str;
 str += "HEATING: ";
 if (getIsHeating())
 str  += "ON\n";
 else
 str += "OFF\n";
 str +=
 String("SetTemp: ") + String(getSetPointTemp()) +
 String(", internal temp: ") + String(getInternalTemp()) +
 String(", external temp: ") + String(getExternalTemp()) +
 String(", lock: ") + String(getLock()) +
 String(", mode: ") + String(getMode()) +
 String(", power: ") + String(getPower()) +
 String(", economy: ") + String(getEconomy()) +
 String("\n\n") +
 String("Schedule:");
 for (int i = 0; i < 3; ++i)
 {
 if (0 == i)
 str += String("\nM-F: ");
 else if (1 == i)
 str += String("\nSat: ");
 else if (2 == i)
 str += String("\nSun: ");
 for(int j = 0; j < 6; ++j)
 {
 uint8_t m = schedule[i * 18 + j * 3 + 0];
 uint8_t h = schedule[i * 18 + j * 3 + 1];
 float t = schedule[i * 18 + j * 3 + 2] / 2.f;
 if (0 != j)
 str += ", ";
 str += String("time") + String(j+1) + String(": ");
 str += String(h) + String(":") + String(m);
 str += String(" temp") + String(j+1) + String(": ");
 str += String(t);
 }
 }
 
 str += "\n";
 str += "curr day:";
 str += String(scheduleCurrentDay);
 str += ", curr period:";
 str += String(scheduleCurrentPeriod);
 str += ", curr temp:";
 str += String(getScheduleCurrentPeriodSetPointTemp());
 return str;
 }
 */

