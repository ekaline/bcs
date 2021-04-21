#include "EkaUserReportQ.h"

EkaUserReportQ::EkaUserReportQ(EkaDev* _dev) {
  dev = _dev;

  //  qElem = (EkaUserReportElem*) malloc(Q_ELEMS * sizeof(EkaUserReportElem));
  qElem = new EkaUserReportElem[Q_ELEMS];
  if (qElem == NULL) on_error("faild on malloc Q_ELEMS");
  //  memset(qElem,0,Q_ELEMS * sizeof(EkaUserReportElem));

  rdPtr = 0;
  wrPtr = 0;
  
  wrCnt = 0;
  rdCnt = 0;

  qLen  = 0;
}

/* -------------------------------- */
bool EkaUserReportQ::isEmpty() {

  if (rdCnt > wrCnt)
    on_error("rdCnt %ju >= wrCnt %ju",(uint64_t)rdCnt, (uint64_t)wrCnt);

  qLen = wrCnt - rdCnt;
  return qLen == 0; 
}
/* -------------------------------- */
inline bool EkaUserReportQ::isFull() {
  qLen = wrCnt - rdCnt;
  return qLen == Q_ELEMS - 1; 
}
/* -------------------------------- */
inline uint32_t EkaUserReportQ::next(uint32_t curr) { 
  return (++curr) % Q_ELEMS; 
}
/* -------------------------------- */

EkaUserReportElem* EkaUserReportQ::push(const void* payload, uint len) {
  if (isFull())
    on_error("User report Q is FULL! (qLen = %jd)",(int64_t)qLen);

  wrPtr = next(wrPtr);
  if (len > MAX_ELEM_SIZE)
    on_error("len %u > MAX_ELEM_SIZE %u",len,MAX_ELEM_SIZE);
  memcpy(&(qElem[wrPtr].hdr), payload,sizeof(qElem[wrPtr].hdr));
  //  memcpy(&(qElem[wrPtr].data),(uint8_t*)payload + sizeof(qElem[wrPtr].hdr),len - sizeof(qElem[wrPtr].hdr));
  memcpy(&(qElem[wrPtr].data),(uint8_t*)payload,len);

  wrCnt++;
  qLen = wrCnt - rdCnt;

  return &qElem[wrPtr];
}
/* -------------------------------- */

EkaUserReportElem* EkaUserReportQ::pop() {
  if (isEmpty())
    on_error("User report Q is EMPTY! (qLen = %jd)",(int64_t)qLen);
  if (rdCnt >= wrCnt)
    on_error("rdCnt %ju >= wrCnt %ju",(uint64_t)rdCnt, (uint64_t)wrCnt);
  rdPtr = next(rdPtr);
  rdCnt++;
  qLen = wrCnt - rdCnt;

  return &qElem[rdPtr];
}

