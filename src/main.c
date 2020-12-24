/*
 * Example of using esp-homekit library to control
 * wired thermostat, by providing abilit to swith on and off a series of relays
 *
 */

#define DEVICE_MANUFACTURER "maccoylton"
#define DEVICE_NAME "wired-thermostat"
#define DEVICE_MODEL "2"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"

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

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include <ota-api.h>

const int SAVE_DELAY = 1000;

homekit_characteristic_t wifi_check_interval   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_CHECK_INTERVAL, 10, .setter=wifi_check_interval_set);
/* checks the wifi is connected and flashes status led to indicated connected */
homekit_characteristic_t task_stats   = HOMEKIT_CHARACTERISTIC_(CUSTOM_TASK_STATS, true , .setter=task_stats_set);
homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
homekit_characteristic_t ota_beta     = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_BETA, false, .setter=ota_beta_set);
homekit_characteristic_t lcm_beta    = HOMEKIT_CHARACTERISTIC_(CUSTOM_LCM_BETA, false, .setter=lcm_beta_set);

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);

void set_target_temperature (homekit_value_t value);
void set_target_state (homekit_value_t value);

homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_( CURRENT_TEMPERATURE, 0);

homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_( TARGET_TEMPERATURE, 22, .setter = set_target_temperature);

homekit_characteristic_t units               = HOMEKIT_CHARACTERISTIC_( TEMPERATURE_DISPLAY_UNITS, 0 );

homekit_characteristic_t current_state       = HOMEKIT_CHARACTERISTIC_( CURRENT_HEATING_COOLING_STATE, 0);

homekit_characteristic_t target_state        = HOMEKIT_CHARACTERISTIC_( TARGET_HEATING_COOLING_STATE, 0,  .setter = set_target_state );

homekit_characteristic_t cooling_threshold   = HOMEKIT_CHARACTERISTIC_( COOLING_THRESHOLD_TEMPERATURE, 25 );
 
homekit_characteristic_t heating_threshold   = HOMEKIT_CHARACTERISTIC_( HEATING_THRESHOLD_TEMPERATURE, 15 );


// The GPIO pin that is oconnected to the button on the wired thermostat controller.
const int BUTTON_GPIO = 0;

const int status_led_gpio = 2; /*set the gloabl variable for the led to be used for showing status */

int led_off_value = -1;

uint8_t uart_port=0;


void set_target_temperature (homekit_value_t value){
 
    printf ("%s: %f", __func__, value.float_value);
    tuya_thermostat_setSetPointTemp (value.float_value, true);
    target_temperature.value = HOMEKIT_FLOAT(setPointTemp);
    homekit_characteristic_bounds_check(&target_temperature);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
    homekit_characteristic_notify(&target_temperature, target_temperature.value);
    printf ("target temperatre: %f", setPointTemp);
    printf ("%s: end\n", __func__);

}


void set_target_state (homekit_value_t value){
    
    printf ("%s: ", __func__);
    switch ( (int) value.int_value)
    {
        case 0:
            tuya_thermostat_setPower( false , true);
            current_state.value = HOMEKIT_UINT8(0);
            homekit_characteristic_bounds_check(&current_state);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_state, current_state.value);
            printf ("Off");
            break;
        case 1:
            tuya_thermostat_setPower( true , true);
            current_state.value = HOMEKIT_UINT8(1);
            homekit_characteristic_bounds_check(&current_state);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_state, current_state.value);
            printf ("Heat");
            break;
        case 2:
        case 3:
            printf ("Unsuported");
            break;
    }
    printf (" %s:end\n", __func__);
}



void gpio_init() {
    
    printf("Initialising buttons\n");
    
}

void tuya_thermostat_emitChange(TUYA_Thermostat_change_type_t cmd)
{
    
    printf ("%s: emit command : ", __func__);
    switch (cmd){
        case CHANGE_TYPE_POWER:
            current_state.value = HOMEKIT_UINT8(powerOn ? 1 : 0);
            homekit_characteristic_bounds_check(&current_state);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_state, current_state.value);
            printf ("power on: %d", powerOn);
            break;
        case CHANGE_TYPE_SETPOINT_TEMP:
            target_temperature.value = HOMEKIT_FLOAT(setPointTemp);
            homekit_characteristic_bounds_check(&target_temperature);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&target_temperature, target_temperature.value);
            printf ("target temperatre: %f", setPointTemp);
            break;
        case CHANGE_TYPE_INTERNAL_TEMP:
            current_temperature.value = HOMEKIT_FLOAT(internalTemp);
            homekit_characteristic_bounds_check(&current_temperature);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            printf ("current temperatre: %f", internalTemp);
            break;
        case CHANGE_TYPE_MODE:
            target_state.value = HOMEKIT_UINT8((setPointTemp > internalTemp) ? 1 : 0);
            homekit_characteristic_bounds_check(&target_state);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&target_state, target_state.value);
            printf ("mode: %d", target_state.value.int_value);
            break;
        case CHANGE_TYPE_ECONOMY:
            break;
        case CHANGE_TYPE_LOCK:
            break;
        case CHANGE_TYPE_SCHEDULE:
            break;
        case CHANGE_TYPE_EXTERNAL_TEMP:
            current_temperature.value = HOMEKIT_FLOAT(externalTemp);
            homekit_characteristic_bounds_check(&current_temperature);
            sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            printf ("current temperatre: %f", externalTemp);
            break;
        default:
            printf ("%s Unknow change type received from MCU\n", __func__);
    }
    printf ("%s: end\n", __func__);
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
            &cooling_threshold,
            &heating_threshold,
            &units,
            &ota_trigger,
            &wifi_reset,
            &ota_beta,
            &lcm_beta,
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
    printf ("%s: reason %d\n", __func__, reason);
    load_characteristic_from_flash(&target_temperature);
    homekit_characteristic_notify (&target_temperature, target_temperature.value);
    tuya_thermostat_setSetPointTemp (target_temperature.value.float_value, true);
    load_characteristic_from_flash(&current_state);
    homekit_characteristic_notify (&current_state, current_state.value);
    tuya_thermostat_setPower( (current_state.value.int_value == 1 )? true : false , true);

}

void save_characteristics ( ){
    
    printf ("%s:\n", __func__);
    save_characteristic_to_flash (&target_temperature, target_temperature.value);
    save_characteristic_to_flash (&current_state, current_state.value);
    save_characteristic_to_flash(&wifi_check_interval, wifi_check_interval.value);
}


void accessory_init_not_paired (void) {
    /* initalise anything you don't want started until wifi and homekit imitialisation is confirmed, but not paired */
    
}

void accessory_init (void ){
    /* initalise anything you don't want started until wifi and pairing is confirmed */
    load_characteristic_from_flash(&wifi_check_interval);
    homekit_characteristic_notify(&wifi_check_interval, wifi_check_interval.value);
    uart_set_baud(uart_port, 9600);
    xTaskCreate(tuya_thermostat_loop, "tuya_thermostat_loop", 1024 , NULL, tskIDLE_PRIORITY+1, NULL);
}

void user_init(void) {
    
    standard_init (&name, &manufacturer, &model, &serial, &revision);
    gpio_init();
    wifi_config_init(DEVICE_NAME, NULL, on_wifi_ready);
    
}

