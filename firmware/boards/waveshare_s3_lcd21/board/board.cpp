// SPDX-License-Identifier: GPL-3.0-or-later
//
// Board bring-up for the Waveshare ESP32-S3-Touch-LCD-2.1:
// I2C bus, TCA9554 expander, ST7701 RGB panel, CST820 touch, backlight, buzzer, microSD.
#include "board/board.h"

#include <cstring>

#include "board/board_pins.h"
#include "board/st7701_init.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/sdmmc_host.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"

namespace board {
namespace {

const char *TAG = "board";

constexpr uint32_t kI2cTimeoutMs = 50;

i2c_master_bus_handle_t s_i2c_bus = nullptr;

// ---------------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------------
bool i2c_add(uint8_t addr, i2c_master_dev_handle_t *out)
{
    i2c_device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = addr;
    cfg.scl_speed_hz = pins::kI2cHz;
    return i2c_master_bus_add_device(s_i2c_bus, &cfg, out) == ESP_OK;
}

bool i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    const uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), kI2cTimeoutMs) == ESP_OK;
}

bool i2c_read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, out, len, kI2cTimeoutMs) == ESP_OK;
}

// ---------------------------------------------------------------------------
// TCA9554 IO expander
// ---------------------------------------------------------------------------
class Expander {
public:
    bool init()
    {
        if (!i2c_add(pins::kAddrExpanderTca9554, &dev_)) {
            return false;
        }
        mutex_ = xSemaphoreCreateMutex();
        // Outputs high = idle (resets released, chip selects deasserted), buzzer off.
        out_ = (1u << pins::kExioLcdReset) | (1u << pins::kExioTouchReset) | (1u << pins::kExioLcdCs) |
               (1u << pins::kExioSdCs);
        const uint8_t inputs = (1u << pins::kExioImuInt1) | (1u << pins::kExioImuInt2) | (1u << pins::kExioRtcInt);
        return i2c_write_reg(dev_, kRegOutput, out_) && i2c_write_reg(dev_, kRegConfig, inputs);
    }

    bool set(uint8_t bit, bool level)
    {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const uint8_t next = level ? (out_ | (1u << bit)) : (out_ & ~(1u << bit));
        const bool ok = i2c_write_reg(dev_, kRegOutput, next);
        if (ok) {
            out_ = next;
        }
        xSemaphoreGive(mutex_);
        return ok;
    }

private:
    static constexpr uint8_t kRegOutput = 0x01;
    static constexpr uint8_t kRegConfig = 0x03;
    i2c_master_dev_handle_t dev_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    uint8_t out_ = 0;
};

Expander s_expander;

// ---------------------------------------------------------------------------
// ST7701 init over 3-wire SPI (9-bit words: D/C bit + 8 data bits)
// ---------------------------------------------------------------------------
bool st7701_send_init_sequence()
{
    spi_bus_config_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.mosi_io_num = pins::kLcdSpiSda;
    bus.miso_io_num = -1;
    bus.sclk_io_num = pins::kLcdSpiScl;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 64;
    ESP_RETURN_ON_FALSE(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED) == ESP_OK, false, TAG, "spi bus");

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.command_bits = 1; // D/C bit
    devcfg.address_bits = 8; // payload byte
    devcfg.mode = 0;
    devcfg.clock_speed_hz = 4 * 1000 * 1000;
    devcfg.spics_io_num = -1; // CS is on the IO expander
    devcfg.queue_size = 1;
    spi_device_handle_t spi = nullptr;
    if (spi_bus_add_device(SPI2_HOST, &devcfg, &spi) != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        return false;
    }

    auto write9 = [spi](bool is_data, uint8_t byte) {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.cmd = is_data ? 1 : 0;
        t.addr = byte;
        return spi_device_polling_transmit(spi, &t) == ESP_OK;
    };

    bool ok = s_expander.set(pins::kExioLcdCs, false);
    for (size_t i = 0; ok && i < kSt7701InitLen; ++i) {
        const St7701Cmd &c = kSt7701Init[i];
        ok = write9(false, c.cmd);
        for (uint8_t n = 0; ok && n < c.len; ++n) {
            ok = write9(true, c.data[n]);
        }
        if (c.delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(c.delay_ms));
        }
    }
    s_expander.set(pins::kExioLcdCs, true);

    // Release GPIO1/2: they are reused by the SDMMC host afterwards.
    spi_bus_remove_device(spi);
    spi_bus_free(SPI2_HOST);
    return ok;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
