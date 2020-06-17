#ifndef _EKA_SERV_THREAD_H_
#define _EKA_SERV_THREAD_H_

#include <thread>

class EkaDev;

void ekaServThread(EkaDev* dev);

class EkaServThread {
 public:
  enum class DMA_TYPE : uint8_t { FIRE_REPORT = 1,  FAST_PATH = 2 };

  EkaServThread(EkaDev* pEkaDev) {
    dev = pEkaDev;
    active = false;
    id = std::thread(ekaServThread,dev);
    id.detach();
  }

  volatile bool active = false;
 private:
  EkaDev*   dev;
  std::thread id;

};

#endif
