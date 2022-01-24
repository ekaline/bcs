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

#include "EkaFhGroup.h"
#include "EkaFhBook.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "eka_fh_q.h"


 /* ##################################################################### */
EkaFhGroup::EkaFhGroup() {
    connectRetryDelayTime = 15;
    pktCnt = 0;
}
 /* ##################################################################### */
uint         EkaFhGroup::getNumSecurities() {
  return numSecurities;
}
 /* ##################################################################### */

int EkaFhGroup::processFromQ(const EfhRunCtx* pEfhRunCtx) {
  bool firstMsg = true;
  while (! q->is_empty()) {
    fh_msg* buf = q->pop();
    /* EKA_LOG("q_len=%u,buf->sequence=%ju, expected_sequence=%ju", */
    /* 	    q->get_len(),buf->sequence,expected_sequence); */
    if (firstMsg) {
      firstMsg = false;
      EKA_LOG("%s:%u: 1st Q msg sequence = %ju",
	  EKA_EXCH_DECODE(exch),id,buf->sequence);
    }
    if (buf->sequence < expected_sequence) continue;
    parseMsg(pEfhRunCtx,(unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
    expected_sequence = buf->sequence + 1;
  }
  EKA_LOG("%s:%u: After Q draining expected_sequence = %ju",
	  EKA_EXCH_DECODE(exch),id,expected_sequence);
  return 0;
}

 /* ##################################################################### */

void EkaFhGroup::sendFeedUp(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kNormal,   //Initializing
    EfhSystemState::kTrading, // Preopen, Trading, Closed
    EfhErrorDomain::kNoError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kFeedRecovery, // Unspecified, FeedRecovery
    gapNum // int64_t code
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}
 /* ##################################################################### */

void EkaFhGroup::sendFeedUpInitial(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kNormal,   //Initializing
    EfhSystemState::kInitial, // Preopen, Trading, Closed
    EfhErrorDomain::kNoError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kFeedSnapshot, // Unspecified, FeedRecovery
    gapNum // int64_t code
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}

 /* ##################################################################### */

void EkaFhGroup::sendFeedDown(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kGap, //Initializing
    EfhSystemState::kTrading, // Preopen, Trading, Closed
    EfhErrorDomain::kNoError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kFeedRecovery, // Unspecified, FeedRecovery
    ++gapNum // int64_t code
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}

 /* ##################################################################### */

void EkaFhGroup::sendFeedDownInitial(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kInitializing,
    EfhSystemState::kInitial, // Preopen, Trading, Closed
    EfhErrorDomain::kNoError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kFeedSnapshot, // Unspecified, FeedRecovery
    ++gapNum // int64_t code
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}

 /* ##################################################################### */

void EkaFhGroup::sendFeedDownClosed(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kClosed,
    EfhSystemState::kClosed, // Preopen, Trading, Closed
    EfhErrorDomain::kNoError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kUnspecified, // Unspecified, FeedRecovery
    ++gapNum // int64_t code
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}
 /* ##################################################################### */

void EkaFhGroup::sendBackInTimeEvent(const EfhRunCtx* pEfhRunCtx, uint64_t badSequence) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kWarning,
    EfhSystemState::kTrading, 
    EfhErrorDomain::kBackInTime, 
    EkaServiceType::kLiveMarketData,
    (int64_t)badSequence 
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}

 /* ##################################################################### */

void EkaFhGroup::sendNoMdTimeOut(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kError,
    EfhSystemState::kUnknown, // Preopen, Trading, Closed
    EfhErrorDomain::kUpdateTimeout, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kUnspecified, // Unspecified, FeedRecovery
    ++gapNum // int64_t code
  };
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
}
 /* ##################################################################### */


void EkaFhGroup::sendRetransmitExchangeError(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kError,
    EfhSystemState::kUnknown, // Preopen, Trading, Closed
    EfhErrorDomain::kExchangeError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kFeedRecovery, // Unspecified, FeedRecovery
    (int64_t)dev->lastExchErr
  };
  dev->lastExchErr = EfhExchangeErrorCode::kNoError;
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
  EKA_LOG("%s:%u re-trying in %d seconds",EKA_EXCH_DECODE(exch),id,connectRetryDelayTime);
  if (connectRetryDelayTime == 0) on_error("connectRetryDelayTime == 0");
  sleep(connectRetryDelayTime);
}
 /* ##################################################################### */

