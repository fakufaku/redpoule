/*
 * Simple implementation of a thread pool
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
#ifndef __REDPOULE_H__
#define __REDPOULE_H__

#include <unistd.h>

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace redpoule {

const size_t max_n_threads = std::thread::hardware_concurrency();

class ThreadPool {
 private:
  // state
  bool _running = false;

  // attributes
  int _n_threads = 0;                 // number of threads
  std::vector<std::thread> _workers;  // the threads in the pool
  std::vector<bool>
      _busy;  // flag that indicates if the threads are working or not
  std::mutex _task_queue_mutex;  // mutex for the task queue
  std::condition_variable cv;    // get notification when the queue is empty
  std::queue<std::function<void()>> _task_queue;  // the task queue

  // private methods
  void infinite_loop(int thread_id);

 public:
  ThreadPool(size_t n_threads) : _running(true), _n_threads(n_threads) {
    _busy.resize(_n_threads, false);
    for (int t = 0; t < _n_threads; t++)
      _workers.emplace_back(std::thread(&ThreadPool::infinite_loop, this, t));
  }
  ThreadPool() : ThreadPool(max_n_threads) {}
  ~ThreadPool() {
    _running = false;

    std::for_each(_workers.begin(), _workers.end(),
                  [](std::thread &x) { x.join(); });
  }

  // getter for the number of threads used
  size_t get_num_threads() { return _n_threads; }

  void push(std::function<void()> &&task);
  template <class Func>
  void parallel_for(size_t start, size_t end, Func &&f);
  void wait() {
    // wait for all tasks to be finished
    std::unique_lock<std::mutex> lock(_task_queue_mutex);
    cv.wait(lock, [this] {
      return _task_queue.empty() && std::none_of(_busy.begin(), _busy.end(), [](bool v) { return v; });
    });
  }
};

void ThreadPool::push(std::function<void()> &&task) {
  /*
   * Push a new task into the queue
   */
  std::lock_guard<std::mutex> lock(_task_queue_mutex);
  _task_queue.push(std::forward<std::function<void()>>(task));
}

template <class Func>
void ThreadPool::parallel_for(size_t start, size_t end, Func &&f) {
  if (start >= end) return;

  for (size_t t = 0; t < get_num_threads(); t++) {
    size_t block_size = end - start;
    size_t block_start = t * block_size / get_num_threads();
    size_t block_end = (t + 1) == get_num_threads()
                           ? block_size
                           : (t + 1) * block_size / get_num_threads();
    push(std::bind(std::forward<Func>(f), block_start, block_end));
  }

  wait();
}

void ThreadPool::infinite_loop(int thread_id) {
  /*
   * The infinite loop run by the worker threads
   */
  std::function<void()> task;
  while (_running) {
    // get new task
    {
      std::lock_guard<std::mutex> lock(_task_queue_mutex);
      if (_task_queue.size() == 0) {
        _busy[thread_id] = false;
        continue;
      }

      task = std::move(_task_queue.front());
      _task_queue.pop();
      _busy[thread_id] = true;
    }

    // execute the task
    task();

    // notify that we finished one job
    {
      std::lock_guard<std::mutex> lock(_task_queue_mutex);
      _busy[thread_id] = false;
      cv.notify_all();
    }
  }
}

}  // namespace redpoule
#endif
