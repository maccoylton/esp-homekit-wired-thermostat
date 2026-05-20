
/*
 * Example of using esp-homekit library to control
 * a a  Sonoff S20 using HomeKit.
 * The esp-wifi-config library is also used in this
 * example. This makes us of an OAT mechanis created by HomeACessoryKid
 *
 * Copyright 2019-2020  David B Brown  (@maccoylton)
 *
 *
 */

#define DEVICE_MANUFACTURER "maccoylton"
#define DEVICE_NAME "wired-thermostat"
#define DEVICE_MODEL "2"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.1"

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>


#include <adv_button.h>
#include <led_codes.h>
#include <udplogger.h>
#include <custom_characteristics.h>
#include <shared_functions.h>

#include <tuya_thermostat.h>
#include <tuya_mcu.h>

#ifdef EXTRAS_TIMEKEEPING
    #include <sntp_impl.h>
#endif


// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include <ota-api.h>

const int SAVE_DELAY = 3000;

homekit_characteristic_t wifi_check_interval   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_CHECK_INTERVAL, 10, .setter=wifi_check_interval_set);
/* checks the wifi is connected and flashes status led to indicated connected */
homekit_characteristic_t task_stats   = HOMEKIT_CHARACTERISTIC_(CUSTOM_TASK_STATS, true , .setter=task_stats_set);
homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
homekit_characteristic_t ota_beta     = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_BETA, false, .setter=ota_beta_set);
homekit_characteristic_t lcm_beta    = HOMEKIT_CHARACTERISTIC_(CUSTOM_LCM_BETA, false, .setter=lcm_beta_set);
homekit_characteristic_t lcm_emergency = HOMEKIT_CHARACTERISTIC_(CUSTOM_LCM_EMERGENCY, false, .setter=lcm_emergency_set);

homekit_characteristic_t preserve_state   = HOMEKIT_CHARACTERISTIC_(CUSTOM_PRESERVE_STATE, false, .setter=preserve_state_set);
homekit_characteristic_t log_level_ch     = HOMEKIT_CHARACTERISTIC_(CUSTOM_LOG_LEVEL, 2, .setter=log_level_set);

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);

void set_target_temperature (homekit_value_t value);
void set_target_state (homekit_value_t value);
void set_external_temp_sensor (homekit_value_t value);

homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_( CURRENT_TEMPERATURE, 0, .min_step = (float[]) {0.5});

homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_( TARGET_TEMPERATURE, 22, .min_step = (float[]) {0.5}, .setter = set_target_temperature);

homekit_characteristic_t units               = HOMEKIT_CHARACTERISTIC_( TEMPERATURE_DISPLAY_UNITS, 0 );

homekit_characteristic_t current_state       = HOMEKIT_CHARACTERISTIC_( CURRENT_HEATING_COOLING_STATE, 0, .valid_values={.count=2, .values=(uint8_t[]) {0,1}});

homekit_characteristic_t target_state        = HOMEKIT_CHARACTERISTIC_( TARGET_HEATING_COOLING_STATE, 0,  .valid_values={.count=2, .values=(uint8_t[]) {0,1}}, .setter = set_target_state );

//homekit_characteristic_t cooling_threshold   = HOMEKIT_CHARACTERISTIC_( COOLING_THRESHOLD_TEMPERATURE, 25 );

//homekit_characteristic_t heating_threshold   = HOMEKIT_CHARACTERISTIC_( HEATING_THRESHOLD_TEMPERATURE, 15 );
homekit_characteristic_t external_temp_sensor = HOMEKIT_CHARACTERISTIC_(CUSTOM_EXTERNALTEMP_SENSOR, false, .setter = set_external_temp_sensor);

// The GPIO pin that is oconnected to the button on the wired thermostat controller.
const int BUTTON_GPIO = 0;

const int status_led_gpio = 2; /*set the gloabl variable for the led to be used for showing status */

int led_off_value = -1;

uint8_t uart_port=0;

