#include <socket.hpp>

#include <Shared/os.hpp>
#include <Shared/utils.hpp>

#include <algorithm>
#include <thread>
#include <print>
#include <cassert>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace NSA::Core::Socket {
	HANDLE Socket::gs_globalIOCP = INVALID_HANDLE_VALUE;
	std::vector<HANDLE> Socket::gs_workers = {};
	std::mutex Socket::gs_globalMutex;
	std::atomic<std::uint32_t> Socket::gs_socketCount = 0;
	std::atomic<bool> Socket::gs_workersRunning = false;
	const auto Socket::gs_shutdownKey = Shared::Utils::RandomInRange<std::uint64_t>
	(
		0x1000000000000000,
		0xFFFFFFFFFFFFFFFE
	);

	namespace IOCP {
		constexpr auto DEFAULT_BUFFER_SIZE = 8 * 1024;
		IOContext::IOContext() noexcept {
			memset(&overlapped, 0, sizeof(overlapped));

			buffer.resize(DEFAULT_BUFFER_SIZE);
			wsabuf.buf = buffer.data();
			wsabuf.len = static_cast<ULONG>(buffer.capacity());
		}
	}

	DWORD WINAPI Socket::IOCPWorkerThread(LPVOID param) noexcept {
		auto sock = reinterpret_cast<Socket*>(param);

		auto threadId = GetCurrentThreadId();
		constexpr auto MAX_EVENTS = 16;

		while (true) {
			if (!Socket::gs_workersRunning)
				break;

			OVERLAPPED_ENTRY entries[MAX_EVENTS];
			ULONG count;

			if (!GetQueuedCompletionStatusEx(
				Socket::gs_globalIOCP,
				entries,
				ARRAYSIZE(entries),
				&count,
				INFINITE,
				false
			)) {
				std::println(
					stderr,
					"{} -> GetQueuedCompletionStatusEx failed: {}",
					threadId,
					Shared::Utils::GetLastErrorString()
				);
				continue;
			}
			for (ULONG i = 0; i < count; i++) {
				auto& entry = entries[i];

				std::println("{} -> IO pre completed: {} bytes (0x{:X})",
					threadId,
					entry.dwNumberOfBytesTransferred,
					reinterpret_cast<uintptr_t>(entry.lpOverlapped)
				);

				auto ctx = reinterpret_cast<IOCP::IOContext*>(entry.lpOverlapped);
				if (!ctx)
					continue;

				std::println("{} -> IO completed: {} bytes ({})",
					threadId,
					entry.dwNumberOfBytesTransferred,
					IOCP::ToString(ctx->operation)
				);


				auto* owner = reinterpret_cast<Socket*>(ctx->owner);
				if (!owner)
					continue;

				ctx->buffer.resize(entry.dwNumberOfBytesTransferred);
				owner->OnIOCompleted(
					ctx,
					entry.dwNumberOfBytesTransferred,
					GetLastError()
				);
			}
		}
		return 0;
	}

	Socket::Socket() noexcept
		: m_socket(INVALID_SOCKET)
	{
		std::lock_guard<std::mutex> lock(gs_globalMutex);
		if (gs_socketCount == 0) {
			static WSAData ms_wsaData;
			if (WSAStartup(MAKEWORD(2, 2), &ms_wsaData)) {
				std::println(stderr, "WSAStartup failed: {}", Shared::Utils::GetLastErrorString());
				return;
			}
			Socket::gs_globalIOCP = CreateIoCompletionPort(
				INVALID_HANDLE_VALUE,
				nullptr,
				reinterpret_cast<ULONG_PTR>(this),
				0
			);
			if (!gs_globalIOCP) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"CreateIOCompletionPort error: {}",
					Shared::Utils::GetLastErrorString()
				);
#endif
				return;
			}
			SYSTEM_INFO sysInfo;
			GetSystemInfo(&sysInfo);
			auto threadCount = sysInfo.dwNumberOfProcessors;
			threadCount = 1;

			for (DWORD i = 0; i < threadCount * 2; i++) {
				HANDLE thread = CreateThread(
					nullptr,
					0,
					Socket::IOCPWorkerThread,
					this,
					CREATE_SUSPENDED,
					nullptr
				);

				if (thread) {
					Socket::gs_workers.push_back(thread);
				}
			}
		}
		
		gs_socketCount++;
	}

	bool Socket::Create(AddressFamily family, SocketType type) noexcept {
		if (m_socket != INVALID_SOCKET)
			return false;

		IPPROTO protocol;
		switch (type) {
			case SocketType::TCP:
				protocol = IPPROTO::IPPROTO_TCP;
				break;
			case SocketType::UDP:
				protocol = IPPROTO::IPPROTO_UDP;
				break;
			default:
				protocol = IPPROTO::IPPROTO_RAW;
		}

		m_socket = WSASocketW(
			std::to_underlying(family),
			std::to_underlying(type),
			protocol,
			nullptr,
			0,
			WSA_FLAG_OVERLAPPED
		);

		if (m_socket == INVALID_SOCKET) {
#ifdef ATS_DEBUG
			std::println(
				stderr,
				"WSASocketW error: {}",
				Shared::Utils::GetLastErrorString()
			);
#endif
			return false;
		}

		bool res = AssociateIOCP();
		Socket::gs_workersRunning = true;
		std::ranges::for_each(Socket::gs_workers, ResumeThread);

		return res;
	}

	bool Socket::AssociateIOCP() const noexcept {
		return CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(m_socket),
			Socket::gs_globalIOCP,
			reinterpret_cast<ULONG_PTR>(this),
			0
		) != nullptr;
	}

	Socket::Socket(SockType&& socket) noexcept {
		std::swap(m_socket, socket);
	}

	Socket::Socket(Socket&& socket) noexcept {
		std::swap(m_socket, socket.m_socket);
	}

	Socket::~Socket() noexcept {
		Close();
	}

	bool Socket::Close() noexcept {
		std::lock_guard<std::mutex> lock(Socket::gs_globalMutex);
		Socket::gs_socketCount--;

		if (Socket::gs_socketCount == 0) {
			assert(Socket::gs_globalIOCP != INVALID_HANDLE_VALUE && "globalIOCP invalid");

			// wake up threads
			for (std::size_t i = 0; i < Socket::gs_workers.size(); i++) {
				PostQueuedCompletionStatus(gs_globalIOCP, 0, gs_shutdownKey, nullptr);
			}

			Socket::gs_workersRunning = false;
			for (auto& thread : Socket::gs_workers) {
				if (thread) {
					WaitForSingleObject(thread, INFINITE);
					CloseHandle(thread);
				}
			}
			Socket::gs_workers.clear();

			CloseHandle(Socket::gs_globalIOCP);
			Socket::gs_globalIOCP = INVALID_HANDLE_VALUE;
		}

		if (m_socket == INVALID_SOCKET)
			return true;

#if NSA_USE_WINDOWS
		if (shutdown(m_socket, SD_BOTH) == SOCKET_ERROR)
			return false;

		if (closesocket(m_socket) == SOCKET_ERROR)
			return false;
#else
		if (shutdown(m_socket, SHUT_RDWR) == -1)
			return false;

		if (close(m_socket) == -1)
			return false;
#endif
		m_socket = INVALID_SOCKET;
		if (Socket::gs_socketCount == 0) {
			if (WSACleanup() == SOCKET_ERROR) {
				std::println(stderr, "WSACleanup failed: {}", Shared::Utils::GetLastErrorString());
				return false;
			}
		}
		return true;
	}
	
	Socket::SockType Socket::GetSocket() const noexcept { return m_socket; }

	bool Socket::IsOpen() const noexcept { return m_socket != INVALID_SOCKET; }

	void swap(Socket& lhs, Socket& rhs) noexcept {
		std::swap(lhs.m_socket, rhs.m_socket);
	}

	void* Socket::GetWinsockFunctionPtr(SOCKET sock, GUID guid) noexcept {
		void* func = nullptr;

		DWORD bytes = 0;
		if (WSAIoctl(
			sock,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid,
			sizeof(guid),
			&func,
			sizeof(func),
			&bytes,
			nullptr,
			nullptr
		) == SOCKET_ERROR)
			return nullptr;

		return func;
	}

	LPFN_CONNECTEX Socket::GetConnectExPtr(SOCKET sock) noexcept {
		static auto func = reinterpret_cast<LPFN_CONNECTEX>(
			GetWinsockFunctionPtr(sock, WSAID_CONNECTEX)
		);
		return func;
	}
	LPFN_ACCEPTEX Socket::GetAcceptExPtr(SOCKET sock) noexcept {
		static auto func = reinterpret_cast<LPFN_ACCEPTEX>(
			GetWinsockFunctionPtr(sock, WSAID_ACCEPTEX)
		);
		return func;
	}

	bool Socket::PostSend(std::unique_ptr<IOCP::IOContext>& ctx, std::string_view data) noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		ctx->buffer.assign(data.begin(), data.end());
		ctx->wsabuf.buf = ctx->buffer.data();
		ctx->wsabuf.len = static_cast<ULONG>(ctx->buffer.size());
		ctx->operation = IOCP::IOOperation::SEND;

		DWORD bytesSent = 0;
		if (WSASend(
			m_socket,
			&ctx->wsabuf,
			1,
			&bytesSent,
			0,
			&ctx->overlapped,
			nullptr
		) == SOCKET_ERROR) {
			auto err = WSAGetLastError();
			if (err != WSA_IO_PENDING) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"WSASend failed: {}",
					Shared::Utils::GetLastErrorString(err)
				);
