#pragma once

namespace cube_field {

// The late room runs at 2,756.25 Hz. The four incommensurate delay lengths
// span 8.3 to 15.6 ms and retain independent stereo histories.
constexpr unsigned delay_lengths[] = {23, 29, 37, 43};
constexpr unsigned delay_offsets[] = {0, 23, 52, 89};
constexpr unsigned delay_samples = 132;
constexpr unsigned room_divisor = 16;
constexpr unsigned room_start = 21299; // About 65% Reso.
constexpr unsigned room_owner_count = 17;

struct ResonanceRoom {
    int16_t history[2][delay_samples];
    int16_t damped[2][4];
    int32_t send[2];
    int32_t dc[2];
    int32_t prior[2];
    int32_t next[2];
    uint16_t level;
    uint8_t cursor[4];
    uint8_t phase;
    uint8_t driven;
};

static_assert(sizeof(ResonanceRoom) == 584, "Cube room memory lease");

static inline unsigned room_level(unsigned resonance) {
    return resonance > room_start ? (resonance - room_start) * 2 : 0;
}

static inline void clear_room(ResonanceRoom& room) {
    for (auto& channel : room.history) {
        volatile int16_t* delay = channel;
        for (unsigned i = 0; i < delay_samples; ++i) delay[i] = 0;
    }
    for (unsigned channel = 0; channel < 2; ++channel) {
        for (unsigned lane = 0; lane < 4; ++lane) room.damped[channel][lane] = 0;
        room.send[channel] = 0;
        room.dc[channel] = 0;
        room.prior[channel] = 0;
        room.next[channel] = 0;
    }
    for (unsigned lane = 0; lane < 4; ++lane) room.cursor[lane] = 0;
    room.phase = 0;
    room.driven = 0;
}

static inline void advance_room(ResonanceRoom& room, unsigned target, unsigned morph) {
    const int previous = room.level;
    const int difference = int(target) - previous;
    const unsigned level = unsigned(previous + ((difference + (difference > 0 ? 7 : 0)) >> 3));
    room.level = uint16_t(level);
    if (level == 0) {
        clear_room(room);
        return;
    }
    const unsigned damping = 148 + (morph >> 10);
    const uint16_t feedback = uint16_t(28160 + (level >> 4));
    for (unsigned channel = 0; channel < 2; ++channel) {
        auto& history = room.history[channel];
        auto& damped = room.damped[channel];
        const int32_t average = room.send[channel] >> 11;
        // Keep the high-Q early section's fixed-point DC floor out of the room.
        int32_t& dc = room.dc[channel];
        dc += (average * 256 - dc) >> 4;
        const int32_t transient = average - ((dc + 128) >> 8);
        const bool driven = room.driven & (1u << channel);
        const int32_t send = !driven || abs_word(transient) < 6 ? 0 : clamp_sym(transient, 22528);
        room.send[channel] = 0;
        int32_t total = 0;
        for (unsigned lane = 0; lane < 4; ++lane) {
            const unsigned at = delay_offsets[lane] + room.cursor[lane];
            const int32_t prior = damped[lane];
            const int32_t change = (int32_t(history[at]) - prior) * int(damping);
            const int32_t step = change < 0 ? -((-change + 255) >> 8) : (change + 255) >> 8;
            damped[lane] = int16_t(prior + step);
            total += damped[lane];
        }
        for (unsigned lane = 0; lane < 4; ++lane) {
            const unsigned at = delay_offsets[lane] + room.cursor[lane];
            const int32_t signal = (total >> 1) - damped[lane];
            const int32_t magnitude = int32_t((abs_word(signal) * feedback) >> 15);
            const int32_t reflected = magnitude < 4 ? 0 : signal < 0 ? -magnitude : magnitude;
            const int32_t input = lane & 1 ? -(send >> 1) : send >> 1;
            history[at] = int16_t(clamp_sym(reflected + input, 32760));
        }
        const int32_t stagger = channel == 0 ?
            damped[0] - damped[1] + damped[2] - damped[3] :
            damped[0] + damped[1] - damped[2] - damped[3];
        room.prior[channel] = room.next[channel];
        room.next[channel] = gain_q15(stagger >> 1, uint16_t(level)) * 128;
    }
    room.driven = 0;
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (++room.cursor[lane] == delay_lengths[lane]) room.cursor[lane] = 0;
    }
}

static inline int32_t room_return(const ResonanceRoom& room, unsigned channel) {
    return room.prior[channel] + (((room.next[channel] - room.prior[channel]) * room.phase) >> 4);
}

}
