#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <thread>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/resource.h> // For setpriority and PRIO_PROCESS
    #include <sys/time.h>     // Required on some Unix systems for setpriority
    #include <unistd.h>       // For general POSIX definitions
#endif


namespace rt {

  enum class Priority { Low, High };

  struct CapturedStack;
  extern thread_local std::shared_ptr<CapturedStack> gCurrentAsyncStack;
  std::shared_ptr<CapturedStack> captureCurrentStack();

  class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
    
    void setLowPriority();    
  public:
    explicit ThreadPool(size_t threads, Priority priority);

    template <class F>
    auto enqueue(F &&f) -> std::future<std::invoke_result_t<F>> {
      using return_type = std::invoke_result_t<F>;
      
      auto currentStack = captureCurrentStack();

      auto task =
        std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
      std::future<return_type> res = task->get_future();
      {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (!stop)
          tasks.emplace([task, currentStack]() { 
            auto old = gCurrentAsyncStack;
            gCurrentAsyncStack = currentStack;
            (*task)(); 
            gCurrentAsyncStack = old;
          });
      }
      condition.notify_one();
      return res;
    }
    
    ~ThreadPool();
  };
  
} // namespace rt

#endif
