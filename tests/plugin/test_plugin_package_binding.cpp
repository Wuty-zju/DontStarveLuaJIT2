#include "plugins/plugin_core_vm/injector/GameInjectorPackages.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    namespace fs = std::filesystem;
    const auto temp = fs::temp_directory_path() /
                      ("ds-package-binding-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto root = temp / fs::path(u8"\u6d4b\u8bd5");
    for (const auto *stem: {"plugin_z", "plugin_a"}) {
        fs::create_directories(root / stem);
        std::ofstream(root / stem / "modinfo.lua") << "-- package marker";
    }
    fs::create_directories(root / "plugin_native_only");
    fs::create_directories(root / "other");
    std::ofstream(root / "other" / "modinfo.lua") << "-- not a plugin";

    int exit_code = 0;
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string);
        auto injector = lua.create_named_table("GameInjector");
        injector.set_function("DS_LUAJIT_list_plugin_packages", &ds::core_vm::list_plugin_packages);
        const auto utf8 = root.generic_u8string();
        lua["plugins_dir"] = std::string(utf8.begin(), utf8.end());
        lua["repo_root"] = fs::path(argv[1]).generic_string();
        const auto result = lua.safe_script(R"(
            local list = GameInjector.DS_LUAJIT_list_plugin_packages
            local ok, packages = pcall(list, plugins_dir)
            assert(ok and type(packages) == "table")
            assert(#packages == 2 and packages[1] == "plugin_a" and packages[2] == "plugin_z")
            local count = 0
            for _, stem in ipairs(packages) do assert(type(stem) == "string"); count = count + 1 end
            assert(count == 2)
            for _, path in ipairs({"", plugins_dir .. "/missing"}) do
                local empty = list(path)
                assert(type(empty) == "table" and next(empty) == nil)
            end
            collectgarbage("collect")
            assert(packages[2] == "plugin_z")

            -- Exercise the real Lua consumer with the real native binding.
            -- No MODROOT means no packages, but flat lifecycle plugins still load.
            package.path = repo_root .. "/Mod/?.lua;" .. package.path
            local registry = require("plugins.init")
            local runtime
            for _, plugin in ipairs(registry) do
                if plugin.id == "jit.runtime" then runtime = plugin end
            end
            assert(runtime, "package discovery disabled the JIT lifecycle plugin")
            for _, enabled in ipairs({true, false}) do
                local on, off, callbacks = 0, 0, {}
                local fake_jit = {off = function() off = off + 1 end, on = function() on = on + 1 end}
                local ctx = {has_luajit = true, jit = fake_jit,
                    config = {EnabledJIT = enabled, HideGlobalJIT = false}}
                package.loaded["jit.opt"] = {start = function() end}
                AddSimPostInit = function(fn) callbacks[#callbacks + 1] = fn end
                runtime.load(ctx)
                assert(off == 1 and on == 0)
                for _, callback in ipairs(callbacks) do callback() end
                assert(on == (enabled and 1 or 0))
            end
        )",
                                            sol::script_pass_on_error);
        if (!result.valid()) {
            const sol::error error = result;
            std::cerr << error.what() << '\n';
            exit_code = 1;
        }
    }
    fs::remove_all(temp);
    return exit_code;
}
