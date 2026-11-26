#include "build_r6e_card.h"
#include "squash.hpp"

extern "C" const Op1fCard op1f_card_squash OP1F_NAMED("op1f_card_squash");

namespace {
constexpr unsigned places = OP1F_PLACE_MASTER + 1;
constexpr unsigned ready = (OP1F_SHOWN_UP | OP1F_SHOWN_OFF | OP1F_SHOWN_COVERED);
constexpr unsigned digits[] = {1, 3, 9, 27};
constexpr unsigned paper = OP1F_COLOUR(205, 208, 194);
const char* const captions[] = {"lid", "cushion", "timing", "mix"};
const char* const titles[] = {"detector", "cushion", "timing", "heat"};
const char* const modes[][3] = {{"peaks", "body", "bass safe"}, {"rubber", "hard", "pillow"},
                               {"tight", "glue", "pump"}, {"cool", "hot", nullptr}};

struct Choice {
    volatile unsigned events[4], recent, active;
    unsigned seen[4];
    bool save;
};
Choice choices_by_place[places]{};
volatile unsigned pressure_by_place[places]{};
volatile unsigned shown_place = places, shown_flags;
unsigned popup_time;
bool popup_open;

unsigned current_modes(unsigned place) {
    const unsigned current = place < places ? choices_by_place[place].active : 0;
    return current ? current - 1 : squash::default_modes;
}

unsigned mode_at(unsigned all, unsigned lane) {
    switch (lane) {
    case 0: return all % 3;
    case 1: return (all / 3) % 3;
    case 2: return (all / 9) % 3;
    default: return all / 27;
    }
}

unsigned file_scope(unsigned place) {
    return place == OP1F_PLACE_MASTER ? unsigned(OP1F_KEEP_MASTER) :
           place >= OP1F_PLACE_DRUM ? unsigned(OP1F_KEEP_DRUM) : unsigned(OP1F_KEEP_SYNTH);
}

void update_modes(unsigned place, unsigned now) {
    auto& choice = choices_by_place[place];
    const unsigned file = file_scope(place), slot = place & 7;
    if (!choice.active) {
        char buf[5]{};
        const int read = op1f_value_get(&op1f_card_squash, file, slot, "squash_modes", buf, sizeof(buf));
        if (read == OP1F_NOT_NOW) return;
        unsigned value = squash::default_modes;
        if (read == 1 && buf[0] >= '0' && buf[0] <= '9') value = unsigned(buf[0] - '0');
        if (read == 2 && buf[0] >= '1' && buf[0] <= '5' && buf[1] >= '0' && buf[1] <= '9') {
            const unsigned n = unsigned(buf[0] - '0') * 10 + unsigned(buf[1] - '0');
            if (n < 54) value = n;
        }
        choice.active = value + 1;
    }
    unsigned next = choice.active - 1;
    bool clicked = false;
    for (unsigned lane = 0; lane < 4; ++lane) {
        const unsigned seen = choice.events[lane];
        const unsigned count = seen - choice.seen[lane];
        if (count) {
            const unsigned old = mode_at(next, lane);
            const unsigned mode = lane == 3 ? (old + count) & 1 : (old + count % 3) % 3;
            next = next - old * digits[lane] + mode * digits[lane];
            clicked = true;
            choice.seen[lane] = seen;
        }
    }
    if (clicked) {
        choice.active = next + 1;
        choice.save = true;
        if (shown_place == place && (shown_flags & ready) == OP1F_SHOWN_UP) {
            popup_open = true;
            popup_time = now;
        }
    }
    if (choice.save) {
        char encoded[] = {char('0' + next / 10), char('0' + next % 10), 0};
        choice.save = op1f_value_set(&op1f_card_squash, file, slot, "squash_modes", encoded + (next < 10)) != 0;
    }
}

void* document;
void* lid;
void* spring[3];
void* detector[3];
void* cushions[6];
void* face[2];
void* meter[12];
void* icon[5];
void* overlay;
void* off;
bool tried, installed;
int previous_values[4] = {-1, -1, -1, -1};
int previous_popup = -1;

void visible(void* node, bool on) { if (node) op1f_show(node, on); }
void move(void* node, int x, int y, int width = 1024, int height = 1024) {
    if (!node) return;
    const int matrix[] = {width,0,0,0,height,0,x,y,1024};
    op1f_move(node, matrix);
}

void bind() {
    tried = true;
    document = op1f_picture(&op1f_card_squash, "squash_off.svg");
    if (!document) document = op1f_picture(&op1f_card_squash, "squash.svg");
    if (!document) return;
    lid = op1f_node(document, "lid");
    char s[] = "spring0", d[] = "detector0", c[] = "cushion0", g[] = "glow0";
    for (unsigned i = 0; i < 3; ++i) {
        s[6] = d[8] = c[7] = g[4] = char('0' + i);
        spring[i] = op1f_node(document, s);
        detector[i] = op1f_node(document, d);
        cushions[i] = op1f_node(document, c);
        cushions[3+i] = op1f_node(document, g);
        visible(spring[i], false);
        visible(detector[i], false);
        visible(cushions[i], false);
        visible(cushions[3+i], false);
    }
    face[0] = op1f_node(document, "calm");
    face[1] = op1f_node(document, "pressed");
    const char* const ticks[] = {"mix0","mix1","mix2","mix3","mix4","mix5",
                                 "mix6","mix7","mix8","mix9","mix10","mix11"};
    for (unsigned i = 0; i < 12; ++i) meter[i] = op1f_node(document, ticks[i]);
    char tile[] = "icon0", label[] = "cap0";
    for (unsigned i = 0; i < 4; ++i) {
        label[3] = char('0' + i);
        op1f_text_set(document, label, captions[i], 0, i < 2 ? OP1F_LEFT : OP1F_RIGHT);
    }
    for (unsigned i = 0; i < 5; ++i) {
        tile[4] = char('0' + i);
        icon[i] = op1f_node(document, tile);
        visible(icon[i], false);
    }
    overlay = op1f_node(document, "mode");
    off = op1f_node(document, "stockFxOff");
    if (!off) {
        off = op1f_node(document, "fxOff");
        op1f_text_set(document, "fxOffText", "fx off", 0, OP1F_CENTRE);
    }
    visible(off, false);
    visible(overlay, false);
}

void readout(const int (&words)[4]) {
    char id[] = "val0";
    for (unsigned i = 0; i < 4; ++i) {
        const int value = (words[i] * 99 + 16383) / 32767;
        if (previous_values[i] == value) continue;
        previous_values[i] = value;
        const int tens = value / 10;
        char text[] = {char('0' + tens), char('0' + value % 10), 0};
        id[3] = char('0' + i);
        op1f_text_set(document, id, text, 0, i < 2 ? OP1F_LEFT : OP1F_RIGHT);
    }
}

void animate(const int (&words)[4], unsigned all, unsigned pressure) {
    const unsigned detector_mode = mode_at(all, 0);
    const unsigned cushion_mode = mode_at(all, 1);
    const unsigned spring_mode = mode_at(all, 2);
    const unsigned hot = mode_at(all, 3);
    const int lid_y = 101 * 1024 + (22 * words[0] >> 5) + int(17 * pressure);
    const int width = 860 + (195 * words[1] >> 15) + int(123 * pressure >> 10);
    const int top = lid_y + 11 * 1024;
    const int height = (177 * 1024 - top) / 48;
    const unsigned ticks = unsigned((words[3] * 12 + 16384) >> 15);
    for (unsigned i = 0; i < 3; ++i) {
        visible(spring[i], i == spring_mode);
        visible(detector[i], i == detector_mode);
        visible(cushions[i], !hot && i == cushion_mode);
        visible(cushions[3+i], hot && i == cushion_mode);
    }
    move(spring[spring_mode], 240*1024, 48*1024, 788 + (630*words[2] >> 15), (lid_y - 48*1024) >> 6);
    move(lid, 0, lid_y);
    move(detector[detector_mode], 0, lid_y);
    move(cushions[hot*3+cushion_mode], 240*1024, top, width, height);
    const unsigned stressed = pressure > 246;
    visible(face[0], !stressed);
    visible(face[1], stressed);
    move(face[stressed], 240*1024, top, width, height);
    for (unsigned i = 0; i < 12; ++i) visible(meter[i], i < ticks);
}
}

