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
#pragma region Static member initialization
	HANDLE Socket::gs_globalIOCP = INVALID_HANDLE_VALUE;
	std::vector<HANDLE> Socket::gs_workers = {};
	std::mutex Socket::gs_globalMutex;
	std::mutex Socket::gs_bufferMutex;
	std::atomic<std::uint32_t> Socket::gs_socketCount = 0;
	std::atomic<bool> Socket::gs_workersRunning = false;
	const auto Socket::gs_shutdownKey = Shared::Utils::RandomInRange<std::uint64_t>
	(
		0x1000000000000000,
		0xFFFFFFFFFFFFFFFE
	);
#pragma endregion

	namespace IOCP {
		constexpr auto DEFAULT_BUFFER_SIZE = 8 * 1024;

		IOContext::IOContext() noexcept {
			memset(&overlapped, 0, sizeof(overlapped));

			buffer.resize(DEFAULT_BUFFER_SIZE);
			wsabuf.buf = buffer.data();
			wsabuf.len = static_cast<ULONG>(buffer.size());
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
				std::lock_guard<std::mutex> lock(Socket::gs_bufferMutex);

				auto& entry = entries[i];

				auto ctx = reinterpret_cast<IOCP::IOContext*>(entry.lpOverlapped);
				if (!ctx)
					continue;

				ctx->buffer.resize(entry.dwNumberOfBytesTransferred);
				
				ctx->owner->OnIOCompleted(
					ctx,
					entry.dwNumberOfBytesTransferred,
					Shared::Utils::GetLastErrorInternal(static_cast<NTSTATUS>(entry.Internal))
				);
			}
		}
		return 0;
	}

#pragma region Socket details

	Socket::Socket() noexcept
		: m_socket(INVALID_SOCKET), m_host(""), m_port(0)
	{
		std::lock_guard<std::mutex> lock(gs_globalMutex);
		if (gs_socketCount == 0) {
			static WSAData ms_wsaData;
			if (WSAStartup(MAKEWORD(2, 2), &ms_wsaData)) {
				std::println(
					stderr,
					"WSAStartup failed: {}",
					Shared::Utils::GetLastWSAErrorString()
				);
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
				Shared::Utils::GetLastWSAErrorString()
			);
#endif
			return false;
		}

		if (!SetFileCompletionNotificationModes(
			reinterpret_cast<HANDLE>(m_socket),
			FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
		)) {
#ifdef ATS_DEBUG
			std::println(
				stderr,
				"SetFileCompletionNotificationModes error: {}",
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

	Socket::Socket(SockType&& socket) noexcept : m_host(""), m_port(0) {
		std::swap(m_socket, socket);
	}

	Socket::Socket(Socket&& socket) noexcept : m_host(""), m_port(0) {
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

	std::optional<std::pair<std::string, std::uint32_t>> Socket::GetSocketAddress(
		SockType sock
	) noexcept {
		sockaddr_in peerAddr;
		int addrLen = sizeof(peerAddr);

		if (getpeername(
			sock,
			reinterpret_cast<sockaddr*>(&peerAddr),
			&addrLen
		) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
			auto wsaErr = WSAGetLastError();
			std::println(
				stderr,
				"getpeername failed: {} ({})",
				Shared::Utils::GetLastWSAErrorString(wsaErr),
				wsaErr
			);
#endif
			return std::nullopt;
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
				Shared::Utils::GetLastWSAErrorString()
			);
#endif
			return std::nullopt;
		}

		auto port = Shared::Utils::StringToInt<std::uint32_t>(service);
		if (!port.has_value()) {
#ifdef ATS_DEBUG
			std::println(stderr, "Port is not a valid number");
#endif
			return std::nullopt;
		}

		return std::pair{ host, port.value() };
	}

	void swap(Socket& lhs, Socket& rhs) noexcept {
		std::swap(lhs.m_socket, rhs.m_socket);
	}

	void* Socket::GetWinsockFunctionPtr(SockType sock, GUID guid) noexcept {
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

	LPFN_CONNECTEX ClientSocket::GetConnectExPtr(SockType sock) noexcept {
		static auto func = reinterpret_cast<LPFN_CONNECTEX>(
			GetWinsockFunctionPtr(sock, WSAID_CONNECTEX)
		);
		return func;
	}
	LPFN_ACCEPTEX ServerSocket::GetAcceptExPtr(SockType sock) noexcept {
		static auto func = reinterpret_cast<LPFN_ACCEPTEX>(
			GetWinsockFunctionPtr(sock, WSAID_ACCEPTEX)
		);
		return func;
	}

#pragma endregion

#pragma region Client Socket

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

			auto ConnectEx = ClientSocket::GetConnectExPtr(m_socket);
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

			auto& ctx = m_postedCtx.emplace_back(new ClientContext);
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

	bool ClientSocket::Recv() noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		std::lock_guard<std::mutex> lock(gs_bufferMutex);

		auto& ctx = m_postedCtx.emplace_back(new ClientContext);
		ctx->owner = this;
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
					Shared::Utils::GetLastWSAErrorString(err)
				);
#endif
				return false;
			}
		} else {
			auto bytesTransferred = static_cast<std::uint32_t>(ctx->overlapped.InternalHigh);
			ctx->buffer.resize(bytesTransferred);

			this->OnIOCompleted(
				ctx.get(),
				bytesTransferred,
				Shared::Utils::GetLastErrorInternal(static_cast<NTSTATUS>(ctx->overlapped.Internal))
			);
		}
		return true;
	}

	bool ClientSocket::Send(const std::string_view& data) noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		std::lock_guard<std::mutex> lock(gs_bufferMutex);

		auto& ctx = m_postedCtx.emplace_back(new ClientContext);
		ctx->owner = this;
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
					Shared::Utils::GetLastWSAErrorString(err)
				);
