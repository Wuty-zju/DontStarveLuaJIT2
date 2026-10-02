#include "plugins/plugin_core_vm/signature_load/NumericApiIdentity.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

using namespace numeric_api_identity;

static void check(bool value) {
    if (!value) {
        std::fputs("numeric API identity regression failed\n", stderr);
        std::exit(1);
    }
}

// Complete x64 Lua numeric wrapper paths. Relative calls are synthesized so
// the test checks relocation and caller identity without shipping a game sample.
static std::array<uint8_t, wrapper_size> wrapper(Kind kind, uintptr_t address, uintptr_t base) {
    auto code = std::to_array<uint8_t>({0x48, 0x83, 0xec, 0x38, 0xe8, 0, 0, 0, 0,
                                        0x83, 0x78, 0x08, 0x03, 0x74, 0x17,
                                        0x48, 0x8d, 0x54, 0x24, 0x20, 0x48, 0x8b, 0xc8,
                                        0xe8, 0, 0, 0, 0, 0x48, 0x85, 0xc0, 0x75, 0x05,
                                        0x48, 0x83, 0xc4, 0x38, 0xc3,
                                        0xb8, 1, 0, 0, 0, 0x48, 0x83, 0xc4, 0x38, 0xc3});
    for (const auto call: {size_t{4}, size_t{23}}) {
        const auto target = call == 4 ? base : base + 0x800;
        const auto displacement = static_cast<int32_t>(target - (address + call + 5));
        std::memcpy(code.data() + call + 1, &displacement, sizeof(displacement));
    }
    if (kind == Kind::ToInteger) {
        constexpr uint8_t convert[] = {0xf2, 0x48, 0x0f, 0x2c, 0x00};
        std::copy(std::begin(convert), std::end(convert), code.begin() + 38);
    }
    return code;
}

struct Record {
    uintptr_t offset;
    std::string pattern;
    int pattern_offset;
};

static void test_pair(uintptr_t base) {
    auto p = wrapper(Kind::IsNumber, base + 0x100, base);
    auto i = wrapper(Kind::ToInteger, base + 0x300, base);
    auto read = [&](uintptr_t address) -> std::span<const uint8_t> {
        if (address == base + 0x100) return p;
        if (address == base + 0x300) return i;
        return {};
    };
    std::unordered_map<std::string, Record> map{
            {"lua_isnumber", {0x300, "integer", -9}},
            {"lua_tointeger", {0x100, "predicate", -3}}};
    check(repair(map, base, read));
    check(map.at("lua_isnumber").offset == 0x100);
    check(map.at("lua_isnumber").pattern == "predicate");
    check(map.at("lua_isnumber").pattern_offset == -3);
    check(map.at("lua_tointeger").offset == 0x300);
    check(map.at("lua_tointeger").pattern == "integer");
    check(map.at("lua_tointeger").pattern_offset == -9);
    check(!repair(map, base, read));
    std::swap(map.at("lua_isnumber"), map.at("lua_tointeger"));
    i[24] ^= 1;// Conversion helpers disagree despite matching body shapes.
    check(!repair(map, base, read));
    i = wrapper(Kind::ToInteger, base + 0x300, base + 1);
    check(!repair(map, base, read));// Neither calls the known index2adr anchor.
    map.erase("lua_tointeger");
    check(!repair(map, base, read));
}

int main() {
    auto code = wrapper(Kind::IsNumber, 0x1100, 0x1000);
    check(classify(code) == Kind::IsNumber);
    check(classify(std::span{code}.first(38)) == Kind::Unknown);
    // Every non-relocation byte belongs to a checked path or success return.
    for (size_t byte = 0; byte < code.size(); ++byte) {
        if ((byte >= 5 && byte <= 8) || (byte >= 24 && byte <= 27)) continue;
        auto corrupted = code;
        corrupted[byte] ^= 0x80;
        check(classify(corrupted) == Kind::Unknown);
    }
    check(classify(wrapper(Kind::ToInteger, 0x1300, 0x1000)) == Kind::ToInteger);
    test_pair(0x140000000);
    test_pair(0x7ff700000000);// Relocation must not depend on a fixed image base.
    std::puts("PASS numeric_api_identity: full paths, pair repair, cache records, relocation, rejection");
}
