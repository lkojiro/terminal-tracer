#pragma once
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

// ---------------------------------------------------------------
// A small bounded producer/consumer queue: push() blocks while the
// queue is full, pop() blocks while it's empty, and close() unblocks
// everyone once the producer is done (pop() then drains whatever's left
// before finally returning false).
//
// This is what gives the video pipeline backpressure: if the render
// thread falls behind (a slow terminal, a big frame), the decode
// thread's push() blocks instead of piling up unboundedly-many decoded
// frames in memory. Generic rather than VideoFrame-specific since
// nothing about it is video-related.
// ---------------------------------------------------------------

// Outcome of pop_for(): Item means `out` was filled; Timeout means the
// deadline passed with nothing available (try again -- the queue is
// still open); Closed means close() was called and every already-pushed
// item has been drained, so there will never be another Item.
enum class PopStatus { Item, Timeout, Closed };

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}

    // Blocks while the queue is at capacity and still open. If close()
    // is called concurrently, the item is silently dropped instead of
    // being pushed after the fact -- the producer is expected to stop
    // calling push() shortly after seeing that happen.
    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [&] { return items_.size() < capacity_ || closed_; });
        if (closed_) return;
        items_.push_back(std::move(item));
        lock.unlock();
        notEmpty_.notify_one();
    }

    // Blocks while empty and still open. Returns false once the queue
    // has been closed AND drained -- the caller's signal to stop.
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [&] { return !items_.empty() || closed_; });
        if (items_.empty()) return false; // closed and drained
        out = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        notFull_.notify_one();
        return true;
    }

    // Like pop(), but gives up and returns PopStatus::Timeout instead of
    // blocking indefinitely if nothing shows up within `timeout` -- for
    // a caller (video_player.cpp's render loop) that needs to keep
    // checking for something else (a quit request) while it waits,
    // rather than being stuck unresponsive inside a plain wait().
    template <typename Rep, typename Period>
    PopStatus pop_for(T& out, std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        bool ready = notEmpty_.wait_for(lock, timeout, [&] { return !items_.empty() || closed_; });
        if (!ready) return PopStatus::Timeout;
        if (items_.empty()) return PopStatus::Closed; // closed and drained
        out = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        notFull_.notify_one();
        return PopStatus::Item;
    }

    // Marks the queue closed: wakes any blocked push()/pop() so the
    // decode/render threads can unwind instead of hanging forever on
    // shutdown or end-of-stream. Idempotent.
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        notFull_.notify_all();
        notEmpty_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable notFull_, notEmpty_;
    std::deque<T> items_;
    size_t capacity_;
    bool closed_ = false;
};
