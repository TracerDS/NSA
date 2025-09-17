#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace NSA::Core::Event {
    template <typename EventType>
    class Event {
    public:
        using FunctionType = std::function<void(EventType&)>;
    public:
        template <typename Func>
        Event& operator=(Func&& func) {
            m_listener = FunctionType(std::forward<Func>(func));
            return *this;
        }

        void operator()(EventType event) const noexcept {
            Call(event);
        }
        void Call(EventType event) const noexcept {
            if (m_listener) m_listener(event);
        }

        operator bool() const noexcept {
            return m_listener.operator bool();
		}

    private:
        FunctionType m_listener;
    };

    struct event_t {
        virtual ~event_t() = default;
    };
}