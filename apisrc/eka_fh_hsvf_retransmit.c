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
#include "EkaDev.h"
#include "Efh.h"
#include "Eka.h"
#include "eka_hsvf_box_messages.h"
#include "eka_fh.h"
#include "EkaHsvfTcp.h"

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port);
int ekaUdpConnect(EkaDev* dev, int* sock, uint32_t ip, uint16_t port);

uint getHsvfMsgLen(const uint8_t* pkt, int bytes2run);
uint64_t getHsvfMsgSequence(uint8_t* msg);
uint trailingZeros(uint8_t* p, uint maxChars);

void hexDump (const char* desc, void *addr, int len);

/* ----------------------------- */
inline static uint8_t getTcpChar(uint8_t* dst, int sock) {
  if (recv(sock,dst,1,MSG_WAITALL) != 1)
    on_error("Retransmit Server connection reset by peer (failed to receive SoM)");
  return *dst;
}

/* ----------------------------- */
/* static EkaOpResult getTcpMsg(uint8_t* msgBuf, int sock) { */
/*   uint charIdx = 0; */
/*   const uint MAX_MSG_SIZE = 1400; // just a number */
/*   if (getTcpChar(&msgBuf[charIdx],sock) != HsvfSom)  */
/*     on_error("SoM \'%c\' != HsvfSom \'%c\'",msgBuf[charIdx],HsvfSom); */
/*   do { */
/*     charIdx++; */
/*     //    if (charIdx > std::max(sizeof(HsvfOptionInstrumentKeys),sizeof(HsvfOptionSummary)) + 20) { */
/*     if (charIdx > MAX_MSG_SIZE) { */
/*       hexDump("msgBuf with no HsvfEom",msgBuf,charIdx); */
/*       on_error("HsvfEom not met after %u characters",charIdx); */
/*     } */
/*   } while (getTcpChar(&msgBuf[charIdx],sock) != HsvfEom); */
/*   return EKA_OPRESULT__OK; */
/* } */

/* ----------------------------- */

