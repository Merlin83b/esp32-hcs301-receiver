#include <stdio.h>
#include <inttypes.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define HCS_RECEIVER_PIN  GPIO_NUM_34    //connected to 433Mhz receiver

struct HCS301 {
    bool     BatteryLow; 
    bool     Repeat;
    uint8_t  Btn1;
    uint8_t  Btn2; 
    uint8_t  Btn3; 
    uint8_t  Btn4;
    uint32_t SerialNum;
    uint32_t Encrypt;
};

uint8_t           HCS_preamble_count = 0;
uint32_t          HCS_last_change = 0;
uint32_t          HCS_start_preamble = 0;
uint8_t           HCS_bit_counter;        
uint8_t           HCS_bit_array[66];     
uint32_t          HCS_Te = 400;                //Typical Te duration
uint32_t          HCS_Te2_3 = 600;             //HCS_TE * 3 / 2

static QueueHandle_t received_code_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // Adapted from https://www.manuelschutze.com/?p=333
    
    uint32_t cur_timestamp = esp_timer_get_time();
    uint8_t  cur_status = gpio_get_level(HCS_RECEIVER_PIN);
    uint32_t pulse_duration = cur_timestamp - HCS_last_change;
    HCS_last_change = cur_timestamp;

    // gets preamble
    if(HCS_preamble_count < 12) {
        if(cur_status == 1){
            if( ((pulse_duration > 150) && (pulse_duration < 500)) || HCS_preamble_count == 0) {
                if(HCS_preamble_count == 0){
                    HCS_start_preamble = cur_timestamp;
                }
            } else {
                HCS_preamble_count = 0;
                goto exit; 
            }
        } else {
            if((pulse_duration > 300) && (pulse_duration < 600)) {
                HCS_preamble_count ++;
                if(HCS_preamble_count == 12) {
                    HCS_Te = (cur_timestamp - HCS_start_preamble) / 23;
                    HCS_Te2_3 = HCS_Te * 3 / 2;
                    HCS_bit_counter = 0;
                    goto exit; 
                }
            } else {
                HCS_preamble_count = 0;
                goto exit; 
            }
        }
    }
    
    // gets data
    if(HCS_preamble_count == 12) {
        if(cur_status == 1){
            if(((pulse_duration > 250) && (pulse_duration < 900)) || HCS_bit_counter == 0){
                // beginning of data pulse
            } else {
                // incorrect pause between pulses
                HCS_preamble_count = 0;
                goto exit; 
            }
        } else {
            // end of data pulse
            if((pulse_duration > 250) && (pulse_duration < 900)) {
                HCS_bit_array[65 - HCS_bit_counter] = (pulse_duration > HCS_Te2_3) ? 0 : 1;
                HCS_bit_counter++;  
                if(HCS_bit_counter == 66){
                    // all bits captured
                    xQueueSendFromISR(received_code_queue, &HCS_bit_array, NULL);
                    HCS_preamble_count = 0;
                }
            } else {
                HCS_preamble_count = 0;
                goto exit; 
            }
        }
    }
    exit:;
}


static void process_hcs301(void* arg)
{
    uint8_t       raw_data[66];  
    struct HCS301 data;

    for(;;) {
        if(xQueueReceive(received_code_queue, &raw_data, portMAX_DELAY)) {
            data.Repeat = raw_data[0];
            data.BatteryLow = raw_data[1];
            data.Btn1 = raw_data[2];
            data.Btn2 = raw_data[3];
            data.Btn3 = raw_data[4];
            data.Btn4 = raw_data[5];

            data.SerialNum = 0;
            for(int i = 6; i < 34;i++){
                data.SerialNum = (data.SerialNum << 1) + raw_data[i];
            };

            data.Encrypt = 0;
            for(int i = 34; i < 66;i++){
                data.Encrypt = (data.Encrypt << 1) + raw_data[i];
            };

            printf("Remote %lu sent %lu with buttons %d %d %d %d (low bat %d, repeat %d)\n", data.SerialNum, data.Encrypt, data.Btn1, data.Btn2, data.Btn3, data.Btn4, data.BatteryLow, data.Repeat);
        }
    }
}


void app_main(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = 1ULL<<HCS_RECEIVER_PIN;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-down and pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //interrupt of any edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);

    received_code_queue = xQueueCreate(10, sizeof(uint8_t[66]));
    xTaskCreate(process_hcs301, "process_hcs301_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(HCS_RECEIVER_PIN, gpio_isr_handler, NULL);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}