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

#define SAVE_DELAY 2000




homekit_characteristic_t wifi_check_interval   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_CHECK_INTERVAL, 10, .setter=wifi_check_interval_set);
/* checks the wifi is connected and flashes status led to indicated connected */
homekit_characteristic_t task_stats   = HOMEKIT_CHARACTERISTIC_(CUSTOM_TASK_STATS, false , .setter=task_stats_set);
homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
homekit_characteristic_t ota_beta     = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_BETA, false, .setter=ota_beta_set);
homekit_characteristic_t lcm_beta    = HOMEKIT_CHARACTERISTIC_(CUSTOM_LCM_BETA, false, .setter=lcm_beta_set);

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);

homekit_value_t get_current_temperature ();
void set_target_temperature (homekit_value_t value);
homekit_value_t get_target_temperature ();
homekit_value_t get_current_state ();
homekit_value_t get_target_state ();
void set_target_state (homekit_value_t value);

homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_( CURRENT_TEMPERATURE, 0 ,.getter = get_current_temperature);

homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_( TARGET_TEMPERATURE, 22, .getter = get_target_temperature, .setter = set_target_temperature);

homekit_characteristic_t units               = HOMEKIT_CHARACTERISTIC_( TEMPERATURE_DISPLAY_UNITS, 0 );

homekit_characteristic_t current_state       = HOMEKIT_CHARACTERISTIC_( CURRENT_HEATING_COOLING_STATE, 0, .getter =get_current_state );

homekit_characteristic_t target_state        = HOMEKIT_CHARACTERISTIC_( TARGET_HEATING_COOLING_STATE, 0, .getter =get_target_state, .setter = set_target_state );

/*homekit_characteristic_t cooling_threshold   = HOMEKIT_CHARACTERISTIC_( COOLING_THRESHOLD_TEMPERATURE, 25, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );

homekit_characteristic_t heating_threshold   = HOMEKIT_CHARACTERISTIC_( HEATING_THRESHOLD_TEMPERATURE, 15, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
*/

// The GPIO pin that is oconnected to the button on the wired thermostat controller.
const int BUTTON_GPIO = 0;

const int status_led_gpio = 2; /*set the gloabl variable for the led to be used for showing status */

int led_off_value = -1;

uint8_t uart_port=0;


//tuya_thermostat thermostat = {false, 20.0f, 20.0f, 20.0f, MODE_SCHEDULE, false, false, {0}};


homekit_value_t get_current_temperature (){
    
    
}

homekit_value_t get_target_temperature (){
    
}


void set_target_temperature (homekit_value_t value){
    
    
}


homekit_value_t get_current_state (){
    
    
}


homekit_value_t get_target_state (){
    
}


void set_target_state (homekit_value_t value){
    
    
}



void gpio_init() {
    
    printf("Initialising buttons\n");
   
}



homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sprinkler, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        
        
        HOMEKIT_SERVICE(VALVE, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Valve 1"),
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
}

void save_characteristics ( ){
    
    printf ("%s:\n", __func__);
    save_characteristic_to_flash(&wifi_check_interval, wifi_check_interval.value);
}


void accessory_init_not_paired (void) {
    /* initalise anything you don't want started until wifi and homekit imitialisation is confirmed, but not paired */
    
}

void accessory_init (void ){
    /* initalise anything you don't want started until wifi and pairing is confirmed */
    load_characteristic_from_flash(&wifi_check_interval);
    homekit_characteristic_notify(&wifi_check_interval, wifi_check_interval.value);
}

void user_init(void) {
    
    standard_init (&name, &manufacturer, &model, &serial, &revision);
    uart_set_baud(uart_port, 9600);
    gpio_init();
    
    wifi_config_init(DEVICE_NAME, NULL, on_wifi_ready);
    
}

