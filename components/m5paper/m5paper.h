#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace m5paper {

class M5PaperComponent : public Component {
    void setup() override;
    void dump_config() override;
    /* Very early setup as takes care of powering other components */
    float get_setup_priority() const override { return setup_priority::BUS; };

public:
    void set_battery_power_pin(GPIOPin *power) { this->battery_power_pin_ = power; }
    void set_main_power_pin(GPIOPin *power) { this->main_power_pin_ = power; }
    void shutdown_main_power();

private:
    GPIOPin *battery_power_pin_{nullptr};
    GPIOPin *main_power_pin_{nullptr};

};

template<typename... Ts> class PowerAction : public Action<Ts...>, public Parented<M5PaperComponent> {
public:
    void play(const Ts &... x) override { this->parent_->shutdown_main_power(); }
};

} //namespace m5paper
} //namespace esphome