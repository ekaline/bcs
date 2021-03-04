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

#include "EkaFhBoxGr.h"
#include "EkaHsvfTcp.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhBoxParser.h"

int ekaTcpConnect(uint32_t ip, uint16_t port);

uint getHsvfMsgLen(const uint8_t* pkt, int bytes2run);
uint64_t getHsvfMsgSequence(const uint8_t* msg);

/* ----------------------------- */

static bool sendLogin (EkaFhBoxGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Box Login sent",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
#endif

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

  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: BOX Login send failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }
  EKA_LOG("%s:%u Box Login sent: %s",
	  EKA_EXCH_DECODE(gr->exch),gr->id,(char*)&msg);

  return true;
}
/* ----------------------------- */
static bool getLoginResponse(EkaFhBoxGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Login Acknowledge received",EKA_EXCH_DECODE(gr->exch),gr->id);
  return false;
#endif

  bool loginAcknowledged = false;
  while (! loginAcknowledged) {
    uint8_t* msgBuf = NULL;
    if (gr->hsvfTcp->getTcpMsg(&msgBuf) != EKA_OPRESULT__OK) {
      //      dev->lastErrno = errno;
      return false;
    }

    HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&msgBuf[1];
    if (memcmp(msgHdr->MsgType,"KI",sizeof(msgHdr->MsgType)) == 0) { // Login Acknowledge
      loginAcknowledged = true;
    } else if (memcmp(msgHdr->MsgType,"ER",sizeof(msgHdr->MsgType)) == 0) {
      HsvfError* msg = (HsvfError*) msgBuf;
      std::string errorCode = std::string(msg->ErrorCode,sizeof(msg->ErrorCode));
      std::string errorMsg  = std::string(msg->ErrorMsg, sizeof(msg->ErrorMsg));

      dev->lastExchErr = EfhExchangeErrorCode::kExplicitBoxError;
      EKA_WARN("%s:%u Login Response Error: ErrorCode=\'%s\', ErrorMsg=\'%s\'",
	       EKA_EXCH_DECODE(gr->exch),gr->id, errorCode.c_str(),errorMsg.c_str());
      return false;
    } else {
      EKA_LOG("%s:%u waiting for Login Acknowledge: received \'%c%c\'",
	      EKA_EXCH_DECODE(gr->exch),gr->id,msgHdr->MsgType[0],msgHdr->MsgType[1]);
    }
  }
  EKA_LOG("%s:%u Login Acknowledge received",EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
}

/* ----------------------------- */
static bool sendRequest(EkaFhBoxGr* gr, uint64_t start, uint64_t end) {
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
  return true;
#endif
  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Retransmit Request send failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }
  EKA_LOG("%s:%u Retransmit Request sent for %s (%ju) .. %s (%ju) messages cnt = %ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,startStrBuf,start,endStrBuf,end,end-start);
  return true;
}

/* ----------------------------- */
static bool getRetransmissionBegins(EkaFhBoxGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmission Begins received",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
#endif


  bool retransmissionBegins = false;
  while (! retransmissionBegins) {
    uint8_t* msgBuf = NULL;
    if (gr->hsvfTcp->getTcpMsg(&msgBuf) != EKA_OPRESULT__OK) {
      dev->lastErrno = errno;
      return false;
    }

    HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&msgBuf[1];

    if (memcmp(msgHdr->MsgType,"RB",sizeof(msgHdr->MsgType)) == 0) { // Retransmission Begin
      retransmissionBegins = true;
    } else if (memcmp(msgHdr->MsgType,"ER",sizeof(msgHdr->MsgType)) == 0) {
      HsvfError* msg = (HsvfError*) msgBuf;
      std::string errorCode = std::string(msg->ErrorCode,sizeof(msg->ErrorCode));
      std::string errorMsg  = std::string(msg->ErrorMsg, sizeof(msg->ErrorMsg));

      dev->lastExchErr = EfhExchangeErrorCode::kExplicitBoxError;
      EKA_WARN("%s:%u Login Response Error: ErrorCode=\'%s\', ErrorMsg=\'%s\'",
	       EKA_EXCH_DECODE(gr->exch),gr->id, errorCode.c_str(),errorMsg.c_str());

      return false;
    } else {
      EKA_LOG("%s:%u waiting for Retransmission Begin: received \'%c%c\'",
	      EKA_EXCH_DECODE(gr->exch),gr->id,msgHdr->MsgType[0],msgHdr->MsgType[1]);
    }
  }
  EKA_LOG("%s:%u Retransmission Begin received",EKA_EXCH_DECODE(gr->exch),gr->id);

  return true;
}

/* ----------------------------- */

static bool sendRetransmissionEnd(EkaFhBoxGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Retransmission End sent",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
#endif

  HsvfRetransmissionEnd msg = {};
  memset(&msg,' ',sizeof(msg));

  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%09ju",gr->txSeqNum ++);
  msg.SoM = HsvfSom;
  memcpy(msg.hdr.sequence , seqStrBuf   , sizeof(msg.hdr.sequence));
  memcpy(msg.hdr.MsgType  , "RE"        , sizeof(msg.hdr.MsgType));
  msg.EoM = HsvfEom;

  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Retransmission End send failed",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }
  EKA_LOG("%s:%u Retransmission End sent",EKA_EXCH_DECODE(gr->exch),gr->id);

  return true;
}

/* ----------------------------- */

static bool sendLogout(EkaFhBoxGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Logout sent",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
#endif
  HsvfRetransmissionEnd msg = {};
  memset(&msg,' ',sizeof(msg));

  char seqStrBuf[10] = {};
  sprintf(seqStrBuf,"%09ju",gr->txSeqNum ++);
  msg.SoM = HsvfSom;
  memcpy(msg.hdr.sequence , seqStrBuf   , sizeof(msg.hdr.sequence));
  memcpy(msg.hdr.MsgType  , "LO"        , sizeof(msg.hdr.MsgType));
  msg.EoM = HsvfEom;

  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Logout send failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }
  EKA_LOG("%s:%u Logout sent",EKA_EXCH_DECODE(gr->exch),gr->id);

  return true;
}
/* ----------------------------- */


