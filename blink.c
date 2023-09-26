#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_types.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "tcpip_adapter.h"

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
//#define TIMER_INTERVAL0_SEC   (3.4179) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

//definicoes para usar os pinos do esp
#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_1     5
#define GPIO_INPUT_IO_0     4
#define GPIO_INPUT_PIN_SEL ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "MQTT";

int varMQTT = 50;

static ip4_addr_t s_ip_addr;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            if (strncmp(event->topic, "/topic/qos0", event->topic_len) == 0) {
                event->data[event->data_len] = 0;
                varMQTT = strtol(event->data,NULL,10);
                printf("%d", varMQTT);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://soldier.cloudmqtt.com",
	.port = 16934,
	.username = "uwavinxs",
	.password = "xEefC08QTzeN",
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
 #endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

static void on_wifi_connect(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
    tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data){
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
}

static void wifi_connect(){
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "teste",
            .password = "12345678",
        },
    };

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

}

int returnValueMQQT(){
 return varMQTT;
}

long int TIMER_INTERVAL0_SEC = 0; 

void IRAM_ATTR timer_group0_isr(void *para){
   
  gpio_set_level(GPIO_OUTPUT_IO_1, 1); 
  int timer_idx = (int) para;
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        TIMERG0.hw_timer[TIMER_0].config.enable = 0;
        TIMERG0.int_clr_timers.t0 = 1;
        uint64_t timer_counter_value = 
        ((uint64_t) TIMERG0.hw_timer[TIMER_0].cnt_high) << 32
        | TIMERG0.hw_timer[TIMER_0].cnt_low;
        timer_counter_value += (uint64_t) ((returnValueMQQT()*8.33e-3)/100 * TIMER_SCALE);
        TIMERG0.hw_timer[TIMER_0].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[TIMER_0].alarm_low = (uint32_t) timer_counter_value;
        
        gpio_set_level(GPIO_OUTPUT_IO_1, 1);
        
        timer_counter_value = 
        ((uint64_t) TIMERG0.hw_timer[TIMER_1].cnt_high) << 32
        | TIMERG0.hw_timer[TIMER_1].cnt_low;
        timer_counter_value += (uint64_t) (1e-6 * TIMER_SCALE);
        TIMERG0.hw_timer[TIMER_1].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[TIMER_1].alarm_low = (uint32_t) timer_counter_value;
        TIMERG0.hw_timer[TIMER_1].config.enable = 1; 
        TIMERG0.hw_timer[TIMER_1].config.alarm_en = TIMER_ALARM_EN; 
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        TIMERG0.int_clr_timers.t1 = 1; 
        TIMERG0.hw_timer[TIMER_1].config.enable = 0;
        gpio_set_level(GPIO_OUTPUT_IO_1, 0);
    }
}

void initTimer(int timer_idx){
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = 0;
    timer_init(TIMER_GROUP_0, timer_idx, &config);
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, 
    (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
}

void iniciaTimer(){
   //timer_start(TIMER_GROUP_0, TIMER_0);  
   TIMERG0.hw_timer[TIMER_0].config.enable = 1;
   TIMERG0.hw_timer[TIMER_0].config.alarm_en = TIMER_ALARM_EN;  
}

//interrupcao
static void IRAM_ATTR gpio_isr_handler(void* arg){
    iniciaTimer();
}

void initInterrupt(){
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);    
    gpio_set_intr_type(GPIO_INPUT_IO_1, GPIO_PIN_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);   
    gpio_pad_select_gpio(GPIO_OUTPUT_IO_1); 
    gpio_set_direction(GPIO_OUTPUT_IO_1, GPIO_MODE_OUTPUT);
}

void app_main(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //start timer
    initTimer(TIMER_0);
    initTimer(TIMER_1);

    initInterrupt();

    //inicializa mqtt
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_connect();
    mqtt_app_start();

    while (1) {
        //gpio_set_level(GPIO_OUTPUT_IO_1, 1);
        //gpio_set_level(GPIO_OUTPUT_IO_0, 0);
        //vTaskDelay(1000 / portTICK_RATE_MS);
    }

}