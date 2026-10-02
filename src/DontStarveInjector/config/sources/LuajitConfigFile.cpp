#include "LuajitConfigFile.hpp"
#include "config/BaseOptionKeys.hpp"
#include "plugins/plugin_core_vm/VmOptionKeys.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

#include "util/platform.hpp"
static std::filesystem::path getGameDir() {
    static std::filesystem::path p = getExePath().parent_path().parent_path();
    return p;
}
namespace nlohmann {
    void to_json(json& j, const luajit_config& s) {
        using ds::config::keys::kAlwaysEnableMod;
        using ds::config::keys::kDisableJITWhenServer;
        using ds::config::keys::kModmainPath;
        j = json{{std::string{kModmainPath}, s.modmain_path},
                  {std::string{kDisableJITWhenServer}, s.server_disable_luajit},
                  {std::string{kAlwaysEnableMod}, s.always_enable_mod}};
    }

    void from_json(const json& j, luajit_config& s) {
        using ds::config::keys::kAlwaysEnableMod;
        using ds::config::keys::kDisableJITWhenServer;
        using ds::config::keys::kModmainPath;
        if (j.contains(std::string{kModmainPath})) {
            j.at(std::string{kModmainPath}).get_to(s.modmain_path);
        }

        if (j.contains(std::string{kDisableJITWhenServer})) {
            j.at(std::string{kDisableJITWhenServer}).get_to(s.server_disable_luajit);
        } else if (j.contains("server_disable_luajit")) {
            j.at("server_disable_luajit").get_to(s.server_disable_luajit);
        }

        if (j.contains(std::string{kAlwaysEnableMod})) {
            j.at(std::string{kAlwaysEnableMod}).get_to(s.always_enable_mod);
        } else if (j.contains("always_enable_mod")) {
            j.at("always_enable_mod").get_to(s.always_enable_mod);
        }
    }
}

std::optional<luajit_config> luajit_config::read_from_file(std::filesystem::path path) {
    if (path.empty()) {
        path = getGameDir() / "data" / "unsafedata" / "luajit_config.json";
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec)
        return std::nullopt;
    std::ifstream sf(path);
    if (!sf.is_open())
        return std::nullopt;
    try {
        nlohmann::json j;
        sf >> j;
        return j.get<luajit_config>();
    } catch (...) {
        return std::nullopt;
    }
}

bool luajit_config::write_to_file(std::filesystem::path path) const {
    if (path.empty()) {
        path = getGameDir() / "data" / "unsafedata" / "luajit_config.json";
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << nlohmann::json(*this).dump();
    output.close();
    return bool(output);
}
