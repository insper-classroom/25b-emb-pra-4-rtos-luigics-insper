/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <stdio.h>
#include <task.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"
#include "ssd1306.h"

// === Definições para SSD1306 ===
ssd1306_t disp;

volatile int flag_BTN_R = 0;
volatile int flag_BTN_G = 0;
volatile int flag_BTN_B = 0;

volatile uint64_t subida, descida, tempo;
QueueHandle_t xQueueTime; //fila com informação do tempo to_us_since_boot
SemaphoreHandle_t xSemaphoreTrigger; //avisa o OLED que uma leitura foi disparada
QueueHandle_t xQueueDistance; //valor da distância em cm lido pela task_echo
volatile double dist;

// == funcoes de inicializacao ===
void btn_callback(uint gpio, uint32_t events) {
    if (events & 0x4) {
        if (gpio == BTN_PIN_R)
            flag_BTN_R = 1;
        else if (gpio == BTN_PIN_G)
            flag_BTN_G = 1;
        else if (gpio == BTN_PIN_B)
            flag_BTN_B = 1;
    }

    if (events == 0x8) {
        if (gpio == BTN_PIN_R)
            flag_BTN_R = 0;
        else if (gpio == BTN_PIN_G)
            flag_BTN_G = 0;
        else if (gpio == BTN_PIN_B)
            flag_BTN_B = 0;
    }
}

void pin_callback(uint gpio, uint32_t events) { //callback do pin_echo
    if(gpio == PIN_ECHO){
        if (events == 0x4) {
            subida = to_us_since_boot(get_absolute_time());
            xQueueSendFromISR(xQueueTime, &subida, 0);
        }
        if(events == 0x8){
            descida = to_us_since_boot(get_absolute_time());
            xQueueSendFromISR(xQueueTime, &descida, 0);
        }
    }
    
}

void oled_display_init(void) {
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    while (true) {

        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(500)) == pdTRUE) {
            disp.external_vcc = false;
            ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
            ssd1306_clear(&disp);
            ssd1306_show(&disp);        
            gpio_init(SSD1306_PIN_LITE);
            gpio_set_dir(SSD1306_PIN_LITE, GPIO_OUT);
            gpio_put(SSD1306_PIN_LITE, 0);
        }
    }
}

void disparar_trig(){
    gpio_put(PIN_TRIGGER,0);
    sleep_us(2);
    gpio_put(PIN_TRIGGER,1);
    sleep_us(10);
    gpio_put(PIN_TRIGGER,0);
    sleep_us(500);
}

void btns_init(void) {
    // Inicialização dos botões e LEDs
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);
    gpio_set_irq_enabled_with_callback(BTN_PIN_R,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, &btn_callback);
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);
    gpio_set_irq_enabled(BTN_PIN_G, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                         true);
    gpio_init(BTN_PIN_B);
    gpio_set_dir(BTN_PIN_B, GPIO_IN);
    gpio_pull_up(BTN_PIN_B);
    gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                         true);

    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_pull_up(PIN_ECHO);
    gpio_set_irq_enabled(PIN_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                        true);

    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);
}

void led_rgb_init(void) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 1);
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_put(LED_PIN_G, 1);
    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);
    gpio_put(LED_PIN_B, 1);
}

void trigger_task(void *p){ // Task responsável por gerar o trigger.
    disparar_trig();
    xSemaphoreGive(xSemaphoreTrigger);
    vTaskDelay(pdMS_TO_TICKS(500));
}


void echo_task(void *p){//Task que faz a leitura do tempo que o pino echo ficou levantado 
    while(1){
        if(xQueueReceive(xQueueTime,&tempo, pdMS_TO_TICKS(500))){
            dist = (0.0343 * (double)tempo) / 2.0;
            xQueueSend(xQueueDistance,&dist,0);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}
void task_1(void *p) {
    oled_display_init();
    btns_init();
    led_rgb_init();
    
    while (1) {
        if (flag_BTN_R == 1) {
            gpio_put(LED_PIN_R, 0);
            ssd1306_draw_string(&disp, 8, 24, 2, "RED");
            ssd1306_clear(&disp);
            ssd1306_show(&disp);
            gpio_put(LED_PIN_R, 1);
        }

        if (flag_BTN_G == 1) {
            gpio_put(LED_PIN_G, 0);
            ssd1306_draw_string(&disp, 8, 24, 2, "GREEN");

            ssd1306_clear(&disp);
            ssd1306_show(&disp);
            gpio_put(LED_PIN_G, 1);
        }

        if (flag_BTN_B == 1) {
            gpio_put(LED_PIN_B, 0);
            ssd1306_draw_string(&disp, 8, 24, 2, "BLUE");
            ssd1306_clear(&disp);
            ssd1306_show(&disp);
            gpio_put(LED_PIN_B, 1);
            flag_BTN_B = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main() {
    stdio_init_all();
    xQueueTime = xQueueCreate(32, sizeof(char));
    xQueueDistance = xQueueCreate(32, sizeof(char));
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xTaskCreate(task_1, "Task 1", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}
