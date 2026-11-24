#pragma once

#include "../core.hpp"

#include <cstdint>

namespace syrup::field {

static constexpr uint32_t sample_rate = 44100;
static constexpr unsigned block_frames = 64;
static constexpr uint16_t neutral_wet_trim = 32768;
static constexpr uint8_t all_main_lanes = 0x0f;

enum class Domain : uint8_t { synth, drum, master, preview };

struct Owner {
    Domain domain = Domain::synth;
    uint8_t slot = 0;
    uint32_t epoch = 0;
    // An opaque instance lease issued by the integration owner, not a guessed native address.
    uintptr_t instance = 0;
};

bool valid(Owner owner) noexcept;
bool same(Owner first, Owner second) noexcept;

struct Settings {
    Owner owner{};
    Parameters stored{};
    Parameters effective{};
    uint8_t effective_mask = 0;
    bool bypass = true;
    uint32_t revision = 0;
};

struct Activity {
    Owner owner{};
    Parameters effective{};
    uint32_t clip_serial = 0;
    uint32_t revision = 0;
    uint16_t wet_activity = 0;
    uint8_t effective_mask = 0;
    bool clipped = false;
    bool playing = false;
    bool activity_available = false;
};

struct Snapshot {
    Owner owner{};
    Parameters stored{};
    Parameters effective{};
    uint8_t effective_mask = 0;
    bool bypass = true;
    Activity activity{};
    bool playing = false;
    bool activity_available = false;
};

using NativeWords = int32_t[8];

// These are already-coherent words supplied by the host for one owner/revision.
// Decode the four main continuous lanes witnessed in Cube's render boundary.
// Spare words are deliberately ignored until a native Shift writer is qualified.
bool make_main_settings(Owner owner, uint32_t revision, const NativeWords& stored,
                        const NativeWords* effective, uint8_t effective_mask,
                        bool bypass, Settings& output) noexcept;

// If the active audio report is fresh, the host may offer it here by value or
// through a read-stable snapshot. A stale, edited or foreign instance uses base.
bool project_snapshot(const Settings& settings, const Activity* active,
                      Snapshot& output) noexcept;

// These names describe Syrup's own four ordinary LFO destinations. Registration,
// lookup widths, ordering and stock fallback remain integration-owner services.
const char* lfo_label(unsigned lane) noexcept;

struct SlotState {
    State core{};
    Owner owner{};
    uint32_t clip_serial = 0;
    uint32_t revision = 0;
    bool armed = false;
    bool need_reset = true;
    bool have_revision = false;
    bool previously_clipped = false;
};

static_assert(sizeof(SlotState) <= 512, "Syrup's leased audio slot exceeds its budget");

// All SlotState operations use one serialized writer, or an explicit handoff
// after audio drains. Publish an Activity value through host-provided storage.
bool prepare(SlotState& state, Owner owner) noexcept;
bool retire(SlotState& state, Owner owner) noexcept;
bool notify_bypass(SlotState& state, Owner owner) noexcept;

// Caller owns two nonoverlapping writable planes of exactly 64 samples. No
// pointer, owner, or control words are fetched from guessed platform addresses.
// Return false on a refused block and leave both planes and telemetry untouched.
// A valid bypass report preserves both planes and invalidates processing history.
bool render_block(SlotState& state, const Settings& settings, int32_t* left,
                  int32_t* right, Activity& report) noexcept;

} // namespace syrup::field
