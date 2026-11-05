// SPDX-License-Identifier: GPL-3.0-or-later
//
// CAN through the ESP32-S3 TWAI controller and an SN65HVD230 transceiver.
#include "board/board.h"
#include "board/board_pins.h"

#include <cstring>

#include "driver/twai.h"
#include "esp_log.h"

namespace board {
namespace {

const char *TAG = "board_can";

class TwaiBus final : public hal::ICanBus {
public:
    bool start(uint32_t bitrate, hal::CanMode mode) override
    {
        if (running_) {
            stop();
        }
        twai_timing_config_t timing;
        if (bitrate == 500000) {
            timing = TWAI_TIMING_CONFIG_500KBITS();
        } else if (bitrate == 250000) {
            timing = TWAI_TIMING_CONFIG_250KBITS();
        } else {
            ESP_LOGE(TAG, "unsupported bitrate %u", static_cast<unsigned>(bitrate));
            return false;
        }
        const twai_mode_t twai_mode = (mode == hal::CanMode::ListenOnly) ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL;
        twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(static_cast<gpio_num_t>(pins::kCanTx),
                                                              static_cast<gpio_num_t>(pins::kCanRx), twai_mode);
        g.rx_queue_len = 64;
        g.tx_queue_len = 8;
        const twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        if (twai_driver_install(&g, &timing, &f) != ESP_OK) {
            return false;
        }
        if (twai_start() != ESP_OK) {
            twai_driver_uninstall();
            return false;
        }
        running_ = true;
        mode_ = mode;
        bitrate_ = bitrate;
        ESP_LOGI(TAG, "TWAI %u bit/s %s", static_cast<unsigned>(bitrate),
                 mode == hal::CanMode::ListenOnly ? "listen-only" : "normal");
        return true;
    }

    void stop() override
    {
        if (!running_) {
            return;
        }
        twai_stop();
        twai_driver_uninstall();
        running_ = false;
    }

    bool running() const override { return running_; }
    hal::CanMode mode() const override { return mode_; }

    bool receive(core::bus::CanFrame &out, uint32_t timeout_ms) override
    {
        if (!running_) {
            return false;
        }
        twai_message_t m;
        if (twai_receive(&m, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
            return false;
        }
        if (m.rtr) {
            return false;
        }
        out.id = m.identifier;
        out.extended = m.extd;
        out.dlc = m.data_length_code > 8 ? 8 : m.data_length_code;
        std::memcpy(out.data, m.data, out.dlc);
        return true;
    }

    bool transmit(const core::bus::CanFrame &frame, uint32_t timeout_ms) override
    {
        // Defence in depth: never transmit while the bus is not identified.
        if (!running_ || mode_ != hal::CanMode::Normal) {
            ESP_LOGE(TAG, "transmit refused (not in normal mode)");
            return false;
        }
        twai_message_t m;
        std::memset(&m, 0, sizeof(m));
        m.identifier = frame.id;
        m.extd = frame.extended ? 1 : 0;
        m.data_length_code = frame.dlc;
        std::memcpy(m.data, frame.data, frame.dlc);
        return twai_transmit(&m, pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
    }

    hal::CanStatus status() override
    {
        hal::CanStatus s;
        twai_status_info_t info;
        if (running_ && twai_get_status_info(&info) == ESP_OK) {
            s.bus_errors = info.bus_error_count;
            s.rx_missed = info.rx_missed_count;
            s.tx_error_counter = static_cast<uint8_t>(info.tx_error_counter > 255 ? 255 : info.tx_error_counter);
            s.rx_error_counter = static_cast<uint8_t>(info.rx_error_counter > 255 ? 255 : info.rx_error_counter);
            s.bus_off = info.state == TWAI_STATE_BUS_OFF;
        }
        return s;
    }

private:
    bool running_ = false;
    hal::CanMode mode_ = hal::CanMode::ListenOnly;
    uint32_t bitrate_ = 0;
};

TwaiBus s_can;

} // namespace

hal::ICanBus &can()
{
    return s_can;
}

} // namespace board