extern "C" void squash_reset(unsigned place, void* state) OP1F_NAMED("squash_reset");
extern "C" void squash_reset(unsigned place, void* state) {
    if (state) static_cast<squash::State*>(state)->valid = 0;
    if (place < places) pressure_by_place[place] = 0;
}

extern "C" void squash_process(const Op1fEffectBlock* block) OP1F_NAMED("squash_process");
extern "C" void squash_process(const Op1fEffectBlock* block) {
    static_assert(sizeof(squash::State) <= 128, "effect state reservation");
    auto* state = static_cast<squash::State*>(block->state);
    if (!state) return;
    if (!block->on) { squash_reset(block->place, state); return; }
    if (block->fresh) state->valid = 0;
    unsigned words[4];
    for (unsigned i = 0; i < 4; ++i) words[i] = unsigned(squash::limit(block->values[i], 0, 32767));
    state->set(words, current_modes(block->place));
    int crest = 0;
    for (unsigned i = 0; i < block->frames; ++i) {
        int l = block->left[i], r = block->right[i];
        state->step(l, r);
        block->left[i] = l;
        block->right[i] = r;
        if (state->reduction > crest) crest = state->reduction;
    }
    if (block->place < places) pressure_by_place[block->place] = unsigned(squash::limit(squash::mul(crest, 683), 0, 1024));
}

