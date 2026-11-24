#pragma once

#include "presenter.hpp"

namespace syrup::ui {

enum class ObjectKind : uint8_t { document, group, stroke_template, font };
enum class Align : uint8_t { left = 0, center = 1, right = 2 };

static constexpr const char* artwork_ids[] = {
    "syrupArtwork", "syrupPlate", "syrupBottle", "syrupBody", "syrupCap",
    "syrupPinch", "syrupClip", "syrupStream", "syrupPool", "syrupDrip",
    "syrupNeedle", "syrupButter", "syrupMelt", "syrupFallingDrop", "syrupWaist00",
    "syrupWaist01", "syrupWaist02", "syrupWaist03", "syrupWaist04", "syrupWaist05",
    "syrupWaist06", "syrupWaist07", "syrupWaist08", "syrupWaist09", "syrupWaist10",
    "syrupWaist11", "syrupWaist12", "syrupWaist13", "syrupWaist14", "syrupWaist15",
    "syrupWaist16", "syrupSpill01", "syrupSpill02", "syrupSpill03", "syrupSpill04",
    "syrupFlow00", "syrupFlow01", "syrupFlow02", "syrupFlow03", "syrupFlow04",
    "syrupFlow05", "syrupFlow06", "syrupFlow07", "syrupFlow08", "syrupFlow09",
    "syrupFlow10", "syrupFlow11", "syrupFlow12", "syrupFlow13", "syrupFlow14",
    "syrupFlow15", "syrupTint01", "syrupTint02", "syrupTint03", "syrupTint04",
    "syrupTint05", "syrupTint06", "syrupTint07", "syrupTint08", "syrupTint09",
    "syrupTint10", "syrupTint11", "syrupTint12", "syrupMeter00", "syrupMeter01",
    "syrupMeter02", "syrupMeter03", "syrupMeter04", "syrupMeter05", "syrupMeter06",
    "syrupMeter07", "syrupMeter08", "syrupMeter09",
};

static constexpr const char* font_ids[] = {
    "syrupLabel0", "syrupLabel1", "syrupLabel2", "syrupLabel3",
    "syrupValue1", "syrupValue2", "syrupFallback",
};

struct MissingBindings {
    uint32_t groups[(sizeof(artwork_ids)/sizeof(*artwork_ids)+31)/32]{};
    uint16_t fonts = 0;
};

// Backend supplies only the integration owner's verified operations. The scene
// binds and updates on the UI thread; the owner draws the document and then the
// existing stock overlays at their normal order. This class never owns native
// memory, registers a page, or calls an address.
template<class Handle>
class RetainedScene {
public:
    template<class Backend>
    bool bind(Backend& backend, Handle document) noexcept {
        *this = {};
        if (!document || !backend.is_kind(document, ObjectKind::document)) return false;
        document_ = document;
        for (unsigned i = 0; i < group_count; ++i) {
            const Handle handle = backend.find(document, artwork_ids[i]);
            if (handle && backend.is_kind(handle, ObjectKind::group)) nodes_[i].handle = handle;
            else missing_.groups[i/32] |= uint32_t(1) << (i%32);
        }
        for (unsigned i = 0; i < font_count; ++i) {
            const Handle placeholder = backend.find(document, font_ids[i]);
            if (placeholder && backend.is_kind(placeholder, ObjectKind::stroke_template)) {
                // The validated stroke-backed rectangle is replaced in place.
                // Store the returned Font handle, never the template handle.
                const Handle font = backend.make_font(document, placeholder, alignment(i));
                if (font && backend.is_kind(font, ObjectKind::font)) fonts_[i].handle = font;
            }
            if (!fonts_[i].handle) missing_.fonts |= uint16_t(1) << i;
        }
        return true;
    }

    void close() noexcept { *this = {}; }
    Handle document() const noexcept { return document_; }
    MissingBindings missing() const noexcept { return missing_; }

