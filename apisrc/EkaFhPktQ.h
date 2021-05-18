#ifndef _EKA_FH_PKT_Q_H_
#define _EKA_FH_PKT_Q_H_

#include "Efh.h"

#include "EkaFhPktQElem.h"

class EkaDev;
class EkaFhGroup;

template <
const int QELEM_SIZE, 
  const int MAX_ELEMS, 
  class PktElem>
class EkaFhPktQ {
 public:
  /* ##################################################################### */
  EkaFhPktQ (EkaDev*     _dev,
	     EkaSource   _exch, 
	     EkaFhGroup* _gr, 
	     uint8_t     _gr_id) {
    dev          = _dev;

    q_len        = 0;
    max_len      = 0;
    ever_max_len = 0;
    exch         = _exch;
    gr_id        = _gr_id;
    EKA_LOG("%s:%u: creating PktQ with %d PktElem",
	    EKA_EXCH_DECODE(exch),gr_id,MAX_ELEMS);

    pktElem = (PktElem*) malloc (MAX_ELEMS * sizeof(PktElem));
    if (pktElem == NULL) on_error("malloc failed");
    wr = 0;
    rd = 0;
    gr = _gr;

    if (gr == NULL) on_error("gr == NULL");
  }
  /* ##################################################################### */
  ~EkaFhPktQ() {
    delete (pktElem);
    EKA_LOG("%s:%u deleted",EKA_EXCH_DECODE(exch),gr_id);
  }
  /* ##################################################################### */

  inline uint32_t next(uint32_t curr) { 
    return (++curr) % MAX_ELEMS; 
  }
  /* ##################################################################### */

  inline bool is_empty() {
    return q_len == 0;
  }

  /* ##################################################################### */
  inline bool is_full() {
    return q_len > MAX_ELEMS - 10;
  }

  /* ##################################################################### */
  PktElem* push() {
    //  assert (q_len < MAX_ELEMS);
    if (is_full()) {
      on_error("%s:%u Q is full -- Too slow Processing (maybe Gap Recovery): q_len (%u) >= MAX_ELEMS (%u) -- emptying Q",
	       EKA_EXCH_DECODE(exch),gr_id,get_len(),MAX_ELEMS);
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
    return &pktElem[wr];
  }


  /* ##################################################################### */
  PktElem* pop() {
    if (is_empty()) on_error("%s:%u: popping from empty Q",EKA_EXCH_DECODE(exch),gr_id);
    rd = next(rd);
    q_len--;
    return &pktElem[rd];
  }
  /* ##################################################################### */
  uint32_t get_len() { 
    return q_len; 
  }
  /* ##################################################################### */
  uint32_t get_max_len() { 
    return max_len; 
  }
  /* ##################################################################### */
  void reset_max_len() { 
    max_len = 0; 
    return; 
  }
  /* ##################################################################### */
  uint32_t get_ever_max_len() { 
    return ever_max_len; 
  }
  /* ##################################################################### */

 //  using PktElem = EkaFhPktQElem<QELEM_SIZE>;
  
  /* PktElem*     push(); */
  /* PktElem*     pop(); */
  /* void         push_completed(); */
  /* void         pop_completed(); */
  /* inline uint32_t     next(uint32_t curr); */
  /* inline bool         is_full(); */
  /* bool         is_empty(); */

  /* uint32_t     get_len(); */
  /* uint32_t     get_max_len(); */
  /* void         reset_max_len(); */
  /* uint32_t     get_ever_max_len(); */


  EkaSource    exch;
  EkaFhGroup*  gr;

  //  volatile uint64_t     currUdpSequence; // for tracing

 private:
  EkaDev*       dev;

  uint         qsize;

  uint32_t     rd;
  uint32_t     wr;
  uint         q_len;

  PktElem*     pktElem;
  uint8_t      gr_id;


  uint32_t     max_len; //for debug
  uint32_t     ever_max_len; //for debug
};

#endif
