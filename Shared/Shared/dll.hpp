#pragma once

#include <Shared/os.hpp>
#include <Shared/utils.hpp>

#include <string_view>
#include <cassert>

namespace NSA::Shared {
    class DynamicLibrary {
    public:
#if NSA_USE_WINDOWS
		using HandleType = HMODULE;
        using ProcType = FARPROC;
#else
		using HandleType = void*;
        using ProcType = void*;
#endif
    public:
        DynamicLibrary() noexcept;
        DynamicLibrary(const std::string_view& path) noexcept;
		DynamicLibrary(const DynamicLibrary&) = delete;

        ~DynamicLibrary() noexcept;

        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        std::string GetError() const noexcept;

		template <typename T>
        T GetFunction(const std::string_view& name) const noexcept;

        bool Open(const std::string_view& path) noexcept;
        bool IsOpen() const noexcept;

        void Close() noexcept;

        template <typename T>
        static T GetFunction(HandleType handle, const std::string_view& name) noexcept;
    protected:
        HandleType m_handle{ nullptr };
    };

    DynamicLibrary::DynamicLibrary() noexcept {}
    DynamicLibrary::DynamicLibrary(const std::string_view& path) noexcept { Open(path); }
    DynamicLibrary::~DynamicLibrary() noexcept { Close(); }

    bool DynamicLibrary::Open(const std::string_view& path) noexcept {
        assert(!path.empty() && "Library path cannot be empty");

#if NSA_USE_WINDOWS
        m_handle = LoadLibraryA(path.data());
        if (!m_handle) {
            return false;
        }
#else
        m_handle = dlopen(path.data(), RTLD_LAZY);
        if (!m_handle) {
            return false;
        }
        dlerror();
#endif
        return true;
    }

    bool DynamicLibrary::IsOpen() const noexcept { return m_handle != nullptr; }

    template <typename T>
    T DynamicLibrary::GetFunction(const std::string_view& name) const noexcept {
        return DynamicLibrary::GetFunction<T>(m_handle, name);
    }

    template <typename T>
    T DynamicLibrary::GetFunction(HandleType handle, const std::string_view& name) noexcept {
        assert(handle && "DynamicLibrary handle is not valid");
        assert(!name.empty() && "Function name cannot be empty");

		ProcType proc = nullptr;
#if NSA_USE_WINDOWS
        proc = GetProcAddress(handle, name.data());
#else
        proc = dlsym(handle, name.data());
#endif
        return reinterpret_cast<T>(proc);
    }


    std::string DynamicLibrary::GetError() const noexcept {
#if NSA_USE_WINDOWS
		auto message = Utils::GetLastErrorString();
#else
        std::string message(dlerror());
#endif

        return message;
    }

    void DynamicLibrary::Close() noexcept {
        if (!m_handle) return;

        // Free the library handle
#if NSA_USE_WINDOWS
        FreeLibrary(m_handle);
#else
        dlclose(m_handle);
#endif
        m_handle = nullptr;
    }
}