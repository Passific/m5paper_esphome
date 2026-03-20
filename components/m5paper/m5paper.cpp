#include "esphome/core/log.h"
#include "m5paper.h"
#include "driver/gpio.h"

namespace esphome::m5paper {

// hack to hold power lines up in deep sleep mode
// battery life isn't great with deep sleep, recommend bm8563 sleep
#define ALLOW_ESPHOME_DEEP_SLEEP true

static const char *TAG = "m5paper.component";

void M5PaperComponent::setup() {
    ESP_LOGCONFIG(TAG, "m5paper starting up!");
    this->main_power_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->main_power_pin_->digital_write(true);

    this->battery_power_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->battery_power_pin_->digital_write(true);

    if (ALLOW_ESPHOME_DEEP_SLEEP) {
        gpio_hold_en(GPIO_NUM_2);
        gpio_hold_en(GPIO_NUM_5);
    }
}

void M5PaperComponent::shutdown_main_power() {
    ESP_LOGW(TAG, "Shutting Down Power");
    if (ALLOW_ESPHOME_DEEP_SLEEP) {
        gpio_hold_dis(GPIO_NUM_2);
        gpio_hold_dis(GPIO_NUM_5);
    }
    this->main_power_pin_->digital_write(false);
}

void M5PaperComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "M5Paper");
}

} //namespace esphome::m5paper
