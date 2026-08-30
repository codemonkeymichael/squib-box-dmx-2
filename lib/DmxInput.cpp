/*
 * Copyright (c) 2021 Jostein Lower
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "DmxInput.h"
#include "DmxInput.pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pico/time.h"

namespace {
constexpr uint kDmxStateMachineFrequency = 2000000;
constexpr int kDmaChannelCount = 12;
bool program_loaded[] = {false, false};
uint program_offsets[] = {0, 0};
volatile DmxInput* active_inputs[kDmaChannelCount] = {};

void start_transfer(volatile DmxInput* instance) {
    dma_channel_set_write_addr(instance->_dma_chan, instance->_buf, true);
    pio_sm_exec(instance->_pio, instance->_sm,
                pio_encode_jmp(program_offsets[pio_get_index(instance->_pio)]));
    pio_sm_clear_fifos(instance->_pio, instance->_sm);
}

void pio_irq_handler() {
    for (int channel = 0; channel < kDmaChannelCount; ++channel) {
        volatile DmxInput* instance = active_inputs[channel];
        if (instance == nullptr || !pio_interrupt_get(instance->_pio, 1)) {
            continue;
        }

        instance->_last_packet_length =
            (instance->_num_channels + 1) - dma_hw->ch[channel].transfer_count;
        dma_channel_abort(channel);
        pio_interrupt_clear(instance->_pio, 1);
        break;
    }
}

void dma_irq_handler() {
    for (int channel = 0; channel < kDmaChannelCount; ++channel) {
        if (active_inputs[channel] == nullptr || !(dma_hw->ints0 & (1u << channel))) {
            continue;
        }

        dma_hw->ints0 = 1u << channel;
        volatile DmxInput* instance = active_inputs[channel];
        start_transfer(instance);
        instance->_last_packet_timestamp = to_ms_since_boot(get_absolute_time());
        if (instance->_cb != nullptr) {
            instance->_cb(const_cast<DmxInput*>(instance));
        }
        instance->_last_packet_length = instance->_num_channels + 1;
    }
}
}  // namespace

DmxInput::return_code DmxInput::begin(uint pin, uint num_channels, PIO pio) {
    const uint pio_index = pio_get_index(pio);
    if (!program_loaded[pio_index]) {
        if (!pio_can_add_program(pio, &DmxInput_program)) {
            return ERR_INSUFFICIENT_PRGM_MEM;
        }
        program_offsets[pio_index] = pio_add_program(pio, &DmxInput_program);
        program_loaded[pio_index] = true;
    }

    const int state_machine = pio_claim_unused_sm(pio, false);
    if (state_machine < 0) {
        return ERR_NO_SM_AVAILABLE;
    }

    pio_sm_set_consecutive_pindirs(pio, state_machine, pin, 1, false);
    pio_gpio_init(pio, pin);
    gpio_pull_up(pin);

    pio_sm_config config = DmxInput_program_get_default_config(program_offsets[pio_index]);
    sm_config_set_in_pins(&config, pin);
    sm_config_set_jmp_pin(&config, pin);
    sm_config_set_in_shift(&config, true, false, 8);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / kDmxStateMachineFrequency);
    pio_sm_init(pio, state_machine, program_offsets[pio_index], &config);

    const uint interrupt = pio_index == 0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
    irq_set_exclusive_handler(interrupt, pio_irq_handler);
    irq_set_enabled(interrupt, true);
    if (pio_index == 0) {
        pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS;
    } else {
        pio1_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS;
    }

    _pio = pio;
    _sm = static_cast<uint>(state_machine);
    _pin = pin;
    _num_channels = static_cast<int32_t>(num_channels);
    _buf = nullptr;
    _cb = nullptr;
    _last_packet_length = _num_channels + 1;
    _dma_chan = dma_claim_unused_channel(true);
    if (_dma_chan >= kDmaChannelCount || active_inputs[_dma_chan] != nullptr) {
        return ERR_NO_SM_AVAILABLE;
    }
    active_inputs[_dma_chan] = this;
    return SUCCESS;
}

void DmxInput::read(volatile uint8_t* buffer) {
    if (_buf == nullptr) {
        read_async(buffer);
    }
    const unsigned long start = _last_packet_timestamp;
    while (_last_packet_timestamp == start) {
        tight_loop_contents();
    }
}

void DmxInput::read_async(volatile uint8_t* buffer, void (*callback)(DmxInput*)) {
    _buf = buffer;
    _cb = callback;
    pio_sm_set_enabled(_pio, _sm, false);
    pio_sm_restart(_pio, _sm);

    dma_channel_config config = dma_channel_get_default_config(_dma_chan);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, pio_get_dreq(_pio, _sm, false));
    dma_channel_configure(_dma_chan, &config, nullptr, &_pio->rxf[_sm],
                          DMXINPUT_BUFFER_SIZE(_num_channels), false);
    dma_channel_set_irq0_enabled(_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    start_transfer(this);
    _last_packet_timestamp = to_ms_since_boot(get_absolute_time());
    pio_sm_set_enabled(_pio, _sm, true);
}

unsigned long DmxInput::latest_packet_timestamp() { return _last_packet_timestamp; }
uint32_t DmxInput::latest_packet_length() { return _last_packet_length; }
uint DmxInput::pin() { return _pin; }

void DmxInput::end() {
    pio_sm_set_enabled(_pio, _sm, false);
    pio_sm_unclaim(_pio, _sm);
    dma_channel_unclaim(_dma_chan);
    active_inputs[_dma_chan] = nullptr;
    _buf = nullptr;
}
