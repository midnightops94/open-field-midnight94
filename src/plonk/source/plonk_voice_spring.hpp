#pragma once
#include "plonk_spring.hpp"
namespace plonk_spring {
// The reviewed tank runs at 88.2 kHz. At 11.025 kHz the delay lengths and
// pole time constants below preserve their time in seconds. Keep both parallel
// springs, the original nine allpasses, signs, and pickup mix. The two decay settings change the feedback of both tanks.
struct VoiceTank {
    Delay<328> first;
    Delay<462> second;
    Allpass<2,22282> a;
    Allpass<4,21365> b;
    Allpass<6,20447> c;
    Allpass<9,19530> d;
    Allpass<12,18612> e;
    Allpass<2,22282> f;
    Allpass<4,21365> g;
    Allpass<7,20447> h;
    Allpass<10,19530> j;
    I input_low, first_low, second_low, pickup_low; // Q23 avoids DC from coarse pole state.

    void clear() {
        first.clear(); second.clear();
        a.delay.clear(); b.delay.clear(); c.delay.clear(); d.delay.clear(); e.delay.clear();
        f.delay.clear(); g.delay.clear(); h.delay.clear(); j.delay.clear();
        input_low = first_low = second_low = pickup_low = 0;
    }

    I run(I input, I first_feedback, I second_feedback) {
        input_low += pole_step(input * 256 - input_low, 3778);
        const I drive = clip(input - q15(input_low));
        const I first_sample = first.arrival();
        const I second_sample = second.arrival();
        first_low += pole_step(first_sample * 256 - first_low, 30581);
        second_low += pole_step(second_sample * 256 - second_low, 28508);
        first.advance(e.run(d.run(c.run(b.run(a.run(clip(drive + multiply(q15(first_low),first_feedback))))))));
        second.advance(j.run(h.run(g.run(f.run(clip(drive + multiply(q15(second_low),second_feedback)))))));
        const I pickup = multiply(first_sample, 20316) + multiply(second_sample, 12452);
        pickup_low += pole_step(pickup * 256 - pickup_low, 31566);
        return q15(pickup_low);
    }
};

}
