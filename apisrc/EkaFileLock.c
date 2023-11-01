#include "eka_macros.h"
#include <chrono>
#include <thread>

#include <sys/file.h>

#include "EkaFileLock.h"

EkaFileLock::EkaFileLock(const char *fileName,
                         volatile bool *active) {
  lockfd_ = open(fileName, O_CREAT | O_RDWR, 0666);
  if (lockfd_ == -1)
    on_error("Failed opening %s", fileName);
  epmActive_ = active;
}

EkaFileLock::~EkaFileLock() {
  unlock("Deleting lock");
  close(lockfd_);
}

int EkaFileLock::lock(const char *msg) {
  auto start = std::chrono::high_resolution_clock::now();

  int rc;
  while (*epmActive_) {
    rc = flock(lockfd_, LOCK_EX | LOCK_NB);
    if (rc == 0)
      break;
    if (errno == EWOULDBLOCK) {
      EKA_LOG("%s: Lock is already held by another process "
              "(rc =%d)",
              msg, rc);
      auto now = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(
              now - start)
              .count() > LockTimeOutSec)
        on_error("%s: Lock acquire time out: "
                 "waiting longer than %d seconds",
                 msg, LockTimeOutSec);

      std::this_thread::yield();
    } else {
      on_error("%s: Failed on getting flock()", msg);
    }
  }
  // EKA_LOG("%s: lock acquired (rc = %d)", msg, rc);

  return 0;
}

int EkaFileLock::unlock(const char *msg) {
  int rc = flock(lockfd_, LOCK_UN);
  if (rc != 0)
    on_error("Failed on releasing flock(): rc = %d", rc);

  // EKA_LOG("%s: lock released", msg);
  return 0;
}
