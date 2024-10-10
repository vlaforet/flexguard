#pragma once

#include <memory>

extern "C"
{
#include <lock_if.h>
}

namespace libslock
{
  // Non-RW Libslock lock
  class LibslockLock
  {
  public:
    LibslockLock()
    {
      libslock_init(&lock_);
    }

    ~LibslockLock()
    {
      libslock_destroy(&lock_);
    }

    inline void lock(uint64_t * = nullptr)
    {
      libslock_lock(&lock_);
    }

    inline void unlock(uint64_t * = nullptr) { libslock_unlock(&lock_); }

    inline void read_lock(uint64_t * = nullptr) { lock(); }

    inline void read_unlock(uint64_t * = nullptr) { unlock(); }

  private:
    libslock_t lock_;
  };

} // namespace libslock