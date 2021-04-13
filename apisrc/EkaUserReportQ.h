#ifndef _EkaUserReportQ_h_
#define _EkaUserReportQ_h_

#include <atomic>

#include "EkaDev.h"

struct EkaUserReportElem {
  feedback_dma_report_t hdr = {};
  uint8_t               data[1536 - sizeof(feedback_dma_report_t)] = {};
};

class EkaUserReportQ {
 public:
  const uint MAX_ELEM_SIZE = 1536;
  const uint Q_ELEMS       = 1024;
  
  /* -------------------------------- */

  EkaUserReportQ(EkaDev* dev);
  
  bool isEmpty();
  inline bool isFull();

  EkaUserReportElem* push(const void* payload, uint len);
  EkaUserReportElem* pop();
  inline uint32_t    next(uint32_t curr);

  /* -------------------------------- */
 private:
  EkaUserReportElem* qElem = NULL;

  uint32_t rdPtr = 0;
  uint32_t wrPtr = 0;
  
  // volatile uint64_t wrCnt = 0;
  // volatile uint64_t rdCnt = 0;

  std::atomic<uint64_t> wrCnt = 0;
  std::atomic<uint64_t> rdCnt = 0;

  //  volatile int64_t qLen  = 0;
  std::atomic<int64_t> qLen  = 0;

 private:
  EkaDev* dev = NULL;

};

#endif
