#pragma once

#include "core/PluginPath.hpp"
#include <sol/sol.hpp>

namespace ds::core_vm {
    // The Lua registry consumes an ordinary array, not sol's container userdata.
    inline auto list_plugin_packages(const std::string &plugins_dir) {
        return sol::as_table(ds::plugin::list_plugin_packages(plugins_dir));
    }
}// namespace ds::core_vm
