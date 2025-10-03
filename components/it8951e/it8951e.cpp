#include "esphome/core/log.h"
#include "it8951e.h"
#include "it8951.h"
#include "esphome/core/application.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace it8951e {

static const char *TAG = "it8951e.display";

void IT8951ESensor::write_two_byte16(uint16_t type, uint16_t cmd) {
    this->wait_busy();
    this->enable();

    this->write_byte16(type);
    this->wait_busy();
    this->write_byte16(cmd);

    this->disable();
}

uint16_t IT8951ESensor::read_word() {
    this->wait_busy();
    this->enable();
    this->write_byte16(0x1000);
    this->wait_busy();

    // dummy
    this->write_byte16(0x0000);
    this->wait_busy();

    uint8_t recv[2];
    this->read_array(recv, sizeof(recv));
    uint16_t word = encode_uint16(recv[0], recv[1]);

    this->disable();
    return word;
}

void IT8951ESensor::read_words(void *buf, uint32_t length) {
    ExternalRAMAllocator<uint16_t> allocator(ExternalRAMAllocator<uint16_t>::ALLOW_FAILURE);
    uint16_t *buffer = allocator.allocate(length);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Read FAILED to allocate.");
        return;
    }

    this->wait_busy();
    this->enable();
    this->write_byte16(0x1000);
    this->wait_busy();

    // dummy
    this->write_byte16(0x0000);
    this->wait_busy();

    for (size_t i = 0; i < length; i++) {
        uint8_t recv[2];
        this->read_array(recv, sizeof(recv));
        buffer[i] = encode_uint16(recv[0], recv[1]);
    }

    this->disable();

    memcpy(buf, buffer, length);

    allocator.deallocate(buffer, length);
}

void IT8951ESensor:: write_command(uint16_t cmd) {
    this->write_two_byte16(0x6000, cmd);
}

void IT8951ESensor::write_word(uint16_t cmd) {
    this->write_two_byte16(0x0000, cmd);
}

void IT8951ESensor::write_reg(uint16_t addr, uint16_t data) {
    this->write_command(0x0011);  // tcon write reg command
    this->wait_busy();
    this->enable();
    this->write_byte(0x0000); // Preamble
    this->wait_busy();
    this->write_byte16(addr);
    this->wait_busy();
    this->write_byte16(data);
    this->disable();
}

void IT8951ESensor::set_target_memory_addr(uint16_t tar_addrL, uint16_t tar_addrH) {
    this->write_reg(IT8951_LISAR + 2, tar_addrH);
    this->write_reg(IT8951_LISAR, tar_addrL);
}

void IT8951ESensor::write_args(uint16_t cmd, uint16_t *args, uint16_t length) {
    this->write_command(cmd);
    for (uint16_t i = 0; i < length; i++) {
        this->write_word(args[i]);
    }
}

void IT8951ESensor::set_area(uint16_t x, uint16_t y, uint16_t w,
                                  uint16_t h) {

    if (0 == x && 0 == y && w == this->get_width_internal() && h == this->get_height_internal()) {
        // Full screen
        uint16_t args[1];
        args[0] = (this->m_endian_type << 8 | this->m_pix_bpp << 4);
        this->write_args(IT8951_TCON_LD_IMG, args, sizeof(args));
    }
    else {
        // Partial update
        uint16_t args[5];
        args[0] = (this->m_endian_type << 8 | this->m_pix_bpp << 4);
        args[1] = x;
        args[2] = y;
        args[3] = w;
        args[4] = h;
        this->write_args(IT8951_TCON_LD_IMG_AREA, args, sizeof(args));
    }
}

void IT8951ESensor::wait_busy(uint32_t timeout) {
    const uint32_t start_time = millis();
    while (1) {
        if (this->busy_pin_->digital_read()) {
            break;
        }

        if (millis() - start_time > timeout) {
            ESP_LOGE(TAG, "Pin busy timeout");
            break;
        }
    }
}

