#include "esphome/core/log.h"
#include "it8951e.h"
#include "it8951.h"
#include "esphome/core/application.h"
#include "esphome/core/gpio.h"

namespace esphome::it8951e {

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
    this->write_byte16(IT8951_PACKET_TYPE_READ);
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

void IT8951ESensor::write_command(uint16_t cmd) {
    this->write_two_byte16(IT8951_PACKET_TYPE_CMD, cmd);
}

void IT8951ESensor::write_word(uint16_t cmd) {
    this->write_two_byte16(IT8951_PACKET_TYPE_WRITE, cmd);
}

void IT8951ESensor::write_reg(uint16_t addr, uint16_t data) {
    this->write_command(IT8951_TCON_REG_WR);  // tcon write reg command
    this->wait_busy();
    this->enable();
    this->write_byte(IT8951_PACKET_TYPE_WRITE); // Preamble
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
    // Batch all args in a single SPI transaction
    this->wait_busy();
    this->enable();
    this->write_byte16(IT8951_PACKET_TYPE_WRITE);
    this->wait_busy();
    for (uint16_t i = 0; i < length; i++) {
        this->write_byte16(args[i]);
    }
    this->disable();
}

void IT8951ESensor::set_area(uint16_t x, uint16_t y, uint16_t w,
                                  uint16_t h) {

    if (0 == x && 0 == y && w == this->get_width_internal() && h == this->get_height_internal()) {
        // Full screen
        uint16_t args[1];
        args[0] = (this->m_endian_type << 8 | this->m_pix_bpp << 4);
        this->write_args(IT8951_TCON_LD_IMG, args, sizeof(args) / sizeof(args[0]));
    }
    else {
        // Partial update
        uint16_t args[5];
        args[0] = (this->m_endian_type << 8 | this->m_pix_bpp << 4);
        args[1] = x;
        args[2] = y;
        args[3] = w;
        args[4] = h;
        this->write_args(IT8951_TCON_LD_IMG_AREA, args, sizeof(args) / sizeof(args[0]));
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

bool IT8951ESensor::is_display_busy_() {
    this->write_command(IT8951_TCON_REG_RD);
    this->write_word(IT8951_LUTAFSR);
    return this->read_word() != 0;
}

void IT8951ESensor::update_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h, update_mode_e mode) {
    uint16_t args[7];
    args[0] = x;
    args[1] = y;
    args[2] = w;
    args[3] = h;
    args[4] = mode;
    args[5] = this->IT8951DevAll[this->model_].devInfo.usImgBufAddrL;
    args[6] = this->IT8951DevAll[this->model_].devInfo.usImgBufAddrH;

    this->write_args(IT8951_I80_CMD_DPY_BUF_AREA, args, sizeof(args) / sizeof(args[0]));
}

void IT8951ESensor::reset(void) {
    this->reset_pin_->digital_write(true);
    this->reset_pin_->digital_write(false);
    delay(this->reset_duration_);
    this->reset_pin_->digital_write(true);
    delay(100);
}

uint32_t IT8951ESensor::get_buffer_length_() {
    // 4bpp: two pixels per byte
    return (this->get_width_internal() >> 1) * this->get_height_internal();
}

uint16_t IT8951ESensor::get_vcom() {
    this->write_command(IT8951_I80_CMD_VCOM); // tcon vcom get command
    this->write_word(IT8951_I80_CMD_VCOM_READ);
    const uint16_t vcom = this->read_word();
    ESP_LOGI(TAG, "VCOM = %.02fV", (float)vcom/1000);
    return vcom;
}

void IT8951ESensor::set_vcom(uint16_t vcom) {
    this->write_command(IT8951_I80_CMD_VCOM); // tcon vcom set command
    this->write_word(IT8951_I80_CMD_VCOM_WRITE);
    this->write_word(vcom);
}

void IT8951ESensor::setup() {
    ESP_LOGCONFIG(TAG, "Init Starting.");
    this->spi_setup();

    if (nullptr != this->reset_pin_) {
        this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
        this->reset();
    }

    this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

    this->dump_config();

    this->write_command(IT8951_TCON_SYS_RUN);

    // enable pack write
    this->write_reg(IT8951_I80CPCR, 0x0001);

    // set vcom to -2.30v
    const uint16_t vcom = this->get_vcom();
    if (IT8951_DEFAULT_VCOM != vcom) {
        this->set_vcom(IT8951_DEFAULT_VCOM);
        this->get_vcom();
    }

    // Allocate display buffer
    this->init_internal_(this->get_buffer_length_());

    this->initialized_ = true;
    this->state_ = EPaperState::IDLE;

    ESP_LOGCONFIG(TAG, "Init Done.");
}

void IT8951ESensor::loop() {
    const auto now = millis();
    if (static_cast<int32_t>(now - this->delay_until_) < 0)
        return;
    if (this->waiting_for_idle_) {
        if (this->is_idle_()) {
            this->waiting_for_idle_ = false;
        } else {
            return;
        }
    }
    this->process_state_();
}

bool IT8951ESensor::is_idle_() const {
    return this->busy_pin_ == nullptr || this->busy_pin_->digital_read();
}

void IT8951ESensor::set_state_(EPaperState state, uint16_t delay) {
    this->state_ = state;
    this->delay_until_ = millis() + delay;
    this->waiting_for_idle_ = (state > EPaperState::SHOULD_WAIT);
    if (state == EPaperState::IDLE) {
        if (this->update_pending_) {
            this->update_pending_ = false;
            this->pending_mode_ = this->queued_update_mode_;
            this->update_started_at_ = millis();
            this->update_timing_active_ = true;
            this->state_ = EPaperState::UPDATE;
            return;
        }
        this->disable_loop();
    }
}

bool IT8951ESensor::prepare_transfer_(update_mode_e &mode) {
    this->partial_update_++;
    if (this->full_update_every_ > 0 && this->partial_update_ >= this->full_update_every_) {
        this->partial_update_ = 0;
        mode = update_mode_e::UPDATE_MODE_GC16;
        this->min_x = 0;
        this->min_y = 0;
        this->max_x = this->get_width_internal() - 1;
        this->max_y = this->get_height_internal() - 1;
    } else {
        // IT8951 partial write requires x and width to be multiples of 4 pixels.
        this->min_x &= 0xFFFC;
        this->max_x = (this->max_x | 0x0003);
        if (this->max_x >= this->get_width_internal())
            this->max_x = this->get_width_internal() - 1;
    }

    if (this->max_x < this->min_x || this->max_y < this->min_y)
        return false;

    const uint16_t x = static_cast<uint16_t>(this->min_x);
    const uint16_t y = static_cast<uint16_t>(this->min_y);
    const uint16_t width = static_cast<uint16_t>(this->max_x - this->min_x + 1);
    const uint16_t height = static_cast<uint16_t>(this->max_y - this->min_y + 1);

    if (x >= this->get_width_internal() || y >= this->get_height_internal()) {
        ESP_LOGE(TAG, "Pos (%d, %d) out of bounds.", x, y);
        return false;
    }
    if ((x + width) > this->get_width_internal() || (y + height) > this->get_height_internal()) {
        ESP_LOGE(TAG, "Dim (%d, %d) out of bounds.", x + width, y + height);
        return false;
    }

    this->pending_x_ = x;
    this->pending_y_ = y;
    this->pending_w_ = width;
    this->pending_h_ = height;
    this->transfer_row_ = 0;

    this->max_x = 0;
    this->max_y = 0;
    this->min_x = this->get_width_internal();
    this->min_y = this->get_height_internal();

    ESP_LOGD(TAG, "Transfer queued: %d x %d @ %d,%d mode=%d", width, height, x, y, static_cast<int>(mode));
    return true;
}

bool IT8951ESensor::transfer_row_data_() {
    this->m_endian_type = IT8951_LDIMG_B_ENDIAN;
    this->m_pix_bpp = IT8951_4BPP;
    const uint16_t bytewidth = this->usPanelW_ >> 1;
    const uint32_t start_time = millis();

    // LISAR stays constant for the whole transfer — set it once, not per row.
    if (this->transfer_row_ == 0) {
        this->set_target_memory_addr(this->IT8951DevAll[this->model_].devInfo.usImgBufAddrL,
                                     this->IT8951DevAll[this->model_].devInfo.usImgBufAddrH);
    }

    // Batch rows in one area/load transaction per loop slice to reduce command overhead.
    const uint16_t remaining_h = this->pending_h_ - this->transfer_row_;
    this->set_area(this->pending_x_, this->pending_y_ + this->transfer_row_, this->pending_w_, remaining_h);
    this->enable();
    this->write_byte16(IT8951_PACKET_TYPE_WRITE);  // data preamble

    // Full-width fast path: rows are contiguous in buffer, send in large chunks
    const bool full_width = (this->pending_x_ == 0 && this->pending_w_ == this->usPanelW_);
    while (this->transfer_row_ < this->pending_h_) {
        if (full_width) {
            const uint16_t remaining = this->pending_h_ - this->transfer_row_;
            const uint16_t chunk = remaining < 50 ? remaining : 50;
            const uint32_t offset = static_cast<uint32_t>(this->pending_y_ + this->transfer_row_) * bytewidth;
            this->write_array(&this->buffer_[offset], static_cast<uint32_t>(chunk) * bytewidth);
            this->transfer_row_ += chunk;
        } else {
            const uint16_t row_y = this->pending_y_ + this->transfer_row_;
            const uint16_t row_bytes = this->pending_w_ >> 1;
            const uint32_t row_index = static_cast<uint32_t>(row_y) * bytewidth + (this->pending_x_ >> 1);
            this->write_array(&this->buffer_[row_index], row_bytes);
            this->transfer_row_++;
        }
        if (millis() - start_time >= MAX_TRANSFER_TIME) {
            break;
        }
    }

    this->disable();
    this->write_command(IT8951_TCON_LD_IMG_END);

    return this->transfer_row_ >= this->pending_h_;
}

void IT8951ESensor::process_state_() {
    switch (this->state_) {
    case EPaperState::IDLE:
        this->disable_loop();
        break;

    case EPaperState::UPDATE:
        this->draw_calls_since_yield_ = 0;
        this->do_update_();
        if (!this->prepare_transfer_(this->pending_mode_)) {
            this->update_timing_active_ = false;
            this->set_state_(EPaperState::IDLE);
            break;
        }
        this->set_state_(this->sleep_when_done_ ? EPaperState::POWER_ON : EPaperState::TRANSFER_DATA);
        break;

    case EPaperState::POWER_ON:
        this->write_command(IT8951_TCON_SYS_RUN);
        this->set_state_(EPaperState::TRANSFER_DATA);
        break;

    case EPaperState::TRANSFER_DATA:
        if (this->transfer_row_data_()) {
            this->set_state_(EPaperState::REFRESH_SCREEN);
        }
        break;

    case EPaperState::REFRESH_SCREEN:
        if (this->pending_mode_ == update_mode_e::UPDATE_MODE_NONE) {
            this->set_state_(EPaperState::IDLE);
            break;
        }
        if (this->is_display_busy_()) {
            return;  // LUT still busy, retry next loop
        }

        this->update_area(this->pending_x_, this->pending_y_, this->pending_w_, this->pending_h_, this->pending_mode_);

        if (this->update_timing_active_) {
            const uint32_t elapsed = millis() - this->update_started_at_;
            ESP_LOGD(TAG, "Display update took %ums (mode=%d area=%ux%u@%u,%u)",
                     elapsed,
                     static_cast<int>(this->pending_mode_),
                     this->pending_w_,
                     this->pending_h_,
                     this->pending_x_,
                     this->pending_y_);
            this->update_timing_active_ = false;
        }
        this->set_state_(this->sleep_when_done_ ? EPaperState::POWER_OFF : EPaperState::IDLE);
        break;

    case EPaperState::POWER_OFF:
        this->set_state_(EPaperState::DEEP_SLEEP);
        break;

    case EPaperState::DEEP_SLEEP:
        this->write_command(IT8951_TCON_SLEEP);
        this->set_state_(EPaperState::IDLE);
        break;

    default:
        ESP_LOGE(TAG, "Display in unhandled state %d", static_cast<int>(this->state_));
        this->set_state_(EPaperState::IDLE);
        break;
    }
}

void IT8951ESensor::write_display(update_mode_e mode) {
    if (!this->initialized_ || this->state_ != EPaperState::IDLE)
        return;
    this->update_started_at_ = millis();
    this->update_timing_active_ = true;
    this->pending_mode_ = mode;
    if (!this->prepare_transfer_(this->pending_mode_)) {
        this->update_timing_active_ = false;
        return;
    }
    this->enable_loop();
    this->set_state_(this->sleep_when_done_ ? EPaperState::POWER_ON : EPaperState::TRANSFER_DATA);
}

/** @brief Clear graphics buffer
 * @param init Screen initialization, If is 0, clear the buffer without initializing
 */
void IT8951ESensor::clear(bool init) {
    Display::clear();

    if (!init || !this->initialized_)
        return;

    const bool use_init = !this->did_init_clear_ || (this->clear_count_ % INIT_CLEAR_EVERY == 0);
    const update_mode_e clear_mode = use_init ? update_mode_e::UPDATE_MODE_INIT
                                              : update_mode_e::UPDATE_MODE_GC16;
    this->did_init_clear_ = true;
    this->clear_count_++;
    this->write_display(clear_mode);
}

void IT8951ESensor::update() {
    if (!this->is_ready() || !this->initialized_)
        return;
    if (this->state_ == EPaperState::IDLE) {
        this->update_started_at_ = millis();
        this->update_timing_active_ = true;
        this->pending_mode_ = update_mode_e::UPDATE_MODE_DU;
        this->enable_loop();
        this->set_state_(EPaperState::UPDATE);
    } else {
        this->update_pending_ = true;
        this->queued_update_mode_ = update_mode_e::UPDATE_MODE_DU;
    }
}

void IT8951ESensor::update_slow() {
    if (!this->is_ready() || !this->initialized_)
        return;
    if (this->state_ == EPaperState::IDLE) {
        this->update_started_at_ = millis();
        this->update_timing_active_ = true;
        this->pending_mode_ = update_mode_e::UPDATE_MODE_GC16;
        this->enable_loop();
        this->set_state_(EPaperState::UPDATE);
    } else {
        this->update_pending_ = true;
        this->queued_update_mode_ = update_mode_e::UPDATE_MODE_GC16;
    }
}

uint8_t IT8951ESensor::color_to_nibble_(const Color &color) const {
    // Fast path for the two most common colors (avoids all arithmetic)
    if (color.raw_32 == 0)          return 0x00;  // COLOR_OFF → white
    if (color.raw_32 == 0xFFFFFFFF) return 0x0F;  // COLOR_ON  → black

    // Backward-compatible direct nibble path used by configs that set raw_32 to 0x0-0xF.
    if (color.g == 0 && color.b == 0 && color.w == 0 && color.r <= 0x0F) {
        return color.r;
    }

    // Convert to 4bpp grayscale from 8-bit channels.
    uint16_t gray = static_cast<uint16_t>(color.r) + color.g + color.b;
    gray /= 3;
    if (color.w > gray) {
        gray = color.w;
    }
    uint8_t nibble = static_cast<uint8_t>((gray + 8) >> 4);
    if (nibble > 0x0F) {
        nibble = 0x0F;
    }
    return nibble;
}

void IT8951ESensor::fill(Color color) {
    uint8_t packed_color = this->color_to_nibble_(color);
    if (!this->reversed_) {
        packed_color = 0x0F - packed_color;
    }
    const uint8_t fill_byte = static_cast<uint8_t>((packed_color << 4) | packed_color);
    memset(this->buffer_, fill_byte, this->get_buffer_length_());
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

    uint8_t internal_color = this->color_to_nibble_(color) & 0x0F;
    if (!this->reversed_) {
        internal_color = 0x0F - internal_color;
    }
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
        if ((buf & 0xF0) == (internal_color << 4)) {
            return;  // No change, skip
        }
        // Even pixel: upper nibble
        buf = (buf & 0x0F) | (internal_color << 4);
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
        ESP_LOGCONFIG(TAG, "  Model: unknown");
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
        "  Full Update Every: %u\n"
        "  Reversed colors: %s\n"
        "  Reset duration: %ums",
        YESNO(this->sleep_when_done_),
        YESNO(this->full_update_every_ > 0),
        this->full_update_every_,
        YESNO(this->reversed_),
        this->reset_duration_);
}

}  // namespace esphome::it8951e