#endif
				return false;
			}
		} else {
			auto bytesTransferred = static_cast<std::uint32_t>(ctx->overlapped.InternalHigh);
			ctx->buffer.resize(bytesTransferred);

			this->OnIOCompleted(
				ctx.get(),
				bytesTransferred,
				Shared::Utils::GetLastErrorInternal(static_cast<NTSTATUS>(ctx->overlapped.Internal))
			);
		}
		return true;
	}

	void ClientSocket::OnIOCompleted(
		IOCP::IOContext* rawCtx,
		std::uint32_t bytesTransferred,
		std::uint32_t error
	) noexcept {
		if (!rawCtx)
			return;

		auto ctx = static_cast<ClientContext*>(rawCtx);

		switch (ctx->operation) {
			case IOCP::IOOperation::CONNECT: {
				if (error != 0) {
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"Connect failed: {}",
						Shared::Utils::GetLastWSAErrorString(error)
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
						Shared::Utils::GetLastWSAErrorString()
					);
#endif
					break;
				}

				auto addr = Socket::GetSocketAddress(m_socket);
				if (!addr.has_value())
					break;

				m_host = addr.value().first;
				m_port = addr.value().second;

				OnConnect({ m_host, m_port });

				for (auto i = 0; i < Socket::MAX_PENDING_RECVS; i++)
					this->Recv();

				break;
			} case IOCP::IOOperation::RECV: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ClientSocket WSARecv closed or error: {}",
						Shared::Utils::GetLastWSAErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				if (bytesTransferred > 0) {
					OnData({ ctx->buffer.data(), bytesTransferred });
				}
				
				this->Recv();

				break;
			} case IOCP::IOOperation::SEND: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ClientSocket WSASend closed or error: {}",
						Shared::Utils::GetLastWSAErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				break;
			}
		}

		std::erase_if(m_postedCtx, [&](auto& p) { return p.get() == ctx; });
	}

#pragma endregion

#pragma region Server Socket

	bool ServerSocket::Listen(const std::string_view& host, std::uint32_t port) noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		if (inet_pton(addr.sin_family, host.data(), &addr.sin_addr) != 1) {
#ifdef ATS_DEBUG
			std::println(stderr,
				"inet_pton failed: {}",
				Shared::Utils::GetLastWSAErrorString()
			);
#endif
			return false;
		}
		if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
			std::println(stderr,
				"bind failed: {}",
				Shared::Utils::GetLastWSAErrorString()
			);
