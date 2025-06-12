#pragma once

#include <Shared/os.hpp>
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
    protected:
		HandleType m_handle{ nullptr };
    public:
        DynamicLibrary() noexcept {}
        DynamicLibrary(const std::string_view& path) noexcept { Open(path); }
		DynamicLibrary(const DynamicLibrary&) = delete;

        ~DynamicLibrary() noexcept { Close(); }

        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		template <typename T>
        T GetFunction(const std::string_view& name) const noexcept {
            assert(m_handle && "DynamicLibrary is not open");
            assert(!name.empty() && "Function name cannot be empty");

            ProcType proc = nullptr;
#if NSA_USE_WINDOWS
			proc = GetProcAddress(m_handle, name.data());
#else
			proc = dlsym(m_handle, name.data());
#endif
            return reinterpret_cast<T>(proc);
        }

        bool Open(const std::string_view& path) noexcept {
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

        std::string GetError() const noexcept {
#if NSA_USE_WINDOWS
            //Get the error message ID, if any.
            DWORD errorMessageID = ::GetLastError();
            if (errorMessageID == 0) {
                return {}; //No error message has been recorded
            }

            char* messageBuffer = nullptr;

            //Ask Win32 to give us the string version of that message ID.
            //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
            std::size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                errorMessageID,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&messageBuffer),
                0,
                nullptr
            );

            //Copy the error message into a std::string.
            std::string message(messageBuffer, size);

            //Free the Win32's string's buffer.
            LocalFree(messageBuffer);
#else
            std::string message(dlerror());
#endif

            return message;
        }

        void Close() noexcept {
			if (!m_handle) return;

			// Free the library handle
#if NSA_USE_WINDOWS
            FreeLibrary(m_handle);
#else
			dlclose(m_handle);
#endif
			m_handle = nullptr;
        }
    private:
    };
}