class Display final : public hal::IDisplay {
public:
    bool init()
    {
        // Hardware reset through the expander.
        s_expander.set(pins::kExioLcdReset, false);
        vTaskDelay(pdMS_TO_TICKS(10));
        s_expander.set(pins::kExioLcdReset, true);
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_RETURN_ON_FALSE(st7701_send_init_sequence(), false, TAG, "st7701 init");

        esp_lcd_rgb_panel_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.clk_src = LCD_CLK_SRC_DEFAULT;
        cfg.timings.pclk_hz = pins::kLcdPclkHz;
        cfg.timings.h_res = pins::kLcdWidth;
        cfg.timings.v_res = pins::kLcdHeight;
        cfg.timings.hsync_pulse_width = 8;
        cfg.timings.hsync_back_porch = 10;
        cfg.timings.hsync_front_porch = 50;
        cfg.timings.vsync_pulse_width = 3;
        cfg.timings.vsync_back_porch = 8;
        cfg.timings.vsync_front_porch = 8;
        cfg.data_width = 16;
        cfg.bits_per_pixel = 16;
        cfg.num_fbs = 2;
        cfg.dma_burst_size = 64;
        cfg.hsync_gpio_num = pins::kLcdHsync;
        cfg.vsync_gpio_num = pins::kLcdVsync;
        cfg.de_gpio_num = pins::kLcdDe;
        cfg.pclk_gpio_num = pins::kLcdPclk;
        cfg.disp_gpio_num = -1;
        for (int i = 0; i < 16; ++i) {
            cfg.data_gpio_nums[i] = pins::kLcdData[i];
        }
        cfg.flags.fb_in_psram = 1;
        ESP_RETURN_ON_FALSE(esp_lcd_new_rgb_panel(&cfg, &panel_) == ESP_OK, false, TAG, "rgb panel");
        ESP_RETURN_ON_FALSE(esp_lcd_panel_reset(panel_) == ESP_OK, false, TAG, "panel reset");
        ESP_RETURN_ON_FALSE(esp_lcd_panel_init(panel_) == ESP_OK, false, TAG, "panel init");

        return init_backlight();
    }

    hal::DisplayInfo info() const override
    {
        return hal::DisplayInfo{pins::kLcdWidth, pins::kLcdHeight, hal::DisplayShape::Round};
    }

    bool set_backlight(uint8_t percent) override
    {
        if (percent > 100) {
            percent = 100;
        }
        const uint32_t duty = (kBlMaxDuty * percent) / 100;
        return ledc_set_duty(LEDC_LOW_SPEED_MODE, kBlChannel, duty) == ESP_OK &&
               ledc_update_duty(LEDC_LOW_SPEED_MODE, kBlChannel) == ESP_OK;
    }

    esp_lcd_panel_handle_t panel() const { return panel_; }

private:
    static constexpr ledc_channel_t kBlChannel = LEDC_CHANNEL_0;
    static constexpr ledc_timer_t kBlTimer = LEDC_TIMER_0;
    static constexpr uint32_t kBlMaxDuty = (1u << 13) - 1;

    bool init_backlight()
    {
        ledc_timer_config_t t;
        memset(&t, 0, sizeof(t));
        t.speed_mode = LEDC_LOW_SPEED_MODE;
        t.duty_resolution = LEDC_TIMER_13_BIT;
        t.timer_num = kBlTimer;
        t.freq_hz = 5000;
        t.clk_cfg = LEDC_AUTO_CLK;
        ESP_RETURN_ON_FALSE(ledc_timer_config(&t) == ESP_OK, false, TAG, "ledc timer");

        ledc_channel_config_t c;
        memset(&c, 0, sizeof(c));
        c.gpio_num = pins::kLcdBacklight;
        c.speed_mode = LEDC_LOW_SPEED_MODE;
        c.channel = kBlChannel;
        c.timer_sel = kBlTimer;
        c.duty = 0;
        ESP_RETURN_ON_FALSE(ledc_channel_config(&c) == ESP_OK, false, TAG, "ledc channel");
        return true;
    }

    esp_lcd_panel_handle_t panel_ = nullptr;
};

Display s_display;

// ---------------------------------------------------------------------------
// CST820 touch
// ---------------------------------------------------------------------------
class Touch final : public hal::ITouch {
public:
    bool init()
    {
        s_expander.set(pins::kExioTouchReset, false);
        vTaskDelay(pdMS_TO_TICKS(10));
        s_expander.set(pins::kExioTouchReset, true);
        vTaskDelay(pdMS_TO_TICKS(50));
        return i2c_add(pins::kAddrTouchCst820, &dev_);
    }

