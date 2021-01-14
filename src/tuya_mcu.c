/*
 *
 * Copyright 2019-2020  David B Brown  (@maccoylton) 
 * 
 */

#include <tuya_mcu.h>
#include <stdlib.h>
#include <string.h>

bool    gotHeartbeat = false;
bool    gotProdKey = false;
bool    gotWifiMode = false;
WifiState_t wifiState;
bool    sendWifiStateMsg = false;
bool    resetBuffer = false;
uint32_t currentByte = 0;
uint8_t dataLength = 0;
uint32_t lastMS = 0;
uint8_t mcu_init_stage = 0;
uint8_t mcuProtocolVersion = 0;
bool    canQuery = false;

/* declare these as global to avoid fragmentation */
uint8_t msg[MAX_BUFFER_LENGTH];
uint8_t payload[MAX_BUFFER_LENGTH-TUYA_MCU_HEADER_SIZE];
uint8_t messageToSend[MAX_BUFFER_LENGTH];


void tuya_mcu_sendTime(bool timeAvailable)
{
    static uint8_t sendTimeCmd[8]= {01,00,00,00,00,00,00,00};
    printf ("%s: timeAvailbale: %s, can query: %s", __func__, timeAvailable ? "True" : "False", canQuery ? "True" : "False");
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
        sendTimeCmd[7] = (new_time->tm_wday == 0 ? 7 : new_time->tm_wday);
        sendTimeCmd[6] = new_time->tm_sec;
        sendTimeCmd[5] = new_time->tm_min;
        sendTimeCmd[4] = new_time->tm_hour;
        sendTimeCmd[3] = new_time->tm_mday;
        sendTimeCmd[2] = new_time->tm_mon  + 1;
        sendTimeCmd[1] = new_time->tm_year % 100;
        tuya_mcu_send_message(MSG_CMD_OBTAIN_LOCAL_TIME, sendTimeCmd, 8);
    }
    printf (" End\n");
}


void tuya_mcu_processRx()
{
    printf ("%s: \n", __func__);
    bool hasMsg = false;
    if (serial_available())
    {
        while (serial_available())
        {
            hasMsg = tuya_mcu_msg_buffer_addbyte(serial_read(), msg);
            if (hasMsg)
            {
                tuya_mcu_process_message(msg);
                printf ( " MCU Init Stage: %d ", mcu_init_stage );
            }
        }
    }
    printf (":End\n");
}


void tuya_mcu_process_message(uint8_t msg[])
{
    printf ("\n\n%s: ", __func__);
    
    if (!tuya_mcu_message_is_valid(msg))
    {
        tuya_mcu_print_message (msg, false);
        return;
    } else {
        //tuya_mcu_print_message(msg, true);
    }
    
    uint8_t cmd = tuya_mcu_get_command(msg);
    printf ("CMD: 0x%02X", cmd );
    
    mcuProtocolVersion = tuya_mcu_get_version(msg);
    uint8_t payload_length = tuya_mcu_get_payload ( msg, payload);
    
    switch(cmd)
    {
        case MSG_CMD_HEARTBEAT:
        {
            printf (" Heartbeat: ");
            //uint8_t payload_length = tuya_mcu_get_payload ( msg, payload);
            if (1 == payload_length)
            {
                if (1 != payload[0]){
                    /* 0x01: this value is returned except for the first return value of 0 after the MCU reboots.*/
                    
                    sendWifiStateMsg = true;
                    printf (" MCU First Heartbeat");
                    /* if we get another zero then MCU has reset */
                }
                if (!gotHeartbeat)
                {
                    gotHeartbeat = true;
                    
                    if (!gotProdKey)
                    {
                        tuya_mcu_send_cmd(MSG_CMD_QUERY_PROD_INFO);
                        mcu_init_stage++;
                    }
                }
            } else
                printf (" INVALID heartbeat length: ");
        }
            break;
        case MSG_CMD_QUERY_PROD_INFO:
        {
            printf (" Query Prod Info: ");
            if ( mcu_init_stage == 2) mcu_init_stage++;
            
            if (tuya_mcu_get_payload_length(msg))
            {
                gotProdKey = true;
                
                if (!gotWifiMode)
                {
                    tuya_mcu_send_cmd (MSG_CMD_QUERY_WIFI_MODE);
                    mcu_init_stage++;
                }
            }
        }
            break;
        case MSG_CMD_QUERY_WIFI_MODE:
        {
            printf (" Query Wifi mode, got WiFi Mode: ");
            if ( mcu_init_stage == 4) mcu_init_stage++;
            
            
            gotWifiMode = true;
            
            //uint8_t payload_length = tuya_mcu_get_payload ( msg, payload);
            if ( payload_length== 2)
            {
                uint8_t wifi_indicator_pin = payload[0];
                uint8_t reset_pin = payload[1];
                wifiMode = WIFI_MODE_WIFI_PROCESSING;
            }
            else
            {
                wifiMode = WIFI_MODE_COOPERATIVE_PROCESSING;
            }
            printf ("wifiMode ");
            
        }
            break;
        case MSG_CMD_REPORT_WIFI_STATUS:
        {
            
            printf (" Report WiFi status: " );
            canQuery = true;
            tuya_mcu_send_cmd (MSG_CMD_QUERY_DEVICE_STATUS);
            mcu_init_stage++;
            
        }
            break;
        case MSG_CMD_RESET_WIFI_SWITCH_NET_CFG:
        {
            printf (" Reset Wifi: ");
            tuya_mcu_send_cmd (MSG_CMD_RESET_WIFI_SWITCH_NET_CFG);
            
            /* need to sort this                if (wifiConfigCallback)
             {
             tuya_mcu_setWifiState(WIFI_STATE_SMART_CONFIG);
             wifiConfigCallback();
             }
             */
        }
            break;
        case MSG_CMD_DP_STATUS:
        {
            printf (" Status: ");
            if (tuya_mcu_get_payload_length(msg))
            {
                tuya_handleDPStatusMsg(msg);
            }
            if (mcu_init_stage==6) mcu_init_stage++;
        }
            break;
        default:
            // unknown command
            printf (" unknown command: ");
            break;
    }
}


