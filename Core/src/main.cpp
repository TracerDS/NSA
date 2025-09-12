#include <main.hpp>
#include <socket.hpp>
#include <event.hpp>

#include <WS2tcpip.h>
#include <filesystem>
#include <print>

#include <Shared/dll.hpp>
#include <Shared/exports.hpp>
#include <Shared/process.hpp>

#include <Modules/OS/hpp/process.hpp>

#include <iostream>
#include <string>

CORE_API void EntryPoint() {
    auto procs = NSA::Shared::Process::Process::GetProcesses();
    auto curProc = GetCurrentProcess();
    std::string curName = NSA::Shared::Process::GetProcessNameFromHandle(curProc);

    auto it = std::ranges::find_if(procs, [&](const NSA::Shared::Process::Process& p) {
        return p.GetName() == curName && p.GetPID() != NSA::Shared::Process::GetProcessPID(curProc);
    });

    namespace Socket = NSA::Core::Socket;

    Socket::ClientSocket sock;
    sock.OnConnect = [](Socket::ClientSocket::on_connect_t& event) {
        std::println("Connected to {}:{}", event.host, event.port);
    };
    sock.OnData = [](Socket::ClientSocket::on_data_t& event) {
        std::println("Received data: {}", event.data);
	};
    if (!sock.Create()) {
        std::println("Failed to create socket");
		return;
    }
    if (!sock.Connect("127.0.0.1", 12345)) {
		std::println("Failed to connect to server");
        return;
    }
    if (!sock.Recv()) {
        std::println("Failed to post recv");
		return;
    }
    if (!sock.Send("Hello, World!")) {
		std::println("Failed to send data");
        return;
    }
    sock.Close();

    /*
    if (it != procs.end()) {
        // Found another process with the same name

        Socket::ClientSocket sock;
        sock.OnConnect = [](Socket::ClientSocket::on_connect_t& event) {
            std::println("Connected to {}:{}", event.host, event.port);
		};

        sock.Create();
        sock.Connect("127.0.0.1", 12345);
        sock.Close();
    } else {
        Socket::ServerSocket sock;
        sock.OnListening = [](Socket::ServerSocket::on_listening_t& event) {
            std::println("Now listening on {}:{}", event.host, event.port);
        };

        sock.Create();
        sock.Listen("127.0.0.1", 12345);
        //sock.AcceptClients();
    }
    */

	while (true) {
		Sleep(1000);
        break;
	}
}