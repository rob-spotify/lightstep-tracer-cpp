#include "benchmark/benchmark.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "common/circular_buffer.h"
#include "test/baseline_circular_buffer.h"
using namespace lightstep;

const int N = 10000;

//--------------------------------------------------------------------------------------------------
// ConsumeBufferNumbers
//--------------------------------------------------------------------------------------------------
uint64_t ConsumeBufferNumbers(
    BaselineCircularBuffer<uint64_t>& buffer) noexcept {
  uint64_t result = 0;
  buffer.Consume([&](std::unique_ptr<uint64_t>&& x) {
    result += *x;
    x.reset();
  });
  return result;
}

uint64_t ConsumeBufferNumbers(CircularBuffer<uint64_t>& buffer) noexcept {
  uint64_t result = 0;
  buffer.Consume(
      buffer.size(), [&](CircularBufferRange<AtomicUniquePtr<uint64_t>> &
                         range) noexcept {
        range.ForEach([&](AtomicUniquePtr<uint64_t> & ptr) noexcept {
          result += *ptr;
          ptr.Reset();
          return true;
        });
      });
  return result;
}

//--------------------------------------------------------------------------------------------------
// GenerateNumbersForThread
//--------------------------------------------------------------------------------------------------
template <class Buffer>
static void GenerateNumbersForThread(Buffer& buffer, int n,
                                     std::atomic<uint64_t>& sum) noexcept {
  thread_local std::mt19937_64 random_number_generator{std::random_device{}()};
  for (int i = 0; i < n; ++i) {
    auto x = random_number_generator();
    std::unique_ptr<uint64_t> element{new uint64_t{x}};
    if (buffer.Add(element)) {
      sum += x;
    }
  }
}

//--------------------------------------------------------------------------------------------------
// GenerateNumbers
//--------------------------------------------------------------------------------------------------
template <class Buffer>
static uint64_t GenerateNumbers(Buffer& buffer, int num_threads,
                                int n) noexcept {
  std::atomic<uint64_t> sum{0};
  std::vector<std::thread> threads(num_threads);
  for (auto& thread : threads) {
    thread = std::thread{GenerateNumbersForThread<Buffer>, std::ref(buffer), n,
                         std::ref(sum)};
  }
  for (auto& thread : threads) {
    thread.join();
  }
  return sum;
}

//--------------------------------------------------------------------------------------------------
// ConsumeNumbers
//--------------------------------------------------------------------------------------------------
template <class Buffer>
static void ConsumeNumbers(Buffer& buffer, uint64_t& sum,
                           std::atomic<bool>& finished) noexcept {
  while (!finished) {
    sum += ConsumeBufferNumbers(buffer);
  }
  sum += ConsumeBufferNumbers(buffer);
}

//--------------------------------------------------------------------------------------------------
// RunSimulation
//--------------------------------------------------------------------------------------------------
template <class Buffer>
static void RunSimulation(Buffer& buffer, int num_threads, int n) noexcept {
  std::atomic<bool> finished{false};
  uint64_t consumer_sum{0};
  std::thread consumer_thread{ConsumeNumbers<Buffer>, std::ref(buffer),
                              std::ref(consumer_sum), std::ref(finished)};
  uint64_t producer_sum = GenerateNumbers(buffer, num_threads, n);
  finished = true;
  consumer_thread.join();
  if (consumer_sum != producer_sum) {
    std::cerr << "Sumulation failed: consumer_sum != producer_sum\n";
    std::terminate();
  }
}

//--------------------------------------------------------------------------------------------------
// BM_BaselineBuffer
//--------------------------------------------------------------------------------------------------
static void BM_BaselineBuffer(benchmark::State& state) {
  const size_t max_elements = 500;
  auto num_threads = state.range(0);
  const int n = N / num_threads;
  BaselineCircularBuffer<uint64_t> buffer{max_elements};
  for (auto _ : state) {
    RunSimulation(buffer, num_threads, n);
  }
}

BENCHMARK(BM_BaselineBuffer)->Arg(1)->Arg(2)->Arg(4);

//--------------------------------------------------------------------------------------------------
// BM_LockFreeBuffer
//--------------------------------------------------------------------------------------------------
static void BM_LockFreeBuffer(benchmark::State& state) {
  const size_t max_elements = 500;
  auto num_threads = state.range(0);
  const int n = N / num_threads;
  CircularBuffer<uint64_t> buffer{max_elements};
  for (auto _ : state) {
    RunSimulation(buffer, num_threads, n);
  }
}

BENCHMARK(BM_LockFreeBuffer)->Arg(1)->Arg(2)->Arg(4);

//--------------------------------------------------------------------------------------------------
// BENCHMARK_MAIN
//--------------------------------------------------------------------------------------------------
BENCHMARK_MAIN();
