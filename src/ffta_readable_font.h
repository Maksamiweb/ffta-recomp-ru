#pragma once
#include "ffta_readable_glyphs.h"

namespace ffta {
// Font copy evidence: 0x08013452 scales the glyph by 128; 0x08013474
// holds 0x09000000. The selected copy function at 0x0836D4BC is
// 0x03005EE9, relocated from 0x08A38A9C (game.toml iwram_ram_helpers).
// Only its LDM/byte-load sites may substitute the authored Cyrillic slots.
inline int readable_font_read(std::uint32_t pc, std::uint32_t address,
                              std::uint32_t width, std::uint32_t original,
                              std::uint32_t* result) {
    (void)original;
    if (address < readable_font::kAddress ||
        address >= readable_font::kAddress + readable_font::kPixels.size()) return 0;
    pc &= ~1u;
    if (pc != 0x03005F04u && pc != 0x03005F12u &&
        pc != 0x08A38AB8u && pc != 0x08A38AC6u) return 0;
    if (!result || (width != 1 && width != 2 && width != 4)) return 0;
    const auto offset = address - readable_font::kAddress;
    if (offset + width > readable_font::kPixels.size()) return 0;
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i)
        value |= std::uint32_t(readable_font::kPixels[offset + i]) << (i * 8);
    *result = value;
    return 1;
}
} // namespace ffta
