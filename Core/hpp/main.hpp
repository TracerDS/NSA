#pragma once

#include <Shared/os.hpp>

#ifdef NSA_CORE_EXPORT
#	define CORE_API extern "C" __declspec(dllexport)
#else
#	define CORE_API extern "C" __declspec(dllimport)
#endif