#endif
			return false;
		}

		if (listen(m_socket, SOMAXCONN) == SOCKET_ERROR) {
#ifdef ATS_DEBUG
			std::println(stderr,
				"listen failed: {}",
				Shared::Utils::GetLastWSAErrorString()
			);
#endif
			return false;
		}

		m_host = host;
		m_port = port;
		OnListening({ host, port });

		for (auto i = 0; i < gs_workers.size(); i++) {
			this->Accept();
		}
		return true;
	}

	bool ServerSocket::Accept() noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		auto AcceptEx = ServerSocket::GetAcceptExPtr(m_socket);
		if (!AcceptEx) {
#ifdef ATS_DEBUG
			std::println(
				stderr,
				"GetAcceptExPtr failed: {}",
				Shared::Utils::GetLastErrorString()
			);
#endif
			return false;
		}

		std::lock_guard<std::mutex> lock(gs_bufferMutex);

		auto& ctx = m_postedCtx.emplace_back(new ServerContext);
		ctx->owner = this;

		auto& sock = m_clients.emplace_back(new ClientSocket);
		if (!sock->Create())
			return false;

		ctx->client = sock.get();
		ctx->operation = IOCP::IOOperation::ACCEPT;
		DWORD bytesReceived = 0;

		if (!AcceptEx(
			m_socket,
			ctx->client->GetSocket(),
			ctx->buffer.data(),
			0,
			sizeof(sockaddr_storage) + 16,
			sizeof(sockaddr_storage) + 16,
			&bytesReceived,
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
				return false;
			}
		} else {
			auto bytesTransferred = static_cast<std::uint32_t>(ctx->overlapped.InternalHigh);
			ctx->buffer.resize(bytesTransferred);

			this->OnIOCompleted(
				ctx.get(),
				bytesTransferred,
				Shared::Utils::GetLastErrorInternal(static_cast<NTSTATUS>(ctx->overlapped.Internal))
			);
		}
		return true;
	}

	bool ServerSocket::Send(const std::string_view& data, ClientSocket* sock) noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		std::lock_guard<std::mutex> lock(gs_bufferMutex);

		auto& ctx = m_postedCtx.emplace_back(new ServerContext);
		ctx->owner = this;
		ctx->client = sock;
		ctx->buffer.assign(data.begin(), data.end());
		ctx->wsabuf.buf = ctx->buffer.data();
		ctx->wsabuf.len = static_cast<ULONG>(ctx->buffer.size());
		ctx->operation = IOCP::IOOperation::SEND;

		DWORD bytesSent = 0;
		if (WSASend(
			ctx->client->GetSocket(),
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
					Shared::Utils::GetLastWSAErrorString(err)
				);
#endif
				return false;
			}
		} else {
			auto bytesTransferred = static_cast<std::uint32_t>(ctx->overlapped.InternalHigh);
			ctx->buffer.resize(bytesTransferred);

			this->OnIOCompleted(
				ctx.get(),
				bytesTransferred,
				Shared::Utils::GetLastErrorInternal(static_cast<NTSTATUS>(ctx->overlapped.Internal))
			);
		}
		return true;
	}

	bool ServerSocket::Recv(ClientSocket* sock) noexcept {
		if (m_socket == INVALID_SOCKET)
			return false;

		std::lock_guard<std::mutex> lock(gs_bufferMutex);

		auto& ctx = m_postedCtx.emplace_back(new ServerContext);
		ctx->owner = this;
		ctx->client = sock;
		ctx->buffer.resize(IOCP::DEFAULT_BUFFER_SIZE);
		ctx->wsabuf.buf = ctx->buffer.data();
		ctx->wsabuf.len = static_cast<ULONG>(ctx->buffer.size());
		ctx->operation = IOCP::IOOperation::RECV;

		DWORD flags = 0;
		DWORD bytesReceived = 0;
		if (WSARecv(
			ctx->client->GetSocket(),
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
					Shared::Utils::GetLastWSAErrorString(err)
				);
#endif
				return false;
			}
		} else {
			auto bytesTransferred = static_cast<std::uint32_t>(ctx->overlapped.InternalHigh);
			ctx->buffer.resize(bytesTransferred);

			this->OnIOCompleted(
				ctx.get(),
				bytesTransferred,
				Shared::Utils::GetLastErrorInternal(static_cast<NTSTATUS>(ctx->overlapped.Internal))
			);
		}
		return true;
	}

	void ServerSocket::OnIOCompleted(
		IOCP::IOContext* rawCtx,
		std::uint32_t bytesTransferred,
		std::uint32_t error
	) noexcept {
		if (!rawCtx)
			return;

		auto ctx = static_cast<ServerContext*>(rawCtx);

		switch (ctx->operation) {
			case IOCP::IOOperation::ACCEPT: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ServerSocket AcceptEx closed or error: {}",
						Shared::Utils::GetLastWSAErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				OnConnect({ ctx->client });

				for (auto i = 0; i < Socket::MAX_PENDING_RECVS; i++)
					this->Recv(ctx->client);

				break;
			} case IOCP::IOOperation::RECV: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ServerSocket WSARecv closed or error: {}",
						Shared::Utils::GetLastWSAErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				if (bytesTransferred > 0) {
					OnData({ ctx->buffer.data(), bytesTransferred, ctx->client });
				}

				if (!this->Recv(ctx->client)) {
					ctx->client->Close();
				}

				break;
			} case IOCP::IOOperation::SEND: {
				if (error != 0) {
					// connection closed or error
#ifdef ATS_DEBUG
					std::println(
						stderr,
						"ServerSocket WSASend closed or error: {}",
						Shared::Utils::GetLastWSAErrorString(error)
					);
#endif
					this->Close();
					break;
				}

				break;
			}
		}

		std::erase_if(m_postedCtx, [&](auto& p) { return p.get() == ctx; });
	}

#pragma endregion

}