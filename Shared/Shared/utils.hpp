#pragma once

#include <Shared/os.hpp>
#include <string>
#include <random>

namespace NSA::Shared::Utils {
    inline std::string GetLastErrorString(DWORD error) noexcept {
        if (error == 0)
            return {}; //No error message has been recorded

        char* messageBuffer = nullptr;

        // Ask Win32 to give us the string version of that message ID.
        // The parameters we pass in, tell Win32 to create the buffer that
        // holds the message for us (because we don't yet know how long the message string will be).
        std::size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer),
            0,
            nullptr
        );

        //Copy the error message into a std::string.
        std::string message(messageBuffer, size);

        //Free the Win32's string's buffer.
        LocalFree(messageBuffer);
        return message;
    }

    inline std::string GetLastErrorString() noexcept {
        //Get the error message ID, if any.
        return GetLastErrorString(::GetLastError());
    }

    template <std::integral T>
    inline T RandomInRange(T min, T max) noexcept {
        static std::mt19937 generator(std::random_device{}());
        if (min > max) {
            std::swap(min, max);
        }

        return std::uniform_int_distribution<T>(min, max)(generator);
    }

    template <std::floating_point T>
    inline T RandomInRange(T min, T max) noexcept {
        static std::mt19937 generator(std::random_device{}());
        if (min > max) {
            std::swap(min, max);
        }

        return std::uniform_real_distribution<T>(min, max)(generator);
    }

    template <std::integral T>
    inline std::optional<T> StringToInt(std::string_view str, int base = 10) noexcept {
		std::string temp(str);
        T out;
        try {
            out = static_cast<T>(std::stoll(temp, nullptr, base));
        } catch (...) {
            return std::nullopt;
        }
        return out;
	}
}