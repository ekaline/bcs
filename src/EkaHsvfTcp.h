#ifndef _EkaHsvfTcp_h_
#define _EkaHsvfTcp_h_

#include "EkaDev.h"

class EkaHsvfTcp {
 public:
  EkaHsvfTcp(EkaDev* dev, int sock);
  // ~EkaHsvfTcp();

  EkaOpResult getTcpMsg(uint8_t** msgBuf);

 private:
  bool        hasValidMsg();
  int         shiftAndFillBuf();
  void        checkIntegrity();
  /* ----------------------------- */
  
  static const int MAX_MSG_SIZE = 1024;
  static const int MSG_BUF_SIZE = 2048;

  EkaDev* dev = NULL;
  int     m_sock = -1;
  uint8_t m_msgBuf[MSG_BUF_SIZE] = {};

  int     m_validBytes = 0;
  int     m_firstValidByte = 0;
  int     m_msgLen = 0;

};

#endif
