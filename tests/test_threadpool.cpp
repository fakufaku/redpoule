/*
 * Test file for the parallel loop
 *
 * Copyright 2020 Robin Scheibler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>

include "threadpool.hpp"

const size_t very_large = 100000000;

int main(int argc, char** argv) {
  int iam, nt;

  std::vector<int> foo(very_large);

  // fill the array
  int i = 0;
  for (auto& item : foo) item = i++;

  // increment in parallel
  // C++17 version, only works with GCC on Apple
  /*
  std::for_each(
      std::execution::par_unseq,
      foo.begin(),
      foo.end(),
      [](auto&& item)
      {
        item += 1;
      });
      */

  // get number of theads available
  const size_t nthreads = std::thread::hardware_concurrency();
  {
    // Pre loop
    std::cout << "parallel (" << nthreads << " threads):" << std::endl;
    std::vector<std::thread> threads(nthreads);
    std::mutex critical;
    for (int t = 0; t < nthreads; t++) {
      threads[t] = std::thread(std::bind(
          [&](const int bi, const int ei, const int t) {
            // loop over all items
            for (int i = bi; i < ei; i++) {
              // inner loop
              {
                /*
                const int j = i * i;
                // (optional) make output critical
                std::lock_guard<std::mutex> lock(critical);
                std::cout << j << std::endl;
                */
                for (int k = 0; k < 100; k++) foo[i]++;
              }
            }
          },
          t * foo.size() / nthreads,
          (t + 1) == nthreads ? foo.size() : (t + 1) * foo.size() / nthreads,
          t));
    }
    std::for_each(threads.begin(), threads.end(),
                  [](std::thread& x) { x.join(); });
    // Post loop
    std::cout << std::endl;
  }

  for (int i = 0; i < 10; i++) std::cout << foo[i] << std::endl;

  // with the thread pool
  std::cout << "Now with the thread pool" << std::endl;
  {
    ThreadPool thread_pool;
    for (int t = 0; t < thread_pool.get_num_threads(); t++) {
      thread_pool.push(std::bind(
          [&](const int bi, const int ei, const int t) {
            // loop over all items
            for (int i = bi; i < ei; i++) {
              // inner loop
              {
                /*
                const int j = i * i;
                // (optional) make output critical
                std::lock_guard<std::mutex> lock(critical);
                std::cout << j << std::endl;
                */
                for (int k = 0; k < 100; k++) foo[i]++;
              }
            }
          },
          t * foo.size() / thread_pool.get_num_threads(),
          (t + 1) == thread_pool.get_num_threads() ? foo.size() : (t + 1) * foo.size() / thread_pool.get_num_threads(),
          t));
    }
  }

  for (int i = 0; i < 10; i++) std::cout << foo[i] << std::endl;

  return 0;
}
