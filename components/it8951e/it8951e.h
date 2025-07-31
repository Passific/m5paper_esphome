#pragma once

#include "esphome/core/component.h"
#include "esphome/core/version.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace it8951e {

enum it8951eModel
{
  M5EPD = 0,
  it8951eModelsEND // MUST be last
};

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2023, 12, 0)
class IT8951ESensor : public display::DisplayBuffer,
#else
class IT8951ESensor : public PollingComponent, public display::DisplayBuffer,
#endif  // VERSION_CODE(2023, 12, 0)
                      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                            spi::DATA_RATE_20MHZ> {
 public:
  float get_loop_priority() const override { return 0.0f; };
  float get_setup_priority() const override { return setup_priority::PROCESSOR; };

/*
---------------------------------------- Refresh mode description
---------------------------------------- INIT The initialization (INIT) mode is
used to completely erase the display and leave it in the white state. It is
useful for situations where the display information in memory is not a faithful
representation of the optical state of the display, for example, after the
device receives power after it has been fully powered down. This waveform
switches the display several times and leaves it in the white state.

DU
The direct update (DU) is a very fast, non-flashy update. This mode supports
transitions from any graytone to black or white only. It cannot be used to
update to any graytone other than black or white. The fast update time for this
mode makes it useful for response to touch sensor or pen input or menu selection
indictors.

GC16
The grayscale clearing (GC16) mode is used to update the full display and
provide a high image quality. When GC16 is used with Full Display Update the
entire display will update as the new image is written. If a Partial Update
command is used the only pixels with changing graytone values will update. The
GC16 mode has 16 unique gray levels.

GL16
The GL16 waveform is primarily used to update sparse content on a white
background, such as a page of anti-aliased text, with reduced flash. The GL16
waveform has 16 unique gray levels.

GLR16
The GLR16 mode is used in conjunction with an image preprocessing algorithm to
update sparse content on a white background with reduced flash and reduced image
artifacts. The GLR16 mode supports 16 graytones. If only the even pixel states
are used (0, 2, 4, … 30), the mode will behave exactly as a traditional GL16
waveform mode. If a separately-supplied image preprocessing algorithm is used,
the transitions invoked by the pixel states 29 and 31 are used to improve
display quality. For the AF waveform, it is assured that the GLR16 waveform data
will point to the same voltage lists as the GL16 data and does not need to be
stored in a separate memory.

GLD16
The GLD16 mode is used in conjunction with an image preprocessing algorithm to
update sparse content on a white background with reduced flash and reduced image
artifacts. It is recommended to be used only with the full display update. The
GLD16 mode supports 16 graytones. If only the even pixel states are used (0, 2,
4, … 30), the mode will behave exactly as a traditional GL16 waveform mode. If a
separately-supplied image preprocessing algorithm is used, the transitions
invoked by the pixel states 29 and 31 are used to refresh the background with a
lighter flash compared to GC16 mode following a predetermined pixel map as
encoded in the waveform file, and reduce image artifacts even more compared to
the GLR16 mode. For the AF waveform, it is assured that the GLD16 waveform data
will point to the same voltage lists as the GL16 data and does not need to be
stored in a separate memory.

DU4
The DU4 is a fast update time (similar to DU), non-flashy waveform. This mode
supports transitions from any gray tone to gray tones 1,6,11,16 represented by
pixel states [0 10 20 30]. The combination of fast update time and four gray
tones make it useful for anti-aliased text in menus. There is a moderate
increase in ghosting compared with GC16.

A2
The A2 mode is a fast, non-flash update mode designed for fast paging turning or
simple black/white animation. This mode supports transitions from and to black
or white only. It cannot be used to update to any graytone other than black or
white. The recommended update sequence to transition into repeated A2 updates is
shown in Figure 1. The use of a white image in the transition from 4-bit to
1-bit images will reduce ghosting and improve image quality for A2 updates.

*/

  struct IT8951DevInfo_s
  {
      int usPanelW;
      int usPanelH;
      uint16_t usImgBufAddrL;
      uint16_t usImgBufAddrH;
      char usFWVersion[16];
      char usLUTVersion[16];
  };

  struct IT8951Dev_s
  {
      struct IT8951DevInfo_s devInfo;
      display::DisplayType displayType;
  };

  enum update_mode_e         //             Typical
  {                          //   Ghosting  Update Time  Usage
      UPDATE_MODE_INIT = 0,  // * N/A       2000ms       Display initialization,
      UPDATE_MODE_DU   = 1,  //   Low       260ms        Monochrome menu, text input, and touch screen input
      UPDATE_MODE_GC16 = 2,  // * Very Low  450ms        High quality images
      UPDATE_MODE_GL16 = 3,  // * Medium    450ms        Text with white background
      UPDATE_MODE_GLR16 = 4, //   Low       450ms        Text with white background
      UPDATE_MODE_GLD16 = 5, //   Low       450ms        Text and graphics with white background
      UPDATE_MODE_DU4 = 6,   // * Medium    120ms        Fast page flipping at reduced contrast
      UPDATE_MODE_A2 = 7,    //   Medium    290ms        Anti-aliased text in menus / touch and screen input
      UPDATE_MODE_NONE = 8
  };  // The ones marked with * are more commonly used

  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }

  void set_reversed(bool reversed) { this->reversed_ = reversed; }
  void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }
  void set_model(it8951eModel model);
  void set_sleep_when_done(bool sleep_when_done) { this->sleep_when_done_ = sleep_when_done; }

  void setup() override;
  void update() override;
  void update_slow();
  void dump_config() override;

  display::DisplayType get_display_type() override { return IT8951DevAll[this->model_].displayType; }

  void clear(bool init);

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  int get_width_internal() override { return usPanelW_; };

  int get_height_internal() override { return usPanelH_; };

  uint32_t get_buffer_length_();


 private:
  struct IT8951Dev_s IT8951DevAll[it8951eModel::it8951eModelsEND]
  { // it8951eModel::M5EPD
    960,    // .devInfo.usPanelW
    540,    // .devInfo.usPanelH
    0x36E0, // .devInfo.usImgBufAddrL
    0x0012, // .devInfo.usImgBufAddrH
    "",     // .devInfo.usFWVersion
    "",     // .devInfo.usFWVersion
    display::DisplayType::DISPLAY_TYPE_GRAYSCALE // .displayType (M5EPD supports 16 gray scale levels)
  };

  void get_device_info(struct IT8951DevInfo_s *info);

  int max_x = 0;
  int max_y = 0;
  int min_x = 960;
  int min_y = 540;
  uint16_t m_endian_type = 0;
  uint16_t m_pix_bpp = 0;

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};

  int usPanelW_{0};
  int usPanelH_{0};
  bool reversed_{false};
  uint32_t reset_duration_{100};
  bool sleep_when_done_{true}; // If true, the display will go to sleep after each update
  enum it8951eModel model_{it8951eModel::M5EPD};

  void reset(void);

  /* 1000ms timeout because I've seen it take up to 750ms (and ~310ms on average)
   * Mostly for screen sleep and run commands */
  void wait_busy(uint32_t timeout = 1000);
  void check_busy(uint32_t timeout = 1000);

  uint16_t get_vcom();
  void set_vcom(uint16_t vcom);

  // comes from ref driver code from waveshare
  uint16_t read_word();
  void read_words(void *buf, uint32_t length);

  void write_two_byte16(uint16_t type, uint16_t cmd);
  void write_command(uint16_t cmd);
  void write_word(uint16_t cmd);
  void write_reg(uint16_t addr, uint16_t data);
  void set_target_memory_addr(uint16_t tar_addrL, uint16_t tar_addrH);
  void write_args(uint16_t cmd, uint16_t *args, uint16_t length);

  void set_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void update_area(uint16_t x, uint16_t y, uint16_t w,
                    uint16_t h, update_mode_e mode);



  void write_buffer_to_display(uint16_t x, uint16_t y, uint16_t w,
                                uint16_t h, const uint8_t *gram);
  void write_display();
  void write_display_slow();
};

template<typename... Ts> class ClearAction : public Action<Ts...>, public Parented<IT8951ESensor> {
 public:
  void play(Ts... x) override { this->parent_->clear(true); }
};

template<typename... Ts> class UpdateSlowAction : public Action<Ts...>, public Parented<IT8951ESensor> {
 public:
  void play(Ts... x) override { this->parent_->update_slow(); }
};

}  // namespace it8951e
}  // namespace esphome
