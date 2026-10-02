#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace ds {

    // Native-only startup state. Lua must never resolve this path through game io.
    class StartupGuard {
    public:
        enum class Result { allowed,
                            blocked,
                            io_error };

        Result arm(const std::filesystem::path &path, std::string token) {
            if (armed_) return Result::allowed;
            std::error_code ec;
            const bool exists = std::filesystem::exists(path, ec);
            if (ec) return Result::io_error;
            if (exists) {
                std::ifstream input(path, std::ios::binary);
                if (!input) return Result::io_error;
                if (input.peek() != std::ifstream::traits_type::eof()) return Result::blocked;
                if (input.bad()) return Result::io_error;
            }
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec) return Result::io_error;
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output) return Result::io_error;
            output << token;
            output.close();
            if (!output) return Result::io_error;
            path_ = path;
            token_ = std::move(token);
            armed_ = true;
            return Result::allowed;
        }

        bool confirm_healthy() {
            if (!armed_) return true;// Steam/debug/server bypass, or already confirmed.
            std::ifstream input(path_, std::ios::binary);
            if (!input) return false;
            const std::string content{std::istreambuf_iterator<char>{input}, {}};
            if (input.bad() || content != token_) return false;// Never clear another session.
            input.close();
            std::ofstream output(path_, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            output.close();
            if (!output) return false;
            armed_ = false;
            return true;
        }

    private:
        std::filesystem::path path_;
        std::string token_;
        bool armed_ = false;
    };

}// namespace ds
