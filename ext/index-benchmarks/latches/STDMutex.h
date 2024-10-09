#pragma once

#include <memory>
#include <mutex>

namespace stdmutex_lock
{
  // Non-RW Mutex
  class MutexLock
  {
  public:
    MutexLock() {}

    inline void lock(uint64_t * = nullptr) { lock_.lock(); }

    inline void unlock(uint64_t * = nullptr) { lock_.unlock(); }

    inline void read_lock(uint64_t * = nullptr) { lock(); }

    inline void read_unlock(uint64_t * = nullptr) { unlock(); }

  private:
    std::mutex lock_;
  };

} // namespace std_lock