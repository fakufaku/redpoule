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
#include <algorithm>
#include <functional>
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
  int _n_threads = 0;                             // number of threads
  std::vector<std::thread> _threads;              // the threads in the pool
  std::mutex _task_queue_mutex;                   // mutex for the task queue
  std::queue<std::function<void()>> _task_queue;  // the task queue

  // private methods
  void infinite_loop();

 public:
  ThreadPool(size_t n_threads) : _running(true), _n_threads(n_threads) {
    _threads.resize(_n_threads);
    for (int t = 0; t < _n_threads; t++)
      _threads[t] = std::thread(&ThreadPool::infinite_loop, this);
  }
  ThreadPool() : ThreadPool(max_n_threads) {}
  ~ThreadPool() {
    _running = false;

    std::for_each(_threads.begin(), _threads.end(),
                  [](std::thread &x) { x.join(); });
  }

  // getter for the number of threads used
  size_t get_num_threads() { return _n_threads; }

  void push(std::function<void()> &&task);
  template <class Func>
  void parallel_for(size_t start, size_t end, Func &&f);
};

void ThreadPool::push(std::function<void()> &&task) {
  /*
   * Push a new task into the queue
   */
  std::unique_lock<std::mutex> lock(_task_queue_mutex);
  _task_queue.push(std::forward<std::function<void()>>(task));
}

template <class Func>
void ThreadPool::parallel_for(size_t start, size_t end, Func &&f) {

  if (start >= end)
    return;

  for (size_t t = 0; t < get_num_threads(); t++) {
    size_t block_size = end - start;
    size_t block_start = t * block_size / get_num_threads();
    size_t block_end = (t + 1) == get_num_threads()
                      ? block_size
                      : (t + 1) * block_size / get_num_threads();
    push(std::bind(std::forward<Func>(f), block_start, block_end));
  }

  // busy wait
  // TODO replace with condition variable
  // https://en.cppreference.com/w/cpp/thread/condition_variable
  while (_task_queue.size() > 0)
    ;
}

void ThreadPool::infinite_loop() {
  /*
   * The infinite loop run by the worker threads
   */
  std::function<void()> task;
  while (_running) {
    // get new task
    {
      std::unique_lock<std::mutex> lock(_task_queue_mutex);
      if (_task_queue.size() == 0) continue;
      task = std::move(_task_queue.front());
      _task_queue.pop();
    }

    // run it
    task();
  }
}

}  // namespace redpoule
