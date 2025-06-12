#include <Shared/path.hpp>
#include <cstdio>
#include <Shared/dll.hpp>
#include <iostream>
#include <format>

using EntryPointFunction_t = void(*)();

int main() {
    auto ROOT_PATH = NSA::Shared::GetExecutablePath().parent_path();
    auto modulesPath = NSA::Shared::PathJoin(ROOT_PATH, "modules");

    auto CORE_PATH = NSA::Shared::PathJoin(ROOT_PATH, "Core.dll");

    NSA::Shared::DynamicLibrary CoreLib;
    if (!CoreLib.Open(CORE_PATH.string())) {
        std::cout << std::format("Failed to load {}: {}\n",
            CORE_PATH.string(),
            CoreLib.GetError()
        );
		return 1;
    }

    auto EntryPoint = CoreLib.GetFunction<EntryPointFunction_t>("EntryPoint");
    if (!EntryPoint) {
        std::cout << "Failed to find EntryPoint function in the module!\n";
        return 1;
	}

    EntryPoint();
    return 0;
}