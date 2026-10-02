#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

namespace numeric_api_identity {
    // MSVC x64 Lua 5.1 numeric wrappers share the conversion path but have
    // different success returns. Check both complete paths, including branches,
    // rather than using the common prologue to identify either API.
    constexpr size_t wrapper_size = 48;

    enum class Kind { Unknown,
                      IsNumber,
                      ToInteger };

    inline Kind classify(std::span<const uint8_t> code) {
        if (code.size() != wrapper_size) return Kind::Unknown;
        constexpr auto body = std::to_array<uint8_t>({0x48, 0x83, 0xec, 0x38, 0xe8, 0, 0, 0, 0,
                                                      0x83, 0x78, 0x08, 0x03, 0x74, 0x17,
                                                      0x48, 0x8d, 0x54, 0x24, 0x20, 0x48, 0x8b, 0xc8,
                                                      0xe8, 0, 0, 0, 0, 0x48, 0x85, 0xc0, 0x75, 0x05,
                                                      0x48, 0x83, 0xc4, 0x38, 0xc3});
        for (size_t i = 0; i < body.size(); ++i) {
            if ((i >= 5 && i <= 8) || (i >= 24 && i <= 27)) continue;
            if (code[i] != body[i]) return Kind::Unknown;
        }
        constexpr auto predicate = std::to_array<uint8_t>({0xb8, 1, 0, 0, 0, 0x48, 0x83, 0xc4, 0x38, 0xc3});
        constexpr auto integer = std::to_array<uint8_t>({0xf2, 0x48, 0x0f, 0x2c, 0x00, 0x48, 0x83, 0xc4, 0x38, 0xc3});
        if (std::ranges::equal(code.subspan(body.size()), predicate)) return Kind::IsNumber;
        if (std::ranges::equal(code.subspan(body.size()), integer)) return Kind::ToInteger;
        return Kind::Unknown;
    }

    inline uintptr_t callee(std::span<const uint8_t> code, uintptr_t address, size_t call) {
        int32_t displacement;
        std::memcpy(&displacement, code.data() + call + 1, sizeof(displacement));
        return address + call + 5 + static_cast<intptr_t>(displacement);
    }

    // Read supplies bounded code from the executable mapping. Swap the complete
    // records so cached pattern/offset relations stay attached to their function.
    // Unrecognized compiler layouts and already correct pairs are unchanged.
    template<class Map, class Read>
    bool repair(Map &funcs, uintptr_t base, Read read) {
        auto predicate = funcs.find("lua_isnumber");
        auto integer = funcs.find("lua_tointeger");
        if (predicate == funcs.end() || integer == funcs.end() ||
            predicate->second.offset == 0 || integer->second.offset == 0 ||
            predicate->second.offset == integer->second.offset) return false;
        if (predicate->second.offset > std::numeric_limits<uintptr_t>::max() - base ||
            integer->second.offset > std::numeric_limits<uintptr_t>::max() - base) return false;
        const auto paddr = base + predicate->second.offset;
        const auto iaddr = base + integer->second.offset;
        const auto p = read(paddr);
        const auto i = read(iaddr);
        if (classify(p) != Kind::ToInteger || classify(i) != Kind::IsNumber) return false;
        if (callee(p, paddr, 4) != base || callee(i, iaddr, 4) != base ||
            callee(p, paddr, 23) != callee(i, iaddr, 23)) return false;
        std::swap(predicate->second, integer->second);
        return true;
    }
}// namespace numeric_api_identity
