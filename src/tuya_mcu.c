/*
 *
 * Copyright 2019-2020  David B Brown  (@maccoylton) 
 * 
 */

#include <tuya_mcu.h>
#include <stdlib.h>
#include <string.h>

extern int uart_port;

bool serial_available(){
    printf ("%s: Start\n", __func__);

    if (uart_rxfifo_wait(uart_port,0) == 0){
        printf ("%s: nothing to read. End\n", __func__);
        return false;
    } else {
        printf ("%s: something to read. End\n", __func__);
        return true;
    }
}


int serial_read (){
    int byte;
    byte = uart_getc_nowait(uart_port);
    return(byte);
}


uint8_t serial_write (const uint8_t* ptr, uint8_t len){

    printf ("%s: Start\n", __func__);

    for(uint8_t i = 0; i < len; i++) {
        /* Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour) */
        if(((char *)ptr)[i] == '\r')
            continue;
        if(((char *)ptr)[i] == '\n')
            uart_putc(0, '\r');
        uart_putc(0, ((char *)ptr)[i]);
    }
    printf ("%s: Sent %d bytes. End\n", __func__, len);
    return len;
    
}


uint8_t tuya_mcu_get_msg_length(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    return (tuya_mcu_get_payload_length(msg) + TUYA_MCU_HEADER_SIZE);
}


uint8_t tuya_mcu_calc_checksum(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    uint8_t chksum = 0, message_length = tuya_mcu_get_msg_length (msg);
    for (uint8_t i = 0 ; i < message_length -1; ++i)
        chksum += msg[i];
    
    chksum %= 256;
    printf ("%s: End\n", __func__);
    return (uint8_t)chksum;
}


void tuya_mcu_set_checksum(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    msg[tuya_mcu_get_msg_length(msg) - 1] = tuya_mcu_calc_checksum(msg);
    printf ("%s: End\n", __func__);
}


bool tuya_mcu_message_is_valid(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    uint8_t message_length = tuya_mcu_get_msg_length (msg);
    
    return (msg[E_MAGIC1] == 0x55 && msg[E_MAGIC2] == 0xaa && message_length >= TUYA_MCU_HEADER_SIZE && tuya_mcu_calc_checksum(msg) == msg[message_length - 1]);
}


uint8_t tuya_mcu_get_command(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    return msg[E_CMD];
}


uint8_t tuya_mcu_get_version(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    return msg[E_VERSION];
}


uint8_t tuya_mcu_get_payload_length(uint8_t msg[])
{
    printf ("%s: Start\n", __func__);
    return (msg[E_PAYLOAD_LENGTH_HI] * 0x100 + msg[E_PAYLOAD_LENGTH_LO]);
}


uint8_t tuya_mcu_get_payload(uint8_t msg[], uint8_t payload[])
{
    printf ("%s: Start\n", __func__);
    uint8_t payload_length = tuya_mcu_get_payload_length(msg);
    memcpy(&msg[E_PAYLOAD], payload, payload_length);
    printf ("%s: paylad length %d. End\n", __func__, payload_length);
    return (payload_length);
}

void tuya_mcu_set_payload_length(uint8_t msg[], uint8_t payload_length)
{
    printf ("%s: Start\n", __func__);
    if (payload_length <= MAX_BUFFER_LENGTH - TUYA_MCU_HEADER_SIZE)
    {
        msg[E_PAYLOAD_LENGTH_HI] = payload_length >> 8;
        msg[E_PAYLOAD_LENGTH_LO] = payload_length & 0xff;
    }
    else
    {
        /* need to add what to do if length is too big */
    }
    printf ("%s: End\n", __func__);
}

void tuya_mcu_set_payload(uint8_t msg[], uint8_t* payload, uint8_t payload_length)
{
    printf ("%s: Start\n", __func__);
    if (payload_length <= MAX_BUFFER_LENGTH - TUYA_MCU_HEADER_SIZE)
    {
        tuya_mcu_set_payload_length ( msg, payload_length);
        memcpy(&msg[E_PAYLOAD], payload, payload_length);
    }
    else
    {
        /* need to add what to do if length is too big */
    }
    printf ("%s: End\n", __func__);
}


void tuya_mcu_send_cmd(uint8_t cmd)
{
    printf ("%s: Start\n", __func__);
    uint8_t msg[MAX_BUFFER_LENGTH];
    
    msg[E_MAGIC1] = 0x55;
    msg[E_MAGIC2] = 0xaa;
    msg[E_VERSION] = 0;
    msg[E_CMD] = cmd;
    tuya_mcu_set_payload_length(msg,0);
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
    printf ("%s: End\n", __func__);
}


void tuya_mcu_send_message(uint8_t cmd, uint8_t payload[], uint8_t payload_length)
{
    printf ("%s: Start\n", __func__);

    uint8_t msg[MAX_BUFFER_LENGTH];
    
    msg[E_MAGIC1] = 0x55;
    msg[E_MAGIC2] = 0xaa;
    msg[E_VERSION] = 0;
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
    printf ("%s: End\n", __func__);

}