bool tuya_mcu_getTime(int dayOfWeek, int hour, int minutes)
{
    printf ("%s: ", __func__);
    bool gotTime = false;
    struct tm* new_time = NULL;
    time_t tnow = (time_t)-1;
    if (timeAvailable)
    {
        struct timezone tz = {0};
        struct timeval tv = {0};
        
        gettimeofday(&tv, &tz);
        tnow = time(NULL);
        
        new_time = localtime (&tnow);
        // sunday = 0, sat = 6
        dayOfWeek = new_time->tm_wday;
        hour = new_time->tm_hour;
        minutes = new_time->tm_min;
        
        gotTime = true;
    }
    printf ("%s: day: %d, hour: %d, min: %d: End\n", ctime(&tnow), dayOfWeek, hour, minutes);
    
    return gotTime;
}


long long tuya_mcu_get_millis() {
    //printf ("%s: ", __func__);
    /*    struct timeval te;
     gettimeofday(&te, NULL);
     printf("Time in microseconds: %0.3f microseconds: ",
     (float)(te.tv_sec));
     //printf ("End: ", __func__);
     return te.tv_sec * 1000LL + te.tv_usec / 1000;*/
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}


bool serial_available(){
    //printf ("%s: ", __func__);
    
    if (uart_rxfifo_wait(uart_port,0) == 0){
        //printf ("nothing to read. End: ");
        return false;
    } else {
        //printf ("something to read. End: ");
        return true;
    }
}


int serial_read (){
    int byte;
    byte = uart_getc_nowait(uart_port);
    return(byte);
}


void reset()
{
    //printf ("%s: ", __func__);
    resetBuffer = true;
    //printf ("End: ");
}


void checkReset()
{
    //printf ("%s: ", __func__);
    if (resetBuffer)
    {
        printf ("Resetting: ");
        currentByte = 0;
        dataLength = 0;
        resetBuffer = false;
    }
    //printf ("End: ", __func__);
}


uint8_t serial_write (const uint8_t* ptr, uint8_t len){
    
    printf ("%s: ", __func__);
    
    for(uint8_t i = 0; i < len; i++) {
        /* Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour) */
        if(((char *)ptr)[i] == '\r')
            continue;
        if(((char *)ptr)[i] == '\n')
            uart_putc(0, '\r');
        printf ("0x%02X ", ptr[i]);
        uart_putc(0, ((char *)ptr)[i]);
    }
    printf (": Sent %d bytes: ", len);
    return len;
    
}


