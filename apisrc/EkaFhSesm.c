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

#include "EkaFhMiaxParser.h"
#include "EkaFhMiaxGr.h"

#include "EkaFhThreadAttr.h"

int ekaTcpConnect(uint32_t ip, uint16_t port);
void* heartBeatThread(EkaDev* dev, EkaFhMiaxGr* gr,int sock);

/* ##################################################################### */

static bool sendLogin (EkaFhMiaxGr* gr) {
#ifdef FH_LAB
  return true;
#endif

  EkaDev* dev = gr->dev;

  //--------------- SESM Login Request -------------------
  struct sesm_login_req sesm_login_msg = {};
  sesm_login_msg.header.length = sizeof(struct sesm_login_req) - sizeof(sesm_login_msg.header.length);
  sesm_login_msg.header.type = 'L';
  memcpy(sesm_login_msg.version,"1.1  ",sizeof(sesm_login_msg.version));
  memcpy(sesm_login_msg.username,gr->auth_user,sizeof(sesm_login_msg.username));
  memcpy(sesm_login_msg.computer_id,gr->auth_passwd,sizeof(sesm_login_msg.computer_id));
  memcpy(sesm_login_msg.app_protocol,"TOM2.2  ",sizeof(sesm_login_msg.app_protocol));
  sesm_login_msg.session = 0;
  sesm_login_msg.sequence = 0;
  EKA_LOG("%s:%u sending SESM login message: sesm_login_msg.version=%s, credentials: |%s|:|%s| taken from config |%s|:|%s|",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  std::string(sesm_login_msg.version,sizeof(sesm_login_msg.version)).c_str(),
	  std::string(sesm_login_msg.username,sizeof(sesm_login_msg.username)).c_str(),
	  std::string(sesm_login_msg.computer_id,sizeof(sesm_login_msg.computer_id)).c_str(),
	  std::string(gr->auth_user,sizeof(gr->auth_user)).c_str(),
	  std::string(gr->auth_passwd,sizeof(gr->auth_passwd)).c_str()
	  );	
  int r = send(gr->recovery_sock,&sesm_login_msg,sizeof(struct sesm_login_req), 0);
  if(r < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: SESM Login send failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }

  EKA_LOG("%s:%u Sesm Login sent",EKA_EXCH_DECODE(gr->exch),gr->id);

  return true;
}
/* ##################################################################### */

