#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace bm8563 {

typedef struct
{
  int8_t hours;
  int8_t minutes;
  int8_t seconds;
} BM8563_TimeTypeDef;

typedef struct
{
  int8_t day;
  int8_t week;
  int8_t month;
  int16_t year;
} BM8563_DateTypeDef;

class BM8563 : public time::RealTimeClock, public i2c::I2CDevice {
  public:
    void setup() override;
    void update() override;
    void dump_config() override;

    void set_sleep_duration(uint32_t time_ms);
    void write_time();
    void read_time();
    void apply_sleep_duration();

  private:
    bool getVoltLow();

    void getTime(BM8563_TimeTypeDef* BM8563_TimeStruct);
    void getDate(BM8563_DateTypeDef* BM8563_DateStruct);

    void setTime(BM8563_TimeTypeDef* BM8563_TimeStruct);
    void setDate(BM8563_DateTypeDef* BM8563_DateStruct);

    int SetAlarmIRQ(int afterSeconds);
    int SetAlarmIRQ(const BM8563_TimeTypeDef &BM8563_TimeStruct);
    int SetAlarmIRQ(const BM8563_DateTypeDef &BM8563_DateStruct, const BM8563_TimeTypeDef &BM8563_TimeStruct);

    void clearIRQ();
    void disableIRQ();

    void WriteReg(uint8_t reg, uint8_t data);
    uint8_t ReadReg(uint8_t reg);

    uint8_t bcd2ToByte(uint8_t value);
    uint8_t byteToBcd2(uint8_t value);

    uint8_t trdata[7];
    optional<uint32_t> sleep_duration_;
    bool setupComplete;
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<BM8563> {
 public:
  void play(const Ts &... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<BM8563> {
 public:
  void play(const Ts &... x) override { this->parent_->read_time(); }
};

template<typename... Ts> class SleepAction : public Action<Ts...>, public Parented<BM8563> {
 public:
  void play(const Ts &... x) override { this->parent_->apply_sleep_duration(); }
};

}  // namespace bm8563
}  // namespace esphome