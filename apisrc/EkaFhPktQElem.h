#ifndef _EKA_FH_PKT_Q_ELEM_H_
#define _EKA_FH_PKT_Q_ELEM_H_

template <const int QELEM_SIZE>
class EkaFhPktQElem {
 public:
  uint64_t    sequence;
  uint8_t     gr_id;
  char        pad[3];
  int32_t     push_cpu_id;
  uint16_t    pktSize;

  static const int PKT_SIZE = 
    QELEM_SIZE          -
    sizeof(sequence)    - 
    sizeof(gr_id)       - 
    sizeof(pad)         - 
    sizeof(push_cpu_id) - 
    sizeof(pktSize);

  uint8_t     data[PKT_SIZE]; // 

};

#endif
