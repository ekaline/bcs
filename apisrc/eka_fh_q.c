#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <errno.h>
#include <thread>
#include <assert.h>
#include <time.h>

#include "EkaDev.h"

#include "eka_fh_q.h"
#include "EkaFhGroup.h"

fh_q::fh_q(struct EfhCtx* pEfhCtx, EkaSource ec, EkaFhGroup* parent_gr, uint8_t id, uint qs) {
  dev = pEfhCtx->dev;

  q_len = 0;
  max_len = 0;
  ever_max_len = 0;
  exch = ec;
  gr_id = id;
  EKA_LOG("creating Q for %s:%u",EKA_EXCH_DECODE(exch),gr_id);

  //  EKA_LOG("preallocating %u free fh_msg elements",qsize);
  qsize = qs;
  msg = (fh_msg*) malloc (qsize * sizeof(fh_msg));
  assert (msg != NULL);
  wr = 0;
  rd = 0;
  /* rd_cnt = 0; */
  /* wr_cnt = 0; */
  gr = parent_gr;
  //  currUdpSequence = 0;
}

fh_q::~fh_q() {
  //  EKA_LOG("GR%u deleting...",gr_id);
  free(msg);
  EKA_LOG("%s:%u deleted",EKA_EXCH_DECODE(exch),gr_id);
}

inline uint32_t fh_q::next (uint32_t curr) { return (++curr) % qsize; }

/* bool fh_q::is_empty() { */
/*   //  return q_len == 0; */
/*   if (wr_cnt < rd_cnt) on_error("%s:%u: wr_cnt %u < rd_cnt %u",EKA_EXCH_DECODE(exch),gr_id,wr_cnt,rd_cnt); */
/*   return wr_cnt == rd_cnt; */
/* } */
bool fh_q::is_empty() {
  return q_len == 0;
}

/* inline bool fh_q::is_full() { */
/*   //  return q_len == 0; */
/*   if (wr_cnt < rd_cnt) on_error("%s:%u: wr_cnt %u < rd_cnt %u",EKA_EXCH_DECODE(exch),gr_id,wr_cnt,rd_cnt); */
/*   return (wr_cnt - rd_cnt == qsize); */
/* } */


inline bool fh_q::is_full() {
  return q_len > qsize - 10;
}

/* fh_msg* fh_q::push() { */
/*   //  assert (q_len < qsize); */
/*   if (is_full()) { */
/*     EKA_WARN("WARNING: %s:%u Q is full -- Too slow Processing (maybe Gap Recovery): q_len (%u) >= qsize (%u) -- emptying Q, gr state = %u",EKA_EXCH_DECODE(exch),gr_id,get_len(),qsize,(uint)gr->state); */
/*     /\* wr = 0; *\/ */
/*     /\* rd = 0; *\/ */
/*     /\* q_len = 0; *\/ */
/*   } else { */
/*     uint32_t len = get_len(); */
/*     if (len > max_len) max_len = len; */
/*     if (max_len > ever_max_len) ever_max_len = max_len; */
/*     wr = next(wr); */
/*     //    wr_cnt++; */
/*   } */
/*   return &msg[wr]; */
/* } */

fh_msg* fh_q::push() {
  //  assert (q_len < qsize);
  if (is_full()) {
    on_error("%s:%u Q is full -- Too slow Processing (maybe Gap Recovery): q_len (%u) >= qsize (%u) -- emptying Q",
	     EKA_EXCH_DECODE(exch),gr_id,get_len(),qsize);
    wr = 0;
    rd = 0;
    q_len = 0;
  } else {
    uint32_t len = get_len();
    if (len > max_len) max_len = len;
    if (max_len > ever_max_len) ever_max_len = max_len;
    wr = next(wr);
    q_len++;
  }
  return &msg[wr];
}

void fh_q::push_completed() {
  //  wr_cnt++;
}
void fh_q::pop_completed() {
  //  rd_cnt++;
}
/* fh_msg* fh_q::pop() { */
/*   if (wr_cnt < rd_cnt) on_error("%s:%u: wr_cnt %u < rd_cnt %u",EKA_EXCH_DECODE(exch),gr_id,wr_cnt,rd_cnt); */
/*   rd = next(rd); */
/*   //  currUdpSequence = msg[rd].sequence; */
/*   //  rd_cnt++; */
/*   return &msg[rd]; */
/* } */

fh_msg* fh_q::pop() {
  if (is_empty()) on_error("%s:%u: popping from empty Q",EKA_EXCH_DECODE(exch),gr_id);
  //  if (wr_cnt < rd_cnt) on_error("%s:%u: wr_cnt %u < rd_cnt %u",EKA_EXCH_DECODE(exch),gr_id,wr_cnt,rd_cnt);
  rd = next(rd);
  q_len--;
  return &msg[rd];
}


//uint32_t fh_q::get_len()          { return wr_cnt - rd_cnt; }
uint32_t fh_q::get_len()          { return q_len; }
uint32_t fh_q::get_max_len()      { return max_len; }
void fh_q::reset_max_len()        { max_len = 0; return; }
uint32_t fh_q::get_ever_max_len() { return ever_max_len; }