bool tuya_mcu_msg_buffer_addbyte(uint8_t byte, uint8_t msg[])
{
    //printf ("%s: Start, byte:0x%02X\n", __func__, byte);
    
    uint32_t newLastMS = tuya_mcu_get_millis();
    
    if (newLastMS - lastMS > 5){
        printf ("%s: newLastMS:%d, lastMS %d, difference %d ", __func__, newLastMS, lastMS, newLastMS - lastMS );
        reset();
    }
    
    checkReset();
    
    lastMS = newLastMS;
    
    if (0 == currentByte)
    {
        if (0x55 == byte)
        {
            //printf ("%s: First Magic Bit\n", __func__);
            msg[currentByte] = byte;
            ++currentByte;
        }
        
    }
    else if (1 == currentByte)
    {
        if (0xAA == byte)
        {
            //printf ("%s: Second Magic Bit\n", __func__);
            msg[currentByte] = byte;
            ++currentByte;
        }
        else
        {
            printf (": not magic byte so reesetting: " );
            reset();
        }
    }
    else
    {
        msg[currentByte] = byte;
        
        if (5 == currentByte){
            dataLength = msg[4] * 0x100 + msg[5];
            //printf (" : Got length bytes, length %d ", dataLength);
        }
        if (currentByte >= 6u && dataLength+6u == currentByte)
        {
            //printf (": Got last byte: ");
            reset();
            return true;
        }
        else
            ++currentByte;
    }
    //printf ("%s: End\n", __func__);
    
    return false;
}


uint8_t tuya_mcu_get_msg_length(uint8_t msg[])
{
    //printf ("%s: Start\n", __func__);
    return (tuya_mcu_get_payload_length(msg) + TUYA_MCU_HEADER_SIZE);
}


uint8_t tuya_mcu_calc_checksum(uint8_t msg[])
{
    //printf ("%s: Start\n", __func__);
    uint8_t chksum = 0, message_length = tuya_mcu_get_msg_length (msg);
    for (uint8_t i = 0 ; i < message_length -1; ++i)
        chksum += msg[i];
    
    chksum %= 256;
    //printf ("%s: End\n", __func__);
    return (uint8_t)chksum;
}


void tuya_mcu_set_checksum(uint8_t msg[])
{
    //printf ("%s: Start\n", __func__);
    msg[tuya_mcu_get_msg_length(msg) - 1] = tuya_mcu_calc_checksum(msg);
    //printf ("%s: End\n", __func__);
}


void tuya_mcu_print_message (uint8_t msg[], bool valid){
    
    uint8_t message_length = MAX_BUFFER_LENGTH;
    if (valid) {
        message_length = tuya_mcu_get_msg_length (msg);
    }
    printf ("Message: ");
    
    
    for (uint8_t i=0 ;  i < message_length ; i++)
        printf (" 0x%02X", msg[i]);
    printf (": End: ");
    
}


bool tuya_mcu_message_is_valid(uint8_t msg[])
{
    uint8_t message_length = tuya_mcu_get_msg_length (msg);
    uint8_t checksum = tuya_mcu_calc_checksum(msg);
    
    if (msg[E_MAGIC1] == 0x55 && msg[E_MAGIC2] == 0xaa && message_length >= TUYA_MCU_HEADER_SIZE && checksum == msg[message_length - 1] ){
        printf ("Valid: ");
        return true;
    } else {
        if ( msg[E_MAGIC1] != 0x55 || msg[E_MAGIC2] != 0xaa)
            printf ("invalid magic bits 0x%02X 0x%02X: ", msg[E_MAGIC1], msg[E_MAGIC2]);
        if (message_length < TUYA_MCU_HEADER_SIZE)
            printf ("invalid message length %d: ", message_length);
        if ( checksum != msg[message_length - 1] )
            printf ("invalid checskum, calculated %d, received %d, length: %d: ", checksum, (uint8_t) msg[message_length - 1], message_length );
        return false;
    }
}


uint8_t tuya_mcu_get_command(uint8_t msg[])
{
    //printf ("%s: Start\n", __func__);
    return msg[E_CMD];
}


uint8_t tuya_mcu_get_version(uint8_t msg[])
{
    //printf ("%s: Start\n", __func__);
    return msg[E_VERSION];
}


uint8_t tuya_mcu_get_payload_length(uint8_t msg[])
{
    //printf ("%s: Start\n", __func__);
    return (msg[E_PAYLOAD_LENGTH_HI] * 0x100 + msg[E_PAYLOAD_LENGTH_LO]);
}


uint8_t tuya_mcu_get_payload(uint8_t msg[], uint8_t payload[])
{
    //printf ("%s: Start: ", __func__);
    uint8_t payload_length = tuya_mcu_get_payload_length(msg);
    memcpy(payload, &msg[E_PAYLOAD], payload_length);
    printf ("payload length %d: ", payload_length);
    return (payload_length);
}


