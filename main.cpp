#include <array>
#include <cstdint>

#include "DmxInput.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

namespace {
constexpr uint kDmxPin = 5;
constexpr uint kDmxLedPin = 0;
constexpr uint kPowerLedPin = 18;
constexpr uint16_t kLedDuty = 5000;
constexpr uint32_t kDmxActiveTimeoutMs = 1000;
constexpr uint32_t kDmxBlinkPeriodMs = 100;

constexpr uint8_t kOnThreshold = 110;
constexpr uint8_t kOffThreshold = 90;
constexpr uint8_t kConfirmFrames = 2;
constexpr uint32_t kPulseDurationMs = 150;
constexpr size_t kChannelCount = 12;

constexpr std::array<uint, kChannelCount> kOutputPins = {
    1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13,
};

DmxInput dmx_input;
volatile uint8_t dmx_input_buffer[DMXINPUT_BUFFER_SIZE(kChannelCount)] = {};
volatile uint8_t latest_channels[kChannelCount] = {};
volatile uint32_t received_frame_sequence = 0;

std::array<bool, kChannelCount> channel_state = {};
std::array<uint8_t, kChannelCount> above_confirm = {};
std::array<uint8_t, kChannelCount> below_confirm = {};

std::array<uint8_t, kChannelCount> pulse_queue = {};
size_t pulse_queue_head = 0;
size_t pulse_queue_tail = 0;
size_t pulse_queue_size = 0;
int active_pulse_channel = -1;
uint32_t active_pulse_end_ms = 0;

void on_dmx_frame(DmxInput* instance) {
    if (dmx_input_buffer[0] != 0 || instance->latest_packet_length() < kChannelCount + 1) {
        return;
    }

    for (size_t channel = 0; channel < kChannelCount; ++channel) {
        latest_channels[channel] = dmx_input_buffer[channel + 1];
    }
    ++received_frame_sequence;
}

void configure_pwm_led(uint pin, uint16_t duty) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, UINT16_MAX);
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(pin, duty);
}

void set_dmx_led(bool on) {
    pwm_set_gpio_level(kDmxLedPin, on ? kLedDuty : 0);
}

void set_all_outputs_low() {
    for (uint pin : kOutputPins) {
        gpio_put(pin, false);
    }
}

void enqueue_pulse(uint8_t channel) {
    if (pulse_queue_size == pulse_queue.size()) {
        return;
    }

    pulse_queue[pulse_queue_tail] = channel;
    pulse_queue_tail = (pulse_queue_tail + 1) % pulse_queue.size();
    ++pulse_queue_size;
}

void service_pulse_queue(uint32_t now_ms) {
    if (active_pulse_channel >= 0 && static_cast<int32_t>(now_ms - active_pulse_end_ms) >= 0) {
        gpio_put(kOutputPins[active_pulse_channel], false);
        active_pulse_channel = -1;
    }

    if (active_pulse_channel < 0 && pulse_queue_size > 0) {
        const uint8_t channel = pulse_queue[pulse_queue_head];
        pulse_queue_head = (pulse_queue_head + 1) % pulse_queue.size();
        --pulse_queue_size;

        gpio_put(kOutputPins[channel], true);
        active_pulse_channel = channel;
        active_pulse_end_ms = now_ms + kPulseDurationMs;
    }
}

void process_dmx_frame(const std::array<uint8_t, kChannelCount>& channels) {
    for (size_t channel = 0; channel < kChannelCount; ++channel) {
        const uint8_t value = channels[channel];

        if (value >= kOnThreshold) {
            if (above_confirm[channel] < kConfirmFrames) {
                ++above_confirm[channel];
            }
            below_confirm[channel] = 0;
        } else if (value <= kOffThreshold) {
            if (below_confirm[channel] < kConfirmFrames) {
                ++below_confirm[channel];
            }
            above_confirm[channel] = 0;
        }

        if (!channel_state[channel] && above_confirm[channel] >= kConfirmFrames) {
            if (channel == kChannelCount - 1) {
                gpio_put(kOutputPins[channel], true);
            } else {
                enqueue_pulse(static_cast<uint8_t>(channel));
            }
            channel_state[channel] = true;
            above_confirm[channel] = 0;
        } else if (channel_state[channel] && below_confirm[channel] >= kConfirmFrames) {
            if (channel == kChannelCount - 1) {
                gpio_put(kOutputPins[channel], false);
            }
            channel_state[channel] = false;
            below_confirm[channel] = 0;
        }
    }
}
}  // namespace

int main() {
    for (uint pin : kOutputPins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    set_all_outputs_low();

    configure_pwm_led(kDmxLedPin, 0);
    configure_pwm_led(kPowerLedPin, kLedDuty);

    if (dmx_input.begin(kDmxPin, kChannelCount) != DmxInput::SUCCESS) {
        while (true) {
            set_all_outputs_low();
            set_dmx_led(false);
            sleep_ms(100);
        }
    }
    dmx_input.read_async(dmx_input_buffer, on_dmx_frame);

    uint32_t processed_frame_sequence = 0;
    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        service_pulse_queue(now_ms);

        const uint32_t latest_frame_ms = dmx_input.latest_packet_timestamp();
        const bool dmx_active = now_ms - latest_frame_ms < kDmxActiveTimeoutMs;
        set_dmx_led(dmx_active && ((now_ms / kDmxBlinkPeriodMs) % 2 == 0));

        const uint32_t sequence = received_frame_sequence;
        if (sequence != processed_frame_sequence) {
            std::array<uint8_t, kChannelCount> channels = {};
            const uint32_t interrupt_state = save_and_disable_interrupts();
            for (size_t channel = 0; channel < kChannelCount; ++channel) {
                channels[channel] = latest_channels[channel];
            }
            processed_frame_sequence = received_frame_sequence;
            restore_interrupts(interrupt_state);
            process_dmx_frame(channels);
        }

        tight_loop_contents();
    }
}
