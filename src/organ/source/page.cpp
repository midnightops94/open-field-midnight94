#include "build_r6e_card.h"
#include "engine_turns.hpp"
#include "organ_controls.hpp"
#include "organ_rotor.hpp"
#include "waves.hpp"

extern "C" const Op1fCard op1f_card_organ OP1F_NAMED("op1f_card_organ");

namespace {
EngineTurns turns{};
unsigned slot = 8, visible = 0;
unsigned clicks[4];
unsigned until;
int wanted_card = -1, visible_card = -1;
void* picture;
bool stood;
void* handles[4][4];
void* switches[4][4][3];
void* cards[4];
int last_position[4] = {-1, -1, -1, -1};
int last_switch[4][4];
unsigned last_model = 4;
constexpr const char* type_names[] = {"hammond", "vox", "farfisa", "church"};

void show(void* part, bool active) {
    if (part) op1f_show(part, active);
}

void bind() {
    picture = op1f_picture(&op1f_card_organ, "organ.svg");
    if (!picture) return;
    for (unsigned lane = 0; lane < 4; ++lane) {
        char handle[] = "orgHand00";
        handle[7] += lane;
        for (unsigned model = 0; model < 4; ++model) {
            handle[8] = char('0' + model);
            handles[lane][model] = op1f_node(picture, handle);
            show(handles[lane][model], false);
        }
        char rocker[] = "orgSwitch000";
        rocker[9] += lane;
        for (unsigned cell = 0; cell < (lane == 0 ? 4u : 2u); ++cell) {
            rocker[10] = char('0' + cell);
            last_switch[lane][cell] = -1;
            for (unsigned state = 0; state < 3; ++state) {
                rocker[11] = char('0' + state);
                switches[lane][cell][state] = op1f_node(picture, rocker);
                show(switches[lane][cell][state], false);
            }
        }
        char card[] = "orgCard0";
        card[7] += lane;
        cards[lane] = op1f_node(picture, card);
        show(cards[lane], false);
        char title[] = "orgTitle0";
        char value[] = "orgValue0";
        title[8] += lane;
        value[8] += lane;
        op1f_text_set(picture, title, "organ", 165, OP1F_CENTRE);
        op1f_text_set(picture, value, type_names[lane], 165, OP1F_CENTRE);
    }
}

void set_card(int next) {
    if (visible_card == next) return;
    if (visible_card >= 0) show(cards[visible_card], false);
    if (next >= 0) show(cards[next], true);
    visible_card = next;
}

unsigned switch_state(unsigned kind, unsigned cell, int word) {
    if (kind == 0) {
        for (unsigned model = 0; model < 4; ++model)
            if (word == organ_type_words[model]) return cell == model;
        return 2;
    }
    if (word == 0) return 0;
    if (kind == 3) {
        if (word == organ_spin_words[cell + 1]) return 1;
        if (word == organ_spin_words[2 - cell]) return 0;
        return 2;
    }
    if (word == organ_dual_words[3] || word == organ_dual_words[cell + 1]) return 1;
    if (word == organ_dual_words[2 - cell]) return 0;
    return 2;
}

void draw(const int* values) {
    const unsigned model = organ_model(values[4]);
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (model != last_model) {
            if (last_model < 4) show(handles[lane][last_model], false);
            show(handles[lane][model], true);
        }
        const int position = (values[lane] * 100 + 16384) >> 15;
        if (position != last_position[lane] || model != last_model) {
            const int shift = position * 942 + ((position * 164 + 1024) >> 11);
            const int matrix[9] = {1024, 0, 0, 0, 1024, 0, 0, shift, 1024};
            if (handles[lane][model]) op1f_move(handles[lane][model], matrix);
            last_position[lane] = position;
        }
        for (unsigned cell = 0; cell < (lane == 0 ? 4u : 2u); ++cell) {
            const int next = switch_state(lane, cell, values[4 + lane]);
            const int prior = last_switch[lane][cell];
            if (next == prior) continue;
            if (prior >= 0) show(switches[lane][cell][prior], false);
            show(switches[lane][cell][next], true);
            last_switch[lane][cell] = next;
        }
    }
    last_model = model;
}

void apply_clicks(unsigned place, unsigned ms) {
    unsigned pending[4];
    const unsigned guard = op1f_mask();
    for (unsigned lane = 0; lane < 4; ++lane) {
        pending[lane] = clicks[lane];
        clicks[lane] = 0;
    }
    op1f_unmask(guard);
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (!pending[lane]) continue;
        int value;
        if (op1f_param_get(&op1f_card_organ, 0, place, 4 + lane, &value)) continue;
        const int next = organ_next_choice(lane, value, pending[lane]);
        if (op1f_param_set(&op1f_card_organ, 0, place, 4 + lane, next) == 0 && lane == 0) {
            wanted_card = organ_model(next);
            until = ms + 1024;
        }
    }
}
}

extern "C" void organ_shown(unsigned place, unsigned how) OP1F_NAMED("organ_shown");
extern "C" void organ_shown(unsigned place, unsigned how) {
    const bool active = (how & (OP1F_SHOWN_UP | OP1F_SHOWN_COVERED)) == OP1F_SHOWN_UP;
    turns.shown(place, active);
    const unsigned guard = op1f_mask();
    if (!active || slot != place) {
        for (unsigned& count : clicks) count = 0;
        wanted_card = -1;
    }
    slot = place;
    visible = active ? how : 0;
    op1f_unmask(guard);
}

extern "C" int organ_input(const Op1fInput* event) OP1F_NAMED("organ_input");
extern "C" int organ_input(const Op1fInput* event) {
    if (!event || event->bytes < sizeof(Op1fInput)) return 0;
    if (turns.take(event)) return 1;
    if (!event->mine || event->shift || slot >= 8 || !visible || event->thread != OP1F_KEY_THREAD ||
        event->key < 54 || event->key > 57) return 0;
    if (event->what == OP1F_UP) return 1;
    if (event->what != OP1F_DOWN) return 0;
    const unsigned guard = op1f_mask();
    unsigned& count = clicks[event->key - 54];
    if (count < 1000) ++count;
    op1f_unmask(guard);
    return 1;
}

extern "C" void organ_frame(const Op1fFrame* now) OP1F_NAMED("organ_frame");
extern "C" void organ_frame(const Op1fFrame* now) {
    if (!now || now->bytes < sizeof(Op1fFrame) || now->disk) return;
    organ_prepare_waves();
    if (!picture) bind();
    if (!stood && picture) stood = op1f_page_stand_in(&op1f_card_organ, 0, picture) != 0;
    const unsigned place = slot;
    if (!stood || place >= 8 || !visible || now->page != &op1f_card_organ) {
        set_card(-1);
        return;
    }
    turns.apply(&op1f_card_organ, 0, place);
    apply_clicks(place, now->ms);
    int values[8];
    for (unsigned lane = 0; lane < 8; ++lane)
        if (op1f_param_heard(&op1f_card_organ, 0, place, lane, values + lane)) return;
    organ_rotor::set_spin(values[7]);
    draw(values);
    if (wanted_card >= 0 && int(now->ms - until) >= 0) wanted_card = -1;
    set_card(wanted_card);
    if ((visible & OP1F_SHOWN_OFF) && now->canvas)
        op1f_paint_text(now->canvas, 151, 107, "switched off", 0xff8f4du, 2);
}
