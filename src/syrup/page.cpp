#include "syrup.hpp"
#include "dsp/ui/retained.hpp"

namespace {
namespace ui = syrup::ui;
constexpr unsigned ask = 0;
constexpr unsigned flags_mask = 7;
volatile unsigned shown;
struct Part {
    void* node;
    const char* id;
    unsigned align;
    ui::ObjectKind kind;
};
struct Backend {
    static constexpr unsigned capacity = sizeof(ui::artwork_ids)/sizeof(*ui::artwork_ids) +
                                         sizeof(ui::font_ids)/sizeof(*ui::font_ids);
    Part document{}, parts[capacity]{};
    unsigned bound = 0;
    Part* find(Part*, const char* id) {
        const unsigned index = bound++;
        auto& p = parts[index];
        p.id = id;
        p.node = op1f_node(document.node, id);
        p.kind = index < sizeof(ui::artwork_ids)/sizeof(*ui::artwork_ids)
                  ? ui::ObjectKind::group : ui::ObjectKind::stroke_template;
        return p.node ? &p : nullptr;
    }
    bool is_kind(Part* p, ui::ObjectKind k) { return p && p->kind == k; }
    Part* make_font(Part*, Part* placeholder, ui::Align align) {
        placeholder->align = static_cast<unsigned>(align);
        if (!op1f_text_set(document.node, placeholder->id, "", 0, placeholder->align)) return nullptr;
        placeholder->node = op1f_text_node(document.node, placeholder->id);
        placeholder->kind = ui::ObjectKind::font;
        return placeholder->node ? placeholder : nullptr;
    }
    void set_visible(Part* p, bool on) { op1f_show(p->node, on); }
    void set_text(Part* p, const char* s) { op1f_text_set(document.node, p->id, s, 0, p->align); }
    void set_matrix(Part* p, const ui::Matrix& m) {
        const int matrix[9] = {m.xx,m.xy,0,m.yx,m.yy,0,m.x,m.y,1024};
        op1f_move(p->node, matrix);
    }
};
Backend backend{};
ui::RetainedScene<Part*> scene{};
bool attempted = false;
void* off_layer = nullptr;
unsigned clip_place = ~0u, seen_clip = 0, clip_time = 0;
bool holding_clip = false;

void bind_picture() {
    attempted = true;
    backend.document = {op1f_picture(&op1f_card_syrup,"syrup.svg"),nullptr,0,ui::ObjectKind::document};
    if (!backend.document.node) {
        backend.document.node = op1f_picture(&op1f_card_syrup,"syrup-safe.svg");
        if (!backend.document.node) return;
        off_layer = op1f_node(backend.document.node,"fxOff");
        op1f_text_set(backend.document.node,"fxOffText","fx off",0,OP1F_CENTRE);
    } else {
        off_layer = op1f_node(backend.document.node,"stockFxOff");
    }
    scene.bind(backend,&backend.document);
    if (off_layer) op1f_show(off_layer,0);
    op1f_page_stand_in(&op1f_card_syrup,ask,backend.document.node);
}
}

extern "C" void syrup_shown(unsigned where, unsigned how) {
    shown = (where << 3) | (how & flags_mask);
}

extern "C" void syrup_frame(const Op1fFrame* frame) {
    if (!frame || frame->bytes < 40) return;
    const unsigned context = shown, where = context >> 3;
    if (!(context & OP1F_SHOWN_UP) || where > OP1F_PLACE_MASTER) {
        clip_place = ~0u;
        return;
    }
    if (!attempted) bind_picture();
    if (frame->page != &op1f_card_syrup) return;
    const bool off = (context & OP1F_SHOWN_OFF) != 0;
    if (off_layer) op1f_show(off_layer, off && !frame->popup && !(context & OP1F_SHOWN_COVERED));
    auto p = syrup::default_parameters();
    uint16_t* const controls[] = {&p.cutoff,&p.resonance,&p.drive,&p.mix};
    int current;
    for (unsigned i=0; i<4; ++i) {
        if (op1f_param_heard(&op1f_card_syrup, ask, where, i, &current)) return;
        *controls[i] = uint16_t(current);
    }
    ui::Frame pose{};
    pose.admitted = true;
    pose.displayed = p;
    pose.native_fx_off = off;
    const char* const labels[] = {"freq","res","drive","mix"};
    for (unsigned i=0; i<4; ++i) {
        pose.labels[i] = labels[i];
        pose.values[i] = ui::detail::format(static_cast<ui::Lane>(i),p,false);
    }
    const unsigned now = op1f_ms();
    ui::detail::set_visuals(pose,now);
    const unsigned serial = syrup_clip_events[where];
    if (clip_place != where || off) holding_clip = false;
    else if (seen_clip != serial) { holding_clip = true; clip_time = now; }
    clip_place = where;
    seen_clip = serial;
    pose.show_clip = holding_clip && now - clip_time < ui::clip_hold_ms;
    if (backend.document.node) scene.apply(backend,pose);
    else if (frame->canvas) {
        op1f_paint_text(frame->canvas,184,65,"SYRUP",OP1F_COLOUR(205,208,194),3);
        if (off && !frame->popup && !(context & OP1F_SHOWN_COVERED))
            op1f_paint_text(frame->canvas,202,115,"FX OFF",OP1F_COLOUR(205,208,194),2);
    }
}
