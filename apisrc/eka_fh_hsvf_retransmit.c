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

#include "eka_fh_book.h"
#include "eka_fh_group.h"
#include "eka_dev.h"
#include "Efh.h"
#include "Eka.h"
#include "eka_hsvf_box_messages.h"
#include "eka_fh.h"

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port);
int ekaUdpConnect(EkaDev* dev, int* sock, uint32_t ip, uint16_t port);
/* ----------------------------- */

static EkaOpResult sendLogin (FhBoxGr* gr) {
  EkaDev* dev = gr->dev;

  HsvfLogin msg = {};
  memset(&msg,' ',sizeof(msg));

  time_t rawtime;
  time (&rawtime);
  struct tm * ct = localtime (&rawtime);

  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%9ju",gr->txSeqNum ++);

  char timeStrBuf[10] = {};
  sprintf(timeStrBuf,"%02u%02u%02u",ct->tm_hour,ct->tm_min,ct->tm_sec);


  msg.SoM = HsvfSom;
  memcpy(msg.hdr.sequence     , seqStrBuf   , sizeof(msg.hdr.sequence));
  memcpy(&msg.hdr.MsgType     , "LI"        , sizeof(msg.hdr.MsgType));

  // USER:PWD are left blank
  /* memcpy(&msg.User,gr->auth_user,sizeof(std::min(sizeof(msg.User),sizeof(gr->auth_user)))); */
  /* memcpy(&msg.Pwd,gr->auth_passwd,sizeof(std::min(sizeof(msg.Pwd),sizeof(gr->auth_passwd)))); */

  memcpy(msg.TimeStamp         , timeStrBuf , sizeof(msg.TimeStamp));
  memcpy(msg.ProtocolVersion   , "C7"       , sizeof(msg.ProtocolVersion));
  msg.EoM = HsvfEom;

#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Box Login sent: %s",EKA_EXCH_DECODE(gr->exch),gr->id,(char*)&msg);
#else	
  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) < 0) {
    EKA_WARN("BOX Login send failed");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
  EKA_LOG("%s:%u Box Login sent: %s",EKA_EXCH_DECODE(gr->exch),gr->id,(char*)&msg);
#endif
  return EKA_OPRESULT__OK;
}
/* ----------------------------- */
static EkaOpResult getLoginResponse(FhBoxGr* gr) {
  EkaDev* dev = gr->dev;

#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Login Acknowledge received",EKA_EXCH_DECODE(gr->exch),gr->id);
#else	
  HsvfLoginAck msg = {};

  if (recv(gr->snapshot_sock,&msg,sizeof(msg),MSG_WAITALL) != sizeof(msg))
    on_error("Retransmit Server connection reset by peer (failed to receive Login Ack)");

  if (msg.SoM != HsvfSom) 
    on_error("msg.SoM = 0x%0x, while expected HsvfSom = 0x%0x",msg.SoM,HsvfSom);

  if (msg.EoM != HsvfEom) 
    on_error("msg.Eom = 0x%0x, while expected HsvfEom = 0x%0x",msg.EoM,HsvfEom);

  if (memcmp(&msg.hdr.MsgType,"KI",2) != 0) 
    on_error("Received msg.hdr.MsgType \'%c%c\' != Expected \'KI\'",
	     msg.hdr.MsgType[0],msg.hdr.MsgType[1]);

  EKA_LOG("%s:%u Login Acknowledge received",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif

  return EKA_OPRESULT__OK;
}

/* ----------------------------- */
static EkaOpResult sendRequest(FhBoxGr* gr) {
  EkaDev* dev = gr->dev;


  HsvfRetransmissionRequest msg = {};
  memset(&msg,' ',sizeof(msg));

  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%9ju",gr->txSeqNum ++);

  char startStrBuf[10] = {};
  sprintf(startStrBuf,"%9ju",static_cast<uint64_t>(1));

  char endStrBuf[10] = {};
  sprintf(endStrBuf,"%9ju",static_cast<uint64_t>(10000)); // 10000 is a patch to be fixed!!!


  msg.SoM = HsvfSom;

  memcpy(msg.hdr.sequence , seqStrBuf   , sizeof(msg.hdr.sequence));
  memcpy(msg.hdr.MsgType  , "RT"        , sizeof(msg.hdr.MsgType));
  memcpy(msg.Line         , gr->line    , sizeof(msg.Line));
  memcpy(msg.Start        , startStrBuf , sizeof(msg.hdr.sequence));
  memcpy(msg.End          , endStrBuf   , sizeof(msg.hdr.sequence));

  msg.EoM = HsvfEom;

#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmit Request sent for %s .. %s messages",
	  EKA_EXCH_DECODE(gr->exch),gr->id,startStrBuf,endStrBuf);
#else
  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) < 0) {
    EKA_WARN("Retransmit Request send failed");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }

  EKA_LOG("%s:%u Retransmit Request sent for %s .. %s messages",
	  EKA_EXCH_DECODE(gr->exch),gr->id,startStrBuf,endStrBuf);
