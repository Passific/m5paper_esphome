#include "esphome/core/log.h"
#include "m5paper.h"
#include "driver/gpio.h"

namespace esphome::m5paper {

static const char *TAG = "m5paper.component";

void M5PaperComponent::setup() {
    ESP_LOGCONFIG(TAG, "m5paper starting up!");
    this->main_power_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->main_power_pin_->digital_write(true);

    this->battery_power_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->battery_power_pin_->digital_write(true);

    if (this->allow_esphome_deep_sleep_) {
        gpio_hold_en(GPIO_NUM_2);
        gpio_hold_en(GPIO_NUM_5);
    }
}

void M5PaperComponent::shutdown_main_power() {
    ESP_LOGW(TAG, "Shutting Down Power");
    if (this->allow_esphome_deep_sleep_) {
        gpio_hold_dis(GPIO_NUM_2);
        gpio_hold_dis(GPIO_NUM_5);
    }
    this->main_power_pin_->digital_write(false);
}

void M5PaperComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "M5Paper"
        " Allow ESPHome Deep Sleep: %s",
        YESNO(this->allow_esphome_deep_sleep_)
    );
}

} //namespace esphome::m5paper
