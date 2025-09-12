#include <process.hpp>
#include <print>

namespace NSA::Modules::OS {
	/*
	Process::Process() noexcept : m_pid(-1), m_handle(nullptr) {}
	Process::Process(std::uint32_t pid) noexcept { Fetch(pid); }

	Process::~Process() noexcept {
		if (m_handle) {
			CloseHandle(m_handle);
			m_pid = -1;
			m_handle = nullptr;
		}
	}

	void Process::Fetch(const std::string_view& name) noexcept {
		
	}

	void Process::Fetch(std::uint32_t pid) noexcept {
		m_pid = pid;
		if (!m_handle) {
			// Try opening the process
			m_handle = OpenProcess(
				PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
				false,
				pid
			);

			if (!m_handle)
				return;
		}

		m_modules.clear();

		DWORD bytesNeeded = 0;
		DWORD arraySize = 256;

		std::vector<HMODULE> modules(arraySize);

		while (true) {
			if (!EnumProcessModules(
				m_handle,
				modules.data(),
				arraySize * sizeof(HMODULE),
				&bytesNeeded
			)) {
				CloseHandle(m_handle);
				return;
			}

			if (bytesNeeded < arraySize * sizeof(HMODULE)) {
				modules.resize(bytesNeeded / sizeof(HMODULE));
				break;
			}

			arraySize *= 2; // Increase buffer and retry
		}

		for (const auto& module : m_modules) {
			m_modules.emplace_back(this);
		}
	}

	std::vector<DWORD> ProcessList::GetProcessIDs() noexcept {
		std::vector<DWORD> processIds;
		DWORD bytesReturned = 0;
		DWORD arraySize = 1024;

		while (true) {
			std::vector<DWORD> temp(arraySize);
			if (!EnumProcesses(temp.data(), arraySize * sizeof(DWORD), &bytesReturned)) {
				return {};
			}

			if (bytesReturned < arraySize * sizeof(DWORD)) {
				temp.resize(bytesReturned / sizeof(DWORD));
				processIds.swap(temp);
				break;
			}

			arraySize *= 2;
		}

		return processIds;
	}

	std::vector<Process> ProcessList::FetchProcesses() noexcept {
		std::vector<DWORD> processes = ProcessList::GetProcessIDs();

		// Remove any invalid process identifiers
		std::erase_if(processes, [](DWORD pid) { return pid == 0; });
		std::sort(processes.begin(), processes.end());

		std::vector<Process> out;
		out.resize(processes.size());

		for (const auto& pid : processes) {
			out.emplace_back(static_cast<std::uint32_t>(pid));
		}
		return out;
	}

	Process::ProcessModule::ProcessModule(Process* process) noexcept
		: m_process(process) { Fetch(); }

	Process::ProcessModule::~ProcessModule() noexcept {
		if (m_handle) {
			CloseHandle(m_handle);
			m_handle = nullptr;
		}
	}

	void Process::ProcessModule::Fetch() noexcept {
		std::string baseName(MAX_PATH, '\0');
		DWORD size = 0;

		size = GetModuleBaseNameA(
			m_process->GetHandle(),
			m_handle,
			baseName.data(),
			MAX_PATH
		);
		baseName.resize(size);

		std::string fileName(MAX_PATH, '\0');
		size = GetModuleFileNameExA(
			m_process->GetHandle(),
			m_handle,
			fileName.data(),
			MAX_PATH
		);
		fileName.resize(size);
	}*/
}