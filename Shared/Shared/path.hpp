#pragma once

#include <Shared/os.hpp>
#include <string>
#include <filesystem>
#include <type_traits>
#include <initializer_list>
#include <concepts>

namespace fs = std::filesystem;

namespace NSA::Shared {
    inline fs::path GetExecutablePath() noexcept {
#if NSA_USE_WINDOWS
        char buffer[MAX_PATH];
        auto length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

        if (length == 0 || length == MAX_PATH) {
            return {};
        }
#else
        char buffer[PATH_MAX];
        ssize_t length = readlink("/proc/self/exe", buffer, PATH_MAX);

        if (length == -1) {
            return {};
        }
#endif
        std::string out(buffer, length);
        return fs::path(out);
    }

    inline fs::path PathJoin(std::initializer_list<fs::path> list) noexcept {
        fs::path out;

        for (auto& path : list) {
            out /= path;
        }

        return out;
    }

    template <std::convertible_to<fs::path>... T>
    inline fs::path PathJoin(T... args) noexcept {
        fs::path out;

        ((out /= args), ...);

        return out;
    }
}