#pragma once

#include <Shared/os.hpp>
#include <Shared/dll.hpp>

#include <tlhelp32.h>
#include <winternl.h>

#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ntdll.lib")

#ifndef STATUS_INFO_LENGTH_MISMATCH
#   define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

namespace NSA::Shared::Process {
    class Process {
    public:
        Process() noexcept {}
        Process(
            std::uint32_t pid,
            std::string name,
            std::string basepath
        ) noexcept : m_pid(pid), m_name(name), m_basePath(basepath) {}

        Process& operator=(const std::string_view& name) noexcept;

        Process* GetParent() noexcept { return m_parent; }
        std::uint32_t GetPID() const noexcept { return m_pid; }
        const std::string& GetName() const noexcept { return m_name; }
        const std::string& GetBasePath() const noexcept { return m_basePath; }

        static Process GetProcessFromName(const std::string_view& name) noexcept;
        static std::vector<Process> GetProcesses() noexcept;
        static std::vector<Process> GetProcessesSorted() noexcept;
    protected:
        Process* m_parent{ nullptr };
        std::uint32_t m_pid = -1;
        std::string m_name{};
        std::string m_basePath{};
		std::vector<Process> m_children;
    private:
        void Fetch() noexcept;
    };

    Process& Process::operator=(const std::string_view& name) noexcept {
        GetProcesses();
        return *this;
    }

    void Process::Fetch() noexcept {
		using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(
            HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG
        );

        struct PROCESS_BASIC_INFORMATION {
            NTSTATUS ExitStatus;
            PPEB PebBaseAddress;
            ULONG_PTR AffinityMask;
            KPRIORITY BasePriority;
            ULONG_PTR UniqueProcessId;
            ULONG_PTR InheritedFromUniqueProcessId;
        };

		if (m_pid == -1) return;

		auto proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m_pid);
		if (!proc) return;

        HMODULE ntDll = GetModuleHandleA("ntdll.dll");
        if (ntDll) {
            auto NtQueryInformationProcess = DynamicLibrary::GetFunction<
                NtQueryInformationProcess_t
            >(ntDll, "NtQueryInformationProcess");

            PROCESS_BASIC_INFORMATION pbi;
            ULONG ReturnLength;

            auto status = NtQueryInformationProcess(
                proc,
                ProcessBasicInformation,
                &pbi,
                sizeof(pbi),
                &ReturnLength
            );
            if (status != 0)
                return;

            std::uint32_t parentPid = static_cast<std::uint32_t>(pbi.InheritedFromUniqueProcessId);
        }
    }

    inline std::string GetProcessNameFromHandle(HANDLE process) {
        char buffer[MAX_PATH];
        if (GetModuleBaseNameA(process, nullptr, buffer, MAX_PATH)) {
            return std::string(buffer);
        }
        return {};
	}
    inline HANDLE GetProcessFromPID(std::uint32_t pid) noexcept {
        return OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	}
    inline std::uint32_t GetProcessPID(HANDLE handle) noexcept {
        return GetProcessId(handle);
    }

    inline Process Process::GetProcessFromName(const std::string_view& name) noexcept {
		auto processes = GetProcesses();

        auto iter = std::ranges::find_if(processes, [&](const Process& proc) {
            return proc.GetName() == name;
		});
        if (iter != processes.end()) {
            return *iter;
        }
        return {};
    }

    inline std::vector<Process> Process::GetProcesses() noexcept {
        std::vector<Process> processes;

        PROCESSENTRY32 entry;
        entry.dwSize = sizeof(PROCESSENTRY32);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        if (Process32First(snapshot, &entry)) {
            while (Process32Next(snapshot, &entry)) {
                processes.emplace_back(entry.th32ProcessID, entry.szExeFile, "");
            }
        }

        CloseHandle(snapshot);
        return processes;
	}

    inline std::vector<Process> Process::GetProcessesSorted() noexcept {
        std::vector<Process> processes = GetProcesses();

        std::ranges::sort(processes, [](const Process& a, const Process& b) {
            return a.GetPID() < b.GetPID();
        });

        return processes;
    }
}