void IT8951ESensor::check_busy(uint32_t timeout) {
    const uint32_t start_time = millis();
    while (1) {
        this->write_command(IT8951_TCON_REG_RD);
        this->write_word(IT8951_LUTAFSR);
        uint16_t word = this->read_word();
        if (word == 0) {
            break;
        }

        if (millis() - start_time > timeout) {
            ESP_LOGE(TAG, "SPI busy timeout %i", word);
            break;
        }

    }
}

void IT8951ESensor::update_area(uint16_t x, uint16_t y, uint16_t w,
                                     uint16_t h, update_mode_e mode) {
    if (mode == update_mode_e::UPDATE_MODE_NONE) {
        return;
    }

    this->check_busy();

    uint16_t args[7];
    args[0] = x;
    args[1] = y;
    args[2] = w;
    args[3] = h;
    args[4] = mode;
    args[5] = this->IT8951DevAll[this->model_].devInfo.usImgBufAddrL;
    args[6] = this->IT8951DevAll[this->model_].devInfo.usImgBufAddrH;

    this->write_args(IT8951_I80_CMD_DPY_BUF_AREA, args, sizeof(args));
}

void IT8951ESensor::reset(void) {
    this->reset_pin_->digital_write(true);
    this->reset_pin_->digital_write(false);
    delay(this->reset_duration_);
    this->reset_pin_->digital_write(true);
    delay(100);
}

uint32_t IT8951ESensor::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }

void IT8951ESensor::get_device_info(struct IT8951DevInfo_s *info) {
    this->write_command(IT8951_I80_CMD_GET_DEV_INFO);
    this->read_words(info, sizeof(struct IT8951DevInfo_s)/2); // Polling HRDY for each words(2-bytes) if possible
}

uint16_t IT8951ESensor::get_vcom() {
    this->write_command(IT8951_I80_CMD_VCOM); // tcon vcom get command
    this->write_word(0x0000);
    const uint16_t vcom = this->read_word();
    ESP_LOGI(TAG, "VCOM = %.02fV", (float)vcom/1000);
    return vcom;
}

void IT8951ESensor::set_vcom(uint16_t vcom) {
    this->write_command(IT8951_I80_CMD_VCOM); // tcon vcom set command
    this->write_word(0x0001);
    this->write_word(vcom);
}

void IT8951ESensor::setup() {
    ESP_LOGCONFIG(TAG, "Init Starting.");
    this->spi_setup();

    if (nullptr != this->reset_pin_) {
        this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
        this->reset();
    }

    this->busy_pin_->pin_mode(gpio::FLAG_INPUT);

    /* Not reliable, hard-coded in the model device info (same as M5Stack) */
    //this->get_device_info(&(this->IT8951DevAll[this->model_].devInfo));
    this->dump_config();

    this->write_command(IT8951_TCON_SYS_RUN);

    // enable pack write
    this->write_reg(IT8951_I80CPCR, 0x0001);

    // set vcom to -2.30v
    const uint16_t vcom = this->get_vcom();
    if (2300 != vcom) {
        this->set_vcom(2300);
        this->get_vcom();
    }

    // Allocate display buffer
    this->init_internal_(this->get_buffer_length_());

    ESP_LOGCONFIG(TAG, "Init Done.");
}

/** @brief Write the image at the specified location, Partial update
 * @param x Update X coordinate, >>> Must be a multiple of 4 <<<
 * @param y Update Y coordinate
 * @param w width of gram, >>> Must be a multiple of 4 <<<
 * @param h height of gram
 * @param gram 4bpp gram data
 */
void IT8951ESensor::write_buffer_to_display(uint16_t x, uint16_t y, uint16_t w,
                                            uint16_t h, const uint8_t *gram) {
    this->m_endian_type = IT8951_LDIMG_B_ENDIAN;
    this->m_pix_bpp     = IT8951_4BPP;

    this->set_target_memory_addr(this->IT8951DevAll[this->model_].devInfo.usImgBufAddrL, this->IT8951DevAll[this->model_].devInfo.usImgBufAddrH);
    this->set_area(x, y, w, h);

    const uint16_t bytewidth = this->usPanelW_ >> 1; // bytes per row (2 pixels per byte)

    this->enable();
    /* Send data preamble */
    this->write_byte16(0x0000);

    ESP_LOGW(TAG, "Transfering %d x %d @ %d,%d", w, h, x, y);

    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t col = 0; col < w; col += 2) {
            // Calculate buffer index for packed 4bpp (2 pixels per byte)
            uint32_t buf_index = (y + row) * bytewidth + ((x + col) >> 1);
            uint8_t data = gram[buf_index];
            if (!this->reversed_) {
                data = 0xFF - data;
            }
            this->transfer_byte(data);
        }
    }

    this->disable();

    this->write_command(IT8951_TCON_LD_IMG_END);
}

