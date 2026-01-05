#include "ThreadPool.h"


namespace rt {

  ThreadPool::ThreadPool(size_t threads, Priority priority) : stop(false) {
    for (size_t i = 0; i < threads; ++i)
      workers.emplace_back([this, priority] {
        if(priority == Priority::Low) this->setLowPriority();
        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(this->queueMutex);
            this->condition.wait(
                                 lock, [this] { return this->stop || !this->tasks.empty(); });
            if (this->stop && this->tasks.empty())
              return;
            task = std::move(this->tasks.front());
            this->tasks.pop();
          }
          task();
        }
      });
  }
  
  ThreadPool::~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
      worker.join();
  }
  
  void ThreadPool::setLowPriority() {
#ifdef _WIN32
    // Windows: Set thread to "Below Normal" or "Lowest"
    // THREAD_PRIORITY_BELOW_NORMAL is ideal for background compilation.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
    // Linux/Unix: Use 'setpriority' for the specific thread (TID).
    // Niceness ranges from -20 (highest) to 19 (lowest).
    // 10 is a good "background" value.
    setpriority(PRIO_PROCESS, 0, 10);
#endif
  }
  
  
} // namespace rt
