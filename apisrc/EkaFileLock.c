#include "eka_macros.h"
#include <chrono>
#include <thread>

#include <sys/file.h>

#include "EkaFileLock.h"

EkaFileLock::EkaFileLock(const char *fileName,
                         volatile bool *active) {
  lockfd_ = open(fileName, O_CREAT | O_RDWR, 0666);
  if (lockfd_ == -1)
    on_error("open");
  epmActive_ = active;
}

EkaFileLock::~EkaFileLock() {
  unlock();
  close(lockfd_);
}

int EkaFileLock::lock() {
  auto start = std::chrono::high_resolution_clock::now();

  while (*epmActive_ &&
         flock(lockfd_, LOCK_EX | LOCK_NB) == -1) {
    if (errno == EWOULDBLOCK) {
      EKA_LOG("Lock is already held by another process");
      auto now = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(
              now - start)
              .count() > LockTimeOutSec)
        on_error("Lock acquire time out: "
                 "waiting longer than %d seconds",
                 LockTimeOutSec);

      std::this_thread::yield();
    } else {
      on_error("Failed on getting flock()");
    }
  }
  return 0;
}

int EkaFileLock::unlock() {
  int rc = flock(lockfd_, LOCK_UN);
  if (rc != 0)
    on_error("Failed on releasing flock(): rc = %d", rc);

  EKA_LOG("Process released the lock!");
  return 0;
}
