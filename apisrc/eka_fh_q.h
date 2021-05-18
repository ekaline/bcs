#ifndef _EKA_FH_Q_H
#define _EKA_FH_Q_H

#include "Efh.h"

class EkaDev;
class EkaFhGroup;

class fh_msg {
 public:
  uint64_t    sequence;
  uint8_t     gr_id;
  char        pad[3];
  int32_t     push_cpu_id;
  uint64_t    push_ts;
  enum { MSG_SIZE = (128 - sizeof(sequence) - sizeof(gr_id) - sizeof(pad) - sizeof(push_cpu_id) - sizeof(push_ts))};

  char        data[MSG_SIZE]; // MSG_SIZE = 104 bytes
};

class fh_q {
 public:
  enum { MAX_ELEMS = 8 * 1024 * 1024 };
  fh_q (struct EfhCtx* pEfhCtx,EkaSource exch, EkaFhGroup* gr, uint8_t gr_id, uint qsize);
  ~fh_q();
  
  fh_msg*      push();
  fh_msg*      pop();
  void         push_completed();
  void         pop_completed();
  inline uint32_t     next(uint32_t curr);
  inline bool         is_full();
  bool         is_empty();

  uint32_t     get_len();
  uint32_t     get_max_len();
  void         reset_max_len();
  uint32_t     get_ever_max_len();
  EkaSource    exch;
  EkaFhGroup*  gr;

  //  volatile uint64_t     currUdpSequence; // for tracing

 private:
  EkaDev*       dev;

  uint         qsize;

  uint32_t     rd;
  uint32_t     wr;
  uint         q_len;

  fh_msg*      msg;
  uint8_t      gr_id;


  uint32_t     max_len; //for debug
  uint32_t     ever_max_len; //for debug
};

#endif
