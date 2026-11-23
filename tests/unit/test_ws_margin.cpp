// FFTA widescreen margin policy — the pure decision logic (src/ffta_ws_margin.h)
// unit-tested here. See the header for the contract and the evidence trail.
//
// CHECK (not <cassert>): Release erases assert() via -DNDEBUG (the
// vacuous-test trap, BRINGUP).
#include "../../src/ffta_ws_margin.h"
#include "../../src/ffta_ui_edges.h"
#include <algorithm>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

namespace {

int failures = 0;

#define WS_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                     \
        }                                                                   \
    } while (0)

// A 0x60-byte IO block, filled from a {offset: value16} map.
std::vector<std::uint8_t> io_from(std::initializer_list<std::pair<unsigned, unsigned>> kv) {
    std::vector<std::uint8_t> io(0x60, 0);
    for (auto [off, v] : kv) {
        io[off] = static_cast<std::uint8_t>(v & 0xFF);
        io[off + 1] = static_cast<std::uint8_t>(v >> 8);
    }
    return io;
}

}  // namespace

int run_ws_margin_tests() {
    using ffta::ws_ui_gate_layers;

    // ── pinned captures (2026-10-16: device town conversation x4, desktop
    //    pub replay — BRINGUP has the register blocks) ───────────────────
    {
        // Town talk bubble: windows configured, enable bits CLEAR (0x1F40).
        // The pub's desktop twin has 0x3F40 — either way the size-0 layers
        // are bg2|bg3 and the gate must admit exactly them.
        for (const unsigned dispcnt : {0x1F40u, 0x3F40u}) {
            const std::vector<std::uint8_t> io = io_from({
                {0x00, dispcnt},
                {0x08, 0x4C03}, {0x0A, 0x4E02},   // battle/pub ring, size=1
                {0x0C, 0x0B08}, {0x0E, 0x0A08},   // UI layers, size=0
            });
            WS_CHECK(ws_ui_gate_layers(io.data(), io.size()) == 0xCu);
        }
    }
    // ── the move state that the withdrawn skip broke: ALL FOUR layers are
    //    the 512-px ring — the gate must be INERT here (this pin is the
    //    regression the 2026-10-15 withdrawal was about) ─────────────────
    {
        const std::vector<std::uint8_t> io = io_from({
            {0x00, 0x1F40},
            {0x08, 0x4C03}, {0x0A, 0x4E02},       // ring on BG0/BG1, size=1
            {0x0C, 0x5C08}, {0x0E, 0x4A09},       // ring on BG2/BG3, size=1
        });
        WS_CHECK(ws_ui_gate_layers(io.data(), io.size()) == 0u);
    }
    // ── per-layer admission: disabled layers never join, even if size-0 ──
    {
        // dispcnt 0x0140: only bg0 enabled (bit 8); bg3 also size-0 but OFF.
        const std::vector<std::uint8_t> io = io_from({
            {0x00, 0x0140},
            {0x08, 0x0B08}, {0x0E, 0x0A08},
        });
        WS_CHECK(ws_ui_gate_layers(io.data(), io.size()) == 0x1u);
    }
    // ── partial: one size-0 layer + one size-1 layer both enabled ────────
    {
        // (Unset BGxCNT reads as size-0 — a real enabled BG always has its
        // cnt programmed, so the partial fixture fills all four rows.)
        const std::vector<std::uint8_t> io = io_from({
            {0x00, 0x3F40},                       // all four BGs enabled
            {0x08, 0x0B08}, {0x0A, 0x4E02},       // bg0 size-0, bg1 size-1
            {0x0C, 0x4E02}, {0x0E, 0x4E02},       // bg2/bg3 size-1 (fillers)
        });
        WS_CHECK(ws_ui_gate_layers(io.data(), io.size()) == 0x1u);
    }
    // ── world map at rest (frozen fixture): bg2/bg3 size-0 HUD layers —
    //    the gate arms there too; harmless (pillarbox already blacks the
    //    margins) and it is the field-idle cleanup case ──────────────────
    {
        const std::vector<std::uint8_t> io = io_from({
            {0x00, 0x1F40},
            {0x08, 0xAA03}, {0x0A, 0xAC02},       // map field BGs, size=2
            {0x0C, 0x2E09}, {0x0E, 0x2F08},       // HUD layers, size=0
        });
        WS_CHECK(ws_ui_gate_layers(io.data(), io.size()) == 0xCu);
    }
    // ── degenerate IO never arms anything (fail-safe) ────────────────────
    {
        const std::vector<std::uint8_t> io(0x60, 0);
        WS_CHECK(ws_ui_gate_layers(io.data(), io.size()) == 0u);
        WS_CHECK(ws_ui_gate_layers(nullptr, 0) == 0u);
    }
    // ── the gate function's span math (the -1/0 contract the engine acts on)
    {
        // Native span [40, 280) at extra_left=40 (view 320).
        WS_CHECK(ffta::ws_ui_gate_sample(40, 40) == 0);    // first native px
        WS_CHECK(ffta::ws_ui_gate_sample(40, 279) == 0);   // last native px
        WS_CHECK(ffta::ws_ui_gate_sample(40, 39) == -1);   // left margin
        WS_CHECK(ffta::ws_ui_gate_sample(40, 280) == -1);  // right margin
        WS_CHECK(ffta::ws_ui_gate_sample(40, 0) == -1);
        WS_CHECK(ffta::ws_ui_gate_sample(40, 319) == -1);
        // Faithful 240 view: no margins, no suppression anywhere.
        WS_CHECK(ffta::ws_ui_gate_sample(0, 0) == 0);
        WS_CHECK(ffta::ws_ui_gate_sample(0, 239) == 0);
        // World map pillarbox shape: extra 104/104 at view 448 (104+240+104).
        WS_CHECK(ffta::ws_ui_gate_sample(104, 103) == -1);
        WS_CHECK(ffta::ws_ui_gate_sample(104, 343) == 0);   // last native px
        WS_CHECK(ffta::ws_ui_gate_sample(104, 344) == -1);  // first right margin
    }

    // Wide UI finishing uses visible regular-BG coverage, not scene IDs.
    {
        std::vector<std::uint8_t> io(0x400), vram(0x18000), pal(0x400), rgb(320 * 160 * 3);
        auto put = [](auto& bytes, int off, unsigned value) {
            bytes[off] = value & 255; bytes[off + 1] = value >> 8;
        };
        put(io, 0, 0x100); // BG0, regular mode, enabled
        put(io, 8, 0x1000); // screenblock 16, charblock 0
        put(pal, 2, 0x7fff);
        for (int i = 0; i < 32; ++i) vram[32 + i] = 0x11; // opaque white tile 1
        for (int ty = 2; ty < 6; ++ty) {
            put(vram, 0x8000 + ty * 64, 1);
            put(vram, 0x8000 + ty * 64 + 29 * 2, 1);
        }
        for (int y = 16; y < 48; ++y)
            for (int x : {40, 279})
                for (int c = 0; c < 3; ++c) rgb[(y * 320 + x) * 3 + c] = 255;
        bool rows[160];
        for (int side : {0, 1}) {
            ffta::ui_edge_runs(io.data(), vram.data(), pal.data(), rgb.data(), 320, 40, side, rows);
            for (int y = 0; y < 160; ++y) WS_CHECK(rows[y] == (y >= 16 && y < 48));
        }
        // Covered UI pixels cannot decorate the overlying scenery/OBJ.
        std::fill(rgb.begin(), rgb.end(), 20);
        ffta::ui_edge_runs(io.data(), vram.data(), pal.data(), rgb.data(), 320, 40, 0, rows);
        for (bool row : rows) WS_CHECK(!row);
        // A six-pixel glyph fragment is not a window edge.
        for (int y = 16; y < 22; ++y)
            for (int c = 0; c < 3; ++c) rgb[(y * 320 + 40) * 3 + c] = 255;
        ffta::ui_edge_runs(io.data(), vram.data(), pal.data(), rgb.data(), 320, 40, 0, rows);
        for (bool row : rows) WS_CHECK(!row);
        // Transparent map entries do not create a full-height rail.
        WS_CHECK(ffta::ui_edge_color(io.data(), vram.data(), pal.data(), 0, 0, 0) == -1);
        // Native width remains completely undecorated.
        ffta::ui_edge_runs(io.data(), vram.data(), pal.data(), rgb.data(), 240, 0, 0, rows);
        for (bool row : rows) WS_CHECK(!row);
        // Both flip bits and banked 4-bpp palette addressing are preserved.
        vram[32] = 0x02; put(pal, (2 + 3 * 16) * 2, 0x1234);
        put(vram, 0x8000, 1 | 0xC00 | 0x3000);
        WS_CHECK(ffta::ui_edge_color(io.data(), vram.data(), pal.data(), 0, 7, 7) == 0x1234);
        // 8-bpp uses the unbanked byte index.
        put(io, 8, 0x1080); put(vram, 0x8000, 2);
        vram[128] = 5; put(pal, 10, 0x4567);
        WS_CHECK(ffta::ui_edge_color(io.data(), vram.data(), pal.data(), 0, 0, 0) == 0x4567);
    }

    {
        std::vector<std::uint8_t> io(0x400), vram(0x18000), pal(0x400);
        io[1] = 1; io[9] = 0x10; // BG0, tile map 0x8000
        vram[0x8000] = 1;
        for (int i = 32; i < 64; ++i) vram[i] = 0x11;
        pal[2] = 0xff; pal[3] = 0x7f;
        io[0x50] = 0x41; io[0x51] = 0x20; // alpha over backdrop
        io[0x52] = io[0x53] = 8;
        const std::uint8_t half_white[] = {132, 123, 132};
        WS_CHECK(ffta::ui_edge_visible(io.data(), vram.data(), pal.data(), 0, 0, 0, half_white));
        io[0x50] = 0;
        WS_CHECK(!ffta::ui_edge_visible(io.data(), vram.data(), pal.data(), 0, 0, 0, half_white));
    }

    if (failures == 0) std::printf("ws_margin: ALL PASS\n");
    return failures == 0 ? 0 : 1;
}

int main() { return run_ws_margin_tests(); }