#endif

  return EKA_OPRESULT__OK;
}

/* ----------------------------- */
static EkaOpResult getRetransmitionBegins(FhBoxGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmition Begins received",EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  HsvfLoginAck msg = {};

  if (recv(gr->snapshot_sock,&msg,sizeof(msg),MSG_WAITALL) != sizeof(msg))
    on_error("Retransmit Server connection reset by peer (failed to receive Login Ack)");

  if (msg.SoM != HsvfSom) 
    on_error("msg.SoM = 0x%0x, while expected HsvfSom = 0x%0x",msg.SoM,HsvfSom);

  if (msg.EoM != HsvfEom) 
    on_error("msg.Eom = 0x%0x, while expected HsvfEom = 0x%0x",msg.EoM,HsvfEom);

  if (memcmp(&msg.hdr.MsgType,"RB",2) != 0) 
    on_error("Received msg.hdr.MsgType \'%c%c\' != Expected \'RB\'",
	     msg.hdr.MsgType[0],msg.hdr.MsgType[1]);

  EKA_LOG("%s:%u Retransmition Begins received",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif
  return EKA_OPRESULT__OK;
}
/* ----------------------------- */
inline static uint8_t getTcpChar(uint8_t* dst, int sock) {
  if (recv(sock,dst,1,MSG_WAITALL) != 1)
    on_error("Retransmit Server connection reset by peer (failed to receive SoM)");

  return *dst;
}

/* ----------------------------- */
static EkaOpResult getTcpMsg(uint8_t* msgBuf, int sock) {
  uint charIdx = 0;

  if (getTcpChar(&msgBuf[charIdx],sock) != HsvfSom) 
    on_error("SoM \'%c\' != HsvfSom \'%c\'",msgBuf[charIdx],HsvfSom);

  do {
    charIdx++;
    if (charIdx > std::max(sizeof(HsvfOptionInstrumentKeys),sizeof(HsvfOptionSummary)))
      on_error("HsvfEom not met after %u characters",charIdx);
  } while (getTcpChar(&msgBuf[charIdx],sock) != HsvfEom);
  return EKA_OPRESULT__OK;
}

/* ----------------------------- */


EkaOpResult eka_hsvf_get_definitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhBoxGr* gr) {
  if (gr == NULL) on_error("gr == NULL");
  EkaDev* dev = gr->dev;
  if (dev == NULL) on_error("dev == NULL");
  EkaOpResult ret = EKA_OPRESULT__OK;

  EKA_LOG("Definitions for %s:%u - BOX to %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id,
	  EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));

  //-----------------------------------------------------------------
  ekaTcpConnect(&gr->snapshot_sock,gr->snapshot_ip,gr->snapshot_port);
  //-----------------------------------------------------------------
  if ((ret = sendLogin(gr))        != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  if ((ret = getLoginResponse(gr)) != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  if ((ret = sendRequest(gr))      != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  if ((ret = getRetransmitionBegins(gr)) != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Defintions done",EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  gr->snapshot_active = true;
  bool definitionsDone = false;
  while (gr->snapshot_active && !definitionsDone) {

    uint8_t msgBuf[1500] = {};
    if ((ret = getTcpMsg(msgBuf,gr->snapshot_sock)) != EKA_OPRESULT__OK) return ret;

    std::string seqString = std::string(((HsvfMsgHdr*)&msgBuf[1])->sequence,sizeof(((HsvfMsgHdr*)&msgBuf[1])->sequence));
    uint64_t seq = std::stoul(seqString,nullptr,10);

    definitionsDone = gr->parseMsg(pEfhRunCtx,msgBuf,seq,EkaFhMode::DEFINITIONS);
  }
#endif
  //-----------------------------------------------------------------

  return  EKA_OPRESULT__OK;
}
