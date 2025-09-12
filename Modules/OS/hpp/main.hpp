#pragma once

#include <Shared/os.hpp>

#include <Modules/OS/hpp/process.hpp>

#include <vector>

#ifdef NSA_OS_EXPORT
#	define OS_API __declspec(dllexport)
#else
#	define OS_API __declspec(dllimport)
#endif

namespace NSA::Modules::OS {
	//OS_API std::vector<Process> ListProcesses() noexcept;
}