extern "C" void squash_changed(unsigned where, unsigned slot, unsigned how) OP1F_NAMED("squash_changed");
extern "C" void squash_changed(unsigned where, unsigned slot, unsigned how) {
    if (how == OP1F_KEPT_ENGINE || how == OP1F_KEPT_SAVED) return;
    const unsigned place = where == OP1F_KEEP_MASTER ? unsigned(OP1F_PLACE_MASTER) :
                           where == OP1F_KEEP_DRUM ? OP1F_PLACE_DRUM + slot : slot;
    if (place >= places) return;
    auto& choice = choices_by_place[place];
    choice.active = 0;
    choice.save = false;
    for (unsigned i = 0; i < 4; ++i) choice.seen[i] = choice.events[i];
}

extern "C" void squash_shown(unsigned place, unsigned flags) OP1F_NAMED("squash_shown");
extern "C" void squash_shown(unsigned place, unsigned flags) {
    if (shown_place != place || (flags & ready) != OP1F_SHOWN_UP) popup_open = false;
    shown_place = place;
    shown_flags = flags;
}

extern "C" int squash_input(const Op1fInput* event) OP1F_NAMED("squash_input");
extern "C" int squash_input(const Op1fInput* event) {
    const unsigned place = shown_place;
    if (!event || event->bytes < 40 || !event->mine || event->shift || event->thread != OP1F_KEY_THREAD ||
        place >= places || (shown_flags & ready) != OP1F_SHOWN_UP || event->key < 54 || event->key > 57) return 0;
    const unsigned lane = event->key - 54;
    if (event->what == OP1F_DOWN) {
        auto& choice = choices_by_place[place];
        ++choice.events[lane];
        choice.recent = lane;
        return 1;
    }
    return event->what == OP1F_UP;
}

extern "C" void squash_frame(const Op1fFrame* frame) OP1F_NAMED("squash_frame");
extern "C" void squash_frame(const Op1fFrame* frame) {
    if (!frame || frame->bytes < 40 || frame->disk) return;
    for (unsigned row = 0; row < places; ++row) update_modes(row, frame->ms);
    const unsigned place = shown_place;
    if (place >= places || !(shown_flags & OP1F_SHOWN_UP) || frame->page != &op1f_card_squash) return;
    if (!tried) bind();
    if (document && !installed) installed = op1f_page_stand_in(&op1f_card_squash, 0, document) != 0;
    if (!installed) return;
    const bool bypass = shown_flags & OP1F_SHOWN_OFF;
    const bool covered = (shown_flags & OP1F_SHOWN_COVERED) || frame->popup;
    visible(off, bypass && !covered);
    if (popup_open && frame->ms - popup_time >= 1024) popup_open = false;
    const unsigned mode = current_modes(place);
    const unsigned lane = choices_by_place[place].recent;
    const int popup = popup_open && !covered && !bypass ? int(lane * 4 + mode_at(mode, lane)) : -1;
    if (popup != previous_popup) {
        visible(overlay, popup >= 0);
        if (popup >= 0) {
            const unsigned tile = lane == 3 ? lane + mode_at(mode, lane) : lane;
            for (unsigned i = 0; i < 5; ++i) visible(icon[i], i == tile);
            op1f_text_set(document, "modeTitle", titles[lane], 165, OP1F_CENTRE);
            op1f_text_set(document, "modeValue", modes[lane][mode_at(mode, lane)], 165, OP1F_CENTRE);
        }
        previous_popup = popup;
    }
    int words[4];
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (op1f_param_heard(&op1f_card_squash, 0, place, lane, words + lane)) return;
        words[lane] = squash::limit(words[lane], 0, 32767);
    }
    readout(words);
    animate(words, mode, bypass ? 0 : pressure_by_place[place]);
}