void EkaFhGroup::sendRetransmitSocketError(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");
  if (pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhGroupStateChangedMsgCb == NULL");
  
  EfhGroupStateChangedMsg msg = {
    EfhMsgType::kGroupStateChanged,
    {exch, id},
    EfhGroupState::kError,
    EfhSystemState::kUnknown, // Preopen, Trading, Closed
    EfhErrorDomain::kSocketError, // SocketError, UpdateTimeout, CredentialError, ExchangeError
    EkaServiceType::kFeedSnapshot, // Unspecified, FeedRecovery
    dev->lastErrno
  };
  dev->lastErrno = 0;
  pEfhRunCtx->onEfhGroupStateChangedMsgCb(&msg, 0, pEfhRunCtx->efhRunUserData);
  EKA_LOG("%s:%u re-trying in %d seconds",EKA_EXCH_DECODE(exch),id,connectRetryDelayTime);
  if (connectRetryDelayTime == 0) on_error("connectRetryDelayTime == 0");
  sleep(connectRetryDelayTime);
}
 /* ##################################################################### */

int EkaFhGroup::init (EfhCtx* _pEfhCtx, 
		      const EfhInitCtx* pInitCtx,
		      EkaFh* _fh, 
		      uint8_t gr_id,
		      EkaSource _exch) {
  pEfhCtx   = _pEfhCtx;

  assert(pEfhCtx != NULL);
  assert(pInitCtx != NULL);

  dev      = pEfhCtx->dev;
  exch     = _exch;
  feed_ver = EFH_EXCH2FEED(exch);

  fh       = _fh;
  if (fh == NULL) on_error("fh == NULL");
  core     = pInitCtx->coreId; //fh->c;

  if (dev->core[core] == NULL) on_error("dev->core[%u] == NULL",core);

  //  no_igmp  = ! EKA_NATIVE_MAC(dev->core[core]->macSa);

  q        = NULL;
  id       = gr_id;
  state    = GrpState::INIT;

  if (pInitCtx->printParsedMessages) {
    std::string parsedMsgFileName = std::string(EKA_EXCH_DECODE(exch)) + std::to_string(id) + std::string("_PARSED_MESSAGES.txt");
    if((parser_log = fopen(parsedMsgFileName.c_str(),"w")) == NULL) on_error ("Error %s",parsedMsgFileName.c_str());
    EKA_LOG("%s:%u created file %s",EKA_EXCH_DECODE(exch),id,parsedMsgFileName.c_str());
  }

  /* EKA_LOG("%s:%u is initialized, feed_ver=%s", */
  /* 	  EKA_EXCH_DECODE(exch),id,EKA_FEED_VER_DECODE(feed_ver)); */

  return 0;
}
 /* ##################################################################### */

void EkaFhGroup::createQ(EfhCtx* pEfhCtx, const uint qsize) {  
  if (qsize != 0) { 
    q = new fh_q(pEfhCtx,exch,(EkaFhGroup*)this,id,(uint)qsize);
    if (q == NULL) on_error("q == NULL");
  } 
  EKA_DEBUG("%s:%u created Q with %u elemets",EKA_EXCH_DECODE(exch),id, qsize);
  return;
}
 /* ##################################################################### */

int EkaFhGroup::stop() {  
  /* send_igmp(false); */
  /* EKA_DEBUG("%s:%u - Stopping: IGMP LEAVE sent",EKA_EXCH_DECODE(exch),id); */

  thread_active    = false;
  snapshot_active  = false;
  recovery_active  = false;
  heartbeat_active = false;

  /* shutdown(recovery_sock,SHUT_RD); */
  /* shutdown(recovery_sock,SHUT_WR); */
  /* shutdown(snapshot_sock,SHUT_RD); */
  /* shutdown(snapshot_sock,SHUT_WR); */
  return 0;
}

 /* ##################################################################### */

EkaFhGroup::~EkaFhGroup () {  
  heartbeat_active = false;
  snapshot_active  = false;
  recovery_active  = false;

  if (fh->print_parsed_messages) {
    EKA_LOG("%s:%u: closing parser_log file",EKA_EXCH_DECODE(exch),id);
    fclose(parser_log);
  }

  if (q) {
    delete q;
    EKA_DEBUG("%s:%u Q is deleted",EKA_EXCH_DECODE(exch),id);
    //    book->invalidate();
  }

  EKA_DEBUG("%s:%u closed",EKA_EXCH_DECODE(exch),id);
}

 /* ##################################################################### */

void EkaFhGroup::print_q_state() {
  if (q == NULL) {
    EKA_DEBUG("NO Q");
    return;
  }
  EKA_DEBUG("Q.max_len=%10u, Q.ever_max_len=%10u, CPU:%u",
	   q->get_max_len(),
	   q->get_ever_max_len(),
	   sched_getcpu()
	   );
  q->reset_max_len();
  return;  
}
 /* ##################################################################### */

 int EkaFhGroup::credentialAcquire(const char* credName,
				   size_t      credNameSize,
				   EkaCredentialLease** lease) {
   
   EKA_LOG("%s:%u trying to acquire %s credentials for threadId %ld",
	   EKA_EXCH_DECODE(exch),id,credName,syscall(SYS_gettid));
     
   if (credentialsAcquired) {
     EKA_WARN("%s:%u Credentials is already acquired",EKA_EXCH_DECODE(exch),id);
     return 0;
   }
   const struct timespec leaseTime = {.tv_sec = 180, .tv_nsec = 0};
   const struct timespec timeout   = {.tv_sec = 60, .tv_nsec = 0};

   char                  name[7] = {};

   memcpy (name,credName,std::min(sizeof(name),credNameSize) - 1);
   const EkaGroup group {exch,id};
   int rc = dev->credAcquire(EkaCredentialType::kSnapshot, 
			     group, 
			     name, 
			     &leaseTime,
			     &timeout,
			     dev->credContext,
			     lease);
   if (rc == 0)
     credentialsAcquired = true;
  
   if (rc != 0) 
     on_error("%s:%u Failed to credAcquire for \'%s\' rc=%d \'%s\'",
	      EKA_EXCH_DECODE(exch),id,name,rc,strerror(errno));
   return rc;
 }
/* ##################################################################### */

 int EkaFhGroup::credentialRelease(EkaCredentialLease* lease) {
   EKA_LOG("%s:%u trying to release credentials for threadId %ld",
	   EKA_EXCH_DECODE(exch),id,syscall(SYS_gettid));
   
   if (! credentialsAcquired) {
     EKA_WARN("%s:%u Credentials is already released",EKA_EXCH_DECODE(exch),id);
     return 0;
   }
   int rc = dev->credRelease(lease, dev->credContext);
   if (rc == 0)
     credentialsAcquired = false;

   if (rc != 0) 
    on_error("%s:%u Failed to credRelease rc=%d \'%s\'",
	     EKA_EXCH_DECODE(exch),id,rc,strerror(errno));
   return rc;
 }