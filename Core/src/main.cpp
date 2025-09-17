#include <main.hpp>
#include <socket.hpp>
#include <event.hpp>

#include <WS2tcpip.h>
#include <filesystem>
#include <print>
#include <string>

#include <Shared/dll.hpp>
#include <Shared/exports.hpp>
#include <Shared/process.hpp>

#include <Modules/OS/hpp/process.hpp>

CORE_API void EntryPoint() {
    auto procs = NSA::Shared::Process::Process::GetProcesses();
    auto curProc = GetCurrentProcess();
    std::string curName = NSA::Shared::Process::GetProcessNameFromHandle(curProc);

    auto it = std::ranges::find_if(procs, [&](const NSA::Shared::Process::Process& p) {
        return p.GetName() == curName && p.GetPID() != NSA::Shared::Process::GetProcessPID(curProc);
    });

    namespace Socket = NSA::Core::Socket;
    
    if (false) {
        Socket::ClientSocket sock;
        sock.OnConnect = [](Socket::ClientSocket::on_connect_t& event) {
            std::println("Connected to {}:{}", event.host, event.port);
        };
        sock.OnData = [](Socket::ClientSocket::on_data_t& event) {
            std::println("Received data: {}", event.data);
        };

        sock.Create();
        sock.Connect("127.0.0.1", 12345);
        sock.Send("Hello, World!");
    } else {
        Socket::ServerSocket sock;
        sock.OnConnect = [&](Socket::ServerSocket::on_connect_t& event) {
			if (!event.client) return;
            std::println("Client connected -> {}:{}", event.client->GetHost(), event.client->GetPort());
			event.client->Send("Welcome to the server!\n");
			sock.Send("A new client has connected!\n", event.client);
        };
        sock.OnData = [](Socket::ServerSocket::on_data_t& event) {
            if (!event.client) return;
            std::println("Received data from client {}:{} -> {}",
                event.client->GetHost(),
                event.client->GetPort(),
                event.data
            );
        };
        sock.OnListening = [](Socket::ServerSocket::on_listening_t& event) {
            std::println("Server listening on {}:{}", event.host, event.port);
		};
        sock.Create();
        sock.Listen("127.0.0.1", 12345);
    }
    /*
    if (it != procs.end()) {
        // Found another process with the same name

    } else {

    }
    */

	while (true) {
		Sleep(1000);
        ;
	}
}