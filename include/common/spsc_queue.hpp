#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

// Lock-free single-producer/single-consumer (SPSC) ring buffer with power-of-two capacity.
//   * No heap interaction: items are constructed in place inside `_storage` and destroyed manually.
//   * Head/tail indices are separated onto individual cache lines to minimise false sharing.
//   * Memory ordering contract:
//       producer thread -> release-store tail after publishing element
//       consumer thread -> acquire-load tail before reading element
//     This ensures that object construction/destruction is observed in the correct order.
namespace hft::spsc
{
template <typename T, std::size_t CapacityPow2> class Queue
{
  // Enforce power-of-two capacity to permit masking instead of modulus (faster wrap-around).
  static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "Capacity must be a power of two");
  // Bitmask used for wrapping the circular buffer indices.
  static constexpr std::size_t kMask = CapacityPow2 - 1;
  // Index of the next element to be consumed. Only the consumer thread modifies it.
  alignas(64) std::atomic<std::size_t> _head{0};
  // Index of the next free slot that the producer will occupy.
  alignas(64) std::atomic<std::size_t> _tail{0};
  // Raw byte storage for the ring buffer slots. Objects are placement-new'ed on demand.
  alignas(64) std::byte _storage[sizeof(T) * CapacityPow2];

  T *slot(std::size_t index) noexcept
  {
    // Translate the logical index into the physical slot pointer with wrap-around.
    return std::launder(reinterpret_cast<T *>(&_storage[sizeof(T) * (index & kMask)]));
  }

public:
  Queue() = default;
  Queue(const Queue &) = delete;
  Queue &operator=(const Queue &) = delete;

  ~Queue()
  {
    // The queue is usually drained before destruction, but clean up in case items remain.
    std::size_t h = _head.load(std::memory_order_relaxed);
    const std::size_t t = _tail.load(std::memory_order_relaxed);
    while (h != t)
    {
      // Explicitly run the destructor for the object stored in the slot.
      slot(h)->~T();
      h = (h + 1);
    }
  }

  bool push(const T &v) noexcept
  {
    // Acquire the current writer and reader positions. Relaxed read on tail is safe because
    // only producer updates it, but we must acquire the head to observe the consumer's progress.
    const std::size_t t = _tail.load(std::memory_order_relaxed);
    const std::size_t h = _head.load(std::memory_order_acquire);
    if ((t - h) >= CapacityPow2) // full
      return false;
    // Construct a copy of the element directly in the target slot.
    new (slot(t)) T(v);
    // Publish the new element so the consumer can see it.
    _tail.store(t + 1, std::memory_order_release);
    return true;
  }

  bool push(T &&v) noexcept
  {
    // Same logic as the const& overload, but forwards the value to avoid extra copies.
    const std::size_t t = _tail.load(std::memory_order_relaxed);
    const std::size_t h = _head.load(std::memory_order_acquire);
    if ((t - h) >= CapacityPow2) // full
      return false;
    new (slot(t)) T(std::move(v));
    _tail.store(t + 1, std::memory_order_release);
    return true;
  }

  bool pop(T &out) noexcept
  {
    // Load positions: consumer owns head, but must acquire tail to observe published writes.
    const std::size_t h = _head.load(std::memory_order_relaxed);
    const std::size_t t = _tail.load(std::memory_order_acquire);
    if (h == t) // empty
      return false;
    T *s = slot(h);
    // Move the value out of the slot, then run its destructor to keep storage clean.
    out = std::move(*s);
    s->~T();
    // Release the slot back to the producer by advancing head.
    _head.store(h + 1, std::memory_order_release);
    return true;
  }

  bool empty() const noexcept
  {
    // Queue is empty when both indices are identical; use acquire to synchronise with opposite
    // side.
    return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
  }

  std::size_t size() const noexcept
  {
    // Size is simply the distance between writer and reader indices.
    const std::size_t h = _head.load(std::memory_order_acquire);
    const std::size_t t = _tail.load(std::memory_order_acquire);
    return t - h;
  }
};
} // namespace hft::spsc