    bool read(hal::TouchPoint &out) override
    {
        out.pressed = false;
        uint8_t buf[5];
        if (!i2c_read_regs(dev_, kRegFingerCount, buf, sizeof(buf))) {
            return false;
        }
        if ((buf[0] & 0x0F) == 0) {
            return true;
        }
        out.pressed = true;
        out.x = static_cast<uint16_t>(((buf[1] & 0x0F) << 8) | buf[2]);
        out.y = static_cast<uint16_t>(((buf[3] & 0x0F) << 8) | buf[4]);
        return true;
    }

private:
    static constexpr uint8_t kRegFingerCount = 0x02; // followed by XH, XL, YH, YL
    i2c_master_dev_handle_t dev_ = nullptr;
};

Touch s_touch;

// ---------------------------------------------------------------------------
// Buzzer (active buzzer switched through the expander)
// ---------------------------------------------------------------------------
class Buzzer final : public hal::IBuzzer {
public:
    bool set(bool on) override { return s_expander.set(pins::kExioBuzzer, on); }
};

Buzzer s_buzzer;

// ---------------------------------------------------------------------------
// microSD (SDMMC 1-bit)
// ---------------------------------------------------------------------------
class SdCard final : public hal::IRemovableStorage {
public:
    bool mount() override
    {
        if (card_ != nullptr) {
            return true;
        }
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.clk = static_cast<gpio_num_t>(pins::kSdClk);
        slot.cmd = static_cast<gpio_num_t>(pins::kSdCmd);
        slot.d0 = static_cast<gpio_num_t>(pins::kSdD0);
        slot.d1 = GPIO_NUM_NC;
        slot.d2 = GPIO_NUM_NC;
        slot.d3 = GPIO_NUM_NC;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        esp_vfs_fat_sdmmc_mount_config_t mount_cfg;
        memset(&mount_cfg, 0, sizeof(mount_cfg));
        mount_cfg.format_if_mount_failed = false; // never wipe the user's card
        mount_cfg.max_files = 4;
        mount_cfg.allocation_unit_size = 16 * 1024;

        if (esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot, &mount_cfg, &card_) != ESP_OK) {
            card_ = nullptr;
            return false;
        }
        return true;
    }

    bool is_mounted() const override { return card_ != nullptr; }

    uint32_t capacity_mib() const override
    {
        if (card_ == nullptr) {
            return 0;
        }
        return static_cast<uint32_t>((static_cast<uint64_t>(card_->csd.capacity) * card_->csd.sector_size) >> 20);
    }

    const char *mount_point() const override { return kMountPoint; }

private:
    static constexpr const char *kMountPoint = "/sdcard";
    sdmmc_card_t *card_ = nullptr;
};

SdCard s_sdcard;

} // namespace

// Accessors used by the LVGL glue (board_lvgl.cpp).
esp_lcd_panel_handle_t internal_panel()
{
    return s_display.panel();
}

bool init()
{
    i2c_master_bus_config_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.i2c_port = I2C_NUM_0;
    bus.sda_io_num = static_cast<gpio_num_t>(pins::kI2cSda);
    bus.scl_io_num = static_cast<gpio_num_t>(pins::kI2cScl);
    bus.clk_source = I2C_CLK_SRC_DEFAULT;
    bus.glitch_ignore_cnt = 7;
    bus.flags.enable_internal_pullup = 1;
    ESP_RETURN_ON_FALSE(i2c_new_master_bus(&bus, &s_i2c_bus) == ESP_OK, false, TAG, "i2c bus");

    ESP_RETURN_ON_FALSE(s_expander.init(), false, TAG, "tca9554");
    ESP_RETURN_ON_FALSE(s_display.init(), false, TAG, "display");
    if (!s_touch.init()) {
        ESP_LOGW(TAG, "touch init failed, continuing without touch");
    }
    s_buzzer.set(false);
    ESP_LOGI(TAG, "board ready");
    return true;
}

hal::IDisplay &display()
{
    return s_display;
}

hal::ITouch &touch()
{
    return s_touch;
}

hal::IBuzzer &buzzer()
{
    return s_buzzer;
}

hal::IRemovableStorage &sdcard()
{
    return s_sdcard;
}

} // namespace board
