#pragma once

#include <socket.hpp>
#include <Shared/singleton.hpp>

namespace NSA::Core::Server {
	class Server : public Shared::Singleton<Server> {
	public:
	protected:
		Socket::ServerSocket m_socket;
	};
}