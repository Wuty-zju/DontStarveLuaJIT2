// L0 residual GAME_API: installer update + mod version + process hygiene.
// fps.render → plugin_fps_render; network.tick → plugin_network_tick.
#include "config/InjectorHostConfig.hpp"
#include "MemorySignature.hpp"
#include "util/inlinehook.hpp"
#include "config/sources/LuajitConfigFile.hpp"
#include "core/PluginPath.hpp"
#include <spdlog/spdlog.h>

#include <filesystem>
#include <frida-gum.h>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

DONTSTARVEINJECTOR_GAME_API int DS_LUAJIT_update(const char *mod_directory, int tt) {
    if (!mod_directory) {
        return 0;
    }
#ifdef _WIN32
    auto mod_dir = std::filesystem::path{mod_directory};
    if (!std::filesystem::exists(mod_dir)) {
        return 0;
    }
    mod_dir = std::filesystem::absolute(mod_dir);
    auto installer = mod_dir / "install.bat";
    if (!std::filesystem::exists(installer)) {
        return 0;
    }
    std::string cmd = std::format("cmd /C \"{}\" {}", installer.string(), tt == 1 ? "uninstall" : "");
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcess(NULL, (char *)cmd.c_str(), 0, 0, FALSE, CREATE_NEW_CONSOLE, 0, mod_directory, &si,
                      &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }
#endif
    return 0;
}

DONTSTARVEINJECTOR_GAME_API const char *DS_LUAJIT_get_mod_version() {
    return MOD_VERSION;
}

DONTSTARVEINJECTOR_GAME_API bool DS_LUAJIT_save_bootstrap_config(
    const char *modmain_path, bool disable_jit_when_server, bool always_enable_mod) {
    luajit_config config;
    config.modmain_path = modmain_path ? modmain_path : "";
    if (config.modmain_path.empty()) {
        const auto candidate = ds::plugin::injector_module_dir() / "modmain.lua";
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            config.modmain_path = candidate.generic_string();
        }
    }
    config.server_disable_luajit = disable_jit_when_server;
    config.always_enable_mod = always_enable_mod;
    const bool ok = config.write_to_file();
    if (!ok) spdlog::error("failed to save native luajit bootstrap config");
    return ok;
}

static void replace_set_thread_name() {
#ifdef _WIN32
    function_relocation::MemorySignature set_thread_name_func{"B9 88 13 6D 40", -0x24};
    if (set_thread_name_func.scan(gum_module_get_path(gum_process_get_main_module()))) {
        static auto set_thread_name = +[](uint32_t /*thread_id*/, const char *name) {
            if (name) {
                SetThreadDescription(GetCurrentThread(), std::filesystem::path{name}.c_str());
            }
        };
        Hook((uint8_t *)set_thread_name_func.target_address, (uint8_t *)set_thread_name);
    }
#endif
}

extern "C" void LoadGameModConfig() {
#ifdef _WIN32
    replace_set_thread_name();
#endif
}