void set_current_state(){
    LOG(LOG_EVENT, "%s: target state: %d, current state: %d, target temp: %2.1f, current temp:%2.1f\n", __func__, target_state.value.int_value, current_state.value.int_value, target_temperature.value.float_value ,current_temperature.value.float_value);
    switch (target_state.value.int_value){
        case 0: /* off */
            current_state.value = HOMEKIT_UINT8 (0);
            break;
        case 1: /* heat */
        {
            /* the BHT 002 doesn't have a curent state, so need to compare current and target temperate */
            if ( current_temperature.value.float_value < (target_temperature.value.float_value) )
                current_state.value = HOMEKIT_UINT8 (1);
            else
                current_state.value = HOMEKIT_UINT8 (0);
        }
        case 2: /* cool */
        case 3: /* auto */
        default:
            break;
    }
    LOG(LOG_EVENT, "%s: New current state: %d\n", __func__, current_state.value.int_value);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
    homekit_characteristic_notify(&current_state, current_state.value);
}


void set_external_temp_sensor (homekit_value_t value){
    
    LOG(LOG_EVENT, "%s: %s\n", __func__, value.bool_value ? "true" : "false");
    externalTempSensor = value.bool_value;
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
    if (externalTempSensor == true) {
        tuya_thermostat_emitChange (CHANGE_TYPE_EXTERNAL_TEMP);
    } else {
        tuya_thermostat_emitChange (CHANGE_TYPE_INTERNAL_TEMP);
    }
    LOG(LOG_FLOW, "%s: end\n", __func__);
    
}

void set_target_temperature (homekit_value_t value){
    /* called when the IOS device updates the value */
    LOG(LOG_EVENT, "%s: %2.1f\n", __func__, value.float_value);
    target_temperature.value = HOMEKIT_FLOAT(value.float_value);
    homekit_characteristic_bounds_check(&target_temperature);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
    set_current_state();
    /* save routine will also notify MCU of new target temperature to avoid bounce */

    LOG(LOG_EVENT, "%s: target temperature: %2.1f\n", __func__, target_temperature.value.float_value);
    homekit_characteristic_notify(&target_temperature, target_temperature.value);
    /* if the target temperature has changed then the current state may have changed */
    
}


void set_target_state (homekit_value_t value){
    /* called when the IOS device updates the value */
    LOG(LOG_ACTION, "%s\n", __func__);
    switch ( (int) value.int_value)
    {
        case 0: /* off */
            tuya_thermostat_setPower( false , true);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            LOG(LOG_ACTION, "%s: Off\n", __func__);
            break;
        case 1: /* heat */
            tuya_thermostat_setPower( true , true);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            LOG(LOG_ACTION, "%s: Heat\n", __func__);
            break;
        case 2: /* cool */
        case 3: /* auto */
        default:
            LOG(LOG_ERR, "%s: Unsuported\n", __func__);
    }
    
    target_state.value = HOMEKIT_UINT8(value.int_value);
    set_current_state();
    /* if the target state has changed then the current state may have changed */
}


void gpio_init() {
    
    LOG(LOG_ACTION, "%s: Initialising buttons\n", __func__);
    
}


void tuya_thermostat_emitChange(TUYA_Thermostat_change_type_t cmd)
{
    
    LOG(LOG_EVENT, "%s: emit command\n", __func__);
    switch (cmd){
        case CHANGE_TYPE_POWER:
            LOG(LOG_EVENT, "%s: power on: %d, target state:%d\n", __func__, powerOn, target_state.value.int_value);
            target_state.value = HOMEKIT_UINT8(powerOn ? 1 : 0);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&target_state, target_state.value);
            set_current_state();
            break;
        case CHANGE_TYPE_SETPOINT_TEMP:
            LOG(LOG_EVENT, "%s: setPoint: %f, target temperature: %f\n", __func__, setPointTemp, target_temperature.value.float_value);
            target_temperature.value = HOMEKIT_FLOAT(setPointTemp);
            homekit_characteristic_bounds_check(&target_temperature);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&target_temperature, target_temperature.value);
            set_current_state();
            break;
        case CHANGE_TYPE_INTERNAL_TEMP:
            LOG(LOG_EVENT, "%s: internalTemp: %f, current temperature: %f\n", __func__, internalTemp, current_temperature.value.float_value);
            current_temperature.value = HOMEKIT_FLOAT(internalTemp);
            homekit_characteristic_bounds_check(&current_temperature);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            set_current_state();
            break;
        case CHANGE_TYPE_MODE:
            LOG(LOG_ERR, "%s: mode: unsupported\n", __func__);
            break;
        case CHANGE_TYPE_ECONOMY:
            LOG(LOG_ERR, "%s: economy: unsupported\n", __func__);
            break;
        case CHANGE_TYPE_LOCK:
            LOG(LOG_ERR, "%s: lock: unsupported\n", __func__);
            break;
        case CHANGE_TYPE_SCHEDULE:
            LOG(LOG_ERR, "%s: schedule: unsupported\n", __func__);
            break;
        case CHANGE_TYPE_EXTERNAL_TEMP:
            LOG(LOG_EVENT, "%s: current temperature: %f\n", __func__, externalTemp);
            current_temperature.value = HOMEKIT_FLOAT(externalTemp);
            homekit_characteristic_bounds_check(&current_temperature);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            set_current_state();
            break;
        default:
            LOG(LOG_ERR, "%s: Unknow change type received from MCU\n", __func__);
    }
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        
        
        HOMEKIT_SERVICE(THERMOSTAT, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, DEVICE_NAME),
            &current_temperature,
            &target_temperature,
            &current_state,
            &target_state,