static EkaOpResult sendLogin (FhBoxGr* gr) {
  EkaDev* dev = gr->dev;
  HsvfLogin msg = {};
  memset(&msg,' ',sizeof(msg));

  time_t rawtime;
  time (&rawtime);
  struct tm * ct = localtime (&rawtime);
  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%09ju",gr->txSeqNum ++);
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
  EkaOpResult ret = EKA_OPRESULT__OK;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Login Acknowledge received",EKA_EXCH_DECODE(gr->exch),gr->id);
#else	
  bool loginAcknowledged = false;
  while (! loginAcknowledged) {
    uint8_t* msgBuf = NULL;
    if ((ret = gr->hsvfTcp->getTcpMsg(&msgBuf)) != EKA_OPRESULT__OK) return ret;

    HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&msgBuf[1];
    if (memcmp(msgHdr->MsgType,"KI",sizeof(msgHdr->MsgType)) == 0) { // Login Acknowledge
      loginAcknowledged = true;
    } else if (memcmp(msgHdr->MsgType,"ER",sizeof(msgHdr->MsgType)) == 0) {
      HsvfError* msg = (HsvfError*) msgBuf;
      std::string errorCode = std::string(msg->ErrorCode,sizeof(msg->ErrorCode));
      std::string errorMsg  = std::string(msg->ErrorMsg, sizeof(msg->ErrorMsg));

      on_error("%s:%u Login Response Error: ErrorCode=\'%s\', ErrorMsg=\'%s\'",
	       EKA_EXCH_DECODE(gr->exch),gr->id, errorCode.c_str(),errorMsg.c_str());
    } else {
      EKA_LOG("%s:%u waiting for Login Acknowledge: received \'%c%c\'",
	      EKA_EXCH_DECODE(gr->exch),gr->id,msgHdr->MsgType[0],msgHdr->MsgType[1]);
    }
  }
    EKA_LOG("%s:%u Login Acknowledge received",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif
  return ret;
}

/* ----------------------------- */
static EkaOpResult sendRequest(FhBoxGr* gr, uint64_t start, uint64_t end) {
  EkaDev* dev = gr->dev;
  HsvfRetransmissionRequest msg = {};
  memset(&msg,' ',sizeof(msg));
  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%09ju",gr->txSeqNum ++);
  char startStrBuf[10] = {};
  sprintf(startStrBuf,"%09ju",start);
  char endStrBuf[10] = {};
  sprintf(endStrBuf,"%09ju",end); // 1,000,000 is a patch to be fixed!!!

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
static EkaOpResult getRetransmissionBegins(FhBoxGr* gr) {
  EkaDev* dev = gr->dev;
  EkaOpResult ret = EKA_OPRESULT__OK;

#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmission Begins received",EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  bool retransmissionBegins = false;
  while (! retransmissionBegins) {
    uint8_t* msgBuf = NULL;
    if ((ret = gr->hsvfTcp->getTcpMsg(&msgBuf)) != EKA_OPRESULT__OK) return ret;

    HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&msgBuf[1];

    if (memcmp(msgHdr->MsgType,"RB",sizeof(msgHdr->MsgType)) == 0) { // Retransmission Begin
      retransmissionBegins = true;
    } else if (memcmp(msgHdr->MsgType,"ER",sizeof(msgHdr->MsgType)) == 0) {
      HsvfError* msg = (HsvfError*) msgBuf;
      std::string errorCode = std::string(msg->ErrorCode,sizeof(msg->ErrorCode));
      std::string errorMsg  = std::string(msg->ErrorMsg, sizeof(msg->ErrorMsg));

      on_error("%s:%u Login Response Error: ErrorCode=\'%s\', ErrorMsg=\'%s\'",
	       EKA_EXCH_DECODE(gr->exch),gr->id, errorCode.c_str(),errorMsg.c_str());
    } else {
      EKA_LOG("%s:%u waiting for Retransmission Begin: received \'%c%c\'",
	      EKA_EXCH_DECODE(gr->exch),gr->id,msgHdr->MsgType[0],msgHdr->MsgType[1]);
    }
  }
  EKA_LOG("%s:%u Retransmission Begin received",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif
  return ret;
}

/* ----------------------------- */

static EkaOpResult sendRetransmissionEnd(FhBoxGr* gr) {
  EkaDev* dev = gr->dev;
  HsvfRetransmissionEnd msg = {};
  memset(&msg,' ',sizeof(msg));

  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%09ju",gr->txSeqNum ++);
  msg.SoM = HsvfSom;
  memcpy(msg.hdr.sequence , seqStrBuf   , sizeof(msg.hdr.sequence));
  memcpy(msg.hdr.MsgType  , "RE"        , sizeof(msg.hdr.MsgType));
  msg.EoM = HsvfEom;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmission End sent",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) < 0) {
    EKA_WARN("Retransmission End send failed");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
  EKA_LOG("%s:%u Retransmission End sent",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif
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
  if ((ret = sendRequest(gr,static_cast<uint64_t>(1),static_cast<uint64_t>(1e6))) != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  if ((ret = getRetransmissionBegins(gr)) != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Defintions done",EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  gr->snapshot_active = true;
  bool definitionsDone = false;
  uint64_t sequence = 1;
  while (gr->snapshot_active && !definitionsDone) {
    uint8_t* msgBuf = NULL;
    if ((ret = gr->hsvfTcp->getTcpMsg(&msgBuf)) != EKA_OPRESULT__OK) return ret;
    sequence = getHsvfMsgSequence(msgBuf);
    definitionsDone = gr->parseMsg(pEfhRunCtx,&msgBuf[1],0,EkaFhMode::DEFINITIONS);
  }
  EKA_LOG("%s:%u Dictionary received after %ju messages",EKA_EXCH_DECODE(gr->exch),gr->id,sequence);
  //-----------------------------------------------------------------
  if ((ret = sendRetransmissionEnd(gr))      != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  close(gr->snapshot_sock);
#endif

  return  EKA_OPRESULT__OK;
}

void* eka_get_hsvf_retransmit(void* attr) {

  pthread_detach(pthread_self());


  //  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  FhBoxGr*   gr             = (FhBoxGr*)((EkaFhThreadAttr*)attr)->gr;
  EkaDev*    dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmission done",EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  uint64_t   start          = ((EkaFhThreadAttr*)attr)->startSeq;
  uint64_t   end            = ((EkaFhThreadAttr*)attr)->endSeq;
  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;
  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();

  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);

  gr->hsvfTcp = new EkaHsvfTcp(dev,gr->recovery_sock);
  if (gr->hsvfTcp == NULL) on_error("Failed on new EkaHsvfTcp");

  EKA_LOG("%s:%u start=%ju, end=%ju",EKA_EXCH_DECODE(gr->exch),gr->id,start,end);
  //-----------------------------------------------------------------
  ekaTcpConnect(&gr->snapshot_sock,gr->snapshot_ip,gr->snapshot_port);
  //-----------------------------------------------------------------
  sendLogin(gr);
  //-----------------------------------------------------------------
  getLoginResponse(gr);
  //-----------------------------------------------------------------
  sendRequest(gr,start,end);
  //-----------------------------------------------------------------
  getRetransmissionBegins(gr);
  //-----------------------------------------------------------------

  gr->snapshot_active = true;

  while (gr->snapshot_active) {
    uint8_t* msgBuf = NULL;
    gr->hsvfTcp->getTcpMsg(&msgBuf);

    uint64_t sequence = getHsvfMsgSequence(msgBuf);
    //    EKA_LOG("%s:%u got sequence = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,sequence);
    bool endOfTransmition = gr->parseMsg(pEfhRunCtx,&msgBuf[1],sequence,op);
    if (sequence == end || endOfTransmition) {
      gr->snapshot_active = false;
      gr->seq_after_snapshot = sequence + 1;
      EKA_LOG("%s:%u Retransmission completed at sequence = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,sequence);
      break;
    }
  }
  gr->gapClosed = true;

  //-----------------------------------------------------------------
  sendRetransmissionEnd(gr);
  //-----------------------------------------------------------------
  close(gr->snapshot_sock);
  delete gr->hsvfTcp;
#endif

  return NULL;
}