void IT8951ESensor::write_display(update_mode_e mode) {
    const bool sleep_when_done = this->sleep_when_done_; // For consistency
    if (sleep_when_done) {
        this->write_command(IT8951_TCON_SYS_RUN);
    }
    this->partial_update_++;
    if (this->full_update_every_ > 0 && this->partial_update_ >= this->full_update_every_) {
        this->partial_update_ = 0;
        mode = update_mode_e::UPDATE_MODE_GC16;
        this->min_x = 0;
        this->min_y = 0;
        this->max_x = this->get_width_internal() - 1;
        this->max_y = this->get_height_internal() - 1;
    }
    else {
        // rounded up to be multiple of 4
        this->min_x = (this->min_x) & 0xFFFC;
        this->min_y = (this->min_y) & 0xFFFC;

    }
    const u_int32_t width = this->max_x - this->min_x + 1;
    const u_int32_t height = this->max_y - this->min_y + 1;
    if (this->min_x > this->get_width_internal() || this->min_y > this->get_height_internal()) {
        ESP_LOGE(TAG, "Pos (%d, %d) out of bounds.", this->min_x, this->min_y);
        return;
    }
    if ((this->min_x + width) > this->get_width_internal() || (this->min_y + height) > this->get_height_internal()) {
        ESP_LOGE(TAG, "Dim (%d, %d) out of bounds.", this->min_x + width, this->min_y + height);
        return;
    }
   // ESP_LOGE(TAG, "write_buffer_to_display (x %d,y %d,w %d,h %d,rot %d).", this->min_x, this->min_y, width, height, this->rotation_);

    this->write_buffer_to_display(this->min_x, this->min_y, width, height, this->buffer_);
    this->update_area(this->min_x, this->min_y, width, height, mode);
    this->max_x = 0;
    this->max_y = 0;
    this->min_x = this->get_width_internal();
    this->min_y = this->get_height_internal();
    if (sleep_when_done) {
        this->write_command(IT8951_TCON_SLEEP);
    }
}

/** @brief Clear graphics buffer
 * @param init Screen initialization, If is 0, clear the buffer without initializing
 */
void IT8951ESensor::clear(bool init) {
    this->m_endian_type = IT8951_LDIMG_L_ENDIAN;
    this->m_pix_bpp     = IT8951_4BPP;

    Display::clear();

    this->set_target_memory_addr(this->IT8951DevAll[this->model_].devInfo.usImgBufAddrL, this->IT8951DevAll[this->model_].devInfo.usImgBufAddrH);
    this->set_area(0, 0, this->get_width_internal(), this->get_height_internal());

    const uint32_t maxPos = (this->get_width_internal() * this->get_height_internal()) >> 1; // 2 pixels per byte

    this->enable();
    /* Send data preamble */
    this->write_byte16(0x0000);

    for (uint32_t i = 0; i < maxPos; ++i) {
        this->transfer_byte(0xFF);
    }

    this->disable();

    this->write_command(IT8951_TCON_LD_IMG_END);

    if (init) {
        this->update_area(0, 0, this->get_width_internal(), this->get_height_internal(), update_mode_e::UPDATE_MODE_INIT);
    }
}

void IT8951ESensor::update() {
    if (this->is_ready()) {
        this->do_update_();
        this->write_display(update_mode_e::UPDATE_MODE_DU); // 2 level
    }
}

void IT8951ESensor::update_slow() {
    if (this->is_ready()) {
        this->do_update_();
        this->write_display(update_mode_e::UPDATE_MODE_GC16);
    }
}