#endif
				return false;
			}
		} else {
			std::println("WSASend completed immediately: {} bytes", bytesSent);
		}
		return true;
	}

	bool Socket::PostRecv(std::unique_ptr<IOCP::IOContext>& ctx) noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		ctx->buffer.resize(IOCP::DEFAULT_BUFFER_SIZE);
		ctx->wsabuf.buf = ctx->buffer.data();
		ctx->wsabuf.len = static_cast<ULONG>(ctx->buffer.size());
		ctx->operation = IOCP::IOOperation::RECV;

		DWORD flags = 0;
		DWORD bytesReceived = 0;
		if (WSARecv(
			m_socket,
			&ctx->wsabuf,
			1,
			&bytesReceived,
			&flags,
			&ctx->overlapped,
			nullptr
		) == SOCKET_ERROR) {
			auto err = WSAGetLastError();
			if (err != WSA_IO_PENDING) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"WSARecv failed: {}",
					Shared::Utils::GetLastErrorString(err)
				);
#endif
				return false;
			}
		} else {
			std::println("WSARecv completed immediately: {} bytes", bytesReceived);
		}
		return true;
	}

	// ClientSocket
		
	ClientSocket::ClientSocket() noexcept : Socket() {}

	ClientSocket::ClientSocket(Socket::SockType&& socket) noexcept : Socket() {
		m_socket = socket;
		AssociateIOCP();
	}
	
	bool ClientSocket::Connect(const std::string_view& host, std::uint32_t port) noexcept {
		addrinfo hints{};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* result = nullptr;
		auto portStr = std::to_string(port);

		{
			auto ret = getaddrinfo(host.data(), portStr.c_str(), &hints, &result);
			if (ret != 0) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"getaddrinfo failed: {}",
					Shared::Utils::GetLastErrorString(ret)
				);
#endif
				return false;
			}
		}

		bool success = false;
		for (auto ai = result; ai; ai = ai->ai_next) {
			sockaddr_storage ss;
			ZeroMemory(&ss, sizeof(ss));

			int aiSize = 0;

			if (ai->ai_family == AF_INET) {
				sockaddr_in* sin = (sockaddr_in*)&ss;
				sin->sin_family = AF_INET;
				aiSize = sizeof(sockaddr_in);
			} else {
				sockaddr_in6* sin6 = (sockaddr_in6*)&ss;
				sin6->sin6_family = AF_INET6;
				aiSize = sizeof(sockaddr_in6);
			}

			if (bind(
				m_socket,
				reinterpret_cast<sockaddr*>(&ss),
				aiSize
			) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"bind failed: {}",
					Shared::Utils::GetLastErrorString()
				);
#endif
				continue;
			}

			LPFN_CONNECTEX ConnectEx = Socket::GetConnectExPtr(m_socket);
			if (!ConnectEx) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"GetConnectExPtr failed: {}",
					Shared::Utils::GetLastErrorString()
				);