EkaOpResult getHsvfDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhBoxGr* gr) {
  if (gr == NULL) on_error("gr == NULL");
  EkaDev* dev = gr->dev;
  if (dev == NULL) on_error("dev == NULL");

#ifdef FH_LAB
  EKA_LOG("%s:%u Dummy FH_LAB Defintions done",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  return EKA_OPRESULT__OK;
#endif
  EkaOpResult ret = EKA_OPRESULT__OK;

  EKA_LOG("Definitions for %s:%u - BOX to %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id,
	  EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));

  gr->snapshot_active = true;
  while (gr->snapshot_active) {
    //-----------------------------------------------------------------
    gr->snapshot_sock = ekaTcpConnect(gr->snapshot_ip,gr->snapshot_port);
    //-----------------------------------------------------------------
    gr->hsvfTcp = new EkaHsvfTcp(dev,gr->snapshot_sock);
    if (gr->hsvfTcp == NULL) on_error("Failed on new EkaHsvfTcp");

    //-----------------------------------------------------------------
    if (! sendLogin(gr)) {
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    if (! getLoginResponse(gr)) {
      sendLogout(gr);
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
      if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }

    //-----------------------------------------------------------------
    if (! sendRequest(gr,static_cast<uint64_t>(1),static_cast<uint64_t>(1e6))) {
      sendLogout(gr);
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
      if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    if (! getRetransmissionBegins(gr)) {
      sendLogout(gr);
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
      if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    break;
  }

  bool definitionsDone = false;
  uint64_t sequence = 1;
  while (gr->snapshot_active && !definitionsDone) {
    uint8_t* msgBuf = NULL;
    if ((ret = gr->hsvfTcp->getTcpMsg(&msgBuf)) != EKA_OPRESULT__OK) {
      dev->lastErrno = errno;
      return ret;
    }
    sequence = getHsvfMsgSequence((const uint8_t*)msgBuf);
    definitionsDone = gr->parseMsg(pEfhRunCtx,&msgBuf[1],0,EkaFhMode::DEFINITIONS);
  }
  EKA_LOG("%s:%u Dictionary received after %ju messages",EKA_EXCH_DECODE(gr->exch),gr->id,sequence);
  //-----------------------------------------------------------------
  sendRetransmissionEnd(gr);
  //-----------------------------------------------------------------
  sendLogout(gr);
  //-----------------------------------------------------------------
  close(gr->snapshot_sock);
  delete gr->hsvfTcp;

  return  EKA_OPRESULT__OK;
}

void* getHsvfRetransmit(void* attr) {
#ifdef FH_LAB
  return NULL;
#endif

  pthread_detach(pthread_self());

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhBoxGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");
  EkaDev*    dev = gr->dev;

  EfhRunCtx* pEfhRunCtx     = params->pEfhRunCtx;
  uint64_t   start          = params->startSeq;
  uint64_t   end            = params->endSeq;
  EkaFhMode  op             = params->op;
  delete params;

  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);
  //  EkaOpResult ret = EKA_OPRESULT__OK;


  EKA_LOG("%s:%u start=%ju, end=%ju, gap=%d",EKA_EXCH_DECODE(gr->exch),gr->id,start,end, end - start);
  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  gr->credentialAcquire(gr->auth_user,sizeof(gr->auth_user),&lease);

  //-----------------------------------------------------------------
  gr->snapshot_active = true;

  while (gr->snapshot_active) {
    gr->snapshot_sock = ekaTcpConnect(gr->snapshot_ip,gr->snapshot_port);
    //-----------------------------------------------------------------
    gr->hsvfTcp = new EkaHsvfTcp(dev,gr->snapshot_sock);
    if (gr->hsvfTcp == NULL) on_error("Failed on new EkaHsvfTcp");
    //-----------------------------------------------------------------
    if (! sendLogin(gr)) {
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    if (! getLoginResponse(gr)) {
      sendLogout(gr);
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
      if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    if (! sendRequest(gr,start,end)) {
      sendLogout(gr);
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
      if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    if (! getRetransmissionBegins(gr)) {
      sendLogout(gr);
      close(gr->snapshot_sock);
      delete gr->hsvfTcp;
      if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
      if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    break;
    //-----------------------------------------------------------------
  }

  while (gr->snapshot_active) {
    uint8_t* msgBuf = NULL;
    if (gr->hsvfTcp->getTcpMsg(&msgBuf) != EKA_OPRESULT__OK) {
      //      dev->lastErrno = errno;
      gr->sendRetransmitSocketError(pEfhRunCtx);
    }

    uint64_t sequence = getHsvfMsgSequence((const uint8_t*)msgBuf);
    //    EKA_LOG("%s:%u got sequence = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,sequence);
    bool endOfTransmition = gr->parseMsg(pEfhRunCtx,&msgBuf[1],sequence,op);
    if (sequence == end || endOfTransmition) {
      gr->snapshot_active = false;
      gr->seq_after_snapshot = sequence + 1;
      EKA_LOG("%s:%u Retransmission completed at sequence = %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,sequence);
      break;
    }
  }
  gr->gapClosed = true;

  //-----------------------------------------------------------------
  sendRetransmissionEnd(gr);
  //-----------------------------------------------------------------
  sendLogout(gr);

  //-----------------------------------------------------------------
  int rc = dev->credRelease(lease, dev->credContext);
  if (rc != 0) on_error("%s:%u Failed to credRelease",
			EKA_EXCH_DECODE(gr->exch),gr->id);
  EKA_LOG("%s:%u BOX retransmit Credentials Released",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------

  close(gr->snapshot_sock);
  delete gr->hsvfTcp;

  return NULL;
}