    template<class Backend>
    bool apply(Backend& backend, const Frame& frame) noexcept {
        if (!document_ || !frame.admitted) return false;
        node(backend, bottle, frame.bottle, true);
        fixed(backend, body, true);
        for (unsigned i = 0; i < waist_count; ++i)
            fixed(backend, waist + i, i == frame.waist);
        fixed(backend, cap, true);
        node(backend, pinch, frame.pinch_cues, frame.show_pinch);
        fixed(backend, clip, frame.show_clip);
        fixed(backend, stream, frame.show_wet);
        for (unsigned i = 0; i < flow_count; ++i) {
            fixed(backend, flow + i, i == frame.flow);
        }
        node(backend, pool, frame.pool, frame.show_wet);
        node(backend, drip, frame.pool, frame.show_drip);
        for (unsigned i = 0; i < tint_count; ++i) {
            fixed(backend, tint + i, i + 1 == frame.tint);
        }
        node(backend, butter, frame.butter, true);
        node(backend, melt, frame.melt, frame.show_melt);
        for (unsigned i = 0; i < spill_count; ++i)
            fixed(backend, spill + i, i+1 == frame.spill);
        node(backend, falling_drop, frame.drop, frame.show_drop);
        node(backend, needle, frame.needle, true);
        for (unsigned i = 0; i < meter_count; ++i) {
            fixed(backend, meter + i, i < frame.mix_ticks);
        }
        for (unsigned i = 0; i < lane_count; ++i) {
            text(backend, i, frame.labels[i], true);
        }
        text(backend, 4, frame.values[1].data, true);
        text(backend, 5, frame.values[2].data, true);
        const bool fallback = !nodes_[artwork].handle || !nodes_[plate].handle ||
                              !nodes_[bottle].handle;
        text(backend, fallback_font, "syrup", fallback);
        return true;
    }

private:
    enum Group : unsigned {
        artwork, plate, bottle, body, cap, pinch, clip, stream, pool, drip, needle,
        butter, melt, falling_drop, waist, spill = waist + waist_count,
        flow = spill + spill_count, tint = flow + flow_count, meter = tint + tint_count
    };
    static constexpr unsigned fallback_font = 6;
    static constexpr unsigned group_count = sizeof(artwork_ids) / sizeof(artwork_ids[0]);
    static constexpr unsigned font_count = sizeof(font_ids) / sizeof(font_ids[0]);
    static_assert(meter + meter_count == group_count);
    static_assert(fallback_font + 1 == font_count);

    struct Visibility {
        bool initialized = false;
        bool shown = false;
    };
    struct Node {
        Handle handle{};
        Visibility visibility{};
        bool moved = false;
    };
    struct Font {
        Handle handle{};
        Text prior{};
        Visibility visibility{};
    };

    static constexpr Align alignment(unsigned index) noexcept {
        if (index == 3 || index == fallback_font) return Align::center;
        if (index == 2 || index == 5) return Align::right;
        return Align::left;
    }

    template<class Backend>
    static void visible(Backend& backend, Handle handle, Visibility& old, bool state) noexcept {
        if (old.initialized && old.shown == state) return;
        backend.set_visible(handle, state);
        old = {true, state};
    }

    template<class Backend>
    void fixed(Backend& backend, unsigned index, bool shown) noexcept {
        auto& bound = nodes_[index];
        if (!bound.handle) return;
        if (shown && !bound.moved) {
            backend.set_matrix(bound.handle, {});
            bound.moved = true;
        }
        visible(backend, bound.handle, bound.visibility, shown);
    }

    // Only the small group before the waist artwork has any animated matrices.
    template<class Backend>
    void node(Backend& backend, unsigned index, const Matrix& pose, bool shown) noexcept {
        auto& bound = nodes_[index];
        if (!bound.handle) return;
        if (shown && (!bound.moved || !(poses_[index] == pose))) {
            backend.set_matrix(bound.handle, pose);
            poses_[index] = pose;
            bound.moved = true;
        }
        visible(backend, bound.handle, bound.visibility, shown);
    }

    template<class Backend>
    void text(Backend& backend, unsigned index, const char* words, bool shown) noexcept {
        auto& bound = fonts_[index];
        if (!bound.handle) return;
        if (shown && words) {
            unsigned count = 0;
            bool changed = false;
            while (count + 1 < sizeof(bound.prior.data) && words[count]) {
                changed |= bound.prior.data[count] != words[count];
                bound.prior.data[count] = words[count];
                ++count;
            }
            changed |= bound.prior.data[count] != '\0';
            bound.prior.data[count] = '\0';
            if (changed) backend.set_text(bound.handle, bound.prior.data);
        }
        visible(backend, bound.handle, bound.visibility, shown && words);
    }

    Handle document_{};
    Node nodes_[group_count]{};
    Matrix poses_[waist]{};
    Font fonts_[font_count]{};
    MissingBindings missing_{};
};

} // namespace syrup::ui
