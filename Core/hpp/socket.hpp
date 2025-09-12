#pragma once

#include <WinSock2.h>
#include <mswsock.h>

#include <string>
#include <optional>
#include <vector>
#include <string_view>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>

#include <event.hpp>

namespace NSA::Core::Socket {
	class Socket;

	namespace IOCP {
		enum class IOOperation : std::uint8_t {
			NONE = 0,
			ACCEPT,
			RECV,
			SEND,
			CONNECT
		};

		struct IOContext {
			IOContext() noexcept;

			WSAOVERLAPPED overlapped; 
			WSABUF wsabuf;
			std::vector<char> buffer;
			IOOperation operation = IOOperation::NONE;
			Socket* owner = nullptr;
			SOCKET acceptSocketForAcceptEx = INVALID_SOCKET;
		};
		constexpr std::string_view ToString(IOOperation op) noexcept {
			switch (op) {
				case IOOperation::ACCEPT:  return "ACCEPT";
				case IOOperation::RECV:    return "RECV";
				case IOOperation::SEND:    return "SEND";
				case IOOperation::CONNECT: return "CONNECT";
				default: return "NONE";
			}
		}
	}

	class Socket {
	public:
		using SockType = SOCKET;

		enum class AddressFamily : std::uint8_t {
			IPV4 = AF_INET,
			IPV6 = AF_INET6,
			UNSPECIFIED = AF_UNSPEC
		};
		enum class SocketType : std::uint8_t {
			TCP = SOCK_STREAM,
			UDP = SOCK_DGRAM,
			RAW = SOCK_RAW
		};
	public:
		Socket() noexcept;

		Socket(SockType&& socket) noexcept;
		Socket(Socket&& socket) noexcept;

		Socket(const SockType& socket) = delete;
		Socket(const Socket& socket) = delete;

		virtual ~Socket() noexcept;

		bool Create(
			AddressFamily family = AddressFamily::IPV4,
			SocketType type = SocketType::TCP
		) noexcept;
		bool Close() noexcept;
		bool IsOpen() const noexcept;

		SockType GetSocket() const noexcept;
		static std::uint64_t GetShutdownKey() noexcept { return gs_shutdownKey; }

		friend void swap(Socket& lhs, Socket& rhs) noexcept;
	protected:
		virtual void OnIOCompleted(
			IOCP::IOContext* ctx,
			std::uint32_t bytesTransferred,
			std::uint32_t error
		) noexcept = 0;

		bool AssociateIOCP() const noexcept;

		static LPFN_CONNECTEX GetConnectExPtr(SOCKET sock) noexcept;
		static LPFN_ACCEPTEX GetAcceptExPtr(SOCKET sock) noexcept;

		bool PostSend(std::unique_ptr<IOCP::IOContext>& ctx, std::string_view data) noexcept;
		bool PostRecv(std::unique_ptr<IOCP::IOContext>& ctx) noexcept;

	private:
		static void* GetWinsockFunctionPtr(SOCKET sock, GUID guid) noexcept;
		static DWORD WINAPI IOCPWorkerThread(LPVOID param) noexcept;
	protected:
		SockType m_socket;
		std::vector<std::string> m_addresses;
	private:
		static HANDLE gs_globalIOCP;
		static std::vector<HANDLE> gs_workers;
		static std::mutex gs_globalMutex;
		static std::atomic<std::uint32_t> gs_socketCount;
		static std::atomic<bool> gs_workersRunning;
		static const std::uint64_t gs_shutdownKey;
	};

	class ClientSocket : public Socket {
	public:
		struct on_connect_t : public Event::event_t {
			std::string_view host;
			std::uint32_t port;

			constexpr on_connect_t(std::string_view host, std::uint32_t port)
				noexcept : host(host), port(port) {}
		};
		struct on_data_t : public Event::event_t {
			std::string data;
			constexpr on_data_t(std::string data) noexcept : data(data) {}
			constexpr on_data_t(const char* data) noexcept : data(data) {}
			constexpr on_data_t(const char* data, std::size_t length) noexcept
				: data(data, length) {}
		};
	public:
		ClientSocket() noexcept;
		ClientSocket(Socket::SockType&& socket) noexcept;

		bool Connect(const std::string_view& host, std::uint32_t port) noexcept;

		bool Send(const std::string_view& data) noexcept;
		bool Recv(Event::Event<on_data_t> callback) noexcept;
		bool Recv() noexcept;

		Event::Event<on_connect_t> OnConnect;
		Event::Event<on_data_t> OnData;
	protected:
		void OnIOCompleted(
			IOCP::IOContext* ctx,
			std::uint32_t bytesTransferred,
			std::uint32_t error
		) noexcept override;
	private:
		std::vector<std::unique_ptr<IOCP::IOContext>> m_postedCtx;
	};

	class ServerSocket : public Socket {
	public:
		struct on_listening_t : public Event::event_t {
			std::string_view host;
			std::uint32_t port;

			constexpr on_listening_t(std::string_view host, std::uint32_t port)
				noexcept : host(host), port(port) {}
		};

	public:
		bool Listen(const std::string_view& host, std::uint32_t port) noexcept;
			
		void Accept() noexcept;

		Event::Event<on_listening_t> OnListening;
	private:
		std::vector<ClientSocket> m_clients;
	};
}