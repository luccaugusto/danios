// lib/pcm_ring/pcm_ring.h — single-producer/single-consumer lock-free ring of
// int16 PCM samples. In the Music app the producer is the MP3 feeder on the
// Arduino loop task and the consumer is the A2DP source callback on the
// Bluetooth stack task; this ring is the ONLY thing both touch. std C++17.
//
// Indices are monotonic counters masked into a power-of-two buffer: head_ is
// written only by the producer, tail_ only by the consumer. requestClear() is
// the one cross-task operation, done as a request the consumer honors on its
// next read (only the consumer may move tail_).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class PcmRing {
 public:
  explicit PcmRing(size_t capacitySamples);  // rounded UP to a power of two
  size_t capacity() const;
  size_t available() const;                  // samples ready to read
  size_t freeSpace() const;                  // samples writable right now
  size_t write(const int16_t* src, size_t n);  // producer only; returns written
  size_t read(int16_t* dst, size_t n);         // consumer only; returns read
  // Producer-side skip: on its NEXT read the consumer discards every sample
  // written before this call (samples written after survive). Used on manual
  // track changes so the old track's buffered tail doesn't play.
  void requestClear();

 private:
  std::vector<int16_t> buf_;
  size_t mask_;
  std::atomic<size_t> head_{0};       // total written (monotonic, producer-owned)
  std::atomic<size_t> tail_{0};       // total read (monotonic, consumer-owned)
  std::atomic<size_t> clearMark_{0};  // discard everything below this mark
  std::atomic<bool> clearPending_{false};
};
