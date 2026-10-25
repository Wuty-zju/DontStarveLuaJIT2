#include "LuaApi.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define REQUIRE(condition)                                            \
    do {                                                              \
        if (!(condition)) {                                           \
            std::fprintf(stderr, "REQUIRE failed: %s\n", #condition); \
            std::abort();                                             \
        }                                                             \
    } while (false)

int main(int argc, char **argv) {
    REQUIRE(argc == 2);
#ifdef _WIN32
    auto module = LoadLibraryA(argv[1]);
    REQUIRE(module);
    const auto resolve = [&](const char *name) { return reinterpret_cast<void *>(GetProcAddress(module, name)); };
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    REQUIRE(module);
    const auto resolve = [&](const char *name) { return dlsym(module, name); };
#endif
    // Use the same name table as LoadAllInterfaces/GetLuaExport. Having an
    // export in the VM/reference alone does not put it in the engine dispatch.
    std::unordered_map<std::string_view, void *> dispatch;
#define BIND_API(name) dispatch[#name] = resolve(#name);
    LUA51_API_DEFINES(BIND_API);
#undef BIND_API
    auto newstate = reinterpret_cast<void *(*)()>(dispatch.at("luaL_newstate"));
    auto close = reinterpret_cast<void (*)(void *)>(dispatch.at("lua_close"));
    auto load = reinterpret_cast<int (*)(void *, const char *, size_t, const char *)>(dispatch.at("luaL_loadbuffer"));
    auto call = reinterpret_cast<int (*)(void *, int, int, int)>(dispatch.at("lua_pcall"));
    auto get = reinterpret_cast<const char *(*)(void *, int)>(dispatch.at("lua_getfuncchunkname"));
    auto set = reinterpret_cast<void (*)(void *, int, const char *)>(dispatch.at("lua_setfuncchunkname"));
    REQUIRE(newstate && close && load && call && get && set);
    void *state = newstate();
    REQUIRE(state);
    const char source[] = "return function() return function() return 42 end end";
    REQUIRE(load(state, source, sizeof(source) - 1, "@dispatch-test.lua") == 0);
    REQUIRE(get(state, -1) == nullptr);
    set(state, -1, "dispatch-test.lua");
    REQUIRE(call(state, 0, 1, 0) == 0);
    REQUIRE(get(state, -1) && std::strcmp(get(state, -1), "dispatch-test.lua") == 0);
    REQUIRE(call(state, 0, 1, 0) == 0);
    REQUIRE(get(state, -1) && std::strcmp(get(state, -1), "dispatch-test.lua") == 0);
    // A display/source name alone must never grant the protected marker.
    REQUIRE(load(state, source, sizeof(source) - 1, "@dispatch-test.lua") == 0);
    REQUIRE(get(state, -1) == nullptr);
    close(state);
#ifdef _WIN32
    FreeLibrary(module);
#else
    dlclose(module);
#endif
    std::puts("Lua provenance APIs dispatched through the engine API name table");
}
