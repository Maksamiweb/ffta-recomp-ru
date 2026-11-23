#pragma once
#include <cstdint>
#include <cstddef>
#include "ffta_ws_margin.h"

namespace ffta {
// Read-only regular-BG sampling, matching gba_ppu.cpp's tile addressing.
// This never calls the guest bus: UI decoration must not accrue guest cycles.
inline int ui_edge_color(const std::uint8_t* io, const std::uint8_t* vram,
                         const std::uint8_t* pal, unsigned bg, int x, int y) {
    const auto u16 = [](const std::uint8_t* p) { return unsigned(p[0]) | unsigned(p[1]) << 8; };
    const unsigned cnt = u16(io + 8 + bg * 2);
    const unsigned size = cnt >> 14;
    const unsigned width = (size & 1) ? 512 : 256;
    const unsigned height = (size & 2) ? 512 : 256;
    const unsigned tx = (x + u16(io + 0x10 + bg * 4)) & (width - 1);
    const unsigned ty = (y + u16(io + 0x12 + bg * 4)) & (height - 1);
    const unsigned block = (tx / 256) + (ty / 256) * (width / 256);
    const unsigned map = ((cnt >> 8) & 31) * 0x800 + block * 0x800 + (((ty / 8) & 31) * 32 + ((tx / 8) & 31)) * 2;
    if (map + 1 >= 0x18000) return -1;
    const unsigned entry = u16(vram + map);
    const unsigned px = (entry & 0x400) ? 7 - (tx & 7) : tx & 7;
    const unsigned py = (entry & 0x800) ? 7 - (ty & 7) : ty & 7;
    const bool wide = (cnt & 0x80) != 0;
    const unsigned addr = ((cnt >> 2) & 3) * 0x4000 + (entry & 1023) * (wide ? 64 : 32)
                          + py * (wide ? 8 : 4) + (wide ? px : px / 2);
    if (addr >= 0x18000) return -1;
    unsigned idx = vram[addr];
    if (!wide) idx = (px & 1) ? idx >> 4 : idx & 15;
    if (!idx) return -1;
    if (!wide) idx |= (entry >> 12) * 16;
    return int(u16(pal + idx * 2));
}

inline bool ui_edge_matches(int color, const std::uint8_t* rgb);

inline bool ui_edge_visible(const std::uint8_t* io, const std::uint8_t* vram,
                            const std::uint8_t* pal, unsigned bg, int x, int y,
                            const std::uint8_t* rgb) {
    const int top = ui_edge_color(io, vram, pal, bg, x, y);
    if (top < 0) return false;
    if (ui_edge_matches(top, rgb)) return true;
    const unsigned bld = io[0x50] | (unsigned(io[0x51]) << 8);
    if (((bld >> 6) & 3) != 1 || !(bld & (1u << bg))) return false;
    const unsigned a = io[0x52] > 16 ? 16 : io[0x52];
    const unsigned b = io[0x53] > 16 ? 16 : io[0x53];
    const auto mixed_matches = [&](int bottom) {
        if (bottom < 0) return false;
        unsigned mixed = 0;
        for (int c = 0; c <= 2; c += 2) {
            unsigned v = (((top >> (c * 5)) & 31) * a + ((bottom >> (c * 5)) & 31) * b + 8) >> 4;
            mixed |= (v > 31 ? 31 : v) << (c * 5);
        }
        const unsigned tg = ((top >> 4) & 62) | (top >> 15);
        const unsigned bg6 = ((bottom >> 4) & 62) | (bottom >> 15);
        unsigned g = (tg * a + bg6 * b + 8) >> 4;
        mixed |= ((g > 63 ? 63 : g) >> 1) << 5;
        return ui_edge_matches(int(mixed), rgb);
    };
    const unsigned top_key = (io[8 + bg * 2] & 3) * 4 + bg;
    for (unsigned behind = 0; behind < ((io[0] & 7) == 1 ? 2u : 4u); ++behind) {
        if (!(io[1] & (1u << behind)) || !(bld & (1u << (8 + behind)))) continue;
        const unsigned key = (io[8 + behind * 2] & 3) * 4 + behind;
        if (key > top_key && mixed_matches(ui_edge_color(io, vram, pal, behind, x, y))) return true;
    }
    return (bld & 0x2000) && mixed_matches(pal[0] | (unsigned(pal[1]) << 8));
}

inline bool ui_edge_matches(int color, const std::uint8_t* rgb) {
    if (color < 0) return false;
    for (int c = 0; c < 3; ++c) {
        const int v = (color >> (c * 5)) & 31;
        const int expected = (v << 3) | (v >> 2);
        const int delta = int(rgb[c]) - expected;
        if (delta < -1 || delta > 1) return false;
    }
    return true;
}

// Only visible UI pixels qualify, including verified alpha combinations.
// Unmatched/covered pixels fail closed; isolated glyph runs cannot become rails.
inline void ui_edge_runs(const std::uint8_t* io, const std::uint8_t* vram,
                         const std::uint8_t* pal, const std::uint8_t* rgb,
                         int width, int left, int side, bool* rows) {
    for (int y = 0; y < 160; ++y) rows[y] = false;
    if (!io || !vram || !pal || !rgb || width <= 240 || left < 0 || left + 240 > width) return;
    const unsigned mode = io[0] & 7;
    if (mode > 1 || (io[0] & 0x80)) return;
    const unsigned mask = ws_ui_gate_layers(io, 0x400);
    const int x = side ? 239 : 0;
    for (int y = 0; y < 160; ++y) {
        const auto* pixel = rgb + (y * width + left + x) * 3;
        for (unsigned bg = 0; bg < (mode == 1 ? 2u : 4u); ++bg) {
            if ((mask & (1u << bg)) && ui_edge_visible(io, vram, pal, bg, x, y, pixel)) {
                rows[y] = true;
                break;
            }
        }
    }
    for (int start = 0; start < 160;) {
        if (!rows[start]) { ++start; continue; }
        int end = start;
        bool lit = false;
        while (end < 160 && rows[end]) {
            const auto* p = rgb + (end * width + left + x) * 3;
            lit |= p[0] > 64 || p[1] > 64 || p[2] > 64;
            ++end;
        }
        if (end - start < 8 || !lit)
            for (int y = start; y < end; ++y) rows[y] = false;
        start = end;
    }
}
} // namespace ffta
