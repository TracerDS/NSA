#pragma once

#ifdef _WIN32
#   include <windows.h>
#	define NSA_USE_WINDOWS 1
#	define NSA_USE_LINUX 0
#else
#   include <unistd.h>
#	include <linux/limits.h>
#	include <dlfcn.h>
#	define NSA_USE_WINDOWS 0
#	define NSA_USE_LINUX 1
#endif