void IT8951ESensor::fill(Color color) {
    for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
        this->buffer_[i] = color.raw_32 & 0x0F;
    }
    this->max_x = this->get_width_internal() - 1;
    this->max_y = this->get_height_internal() - 1;
    this->min_x = 0;
    this->min_y = 0;
}

void HOT IT8951ESensor::draw_pixel_at(int x, int y, Color color) {
    if (!Display::get_clipping().inside(x, y))
    return;  // NOLINT

    switch (this->rotation_) {
    case esphome::display::DisplayRotation::DISPLAY_ROTATION_0_DEGREES:
        break;
    case esphome::display::DisplayRotation::DISPLAY_ROTATION_90_DEGREES:
        std::swap(x, y);
        x = this->usPanelW_ - x - 1;
        break;
    case esphome::display::DisplayRotation::DISPLAY_ROTATION_180_DEGREES:
        x = this->usPanelW_ - x - 1;
        y = this->usPanelH_ - y - 1;
        break;
    case esphome::display::DisplayRotation::DISPLAY_ROTATION_270_DEGREES:
        std::swap(x, y);
        y = this->usPanelH_ - y - 1;
        break;
    }
    this->draw_absolute_pixel_internal(x, y, color);

    // Removed compare to original function to speed up drawing
    // App.feed_wdt();
}

void HOT IT8951ESensor::draw_absolute_pixel_internal(int x, int y, Color color) {
    // Fast path: bounds and buffer check first
    if (x < 0 || y < 0 || this->buffer_ == nullptr || x >= this->usPanelW_ || y >= this->usPanelH_) {
        return;
    }

    // Track min/max for partial updates
    if (x > this->max_x) {
        this->max_x = x;
    }
    if (y > this->max_y) {
        this->max_y = y;
    }
    if (x < this->min_x) {
        this->min_x = x;
    }
    if (y < this->min_y) {
        this->min_y = y;
    }

    uint32_t internal_color = color.raw_32 & 0x0F;
    uint16_t _bytewidth = this->usPanelW_ >> 1;

    uint32_t index = static_cast<uint32_t>(y) * _bytewidth + (static_cast<uint32_t>(x) >> 1);

    uint8_t &buf = this->buffer_[index];
    if (x & 0x1) {
        if ((buf & 0x0F) == internal_color) {
            return;  // No change, skip
        }
        // Odd pixel: lower nibble
        buf = (buf & 0xF0) | internal_color;
    } else {
        if ((buf & 0xF0) == internal_color) {
            return;  // No change, skip
        }
        // Even pixel: upper nibble
        buf = (buf & 0x0F) | (internal_color << 4);
    }
}

void IT8951ESensor::set_model(it8951eModel model) {
    this->model_ = model;
    // Provide fast access to panel width and height
    usPanelW_ = IT8951DevAll[model].devInfo.usPanelW;
    usPanelH_ = IT8951DevAll[model].devInfo.usPanelH;
}

void IT8951ESensor::dump_config() {
    LOG_DISPLAY("", "IT8951E", this);
    switch (this->model_) {
    case it8951eModel::M5EPD:
        ESP_LOGCONFIG(TAG, "  Model: M5EPD");
        break;
    default:
        ESP_LOGCONFIG(TAG, "  Model: unkown");
        break;
    }
    ESP_LOGCONFIG(TAG, "LUT: %s, FW: %s, Mem:%x (%d x %d)",
        this->IT8951DevAll[this->model_].devInfo.usLUTVersion,
        this->IT8951DevAll[this->model_].devInfo.usFWVersion,
        this->IT8951DevAll[this->model_].devInfo.usImgBufAddrL | (this->IT8951DevAll[this->model_].devInfo.usImgBufAddrH << 16),
        this->IT8951DevAll[this->model_].devInfo.usPanelW,
        this->IT8951DevAll[this->model_].devInfo.usPanelH
    );

    ESP_LOGCONFIG(TAG,
        "  Sleep when done: %s\n"
        "  Partial Updating: %s\n"
        "  Full Update Every: %u",
        YESNO(this->sleep_when_done_), YESNO(this->partial_updating_), this->full_update_every_);
}

}  // namespace it8951e
}  // namespace esphome
