#include "Progress.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#define REQUIRE(condition)                                            \
    do {                                                              \
        if (!(condition)) {                                           \
            std::fprintf(stderr, "REQUIRE failed: %s\n", #condition); \
            std::abort();                                             \
        }                                                             \
    } while (false)

static Generator<int> steps(bool fail, int &completed) {
    co_yield 0;
    set_progress(1, "signature progress regression");
    if (fail) throw std::runtime_error("expected signature failure");
    co_yield 1;
    ++completed;
}

static void require_clean_window_state() {
    REQUIRE(FindWindowA("ProgressWindowClass", nullptr) == nullptr);
    MSG message{};
    REQUIRE(!PeekMessage(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE));
}

int main() {
    int completed = 0;
    {
        auto generator = steps(false, completed);
        ShowProgressWindow(4, generator);
    }
    REQUIRE(completed == 1);
    require_clean_window_state();
    bool caught = false;
    try {
        auto generator = steps(true, completed);
        ShowProgressWindow(4, generator);
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught && completed == 1);
    require_clean_window_state();
    // Reopening after an error must not reuse a timer or coroutine pointer.
    {
        auto generator = steps(false, completed);
        ShowProgressWindow(4, generator);
    }
    REQUIRE(completed == 2);
    require_clean_window_state();
    std::puts("signature progress lifecycle passed");
}