//            &cooling_threshold,
//            &heating_threshold,
            &units,
            &external_temp_sensor,
            &preserve_state,
            &log_level_ch,
            &ota_trigger,
            &wifi_reset,
            &ota_beta,
            &lcm_beta,
            &lcm_emergency,
            &task_stats,
            &wifi_check_interval,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111",
    .setupId = "1234",
    .on_event = on_homekit_event
};


void recover_from_reset (int reason){
    /* called if we restarted abnormally */
    LOG(LOG_ACTION, "%s: reason %d\n", __func__, reason);
    load_characteristic_from_flash(&target_temperature);
    homekit_characteristic_notify (&target_temperature, target_temperature.value);
    tuya_thermostat_setSetPointTemp (target_temperature.value.float_value, true);
    
    load_characteristic_from_flash(&target_state);
    homekit_characteristic_notify (&target_state, target_state.value);
    tuya_thermostat_setPower( (target_state.value.int_value == 1 )? true : false , true);
    
}

void save_characteristics ( ){
    
    LOG(LOG_EVENT, "%s:\n", __func__);
    save_characteristic_to_flash (&target_temperature, target_temperature.value);
    save_characteristic_to_flash (&target_state, target_state.value);
    save_characteristic_to_flash (&external_temp_sensor, external_temp_sensor.value);
    save_characteristic_to_flash(&preserve_state, preserve_state.value);
    save_characteristic_to_flash(&wifi_check_interval, wifi_check_interval.value);
    
    /* do this hear to stop temperature bounce */
    tuya_thermostat_setSetPointTemp (target_temperature.value.float_value, true);
}


void accessory_init_not_paired (void) {
    /* initalise anything you don't want started until wifi and homekit imitialisation is confirmed, but not paired */
    
}

void accessory_init (void ){
    /* initalise anything you don't want started until wifi and pairing is confirmed */

    setup_sntp();
    
    timeAvailable = sntp_on;
    /* global  used in tuya_mcu, set after sntp is initialised*/
    
    load_characteristic_from_flash(&wifi_check_interval);
    homekit_characteristic_notify(&wifi_check_interval, wifi_check_interval.value);
    
    load_characteristic_from_flash(&target_temperature);
    homekit_characteristic_notify (&target_temperature, target_temperature.value);
    
    load_characteristic_from_flash(&target_state);
    homekit_characteristic_notify (&target_state, target_state.value);
    tuya_thermostat_setPower( (target_state.value.int_value == 1 )? true : false , true);
    
    task_stats.value.bool_value = true;
    task_stats_set (task_stats.value);
    

    tuya_mcu_init();
    uart_set_baud(uart_port, 9600);
    /* move to inside tuya_mcu_init - xTaskCreate(tuya_mcu_init, "tuya_mcu_init", 512 , NULL, tskIDLE_PRIORITY+1, NULL); */
    /*tuya_thermostat_setSetPointTemp (target_temperature.value.float_value, true);*/

}

void user_init(void) {
    
    standard_init (&name, &manufacturer, &model, &serial, &revision);
    gpio_init();
    wifi_config_init(DEVICE_NAME, NULL, on_wifi_ready);
    
}

