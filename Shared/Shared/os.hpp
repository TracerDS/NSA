#pragma once

#include <Shared/ats.hpp>

#ifdef ATS_WINDOWS
#   include <windows.h>
#   include <psapi.h>
#else
#   include <unistd.h>
#	include <linux/limits.h>
#	include <dlfcn.h>
#endif

#if defined(ATS_WINDOWS) || defined(ATS_MINGW)
#   define NSA_USE_WINDOWS 1
#   define NSA_USE_LINUX 0
#   ifdef NSA_EXPORT
#       ifdef __GNUC__
#           define API __attribute__ ((dllexport))
#       else
#           define API __declspec(dllexport)
#       endif
#   else
#		ifdef __GNUC__
#			define API __attribute__ ((dllimport))
#		else
#			define API __declspec(dllimport)
#		endif
#   endif
#   define API_INTERNAL
#else
#   define NSA_USE_WINDOWS 0
#   define NSA_USE_LINUX 1
#   if __GNUC__ >= 4
#		define API __attribute__ ((visibility ("default")))
#		define API_INTERNAL  __attribute__ ((visibility ("hidden")))
#	else
#		define API 
#		define API_INTERNAL
#   endif
#endif