#ifndef __EKA_FILE_LOCK_H__
#define __EKA_FILE_LOCK_H__

#include "eka_macros.h"

class EkaFileLock {
public:
  EkaFileLock(const char *fileName, volatile bool *active);

  ~EkaFileLock();

  int lock(const char *msg);

  int unlock(const char *msg);

private:
  int lockfd_ = -1;
  volatile bool *epmActive_;
  const int LockTimeOutSec = 4;
};

#endif
