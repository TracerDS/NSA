#pragma once

#include <Shared/os.hpp>

#ifdef NSA_FFMPEG_EXPORT
#	define FFMPEG_API extern "C" __declspec(dllexport)
#else
#	define FFMPEG_API extern "C" __declspec(dllimport)
#endif