#include "ffta_readable_font.h"
#include <cstdio>
#include <array>

int main() {
    using namespace ffta;
    int failures = 0;
    const auto check = [&](bool ok) { if (!ok) ++failures; };
    const auto pixel = [](int glyph, int x, int y) {
        const int index = glyph * 128 + (x / 8) * 64 + y * 4 + (x % 8) / 2;
        return (readable_font::kPixels[index] >> ((x & 1) * 4)) & 15;
    };
    std::array<std::array<std::uint16_t, 16>, 144> bodies{};
    for (int glyph = 0; glyph < 144; ++glyph) {
        int ink = 0;
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
            const auto p = pixel(glyph, x, y);
            check(p == 0 || p == 2 || p == 3);
            if (p) check(x < readable_font::kWidths[glyph]);
            if (p == 3) { ++ink; bodies[glyph][y] |= 1u << x; }
        }
        check(ink > 0);
    }
    for (int profile = 0; profile < 2; ++profile) {
        const int base = profile * 72;
        for (int i = 0; i < 66; ++i)
            for (int j = i + 1; j < 66; ++j) check(bodies[base + i] != bodies[base + j]);
        // K/k retain an unbroken upright, unlike the crossing X/x.
        for (int y = 3; y < 12; ++y) check((bodies[base + 11][y] & 1u) != 0);
        for (int y = 5; y < 12; ++y) check((bodies[base + 44][y] & 1u) != 0);
    }
    std::uint32_t value = 0xdeadbeefu;
    const auto start = readable_font::kAddress;
    check(readable_font_read(0x03005F04, start, 4, 0, &value) == 1);
    check(value == (std::uint32_t(readable_font::kPixels[0]) |
                   std::uint32_t(readable_font::kPixels[1]) << 8 |
                   std::uint32_t(readable_font::kPixels[2]) << 16 |
                   std::uint32_t(readable_font::kPixels[3]) << 24));
    for (auto pc : {0x03005F12u, 0x08A38AC6u})
        check(readable_font_read(pc, start + 63, 1, 0, &value) == 1);
    check(readable_font_read(0x08013454, start, 4, 0, &value) == 0);
    check(readable_font_read(0x03005F04, start - 1, 4, 0, &value) == 0);
    check(readable_font_read(0x03005F04, start + 18431, 4, 0, &value) == 0);
    check(readable_font_read(0x03005F04, start + 18432, 1, 0, &value) == 0);
    check(readable_font_read(0x03005F04, start, 3, 0, &value) == 0);
    std::printf("readable_font: %s (%d failures)\n", failures ? "FAIL" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
