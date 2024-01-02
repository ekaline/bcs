#ifndef _EKA_FH_Q_H
#define _EKA_FH_Q_H

#include "Efh.h"

class EkaDev;
class EkaFhGroup;

static const size_t FH_MSG_ELEM_SIZE = 256;
class alignas(FH_MSG_ELEM_SIZE) fh_msg {
 public:
  enum class MsgType : uint8_t { REAL = 0, DUMMY = 1 };
  
  uint64_t    sequence;
  uint8_t     gr_id;
  MsgType     type;
  char        pad[2];
  int32_t     push_cpu_id;
  uint64_t    push_ts;

  static const size_t MSG_SIZE = FH_MSG_ELEM_SIZE -
    sizeof(sequence) -
    sizeof(gr_id) -
    sizeof(pad) -
    sizeof(push_cpu_id) -
    sizeof(push_ts);

  char        data[MSG_SIZE]; // MSG_SIZE = 232 bytes
};

class fh_q {
 public:
  enum { MAX_ELEMS = 8 * 1024 * 1024 };
  fh_q (struct EfhCtx* pEfhCtx,
	EkaSource _exch,
	EkaFhGroup* parent_gr,
	uint8_t _id,
	uint _qsize) {
    dev = pEfhCtx->dev;

    q_len        = 0;
    max_len      = 0;
    ever_max_len = 0;
    exch         = _exch;
    gr_id        = _id;
    EKA_LOG("creating Q for %s:%u",EKA_EXCH_DECODE(exch),gr_id);

    //  EKA_LOG("preallocating %u free fh_msg elements",qsize);
    qsize        = _qsize;
    msg          = new fh_msg[qsize];
    if (msg == NULL) on_error("msg == NULL");
    wr           = 0;
    rd           = 0;
    gr           = parent_gr;
  }
  
  ~fh_q() {
    delete[] msg;
    EKA_LOG("%s:%u Q deleted",EKA_EXCH_DECODE(exch),gr_id);
  }

  int invalidate() {
    delete[] msg;
    msg          = new fh_msg[qsize];
    wr           = 0;
    rd           = 0;
    EKA_LOG("%s:%u Q invalidated",EKA_EXCH_DECODE(exch),gr_id);
    return 0;
  }

  inline auto next   (uint32_t curr) { return (curr + 1) % qsize; }  
  inline auto is_full()              { return q_len > qsize - 10; }  
  inline auto is_empty()             { return q_len == 0;         }
  inline auto get_len()              { return q_len;              }
  inline auto get_max_len()          { return max_len;            }
  inline void reset_max_len()        { max_len = 0; return;       }
  inline auto get_ever_max_len()     { return ever_max_len;       }
  inline auto get_1st_seq()          { return msg[next(rd)].sequence; }  
    
  inline fh_msg*      push() {
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
  
  inline fh_msg* pop() {
    if (is_empty()) on_error("%s:%u: popping from empty Q",EKA_EXCH_DECODE(exch),gr_id);
    rd = next(rd);
    q_len--;
    return &msg[rd];
  }

  EkaSource    exch;
  EkaFhGroup*  gr = NULL;

 private:
  EkaDev*       dev = NULL;

  uint         qsize = 0;

  uint32_t     rd = 0;
  uint32_t     wr = 0;
  uint         q_len = 0;

  fh_msg*      msg = NULL;
  uint8_t      gr_id = 0xFF;


  uint32_t     max_len = 0; //for debug
  uint32_t     ever_max_len = 0; //for debug
};

#endif
