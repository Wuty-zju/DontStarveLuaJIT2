#include "core/StartupGuard.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>

static void require(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main() {
    namespace fs = std::filesystem;
    using Result = ds::StartupGuard::Result;
    const auto root = fs::temp_directory_path() /
                      ("ds-startup-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto native = root / "game/data/unsafedata/luajit_crash.json";
    const auto mapped = root / "save/unsafedata/luajit_crash.json";
    fs::create_directories(mapped.parent_path());
    ds::StartupGuard first;
    require(first.arm(native, "session-1") == Result::allowed, "fresh startup rejected");
    std::ofstream(mapped).close();// Beta Lua clears a different physical file.
    ds::StartupGuard pending;
    require(pending.arm(native, "session-2") == Result::blocked, "unconfirmed startup not protected");
    require(first.confirm_healthy(), "native health confirmation failed");
    require(fs::file_size(native) == 0, "native marker not cleared");
    require(first.confirm_healthy(), "confirmation not idempotent");
    ds::StartupGuard second;
    require(second.arm(native, "session-2") == Result::allowed, "second cold startup rejected");
    std::ofstream(native) << "another-session";
    require(!second.confirm_healthy(), "cleared another session's state");
    require(fs::file_size(native) != 0, "foreign state lost");
    std::ofstream(native) << "{1}";
    ds::StartupGuard legacy;
    require(legacy.arm(native, "session-3") == Result::blocked, "legacy evidence discarded");
    fs::rename(native, native.string() + ".backup");
    require(legacy.arm(native, "session-3") == Result::allowed, "installer recovery failed");
    require(legacy.confirm_healthy(), "recovered startup not confirmed");
    const auto file_parent = root / "not-a-directory";
    std::ofstream(file_parent) << "occupied";
    ds::StartupGuard invalid;
    require(invalid.arm(file_parent / "marker", "session-4") == Result::io_error, "IO error not reported");
    fs::remove_all(root);
    std::cout << "PASS: beta mapping, repeated startup, ownership, recovery, IO errors\n";
}
