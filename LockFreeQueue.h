#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace cray
{
    /**
     * Wait-free Single-Producer Single-Consumer ring buffer for real-time audio telemetry
     * or lock-free command passing without heap allocations.
     */
    template <typename T, size_t Capacity>
    class LockFreeSpscQueue
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

    public:
        bool push(const T& item) noexcept
        {
            const auto writeIdx = writeIndex_.load(std::memory_order_relaxed);
            const auto nextIdx  = (writeIdx + 1) & kMask;
            if (nextIdx == readIndex_.load(std::memory_order_acquire))
                return false;

            buffer_[writeIdx] = item;
            writeIndex_.store(nextIdx, std::memory_order_release);
            return true;
        }

        bool pop(T& outItem) noexcept
        {
            const auto readIdx = readIndex_.load(std::memory_order_relaxed);
            if (readIdx == writeIndex_.load(std::memory_order_acquire))
                return false;

            outItem = buffer_[readIdx];
            readIndex_.store((readIdx + 1) & kMask, std::memory_order_release);
            return true;
        }

        void clear() noexcept
        {
            readIndex_.store(0, std::memory_order_relaxed);
            writeIndex_.store(0, std::memory_order_relaxed);
        }

    private:
        static constexpr size_t kMask = Capacity - 1;
        alignas(64) std::atomic<size_t> writeIndex_{0};
        alignas(64) std::atomic<size_t> readIndex_{0};
        std::array<T, Capacity> buffer_{};
    };
}
