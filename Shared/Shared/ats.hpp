#ifndef __ATS_H__
#define __ATS_H__

// ATS -> Auto Target System

#ifdef __MINGW32__
#	ifndef ATS_MINGW
#		define ATS_MINGW
#	endif
#	ifdef __MINGW64__
#		ifndef ATS_X64
#			define ATS_X64
#		endif
#	else
#		ifndef ATS_X86
#			define ATS_X86
#		endif
#	endif
#elifdef _WIN32
#	ifndef ATS_WINDOWS
#		define ATS_WINDOWS
#	endif
#	if defined(_WIN64) || defined(_M_AMD64) || defined(_M_X64)
#		ifndef ATS_X64
#			define ATS_X64
#		endif
#	else
#		ifndef ATS_X86
#			define ATS_X86
#		endif
#	endif
#elif defined(__linux__) || defined(__linux) || defined(linux)
#	ifndef ATS_LINUX
#		define ATS_LINUX
#	endif
#	ifdef __gnu_linux__
#		ifndef ATS_GLIBC
#			define ATS_GLIBC
#		endif
#	endif
#elif defined(__unix__) || defined(__unix) || defined(unix)
#	ifndef ATS_UNIX
#		define ATS_UNIX
#	endif
#elif defined(__APPLE__) || defined(__MACH__)
#	ifndef ATS_APPLE
#		define ATS_APPLE
#	endif
#endif

#ifdef __STDC_HOSTED__
#	ifndef ATS_HOSTED
#		define ATS_HOSTED
#	endif
#else
#	ifndef ATS_FREESTANDING
#		define ATS_FREESTANDING
#	endif
#endif

#if defined(__LP64__) || defined(_LP64)
#	ifndef ATS_64BIT
#		define ATS_64BIT
#	endif
#elif defined(__ILP32__) || defined(_ILP32)
#	ifndef ATS_32BIT
#		define ATS_32BIT
#	endif
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#	ifndef ATS_X64
#		define ATS_X64
#	endif
#elif defined(__i386__) || defined(__i386) || defined(i386) || defined(_X86_) \
	|| defined(__i686__) || defined(__i686) || defined(i686)
#	ifndef ATS_X86
#		define ATS_X86
#	endif
#endif

#ifdef __ELF__
#	ifndef ATS_ELF
#		define ATS_ELF
#	endif
#endif

#ifdef __clang__ // Is using Clang?
#	ifndef ATS_CLANG
#		define ATS_CLANG
#	endif
#	ifdef __cplusplus
#		ifndef ATS_CPP
#			define ATS_CPP
#		endif
#		ifndef ATS_CPP_VERSION
#			define ATS_CPP_VERSION __cplusplus
#		endif
#	else
#		ifndef ATS_C
#			define ATS_C
#		endif
#		ifndef ATS_C_VERSION
#			define ATS_C_VERSION __STDC_VERSION__
#		endif
#	endif
#elifdef __GNUC__ // Is using GCC?
#	ifndef ATS_GCC
#		define ATS_GCC
#	endif
#	ifdef __GNUG__
#		ifndef ATS_CPP
#			define ATS_CPP
#		endif
#		ifndef ATS_CPP_VERSION
#			define ATS_CPP_VERSION __cplusplus
#		endif
#	else
#		ifndef ATS_C
#			define ATS_C
#		endif
#		ifndef ATS_C_VERSION
#			define ATS_C_VERSION __STDC_VERSION__
#		endif
#	endif
#elifdef _MSC_VER // Is using MSVC?
#	ifndef ATS_MSVC
#		define ATS_MSVC
#	endif
#	ifdef _MSVC_LANG
#		ifndef ATS_CPP
#			define ATS_CPP
#		endif
#		ifndef ATS_CPP_VERSION
#			define ATS_CPP_VERSION _MSVC_LANG
#		endif
#	else
#		ifndef ATS_C
#			define ATS_C
#		endif
#		ifndef ATS_C_VERSION
#			define ATS_C_VERSION __STDC_VERSION__
#		endif
#	endif
#endif

#ifdef _DEBUG
#	define ATS_DEBUG
#elifdef NDEBUG
#	define ATS_RELEASE
#endif

#endif // __ATS_H__