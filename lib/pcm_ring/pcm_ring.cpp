#include "pcm_ring.h"

namespace {
size_t roundUpPow2(size_t v) {
  size_t p = 1;
  while (p < v) p <<= 1;
  return p;
}
}  // namespace

PcmRing::PcmRing(size_t capacitySamples)
    : buf_(roundUpPow2(capacitySamples)), mask_(buf_.size() - 1) {}

size_t PcmRing::capacity() const { return buf_.size(); }

size_t PcmRing::available() const {
  return head_.load(std::memory_order_acquire) -
         tail_.load(std::memory_order_acquire);
}

size_t PcmRing::freeSpace() const { return capacity() - available(); }

size_t PcmRing::write(const int16_t* src, size_t n) {
  const size_t head = head_.load(std::memory_order_relaxed);
  const size_t free =
      capacity() - (head - tail_.load(std::memory_order_acquire));
  if (n > free) n = free;
  for (size_t i = 0; i < n; ++i) buf_[(head + i) & mask_] = src[i];
  head_.store(head + n, std::memory_order_release);
  return n;
}

size_t PcmRing::read(int16_t* dst, size_t n) {
  if (clearPending_.load(std::memory_order_acquire)) {
    const size_t mark = clearMark_.load(std::memory_order_relaxed);
    if (mark > tail_.load(std::memory_order_relaxed)) {
      tail_.store(mark, std::memory_order_release);
    }
    clearPending_.store(false, std::memory_order_release);
  }
  const size_t tail = tail_.load(std::memory_order_relaxed);
  size_t avail = head_.load(std::memory_order_acquire) - tail;
  if (n > avail) n = avail;
  for (size_t i = 0; i < n; ++i) dst[i] = buf_[(tail + i) & mask_];
  tail_.store(tail + n, std::memory_order_release);
  return n;
}

void PcmRing::requestClear() {
  clearMark_.store(head_.load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
  clearPending_.store(true, std::memory_order_release);
}