static bool getLoginResponse(EkaFhMiaxGr* gr) {
#ifdef FH_LAB
  return true;
#endif

  EkaDev* dev = gr->dev;

  sesm_login_response sesm_response_msg ={};
  const int r = recv(gr->recovery_sock,&sesm_response_msg,sizeof(sesm_login_response),MSG_WAITALL);
  if (r < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: failed to receive SESM login response: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  } else if (r == 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: unexpected request server socket EOF (expected login SESM response): %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }
  if (sesm_response_msg.header.type == 'G')  {
    EKA_WARN("SESM sent GoodBye Packet with reason: \'%c\'",sesm_response_msg.status);
    return false;
  }
  if (sesm_response_msg.header.type != 'R')  {
    EKA_WARN("sesm_response_msg.header.type \'%c\' != \'R\' ",sesm_response_msg.header.type);
    return false;
  }

  switch (sesm_response_msg.status) {
  case 'X' :
    on_error("%s:%u: SESM Login Response: \'X\' -- Rejected: Invalid Username/Computer ID combination",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
  case 'S' :
    on_error("%s:%u: SESM Login Response: \'S\' -- Rejected: Requested session is not available",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
  case 'N' : 
    on_error("%s:%u: SESM Login Response: \'N\' -- Rejected: Invalid start sequence number requested",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
  case 'I' :
    on_error("%s:%u: SESM Login Response: \'I\' -- Rejected: Incompatible Session protocol version",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
  case 'A' :
    on_error("%s:%u: SESM Login Response: \'A\' -- Rejected: Incompatible Application protocol version",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
  case 'L' :
    EKA_WARN("%s:%u: SESM Login Response: \'L\' -- Rejected: Request rejected because client already logged in",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
    return false;
  case  ' ' :
    EKA_LOG("%s:%u: SESM Login accepted. Highest sequence available=%ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,sesm_response_msg.sequence);
    return true;
  default:
    on_error("%s:%u: Unknown SESM Login Response: \'%c\'",
	     EKA_EXCH_DECODE(gr->exch),gr->id,sesm_response_msg.status);
  }
}

/* ##################################################################### */
static bool sendRequest(EkaFhMiaxGr* gr, char refreshType) {
#ifdef FH_LAB
  return true;
#endif

  EkaDev* dev = gr->dev;

  struct miax_request def_request_msg = {};
  def_request_msg.header.length = sizeof(struct miax_request) - sizeof(def_request_msg.header.length);
  def_request_msg.header.type = 'U';  // Unsequenced
  def_request_msg.request_type = 'R'; // Refresh
  def_request_msg.refresh_type = refreshType;

  //  hexDump("MIAX definitions request",(char*) &def_request_msg,sizeof(struct miax_request));
  int r = send(gr->recovery_sock,&def_request_msg,sizeof(def_request_msg), 0);
  if(r <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: SESM Request send failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }

  EKA_LOG("%s:%u: \'%c\' Request sent",
	  EKA_EXCH_DECODE(gr->exch),gr->id, refreshType);
  return true;
}

/* ##################################################################### */
static bool sendRetransmitRequest(EkaFhMiaxGr* gr, uint64_t start, uint64_t end) {
#ifdef FH_LAB
  return true;
#endif
  EkaDev* dev = gr->dev;
  sesm_retransmit_req retransmit_req = {};
  retransmit_req.header.length = sizeof(retransmit_req) - sizeof(retransmit_req.header.length);
  retransmit_req.header.type   = 'A';
  retransmit_req.start         = start;
  retransmit_req.end           = end;

  int r = send(gr->recovery_sock,&retransmit_req,sizeof(retransmit_req), 0);
  if(r < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: SESM Retransmit Request send failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return false;
  }
  EKA_LOG("%s:%u: Retransmit Request sent for %ju .. %ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id, start, end);
  return true;
}

/* ##################################################################### */
static bool sendLogOut(EkaFhMiaxGr* gr) {
#ifdef FH_LAB
  return true;
#endif

  EkaDev* dev = gr->dev;

  //--------------- SESM Logout Request -------------------
  struct sesm_logout_req sesm_logout_msg = {};
  sesm_logout_msg.header.length = sizeof(struct sesm_logout_req) - sizeof(sesm_logout_msg.header.length);
  sesm_logout_msg.header.type   = 'X';
  sesm_logout_msg.reason        = ' '; // Graceful Logout (Done for now)

  if(send(gr->recovery_sock,&sesm_logout_msg,sizeof(struct sesm_logout_req), 0) < 0) {
    EKA_WARN("%s:%u: SESM Logout send failed");
    return false;
  }

  EKA_LOG("%s:%u: SESM Logout sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
}


/* ##################################################################### */
static bool procSesm(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, int sock, EkaFhMiaxGr* gr,EkaFhMode op) {
  EkaDev* dev = gr->dev;
  sesm_header sesm_hdr ={};
  int recv_size = recv(sock,&sesm_hdr,sizeof(struct sesm_header),MSG_WAITALL);
  if (recv_size == -1)
    on_error("%s:%u failed to receive SESM header", EKA_EXCH_DECODE(gr->exch),gr->id);
  else if (recv_size == 0)
    on_error("%s:%u unexpected request server socket EOF (expected SESM header)",EKA_EXCH_DECODE(gr->exch),gr->id);

  uint8_t msg[1536] = {};
  uint8_t* m = msg;
  recv_size = recv(sock,msg,sesm_hdr.length - sizeof(sesm_hdr.type),MSG_WAITALL);
  if (recv_size == -1)
    on_error("%s:%u failed to receive SESM payload", EKA_EXCH_DECODE(gr->exch),gr->id);
  /* else if (recv_size == 0) */
  /*   on_error("%s:%u unexpected request server socket EOF (expected SESM payload) type=\'%c\' len = %u", */
  /* 	     EKA_EXCH_DECODE(gr->exch),gr->id,sesm_hdr.type, sesm_hdr.length); */

  uint64_t sequence = 0;

  switch ((EKA_SESM_TYPE)sesm_hdr.type) {
  case EKA_SESM_TYPE::GoodBye : {
    EKA_LOG("%s:%u Sesm Server sent GoodBye with reason \'%c\' : %s",EKA_EXCH_DECODE(gr->exch),gr->id,((sesm_goodbye*)m)->reason,((sesm_goodbye*)m)->text);
    return true;
  }

  case EKA_SESM_TYPE::ServerHeartbeat :
    EKA_LOG("%s:%u Sesm Server Heartbeat received",EKA_EXCH_DECODE(gr->exch),gr->id);
    return false;
 
  case EKA_SESM_TYPE::EndOfSession :
    EKA_LOG("%s:%u %s End-of-Session message. gr->seq_after_snapshot = %ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,
	    op == EkaFhMode::DEFINITIONS ? "DEFINITIONS" : "SNAPSHOT",
	    gr->seq_after_snapshot
	    );
    return true;

  case EKA_SESM_TYPE::SyncComplete :
    EKA_LOG("%s:%u SESM server sent SyncComplete message",EKA_EXCH_DECODE(gr->exch),gr->id);
    return true;

  case EKA_SESM_TYPE::TestPacket : {
    EKA_LOG("%s:%u Sesm Server sent Test Packet: %s",EKA_EXCH_DECODE(gr->exch),gr->id,(char*)m);
    return false;
  }

  case EKA_SESM_TYPE::Sequenced : 
    sequence = *(uint64_t*)m;
    m += sizeof(sequence);
    gr->seq_after_snapshot = sequence;
    if (gr->parseMsg(pEfhRunCtx,m,sequence,op)) {
      EKA_LOG("%s:%u %s End Of Refresh: gr->seq_after_snapshot = %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,
	      op == EkaFhMode::DEFINITIONS ? "DEFINITIONS" : "SNAPSHOT",
	      gr->seq_after_snapshot
	      );
      return true;
    }  
    break;

  case EKA_SESM_TYPE::UnSequenced : {
    char unsequencedType = ((sesm_unsequenced*)m)->type;
    switch (unsequencedType) {
    case 'R' :
      sequence = ((sesm_unsequenced*)m)->sequence;
      gr->seq_after_snapshot = sequence;
      m += sizeof(sesm_unsequenced);
      if (gr->parseMsg(pEfhRunCtx,m,sequence,op)) {
	EKA_LOG("%s:%u %s End Of Refresh: gr->seq_after_snapshot = %ju",
		EKA_EXCH_DECODE(gr->exch),gr->id,
		op == EkaFhMode::DEFINITIONS ? "DEFINITIONS" : "SNAPSHOT",
		gr->seq_after_snapshot
		);
	return true;
      }
      break;
    case 'E' :
      EKA_LOG("%s:%u %s End Of Request: gr->seq_after_snapshot = %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,
	      op == EkaFhMode::DEFINITIONS ? "DEFINITIONS" : "SNAPSHOT",
	      gr->seq_after_snapshot
	      );
      return true;
      break;
    default:
      on_error ("%s:%u Unexpected UnSequenced Packet type \'%c\'",
		EKA_EXCH_DECODE(gr->exch),gr->id,unsequencedType);
    }
  }
    break;
    
  default:
    EKA_WARN("%s:%u Unexpected sesm_hdr.type: \'%c\'",EKA_EXCH_DECODE(gr->exch),gr->id,sesm_hdr.type);
    return false;
  }
  return false;
}
/* ##################################################################### */

void* getSesmRetransmit(void* attr) {

  pthread_detach(pthread_self());

  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  EkaFhMiaxGr*  gr             = (EkaFhMiaxGr*)((EkaFhThreadAttr*)attr)->gr;
  uint64_t   start          = ((EkaFhThreadAttr*)attr)->startSeq;
  uint64_t   end            = ((EkaFhThreadAttr*)attr)->endSeq;
  //  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;
  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();

  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);
  EkaDev* dev = gr->dev;

  EKA_LOG("%s:%u start=%ju, end=%ju",EKA_EXCH_DECODE(gr->exch),gr->id,start,end);

  //  EkaDev* dev = gr->dev;
  //  if (end - start > 65000) on_error("Gap %ju is too high (> 65000), start = %ju, end = %ju",end - start, start, end);
  //-----------------------------------------------------------------
  gr->recovery_active = true;
  while (gr->recovery_active) {
    gr->recovery_sock = ekaTcpConnect(gr->recovery_ip,gr->recovery_port);
    //-----------------------------------------------------------------
    if (! sendLogin(gr)) {
      close(gr->recovery_sock);
      gr->sendRetransmitExchangeError(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------
    if (! getLoginResponse(gr)) {
      sendLogOut(gr);
      close(gr->recovery_sock);
      gr->sendRetransmitExchangeError(pEfhRunCtx);
      continue;
    }
    break;
  }
  //-----------------------------------------------------------------
  std::thread heartBeat = std::thread(heartBeatThread,dev,gr,gr->recovery_sock);
  heartBeat.detach();
  //-----------------------------------------------------------------
  if (! sendRetransmitRequest(gr,start,end)) {
    sendLogOut(gr);
    close(gr->recovery_sock);
    gr->sendRetransmitExchangeError(pEfhRunCtx);
    return NULL;
  }
  //-----------------------------------------------------------------

  while (gr->recovery_active) {
    if (procSesm(pEfhCtx,pEfhRunCtx,gr->recovery_sock,gr,EkaFhMode::MCAST)) break;
  } 
  gr->heartbeat_active = false;

  gr->gapClosed = true;
  close(gr->recovery_sock);
  gr->recovery_sock = -1;
  return NULL;
}
/* ##################################################################### */

void* getSesmData(void* attr) {

  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  EkaFhMiaxGr*   gr            = (EkaFhMiaxGr*) ((EkaFhThreadAttr*)attr)->gr;
  /* uint64_t   start_sequence = ((EkaFhThreadAttr*)attr)->startSeq; */
  /* uint64_t   end_sequence   = ((EkaFhThreadAttr*)attr)->endSeq; */
  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;

  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();

  if (op != EkaFhMode::DEFINITIONS) pthread_detach(pthread_self());

  EkaDev* dev = pEfhCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  if (dev->fh[pEfhCtx->fhId] == NULL) on_error("dev->fh[pEfhCtx->fhId] == NULL for pEfhCtx->fhId = %u",pEfhCtx->fhId);
  if (gr == NULL) on_error("gr == NULL");

  EKA_LOG("%s:%u %s",EKA_EXCH_DECODE(gr->exch),gr->id,op==EkaFhMode::SNAPSHOT ? "GAP_SNAPSHOT" : "DEFINITIONS");

  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  const struct timespec leaseTime = {.tv_sec = 180, .tv_nsec = 0};
  const struct timespec timeout   = {.tv_sec = 60,  .tv_nsec = 0};
  char credName[7] = {};
  memset (credName,'\0',sizeof(credName));
  memcpy (credName,gr->auth_user,sizeof(credName) - 1);
  const EkaGroup group{gr->exch, (EkaLSI)gr->id};
  int rc = dev->credAcquire(EkaCredentialType::kSnapshot, group, (const char*)credName, &leaseTime,&timeout,dev->credContext,&lease);
  if (rc != 0) on_error("%s:%u Failed to credAcquire for %s",EKA_EXCH_DECODE(gr->exch),gr->id,credName);
  EKA_LOG("%s:%u Sesm Credentials Accquired",EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------
  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);

  EKA_LOG("%s:%u - SESM to %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id,EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));

  gr->snapshot_active = true;
  if (op == EkaFhMode::DEFINITIONS) { 
    //-----------------------------------------------------------------
    while (gr->snapshot_active) {
      gr->recovery_sock = ekaTcpConnect(gr->snapshot_ip,gr->snapshot_port);
      //-----------------------------------------------------------------
      if (! sendLogin(gr)) {
	gr->sendRetransmitExchangeError(pEfhRunCtx);
	close(gr->recovery_sock);
	continue;
      }
      //-----------------------------------------------------------------
      if (! getLoginResponse(gr)) {
	gr->sendRetransmitExchangeError(pEfhRunCtx);
	sendLogOut(gr);
	close(gr->recovery_sock);
	continue;
      }
      break;
    }
    //-----------------------------------------------------------------
    std::thread heartBeat = std::thread(heartBeatThread,dev,gr,gr->recovery_sock);
    heartBeat.detach();
    //-----------------------------------------------------------------
    sendRequest(gr,'P');
    //-----------------------------------------------------------------
    while (gr->snapshot_active) { 
      if (procSesm(pEfhCtx,pEfhRunCtx,gr->recovery_sock,gr,op)) break;
    } 
    gr->heartbeat_active = false;
    //-----------------------------------------------------------------
    sendLogOut(gr);
    //-----------------------------------------------------------------
    close(gr->recovery_sock);
    gr->recovery_sock = -1;
  } else { // SNAPSHOT
    char snapshotRequests[] = {
      'S', // System State Refresh
      'U', // Underlying Trading Status Refresh
      'P', // Simple Series Update Refresh
      'Q'};// Simple Top of Market Refresh
    for (int i = 0; i < (int)sizeof(snapshotRequests); i ++) {
      EKA_LOG("%s:%u SESM Snapshot for \'%c\'",
	      EKA_EXCH_DECODE(gr->exch),gr->id,snapshotRequests[i]);
      while (gr->snapshot_active) {
	gr->recovery_sock = ekaTcpConnect(gr->snapshot_ip,gr->snapshot_port);
	//-----------------------------------------------------------------
	sendLogin(gr);
	//-----------------------------------------------------------------
	if (! getLoginResponse(gr)) {
	  gr->sendRetransmitExchangeError(pEfhRunCtx);
	  sendLogOut(gr);
	  close(gr->recovery_sock);
	  continue;
	}
	break;
      }
      //-----------------------------------------------------------------

      std::thread heartBeat = std::thread(heartBeatThread,dev,gr,gr->recovery_sock);
      heartBeat.detach();
      //-----------------------------------------------------------------
      sendRequest(gr,snapshotRequests[i]); 
      //-----------------------------------------------------------------
      while (gr->snapshot_active) { 
	if (procSesm(pEfhCtx,pEfhRunCtx,gr->recovery_sock,gr,op)) break;
      } 
      gr->heartbeat_active = false;
      //-----------------------------------------------------------------
      sendLogOut(gr);
      //-----------------------------------------------------------------
      close(gr->recovery_sock);
      gr->recovery_sock = -1;
    }
  }
  //-------------------------------------------------------
  gr->gapClosed = true;
  //-------------------------------------------------------
  EKA_TRACE("%s:%u End Of %s, gr->seq_after_snapshot = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,
	    op==EkaFhMode::SNAPSHOT ? "GAP_RECOVERY" : "DEFINITIONS" ,gr->seq_after_snapshot);

  rc = dev->credRelease(lease, dev->credContext);
  if (rc != 0) on_error("%s:%u Failed to credRelease for %s",EKA_EXCH_DECODE(gr->exch),gr->id,credName);
  EKA_LOG("%s:%u Sesm Credentials Released",EKA_EXCH_DECODE(gr->exch),gr->id);

  return NULL;
}



void* heartBeatThread(EkaDev* dev, EkaFhMiaxGr* gr, int sock) {
  sesm_header heartbeat = {
    .length		= 1, //sizeof(struct sesm_header) - sizeof(heartbeat.length),
    .type		= '1'
  };

  EKA_LOG("gr=%u to sock = %d",gr->id,sock);

  while(gr->heartbeat_active) {
    EKA_LOG("sending SESM hearbeat for gr=%u",gr->id);
    if(send(sock,&heartbeat,sizeof(sesm_header), 0) < 0) on_error("heartbeat send failed");
    usleep(900000);
  }
  EKA_LOG("SESM hearbeat thread terminated for gr=%u",gr->id);
  return NULL;
}