void tuya_mcu_set_payload_length(uint8_t msg[], uint8_t payload_length)
{
    //printf ("%s: Start\n", __func__);
    if (payload_length <= MAX_BUFFER_LENGTH - TUYA_MCU_HEADER_SIZE)
    {
        msg[E_PAYLOAD_LENGTH_HI] = payload_length >> 8;
        msg[E_PAYLOAD_LENGTH_LO] = payload_length & 0xff;
    }
    else
    {
        /* need to add what to do if length is too big */
        
    }
    //printf ("%s: End\n", __func__);
}


void tuya_mcu_set_payload(uint8_t msg[], uint8_t* payload, uint8_t payload_length)
{
    //printf ("%s: Start\n", __func__);
    if (payload_length <= MAX_BUFFER_LENGTH - TUYA_MCU_HEADER_SIZE)
    {
        tuya_mcu_set_payload_length ( msg, payload_length);
        memcpy(&msg[E_PAYLOAD], payload, payload_length);
    }
    else
    {
        /* need to add what to do if length is too big */
        printf ("%s: Something went wrong, Payload too long: ", __func__);
    }
    //printf ("%s: End\n", __func__);
}


void tuya_mcu_send_cmd(uint8_t cmd)
{
    printf ("%s: 0x%02X ", __func__, cmd);
    
    messageToSend[E_MAGIC1] = 0x55;
    messageToSend[E_MAGIC2] = 0xaa;
    messageToSend[E_VERSION] = 0;
    messageToSend[E_CMD] = cmd;
    tuya_mcu_set_payload_length(messageToSend,0);
    tuya_mcu_set_checksum (messageToSend);
    
    if (tuya_mcu_message_is_valid(messageToSend))
    {
        //tuya_mcu_print_message (msg, true);
        serial_write(messageToSend, tuya_mcu_get_msg_length (messageToSend));
    } else {
        tuya_mcu_print_message (messageToSend, false);
    }
    printf ("End\n");
}


void tuya_mcu_send_message(uint8_t cmd, uint8_t payload[], uint8_t payload_length)
{
    printf ("%s: ", __func__);
    
    uint8_t messageToSend[MAX_BUFFER_LENGTH];
    
    messageToSend[E_MAGIC1] = 0x55;
    messageToSend[E_MAGIC2] = 0xaa;
    messageToSend[E_VERSION] = 0x00;
    messageToSend[E_CMD] = cmd;
    tuya_mcu_set_payload_length(messageToSend,payload_length);
    tuya_mcu_set_payload ( messageToSend, payload, payload_length);
    tuya_mcu_set_checksum (messageToSend);
    
    if (tuya_mcu_message_is_valid(messageToSend))
    {
        serial_write(msg, tuya_mcu_get_msg_length (messageToSend));
    }
    printf (": End: ");
}


void tuya_mcu_updateWifiState()
{
    // check once per second for wifi state change
    printf ("%s: ", __func__);
    
    static uint32_t timeLastSend = 0;
    uint32_t timeNow = tuya_mcu_get_millis();
    
    if (timeNow - timeLastSend > 1000)
    {
        timeLastSend = timeNow;
        
        WifiState_t newState = WIFI_STATE_CONNECT_FAILED;
        
        uint8_t status = sdk_wifi_station_get_connect_status();
        
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
        
        tuya_mcu_setWifiState(newState);
    }
    printf ("End\n");
}


void tuya_mcu_sendHeartbeat()
{
    printf ("%s: ", __func__);
    
    static uint32_t timeLastSend = 0;
    static uint32_t delay = 3000;
    uint32_t timeNow = tuya_mcu_get_millis();
    
    printf ("time last %d, time now: %d, delay: %d, diff: %d : got heartbeat %s\n", timeLastSend, timeNow, delay, timeNow - timeLastSend, gotHeartbeat ? "True" : "False");
    if (timeNow - timeLastSend > delay)
    {
        timeLastSend = timeNow;
        if (gotHeartbeat)
            delay = 10000;
        else
            delay = 3000;
        
        tuya_mcu_send_cmd(MSG_CMD_HEARTBEAT);
    }
}


void tuya_mcu_setWifiState(WifiState_t newState)
{
    printf (" current state: %d: new state: %d:", wifiState, newState );
    
    if (wifiState != newState || sendWifiStateMsg)
    {
        wifiState = newState;
        
        sendWifiStateMsg = true;
        
        if (gotWifiMode)
        {
            payload[0] = (uint8_t)wifiState;
            tuya_mcu_send_message (MSG_CMD_REPORT_WIFI_STATUS, payload, 1);
            sendWifiStateMsg = false;
        } else
        {
            printf (" Not gotWifiMode:");
        }
        
    }
    //printf ("%s: End\n", __func__);
    
}
