#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_JTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_JTHREAD_HPP

#if defined(__APPLE__)

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

// libc++ does not provide std::jthread/std::stop_token. This shim covers the
// cooperative-cancellation subset used by the VideoOut driver: constructor
// injection of a stop token, request_stop, join and join-on-destruction.

namespace StdCompat {

class stop_token {
public:
    stop_token() = default;
    bool stop_requested() const noexcept {
        return state != nullptr && state->load(std::memory_order_acquire);
    }

private:
    friend class jthread;
    explicit stop_token(std::shared_ptr<std::atomic_bool> shared) : state(std::move(shared)) {}
    std::shared_ptr<std::atomic_bool> state;
};

class jthread {
public:
    jthread() = default;
    explicit jthread(std::function<void(stop_token)> callable)
        : flag(std::make_shared<std::atomic_bool>(false)) {
        auto captured = flag;
        thread = std::thread([callable = std::move(callable), captured]() {
            callable(stop_token(captured));
        });
    }
    ~jthread() {
        if (thread.joinable()) {
            if (flag) flag->store(true, std::memory_order_release);
            thread.join();
        }
    }
    jthread(jthread&& other) noexcept = default;
    jthread& operator=(jthread&& other) noexcept {
        if (this != &other) {
            if (thread.joinable()) {
                if (flag) flag->store(true, std::memory_order_release);
                thread.join();
            }
            thread = std::move(other.thread);
            flag = std::move(other.flag);
        }
        return *this;
    }
    jthread(const jthread&) = delete;
    jthread& operator=(const jthread&) = delete;
    bool joinable() const noexcept { return thread.joinable(); }
    void request_stop() noexcept {
        if (flag) flag->store(true, std::memory_order_release);
    }
    void join() {
        if (thread.joinable()) thread.join();
    }
    std::thread::id get_id() const noexcept { return thread.get_id(); }

private:
    std::thread thread;
    std::shared_ptr<std::atomic_bool> flag;
};

}

#endif

#endif
