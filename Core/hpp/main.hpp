#pragma once

#include <Shared/os.hpp>

#ifndef NSA_API
#	if NSA_USE_WINDOWS
#		define NSA_API extern "C" __declspec(dllexport)
#	else
#		define NSA_API extern "C"
#	endif
#endif

NSA_API void EntryPoint();