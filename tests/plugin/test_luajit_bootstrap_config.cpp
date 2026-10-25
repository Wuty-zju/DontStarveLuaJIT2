#include "config/sources/LuajitConfigFile.hpp"
#include "core/PluginPath.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
static fs::path module_dir, state_dir;
namespace ds::plugin {
    fs::path injector_module_dir() { return module_dir; }
}// namespace ds::plugin
namespace ds::unsafedata {
    fs::path dir() { return state_dir; }
}// namespace ds::unsafedata
static void require(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
int main() {
    const auto root = fs::temp_directory_path() /
                      ("ds-bootstrap-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    state_dir = root / "save/unsafedata";
    fs::create_directories(state_dir);
    const auto file = state_dir / "luajit_config.json";
    const auto write = [&](const char *text) { std::ofstream(file) << text; };
    require(!luajit_config::read_from_file(), "missing config must remain absent");
    for (const auto &layout: {fs::path("mods/DontStarveLuaJit2"), fs::path("workshop/content/322330/3444078585"), fs::path(u8"mods/\u6d4b\u8bd5")}) {
        module_dir = root / layout;
        fs::create_directories(module_dir);
        std::ofstream(module_dir / "modmain.lua") << "--";
        write(R"({"AlwaysEnableMod":true,"DisableJITWhenServer":true})");
        const auto config = luajit_config::read_from_file();
        const auto utf8 = (module_dir / "modmain.lua").generic_u8string();
        require(config && config->always_enable_mod && config->server_disable_luajit,
                "missing path discarded legal options");
        require(config->modmain_path == std::string(utf8.begin(), utf8.end()), "loaded module path not used");
    }
    write(R"({"modmain_path":"Don't Starve Together/mods/custom/modmain.lua","always_enable_mod":true,"server_disable_luajit":false})");
    auto config = luajit_config::read_from_file();
    require(config && config->modmain_path == "Don't Starve Together/mods/custom/modmain.lua" && config->always_enable_mod && !config->server_disable_luajit,
            "explicit path or legacy keys lost");
    for (const auto *invalid: {R"({"modmain_path":42})", R"({"AlwaysEnableMod":"true"})", "{", "[]", "null"}) {
        write(invalid);
        require(!luajit_config::read_from_file(), "invalid config accepted");
    }
    module_dir = root / "missing";
    write(R"({"AlwaysEnableMod":true})");
    config = luajit_config::read_from_file();
    require(config && config->always_enable_mod && config->modmain_path.empty(), "invented absent mod path");
    module_dir.clear();
    require(ds::plugin::modmain_path_from_module_dir({}).empty(), "empty module root used working directory");
    fs::remove_all(root);
    return 0;
}