#endif
				continue;
			}

			auto& ctx = m_postedCtx.emplace_back(new IOCP::IOContext());
			ctx->operation = IOCP::IOOperation::CONNECT;
			ctx->owner = this;
			
			// Silence the C6387 warning
			DWORD bytesSent = 0;
			if (!ConnectEx(
				m_socket,
				ai->ai_addr,
				static_cast<int>(ai->ai_addrlen),
				nullptr,
				0,
				&bytesSent,
				&ctx->overlapped
			)) {
				auto err = WSAGetLastError();
				if (err != WSA_IO_PENDING) {
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ConnectEx failed: {}",
						Shared::Utils::GetLastErrorString(err)
					);
#endif
					continue;
				}
			}
			success = true;
			break;
		}
		freeaddrinfo(result);

		return success;
	}

	bool ClientSocket::Send(const std::string_view& data) noexcept {
		auto& ctx = m_postedCtx.emplace_back(new IOCP::IOContext());
		ctx->owner = this;		
		return PostSend(ctx, data);
	}

	bool ClientSocket::Recv(Event::Event<on_data_t> callback) noexcept {
		OnData = callback;
		return Recv();
	}

	bool ClientSocket::Recv() noexcept {
		auto& ctx = m_postedCtx.emplace_back(new IOCP::IOContext());
		ctx->owner = this;
		return PostRecv(ctx);
	}

	void ClientSocket::OnIOCompleted(
		IOCP::IOContext* ctx,
		std::uint32_t bytesTransferred,
		std::uint32_t error
	) noexcept {
		if (!ctx)
			return;

		switch (ctx->operation) {
			case IOCP::IOOperation::CONNECT: {
				if (error != 0) {
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"Connect failed: {}",
						Shared::Utils::GetLastErrorString(error)
					);
#endif
					break;
				}

				// CompleteConnect: update socket to be usable with getsockname etc.
				// set SO_UPDATE_CONNECT_CONTEXT
				if (setsockopt(
					m_socket,
					SOL_SOCKET,
					SO_UPDATE_CONNECT_CONTEXT,
					nullptr,
					0
				) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"OnIOCompleted error: {}",
						Shared::Utils::GetLastErrorString()
					);
#endif
					break;
				}
				sockaddr_in peerAddr;
				int addrLen = sizeof(peerAddr);

				if (getpeername(
					m_socket,
					reinterpret_cast<sockaddr*>(&peerAddr),
					&addrLen
				) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"getpeername failed: {}",
						Shared::Utils::GetLastErrorString()
					);
#endif
					break;
				}

				char host[NI_MAXHOST];
				char service[NI_MAXSERV];

				if (getnameinfo(
					reinterpret_cast<sockaddr*>(&peerAddr),
					addrLen,
					host,
					sizeof(host),
					service,
					sizeof(service),
					NI_NUMERICHOST | NI_NUMERICSERV
				) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"getnameinfo failed: {}",
						Shared::Utils::GetLastErrorString()
					);
