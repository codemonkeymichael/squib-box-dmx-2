/*
 * Copyright (c) 2021 Jostein Lower
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef DMX_INPUT_H
#define DMX_INPUT_H

#include "hardware/dma.h"
#include "hardware/pio.h"

#define DMXINPUT_BUFFER_SIZE(num_channels) ((num_channels) + 1)

class DmxInput {
public:
    enum return_code {
        SUCCESS = 0,
        ERR_NO_SM_AVAILABLE = -1,
        ERR_INSUFFICIENT_PRGM_MEM = -2,
        ERR_UNKNOWN = -100,
    };

    int32_t _num_channels;
    volatile uint8_t* _buf;
    volatile PIO _pio;
    volatile uint _sm;
    volatile uint _dma_chan;
    volatile unsigned long _last_packet_timestamp = 0;
    volatile uint32_t _last_packet_length = 0;
    void (*_cb)(DmxInput*);

    return_code begin(uint pin, uint num_channels, PIO pio = pio0);
    void read(volatile uint8_t* buffer);
    void read_async(volatile uint8_t* buffer, void (*callback)(DmxInput*) = nullptr);
    unsigned long latest_packet_timestamp();
    uint32_t latest_packet_length();
    uint pin();
    void end();

private:
    uint _pin;
};

#endif
