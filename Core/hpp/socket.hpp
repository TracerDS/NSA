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

			OVERLAPPED overlapped; 
			WSABUF wsabuf;
			std::vector<char> buffer;
			IOOperation operation = IOOperation::NONE;
			Socket* owner = nullptr;
		};
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
		std::string_view GetHost() const noexcept { return m_host; }
		std::uint32_t GetPort() const noexcept { return m_port; }

		static std::uint64_t GetShutdownKey() noexcept { return gs_shutdownKey; }

		static std::optional<std::pair<
			std::string, std::uint32_t
		>> GetSocketAddress(
			SockType sock
		) noexcept;

		friend void swap(Socket& lhs, Socket& rhs) noexcept;
	protected:
		virtual void OnIOCompleted(
			IOCP::IOContext* ctx,
			std::uint32_t bytesTransferred,
			std::uint32_t error
		) noexcept = 0;

		static void* GetWinsockFunctionPtr(SockType sock, GUID guid) noexcept;

		bool AssociateIOCP() const noexcept;
	private:
		static DWORD WINAPI IOCPWorkerThread(LPVOID param) noexcept;
	protected:
		constexpr static std::uint32_t MAX_PENDING_RECVS = 4;
		static std::vector<HANDLE> gs_workers;

		SockType m_socket;
		std::string m_host;
		std::uint32_t m_port;
		static std::mutex gs_bufferMutex;
	private:
		static HANDLE gs_globalIOCP;
		static std::mutex gs_globalMutex;
		static std::atomic<std::uint32_t> gs_socketCount;
		static std::atomic<bool> gs_workersRunning;
		static const std::uint64_t gs_shutdownKey;
	};

	class ClientSocket : public Socket {
	public:
		struct ClientContext : public IOCP::IOContext {};
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

		Event::Event<on_connect_t> OnConnect;
		Event::Event<on_data_t> OnData;
	protected:
		void OnIOCompleted(
			IOCP::IOContext* ctx,
			std::uint32_t bytesTransferred,
			std::uint32_t error
		) noexcept override;
	private:
		static LPFN_CONNECTEX GetConnectExPtr(SockType sock) noexcept;

		bool Recv() noexcept;
	private:
		std::vector<std::unique_ptr<ClientContext>> m_postedCtx;
	};

	class ServerSocket : public Socket {
	public:
		struct ServerContext : public IOCP::IOContext {
			ClientSocket* client;
		};
	public:
		struct on_listening_t : public Event::event_t {
			std::string_view host;
			std::uint32_t port;

			constexpr on_listening_t(std::string_view host, std::uint32_t port)
				noexcept : host(host), port(port) {}
		};
		struct on_connect_t : public Event::event_t {
			ClientSocket* client;

			on_connect_t(ClientSocket* client) noexcept
				: client(client) {}
		};
		struct on_data_t : public Event::event_t {
			std::string data;
			ClientSocket* client;

			on_data_t(const char* data, ClientSocket* client) noexcept
				: data(data), client(client) {}
			on_data_t(const char* data, std::size_t length, ClientSocket* client) noexcept
				: data(data, length), client(client) {}
			on_data_t(std::string data, ClientSocket* client) noexcept
				: data(data), client(client) {}
		};

	public:
		bool Listen(const std::string_view& host, std::uint32_t port) noexcept;
		bool Send(const std::string_view& data, ClientSocket* sock) noexcept;

		Event::Event<on_listening_t> OnListening;
		Event::Event<on_connect_t> OnConnect;
		Event::Event<on_data_t> OnData;
	protected:
		void OnIOCompleted(
			IOCP::IOContext* ctx,
			std::uint32_t bytesTransferred,
			std::uint32_t error
		) noexcept override;
	private:
		static LPFN_ACCEPTEX GetAcceptExPtr(SockType sock) noexcept;

		bool Accept() noexcept;
		bool Recv(ClientSocket* sock) noexcept;
	private:
		std::atomic<std::uint32_t> m_pendingAccepts;
		std::vector<std::unique_ptr<ClientSocket>> m_clients;
		std::vector<std::unique_ptr<ServerContext>> m_postedCtx;
	};
}