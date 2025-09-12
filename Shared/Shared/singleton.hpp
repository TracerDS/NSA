#pragma once

namespace NSA::Shared {
    template <typename T>
    class Singleton {
    public:
        // Delete copy/move to enforce single instance
        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&&) = delete;

        static T& getInstance() noexcept {
            static T instance;
            return instance;
        }

    protected:
        Singleton() = default;
        virtual ~Singleton() = default;
    };
}