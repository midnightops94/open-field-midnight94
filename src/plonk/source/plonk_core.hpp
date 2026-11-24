#pragma once

namespace plonk {
using u32 = unsigned;
using i32 = int;
using u16 = unsigned short;
constexpr unsigned rate = 88200;
constexpr unsigned frames = 128;
constexpr i32 unity = 32768;
extern const short sine[257];

struct Controls { i32 fold, ratio, strike, chance; };
struct Voice {
    u32 phase, mod_phase, age, attack_phase, release_phase;
    u32 attack_step;
    i32 filter, fold_variation, time_variation, brightness_variation;
    i32 mod_ratio, mod_variation, gain_variation, velocity;
    unsigned wave;
    i32 cached_ratio_control;
    u32 cached_ratio, cached_decay_samples, cached_decay_step;
    bool active, stock_velocity;
};

unsigned wave_mode(unsigned raw);
Controls controls(const i32* raw);
void start(Voice& voice, Controls params, unsigned wave, unsigned velocity, u32& random_counter,
           bool stock_velocity = false);
void render(Voice& voice, Controls params, u32 step, i32* output, unsigned count, bool released);
}

extern "C" unsigned plonk_ratio_q16(unsigned q15_control);
