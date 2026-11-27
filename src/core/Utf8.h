#pragma once
#include <string_view>

namespace lv::detail {
inline bool ValidUtf8(std::string_view text) noexcept {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first < 0x80) { continue; }
        unsigned remaining = 0, code = 0, minimum = 0;
        if (first >= 0xC2 && first <= 0xDF) { remaining = 1; code = first & 0x1F; minimum = 0x80; }
        else if (first >= 0xE0 && first <= 0xEF) { remaining = 2; code = first & 0x0F; minimum = 0x800; }
        else if (first >= 0xF0 && first <= 0xF4) { remaining = 3; code = first & 0x07; minimum = 0x10000; }
        else { return false; }
        if (remaining > text.size()-i) { return false; }
        while (remaining--) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xC0) != 0x80) { return false; }
            code = (code << 6) | (next & 0x3F);
        }
        if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) { return false; }
    }
    return true;
}
} // namespace lv::detail
