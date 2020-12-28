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



bool tuya_mcu_getTime(int dayOfWeek, int hour, int minutes)
{
    printf ("%s: \n", __func__);
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
    //printf ("%s: Start\n", __func__);
    /*    struct timeval te;
     gettimeofday(&te, NULL);
     printf("Time in microseconds: %0.3f microseconds\n",
     (float)(te.tv_sec));
     //printf ("%s: End\n", __func__);
     return te.tv_sec * 1000LL + te.tv_usec / 1000;*/
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}



bool serial_available(){
    //printf ("%s: Start\n", __func__);
    
    if (uart_rxfifo_wait(uart_port,0) == 0){
        //rintf ("%s: nothing to read. End\n", __func__);
        return false;
    } else {
        //printf ("%s: something to read. End\n", __func__);
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
    //printf ("%s: Start\n", __func__);
    resetBuffer = true;
    //printf ("%s: End\n", __func__);
}


void checkReset()
{
    //printf ("%s: Start\n", __func__);
    if (resetBuffer)
    {
        printf ("%s: Resetting ", __func__);
        currentByte = 0;
        dataLength = 0;
        resetBuffer = false;
    }
    //printf ("%s: End\n", __func__);
}


uint8_t serial_write (const uint8_t* ptr, uint8_t len){
    
    printf ("%s: ", __func__);
    
    for(uint8_t i = 0; i < len; i++) {
        /* Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour) */
        if(((char *)ptr)[i] == '\r')
            continue;
        if(((char *)ptr)[i] == '\n')
            uart_putc(0, '\r');
        printf (" 0x%02X", ptr[i]);
        uart_putc(0, ((char *)ptr)[i]);
    }
    printf (" : Sent %d bytes: ", len);
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
    printf (" Message:");
    
    
    for (uint8_t i=0 ;  i < message_length ; i++)
        printf (" 0x%02X", msg[i]);
    printf (" %s: End ", __func__);
    
}


bool tuya_mcu_message_is_valid(uint8_t msg[])
{
    uint8_t message_length = tuya_mcu_get_msg_length (msg);
    uint8_t checksum = tuya_mcu_calc_checksum(msg);
    
    if (msg[E_MAGIC1] == 0x55 && msg[E_MAGIC2] == 0xaa && message_length >= TUYA_MCU_HEADER_SIZE && checksum == msg[message_length - 1] ){
        printf (" Valid: ");
        return true;
    } else {
        if ( msg[E_MAGIC1] != 0x55 || msg[E_MAGIC2] != 0xaa)
            printf (" invalid magic bits 0x%02X 0x%02X\n", msg[E_MAGIC1], msg[E_MAGIC2]);
        if (message_length < TUYA_MCU_HEADER_SIZE)
            printf (" invalid message length %d\n", message_length);
        if ( checksum != msg[message_length - 1] )
            printf (" invalid checkum, calculated %d, received %d\n", checksum, (uint8_t) msg[message_length - 1] );
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
    printf (" payload length %d ", payload_length);
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
        printf ("%s: Something went wrong, Payload too long\n", __func__);
    }
    //printf ("%s: End\n", __func__);
}


void tuya_mcu_send_cmd(uint8_t cmd)
{
    printf ("%s: ", __func__);
    uint8_t msg[MAX_BUFFER_LENGTH];
    
    msg[E_MAGIC1] = 0x55;
    msg[E_MAGIC2] = 0xaa;
    msg[E_VERSION] = 0;
    msg[E_CMD] = cmd;
    tuya_mcu_set_payload_length(msg,0);
    tuya_mcu_set_checksum (msg);
    
    if (tuya_mcu_message_is_valid(msg))
    {
        //tuya_mcu_print_message (msg, true);
        serial_write(msg, tuya_mcu_get_msg_length (msg));
        /*        logger.addBytes("TX:", msg, msg.getLength());    }
         else
         {
         logger.addBytes("TX INVALID MSG:", msg, msg.getLength());
         */
    } else {
        tuya_mcu_print_message (msg, false);
    }
    printf (" End\n");
}


void tuya_mcu_send_message(uint8_t cmd, uint8_t payload[], uint8_t payload_length)
{
    printf ("%s: \n", __func__);
    
    uint8_t msg[MAX_BUFFER_LENGTH];
    
    msg[E_MAGIC1] = 0x55;
    msg[E_MAGIC2] = 0xaa;
    msg[E_VERSION] = 0x00;
    msg[E_CMD] = cmd;
    tuya_mcu_set_payload_length(msg,payload_length);
    tuya_mcu_set_payload ( msg, payload, payload_length);
    tuya_mcu_set_checksum (msg);
    
    if (tuya_mcu_message_is_valid(msg))
    {
        
        serial_write(msg, tuya_mcu_get_msg_length (msg));
        /*        logger.addBytes("TX:", msg, msg.getLength());    }
         else
         {
         logger.addBytes("TX INVALID MSG:", msg, msg.getLength());
         */
    }
    printf (": End");
    
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
    printf (" End\n");
}



void tuya_mcu_sendHeartbeat()
{
    printf ("%s: ", __func__);
    
    static uint32_t timeLastSend = 0;
    static uint32_t delay = 3000;
    uint32_t timeNow = tuya_mcu_get_millis();
    
    printf (" time last send %d, time now: %d, delay: %d, diff: %d : got heartbeat %s\n", timeLastSend, timeNow, delay, timeNow - timeLastSend, gotHeartbeat ? "True" : "False");
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
            uint8_t payload[1] = {(uint8_t)wifiState};
            tuya_mcu_send_message (MSG_CMD_REPORT_WIFI_STATUS, payload, 1);
            sendWifiStateMsg = false;
        } else
        {
            printf (" Not gotWifiMode:");
        }
        
    }
    //printf ("%s: End\n", __func__);
    
}
