#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <new>

// Single-Producer Single-Consumer ring buffer with power-of-two capacity.
// Zero dynamic allocation. Cache-line separated indices to reduce false sharing.
// Memory order: producer uses release store on tail; consumer uses acquire load on tail.
namespace hft::spsc
{
    template <typename T, std::size_t CapacityPow2>
    class Queue
    {
        static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "Capacity must be a power of two");
        static constexpr std::size_t kMask = CapacityPow2 - 1;
        alignas(64) std::atomic<std::size_t> _head{0}; // next item the consumer will read
        alignas(64) std::atomic<std::size_t> _tail{0}; // next slot the producer will write
        alignas(64) std::byte _storage[sizeof(T) * CapacityPow2]; // raw storage to avoid default-constructing T

        T* slot(std::size_t index) noexcept
        {
            return std::launder(reinterpret_cast<T*>(&_storage[sizeof(T) * (index & kMask)]));
        }

    public:
        Queue() = default;
        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;

        ~Queue()
        {
            // Destroy any remaining objects to be neat. In hot paths you would skip this.
            std::size_t h = _head.load(std::memory_order_relaxed);
            const std::size_t t = _tail.load(std::memory_order_relaxed);
            while (h != t)
            {
                slot(h)->~T();
                h = (h + 1);
            }
        }

        bool push(const T& v) noexcept
        {
            const std::size_t t = _tail.load(std::memory_order_relaxed);
            const std::size_t h = _head.load(std::memory_order_acquire);
            if ((t - h) >= CapacityPow2) // full
                return false;
            new (slot(t)) T(v);
            _tail.store(t + 1, std::memory_order_release);
            return true;
        }

        bool push(T&& v) noexcept
        {
            const std::size_t t = _tail.load(std::memory_order_relaxed);
            const std::size_t h = _head.load(std::memory_order_acquire);
            if ((t - h) >= CapacityPow2) // full
                return false;
            new (slot(t)) T(std::move(v));
            _tail.store(t + 1, std::memory_order_release);
            return true;
        }

        bool pop(T& out) noexcept
        {
            const std::size_t h = _head.load(std::memory_order_relaxed);
            const std::size_t t = _tail.load(std::memory_order_acquire);
            if (h == t) // empty
                return false;
            T* s = slot(h);
            out = std::move(*s);
            s->~T();
            _head.store(h + 1, std::memory_order_release);
            return true;
        }

        bool empty() const noexcept
        {
            return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
        }

        std::size_t size() const noexcept
        {
            const std::size_t h = _head.load(std::memory_order_acquire);
            const std::size_t t = _tail.load(std::memory_order_acquire);
            return t - h;
        }
    };
} // namespace hft::spsc
