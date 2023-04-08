#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
//#include "led_strip.h"
#include "sdkconfig.h"


static const char *TAG = "LOG";
//static uint8_t wifiLedStatus = 0;

static uint32_t flowMeterLastState = 0;
static uint32_t flowMeterCounter = 0;

#define WIFI_ENABLED_LED 2
#define FLOW_METER_INPUT 5

#define INPUT_HIGH 1
#define INPUT_LOW 0

#define MAX_COUNTER 2147483646

typedef enum {
    NO_CHANGE = 0,
    CHANGE_TO_LOW = 1,
    CHANGE_TO_HIGH = 2
} input_level_change;

static input_level_change flowMeterInputLevel = NO_CHANGE;

input_level_change inputLevelChange(gpio_num_t inputAddress, uint32_t lastLevelState)
{
    int currentInputLevel = gpio_get_level(inputAddress);
    if (currentInputLevel == lastLevelState)
        return false;

    if (currentInputLevel == INPUT_LOW)
        return CHANGE_TO_LOW;
    // else
    return CHANGE_TO_HIGH;
}

void setup() {
    ESP_LOGI(TAG, "Setup stage started");
    
    ESP_LOGI(TAG, "Setup stage ended");

    // Configure LED
    gpio_reset_pin(WIFI_ENABLED_LED);
    gpio_set_direction(WIFI_ENABLED_LED, GPIO_MODE_OUTPUT);

    // Configure flow meter input
    gpio_reset_pin(FLOW_METER_INPUT);
    gpio_set_direction(FLOW_METER_INPUT, GPIO_MODE_INPUT);
    
}
void app_main(void)
{
    setup();
    ESP_LOGI(TAG,"Main stage started");

    while(1)
    {
        //ESP_LOGI(TAG,"Infinite loop enter");
        //gpio_set_level(WIFI_ENABLED_LED, wifiLedStatus);
        //wifiLedStatus = !wifiLedStatus;
        
        // Flow meter - add to counter on rising edge
        ESP_LOGI(TAG,"Flow meter counter = %d", (int)flowMeterCounter);
        
        flowMeterInputLevel = inputLevelChange(FLOW_METER_INPUT,flowMeterLastState);
        if (flowMeterInputLevel == CHANGE_TO_HIGH)
        {
            flowMeterLastState = !flowMeterLastState;
            flowMeterCounter++;
        }
        else if (flowMeterInputLevel == CHANGE_TO_LOW)
        {
            flowMeterLastState = !flowMeterLastState;
        }
        
        // reset counter
        if (flowMeterCounter >= MAX_COUNTER)
            flowMeterCounter = 0;
        vTaskDelay(10);
    }

}
