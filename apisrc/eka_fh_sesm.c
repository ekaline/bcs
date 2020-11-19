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

#include "eka_fh_book.h"
#include "eka_data_structs.h"
#include "eka_fh_group.h"
#include "EkaDev.h"
#include "eka_fh.h"

#include "eka_fh_miax_messages.h"

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port);
void* heartBeatThread(EkaDev* dev, FhMiaxGr* gr,int sock);

/* ##################################################################### */

static void sendLogin (FhMiaxGr* gr) {
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
#ifndef FH_LAB
  if(send(gr->recovery_sock,&sesm_login_msg,sizeof(struct sesm_login_req), 0) < 0) on_error("SESM Login send failed");
#endif
  EKA_LOG("%s:%u Sesm Login sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  return;
}
/* ##################################################################### */

static bool getLoginResponse(FhMiaxGr* gr) {
  EkaDev* dev = gr->dev;

  struct sesm_login_response sesm_response_msg ={};
  if (const int r = recv(gr->recovery_sock,&sesm_response_msg,sizeof(struct sesm_login_response),MSG_WAITALL); r == -1)
    on_error("%s:%u failed to receive SESM login response",EKA_EXCH_DECODE(gr->exch),gr->id);
  else if (r == 0)
    on_error("%s:%u unexpected request server socket EOF (expected login SESM response)",EKA_EXCH_DECODE(gr->exch),gr->id);

  if (sesm_response_msg.header.type == 'G')  on_error("SESM sent GoodBye Packet with reason: \'%c\'",sesm_response_msg.status);
  if (sesm_response_msg.header.type != 'R')  on_error("sesm_response_msg.header.type \'%c\' != \'R\' ",sesm_response_msg.header.type);

  if (sesm_response_msg.status == 'X') on_error("SESM Login Response: \'X\' -- Rejected: Invalid Username/Computer ID combination");
  if (sesm_response_msg.status == 'S') on_error("SESM Login Response: \'S\' -- Rejected: Requested session is not available");
  if (sesm_response_msg.status == 'N') on_error("SESM Login Response: \'N\' -- Rejected: Invalid start sequence number requested");
  if (sesm_response_msg.status == 'I') on_error("SESM Login Response: \'I\' -- Rejected: Incompatible Session protocol version");
  if (sesm_response_msg.status == 'A') on_error("SESM Login Response: \'A\' -- Rejected: Incompatible Application protocol version");
  if (sesm_response_msg.status == 'L') on_error("SESM Login Response: \'L\' -- Rejected: Request rejected because client already logged in");
  if (sesm_response_msg.status != ' ') on_error("Unknown SESM Login Response: \'%c\'",sesm_response_msg.status);

  EKA_LOG("%s:%u SESM Login accepted. Highest sequence available=%ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,sesm_response_msg.sequence);
  return true;
}

/* ##################################################################### */
static void sendRequest(FhMiaxGr* gr, char refreshType) {
  EkaDev* dev = gr->dev;

  struct miax_request def_request_msg = {};
  def_request_msg.header.length = sizeof(struct miax_request) - sizeof(def_request_msg.header.length);
  def_request_msg.header.type = 'U';  // Unsequenced
  def_request_msg.request_type = 'R'; // Refresh
  def_request_msg.refresh_type = refreshType;

  //  hexDump("MIAX definitions request",(char*) &def_request_msg,sizeof(struct miax_request));
#ifndef FH_LAB
  if(send(gr->recovery_sock,&def_request_msg,sizeof(def_request_msg), 0) < 0) on_error("SESM Request send failed");
#endif
  EKA_LOG("%s:%u \'%c\' Request sent",EKA_EXCH_DECODE(gr->exch),gr->id, refreshType);
  return;
}

/* ##################################################################### */
static void sendRetransmitRequest(FhMiaxGr* gr, uint64_t start, uint64_t end) {
  EkaDev* dev = gr->dev;
  sesm_retransmit_req retransmit_req = {};
  retransmit_req.header.length = sizeof(retransmit_req) - sizeof(retransmit_req.header.length);
  retransmit_req.header.type = 'A';
  retransmit_req.start = start;
  retransmit_req.end = end;
#ifndef FH_LAB
  if(send(gr->recovery_sock,&retransmit_req,sizeof(retransmit_req), 0) < 0) 
    on_error("%s:%u SESM Retransmit Request send failed",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif
  EKA_LOG("%s:%u Retransmit Request sent for %ju .. %ju",EKA_EXCH_DECODE(gr->exch),gr->id, start, end);
  return;
}

/* ##################################################################### */
static void sendLogOut(FhMiaxGr* gr) {
  EkaDev* dev = gr->dev;

  //--------------- SESM Logout Request -------------------
  struct sesm_logout_req sesm_logout_msg = {};
  sesm_logout_msg.header.length = sizeof(struct sesm_logout_req) - sizeof(sesm_logout_msg.header.length);
  sesm_logout_msg.header.type = 'X';
  sesm_logout_msg.reason = ' '; // Graceful Logout (Done for now)
#ifndef FH_LAB
  if(send(gr->recovery_sock,&sesm_logout_msg,sizeof(struct sesm_logout_req), 0) < 0) on_error("SESM Logout send failed");
#endif
  EKA_LOG("%s:%u SESM Logout sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  return;
}


/* ##################################################################### */
static bool procSesm(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, int sock, FhMiaxGr* gr,EkaFhMode op) {
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
  else if (recv_size == 0)
    on_error("%s:%u unexpected request server socket EOF (expected SESM payload)",EKA_EXCH_DECODE(gr->exch),gr->id);

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
//void eka_get_sesm_retransmit(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhMiaxGr* gr, uint64_t start, uint64_t end) {
void* eka_get_sesm_retransmit(void* attr) {

  pthread_detach(pthread_self());

  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  FhMiaxGr*  gr             = (FhMiaxGr*)((EkaFhThreadAttr*)attr)->gr;
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
  ekaTcpConnect(&gr->recovery_sock,gr->recovery_ip,gr->recovery_port);
  //-----------------------------------------------------------------
  sendLogin(gr);
  //-----------------------------------------------------------------
  getLoginResponse(gr);
  //-----------------------------------------------------------------
  /* std::thread heartBeat = std::thread(heartBeatThread,dev,gr,gr->recovery_sock); */
  /* heartBeat.detach(); */
  //-----------------------------------------------------------------
  sendRetransmitRequest(gr,start,end);
  //-----------------------------------------------------------------
  gr->recovery_active = true;
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

void* eka_get_sesm_data(void* attr) {

  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  FhMiaxGr*   gr            = (FhMiaxGr*) ((EkaFhThreadAttr*)attr)->gr;
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
    ekaTcpConnect(&gr->recovery_sock,gr->snapshot_ip,gr->snapshot_port);
    //-----------------------------------------------------------------
    sendLogin(gr);
    //-----------------------------------------------------------------
    getLoginResponse(gr);
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
      ekaTcpConnect(&gr->recovery_sock,gr->snapshot_ip,gr->snapshot_port);
      //-----------------------------------------------------------------
      sendLogin(gr);
      //-----------------------------------------------------------------
      getLoginResponse(gr);
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



void* heartBeatThread(EkaDev* dev, FhMiaxGr* gr, int sock) {
  sesm_header heartbeat = {
    .length		= sizeof(struct sesm_header) - sizeof(heartbeat.length),
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
