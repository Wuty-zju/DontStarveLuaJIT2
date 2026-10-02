#include "config/sources/LuajitConfigFile.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

static std::filesystem::path game_root;
std::filesystem::path getExePath() { return game_root / "bin64/game"; }

static void require(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main() {
    namespace fs = std::filesystem;
    game_root = fs::temp_directory_path() /
                ("ds-config-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = game_root / "data/unsafedata/luajit_config.json";
    fs::create_directories(path.parent_path());
    std::ofstream(path) << R"({"AlwaysEnableMod":true,"DisableJITWhenServer":false})";
    auto legacy = luajit_config::read_from_file();
    require(legacy.has_value() && legacy->always_enable_mod && legacy->modmain_path.empty(),
            "missing modmain_path discarded valid legacy settings");
    luajit_config config;
    config.modmain_path = (game_root / "mods/Luajit/modmain.lua").generic_string();
    config.server_disable_luajit = true;
    config.always_enable_mod = true;
    require(config.write_to_file(), "native config write failed");
    auto saved = luajit_config::read_from_file();
    require(saved.has_value() && saved->modmain_path == config.modmain_path &&
                    saved->server_disable_luajit && saved->always_enable_mod,
            "native config roundtrip failed");
    std::ofstream(path) << "invalid JSON";
    require(!luajit_config::read_from_file(), "corrupt JSON accepted");
    require(!config.write_to_file(path / "child"), "invalid destination accepted");
    fs::remove_all(game_root);
    std::cout << "PASS: optional mod path, native config roundtrip, corrupt data and IO errors\n";
}