#endif
					break;
				}
				auto port = Shared::Utils::StringToInt<std::uint32_t>(service);
				if (!port.has_value()) {
#ifdef ATS_DEBUG
					std::println(stderr, "Port is not a valid number");
#endif
					break;
				}

				OnConnect({ host, port.value() });

				break;
			} case IOCP::IOOperation::RECV: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ClientSocket WSARecv closed or error: {}",
						Shared::Utils::GetLastErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				if (bytesTransferred > 0) {
					OnData({ ctx->buffer.data(), bytesTransferred });
				}
				break;
			} case IOCP::IOOperation::SEND: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ClientSocket WSASend closed or error: {}",
						Shared::Utils::GetLastErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				break;
			}
		}
		std::erase_if(m_postedCtx, [ctx](const auto& item) { return item.get() == ctx; });
	}

	// ServerSocket
	bool ServerSocket::Listen(const std::string_view& host, std::uint32_t port) noexcept {
		addrinfo hints{};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* result = nullptr;
		auto portStr = std::to_string(port);

		{
			auto ret = getaddrinfo(host.data(), portStr.c_str(), &hints, &result);
			if (ret != 0) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"getaddrinfo failed: {}",
					Shared::Utils::GetLastErrorString(ret)
				);
#endif
				return false;
			}
		}

		bool success = false;
		for (auto ai = result; ai; ai = ai->ai_next) {
			sockaddr_storage ss;
			ZeroMemory(&ss, sizeof(ss));

			int aiSize = 0;

			if (ai->ai_family == AF_INET) {
				sockaddr_in* sin = (sockaddr_in*)&ss;
				sin->sin_family = AF_INET;
				aiSize = sizeof(sockaddr_in);
			} else {
				sockaddr_in6* sin6 = (sockaddr_in6*)&ss;
				sin6->sin6_family = AF_INET6;
				aiSize = sizeof(sockaddr_in6);
			}

			if (bind(
				m_socket,
				reinterpret_cast<sockaddr*>(&ss),
				aiSize
			) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
				std::println(
					stderr,
					"bind failed: {}",
					Shared::Utils::GetLastErrorString()
				);
#endif
				continue;
			}
		}
		freeaddrinfo(result);
		return success;
	}

	void ServerSocket::Accept() noexcept {
	}
}