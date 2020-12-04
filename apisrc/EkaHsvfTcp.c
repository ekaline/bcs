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
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sched.h>
#include <time.h>

#include "EkaHsvfTcp.h"
#include "eka_hsvf_box_messages.h"

uint trailingZeros(uint8_t* p, uint maxChars);

EkaHsvfTcp::EkaHsvfTcp(EkaDev* dev, int sock) {
  m_dev            = dev;
  m_sock           = sock;
  m_validBytes     = 0;
  m_firstValidByte = 0;
}
/* ----------------------------- */

bool EkaHsvfTcp::hasValidMsg() {
  if (m_msgBuf[m_firstValidByte] != HsvfSom) 
    on_error("0x%x != 0x%x",m_msgBuf[m_firstValidByte],HsvfSom);

  for (auto idx = m_firstValidByte + m_msgLen + 1; idx < m_firstValidByte + m_validBytes; idx ++) {
    if (m_msgBuf[idx] == HsvfEom) {
      m_msgLen += trailingZeros(&m_msgBuf[idx + 1], m_validBytes - m_msgLen);
      return true;
    }
    m_msgLen++;
  }
  if (m_validBytes > MAX_MSG_SIZE)
    on_error("Valid message is not found in %d validBytes",m_validBytes);

  return false;
}
/* ----------------------------- */
int EkaHsvfTcp::shiftAndFillBuf() {
  if (m_firstValidByte != 0) {
    memcpy(&m_msgBuf[0],&m_msgBuf[m_firstValidByte],m_validBytes);
    m_firstValidByte = 0;
  }
  
  int readBytes = recv(m_sock,&m_msgBuf[m_validBytes],MSG_BUF_SIZE - m_validBytes,MSG_WAITALL);
  if (readBytes < 0) on_error("Retransmit Server connection reset by peer");

  m_validBytes += readBytes;
  return 0;
}

/* ----------------------------- */

EkaOpResult EkaHsvfTcp::getTcpMsg(uint8_t** msgBuf) {
  checkIntegrity();

  while (! hasValidMsg()) {
    shiftAndFillBuf();
  }

  *msgBuf = &m_msgBuf[m_firstValidByte];

  m_validBytes     -= m_msgLen;
  m_firstValidByte += m_msgLen;
  m_msgLen          = 0;

  return EKA_OPRESULT__OK;
}

/* ----------------------------- */
void EkaHsvfTcp::checkIntegrity() {
  if (m_validBytes < 0 || m_validBytes > MSG_BUF_SIZE) 
    on_error("m_validBytes = %d ",m_validBytes);

  if (m_firstValidByte < 0 || m_firstValidByte > m_validBytes)
    on_error("m_firstValidByte = %d, m_validBytes = %d",m_firstValidByte,m_validBytes);

  if (m_msgLen < 0 || m_msgLen > m_validBytes)
    on_error("m_msgLen = %d, m_validBytes = %d",m_msgLen,m_validBytes);
}
