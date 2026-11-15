#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

// Reserve the large MCU search stack once, rather than allocating/freeing a
// contiguous block on every move and risking internal-heap fragmentation.
class EmbeddedWorker {
    std::mutex mutex;
    std::condition_variable ready, finished;
    std::function<void()> job;
    std::thread thread;
    bool busy = false, closing = false;
    void run() {
        for (;;) {
            std::unique_lock<std::mutex> lock(mutex);
            ready.wait(lock, [&] { return closing || bool(job); });
            if (closing) return;
            auto current = std::move(job);
            lock.unlock();
            current(); current = {};
            lock.lock(); busy = false; finished.notify_all();
        }
    }
public:
    void start(std::function<void()> next) {
        wait();
        if (!thread.joinable()) thread = std::thread([this] { run(); });
        std::lock_guard<std::mutex> lock(mutex);
        job = std::move(next); busy = true; ready.notify_one();
    }
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        finished.wait(lock, [&] { return !busy; });
    }
    ~EmbeddedWorker() {
        wait();
        { std::lock_guard<std::mutex> lock(mutex); closing = true; ready.notify_one(); }
        if (thread.joinable()) thread.join();
    }
};
