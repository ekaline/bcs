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

#include "EkaFhBatsParser.h"
#include "EkaFhBatsGr.h"
#include "EkaFhThreadAttr.h"

int ekaTcpConnect(uint32_t ip, uint16_t port);
int ekaUdpConnect(EkaDev* dev, uint32_t ip, uint16_t port);

/* ##################################################################### */

void* spin_heartbeat_thread(void* attr) {
  pthread_detach(pthread_self());
  EkaFhBatsGr* gr = (EkaFhBatsGr*) attr;

  EkaDev* dev = gr->dev;
  batspitch_sequenced_unit_header heartbeat = {};
  heartbeat.length		= sizeof(heartbeat);
  heartbeat.count       	= 0;
  heartbeat.unit        	= gr->batsUnit;
  heartbeat.sequence       	= 0;
  //  EKA_LOG("%s:%u: start sending heartbeats",EKA_EXCH_DECODE(gr->exch),gr->id);
  while(gr->heartbeat_active) {
    EKA_LOG("%s:%u: sending Spin hearbeat",EKA_EXCH_DECODE(gr->exch),gr->id);
    if(send(gr->snapshot_sock,&heartbeat,sizeof(heartbeat), 0) < 0) on_error("heartbeat send failed");
    usleep(900000);
  }
  EKA_LOG("%s:%u: stop sending heartbeats",EKA_EXCH_DECODE(gr->exch),gr->id);
  return NULL;
}
/* ##################################################################### */

static void sendLogin (EkaFhBatsGr* gr) {
  EkaDev* dev = gr->dev;
  struct batspitch_login_request login_message = {};
  memset(&login_message,' ',sizeof(login_message));
  login_message.hdr.length = sizeof(login_message);
  login_message.hdr.count = 1;
  login_message.hdr.unit = gr->batsUnit;
  login_message.hdr.sequence = 0;

  login_message.length =  sizeof(login_message) - sizeof(login_message.hdr);
  login_message.type = (uint8_t)EKA_BATS_PITCH_MSG::LOGIN_REQUEST;

  memcpy(login_message.username,       gr->auth_user,    sizeof(login_message.username));
  memcpy(login_message.password,       gr->auth_passwd,  sizeof(login_message.password));
  memcpy(login_message.session_sub_id, gr->sessionSubID, sizeof(login_message.session_sub_id));

  char username2print[20] = {};
  char password2print[20] = {};
  char session_sub_id2print[20] = {};
  char filler2print[20] = {};
  memcpy(username2print,login_message.username,sizeof(login_message.username));
  memcpy(password2print,login_message.password,sizeof(login_message.password));
  memcpy(session_sub_id2print,login_message.session_sub_id,sizeof(login_message.session_sub_id));
  memcpy(filler2print,login_message.filler,sizeof(login_message.filler));

  EKA_LOG("sending login message: unit=%u, length=%u, type=0x%02x, session_sub_id[4]=|%s|, user[4]=|%s|, filler[2]=|%s|, passwd[10]=|%s|",
	  login_message.hdr.unit,
	  login_message.length,
	  login_message.type,
	  session_sub_id2print,
	  username2print,
	  filler2print,
	  password2print
	  );
#ifndef FH_LAB
  if(send(gr->snapshot_sock,&login_message,sizeof(login_message), 0) < 0) on_error("Login send failed for %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id);
#endif
  EKA_LOG("%s:%u Spin Login sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  //  hexDump("Spin Login Message sent",&login_message,sizeof(login_message));
  return;
}
/* ##################################################################### */

static bool getLoginResponse(EkaFhBatsGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u FH_LAB DUMMY Login Accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;  
#endif
  batspitch_login_response login_response ={};
  if (recv(gr->snapshot_sock,&login_response,sizeof(login_response),MSG_WAITALL) <= 0) 
    EKA_WARN("Spin connection reset by peer after Login (failed to receive Login Response) for %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id);
  if (login_response.type != EKA_BATS_PITCH_MSG::LOGIN_RESPONSE) EKA_WARN ("Unexpected Spin resonse type 0x%02x",login_response.type);

  if (login_response.status == 'N') EKA_WARN("Spin rejected login (\'N\') Not authorized (Invalid Username/Password)");
  if (login_response.status == 'B') EKA_WARN("Spin rejected login (\'B\') Session in use");
  if (login_response.status == 'S') EKA_WARN("Spin rejected login (\'S\') Invalid Session");
  if (login_response.status != 'A') EKA_WARN("Unknown Spin Login response status |%c|",login_response.status);
  if (login_response.hdr.count != 1) EKA_WARN("More than 1 message (%u) come with the Login Response",login_response.hdr.count);
  if (login_response.status == 'A') {
    EKA_LOG("%s:%u Login Accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
    return true;
  }
  return false;  
}
/* ##################################################################### */
static uint64_t getSpinImageSeq(EkaFhBatsGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u FH_LAB DUMMY \'Spin Image Available\' Accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
#else
  while (gr->snapshot_active) { // Accepted Login
    struct batspitch_sequenced_unit_header hdr = {};
    if (recv(gr->snapshot_sock,&hdr,sizeof(hdr),MSG_WAITALL) < (int) sizeof(struct batspitch_sequenced_unit_header)) {
      EKA_LOG("%s:%u: Spin Server connection reset by peer (failed to receive Unit Header)",EKA_EXCH_DECODE(gr->exch),gr->id);
      break;
    }
    int payload_size = hdr.length - sizeof(struct batspitch_sequenced_unit_header);
    uint8_t buf[1536] = {};
    int size = 0;
    if ((size = recv(gr->snapshot_sock,buf,payload_size,MSG_WAITALL)) != payload_size) {
      EKA_WARN("%s:%u: Spin Server connection reset by peer (received %u bytes < expected payload size of %d bytes)",
	       EKA_EXCH_DECODE(gr->exch),gr->id,size,payload_size);
      break;
    }
    uint8_t* ptr = &buf[0];
    for (uint i = 0; i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",EKA_EXCH_DECODE(gr->exch),gr->id,size);
      struct batspitch_dummy_header* msg_hdr = (struct batspitch_dummy_header*) ptr;
      if ((EKA_BATS_PITCH_MSG)msg_hdr->type == EKA_BATS_PITCH_MSG::SPIN_IMAGE_AVAILABLE) {
	EKA_LOG("%s:%u Spin Image Available at Sequence = %u",EKA_EXCH_DECODE(gr->exch),gr->id,((batspitch_spin_image_available*)ptr)->sequence);
	return ((batspitch_spin_image_available*)ptr)->sequence;
      }
      ptr += msg_hdr->length;
      //-----------------------------------------------------------------
    }
  }
#endif
  return true;  
}
/* ##################################################################### */
static bool getSpinResponse(EkaFhBatsGr* gr, EkaFhMode op) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u FH_LAB DUMMY Spin Response Accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
  return true;
#endif
  while (gr->snapshot_active) { // Accepted Login
    struct batspitch_sequenced_unit_header hdr = {};
    if (recv(gr->snapshot_sock,&hdr,sizeof(hdr),MSG_WAITALL) < (int) sizeof(struct batspitch_sequenced_unit_header)) {
      EKA_LOG("%s:%u: Spin Server connection reset by peer (failed to receive Unit Header)",EKA_EXCH_DECODE(gr->exch),gr->id);
      break;
    }
    int payload_size = hdr.length - sizeof(struct batspitch_sequenced_unit_header);
    uint8_t buf[1536] = {};
    int size = 0;
    if ((size = recv(gr->snapshot_sock,buf,payload_size,MSG_WAITALL)) != payload_size) {
      EKA_WARN("%s:%u: Spin Server connection reset by peer (received %u bytes < expected payload size of %d bytes)",
	       EKA_EXCH_DECODE(gr->exch),gr->id,size,payload_size);
      break;
    }
    uint8_t* ptr = &buf[0];
    for (uint i = 0; i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",EKA_EXCH_DECODE(gr->exch),gr->id,size);
      struct batspitch_dummy_header* msg_hdr = (struct batspitch_dummy_header*) ptr;
      if ((EKA_BATS_PITCH_MSG)msg_hdr->type == EKA_BATS_PITCH_MSG::SNAPSHOT_RESPONSE ||
	  (EKA_BATS_PITCH_MSG)msg_hdr->type == EKA_BATS_PITCH_MSG::DEFINITIONS_RESPONSE
	  ) {
	switch (((batspitch_spin_response*)ptr)->status) {
	case 'A' : // Accepted
	  EKA_LOG("%s:%u Spin %s Request Accepted: Count=%u, Sequence=%u",
		  EKA_EXCH_DECODE(gr->exch),gr->id,
		  op == EkaFhMode::DEFINITIONS ? "Definitions" : "Snapshot",
		  ((batspitch_spin_response*)ptr)->count,((batspitch_spin_response*)ptr)->sequence);
	  return true;
	case 'O' : 
	  EKA_WARN("%s:%u: Spin request rejected with (\'O\') - Out of range",
		   EKA_EXCH_DECODE(gr->exch),gr->id);
	  break;
	case 'S' : 
	  EKA_WARN("%s:%u: Spin request rejected with (\'S\') - Spin already in progress",
		   EKA_EXCH_DECODE(gr->exch),gr->id);
	  break;
	default  : 
	  EKA_WARN("%s:%u: Spin request rejected with unknown status |%c|",
		   EKA_EXCH_DECODE(gr->exch),gr->id,((batspitch_spin_response*)ptr)->status);
	}
	return false;
      }
      ptr += msg_hdr->length;
      //-----------------------------------------------------------------
    }
  }

  return false;  
}
/* ##################################################################### */
static void sendSpinRequest(EkaFhBatsGr* gr, EkaFhMode op, uint64_t image_sequence) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u FH_LAB DUMMY Spin Request sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  return;
#endif
  struct batspitch_spin_request request_message = {};
  request_message.hdr.length = sizeof(request_message);
  request_message.hdr.count = 1;
  request_message.hdr.unit = gr->batsUnit;
  request_message.hdr.sequence = 0;

  request_message.length   = sizeof(request_message) - sizeof(request_message.hdr);
  request_message.type     = op == EkaFhMode::DEFINITIONS ? (uint8_t) EKA_BATS_PITCH_MSG::DEFINITIONS_REQUEST : (uint8_t) EKA_BATS_PITCH_MSG::SNAPSHOT_REQUEST;
  request_message.sequence = op == EkaFhMode::DEFINITIONS ? 0 : image_sequence & 0xFFFFFFFF;
  EKA_LOG("%s:%u Sending %s Request, request_message.type = 0x%02x, request_message.sequence = %u",
	  EKA_EXCH_DECODE(gr->exch),
	  gr->id, 
	  op == EkaFhMode::DEFINITIONS ? "Definitions" : "Snapshot", 
	  request_message.type,
	  request_message.sequence
	  );

  if(send(gr->snapshot_sock,&request_message,sizeof(request_message), 0) < 0) on_error("Spin Request send failed");
  //  hexDump("Spin Request Message sent",&request_message,sizeof(request_message));
  return;
}

/* ##################################################################### */

void* getSpinData(void* attr) {
  //  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  EkaFhBatsGr*   gr            = (EkaFhBatsGr*)((EkaFhThreadAttr*)attr)->gr;
  //  uint64_t   start          = ((EkaFhThreadAttr*)attr)->startSeq;
  //  uint64_t   end            = ((EkaFhThreadAttr*)attr)->endSeq;
  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;
  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();

  if (op != EkaFhMode::DEFINITIONS) pthread_detach(pthread_self());

  EkaDev* dev = gr->dev;

  EKA_LOG("%s:%u: Spin %s at %s:%d",
	  EKA_EXCH_DECODE(gr->exch),gr->id, op==EkaFhMode::SNAPSHOT ? "GAP_RECOVERY" : "DEFINITIONS",
	  EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));
  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  const struct timespec leaseTime = {.tv_sec = 180, .tv_nsec = 0};
  const struct timespec timeout   = {.tv_sec = 60, .tv_nsec = 0};
  char credName[7] = {};
  memset (credName,'\0',sizeof(credName));
  memcpy (credName,gr->auth_user,sizeof(credName) - 1);
  const EkaGroup group{gr->exch, (EkaLSI)gr->id};
  int rc = dev->credAcquire(EkaCredentialType::kSnapshot, group, (const char*)credName, &leaseTime,&timeout,dev->credContext,&lease);
  if (rc != 0) on_error("Failed to credAcquire for %s",credName);

  int64_t requestedSpinSequence = 0;
  while (1) {
    //-----------------------------------------------------------------
    gr->snapshot_sock = ekaTcpConnect(gr->snapshot_ip,gr->snapshot_port);
    //-----------------------------------------------------------------
    sendLogin(gr);
    //-----------------------------------------------------------------
    if (! getLoginResponse(gr)) {
      gr->sendRetransmitExchangeError(pEfhRunCtx);
      close(gr->snapshot_sock);
      continue;
    }
    //-----------------------------------------------------------------

    if (op == EkaFhMode::SNAPSHOT) requestedSpinSequence = getSpinImageSeq(gr);
    //-----------------------------------------------------------------
    sendSpinRequest(gr, op, requestedSpinSequence);
    //-----------------------------------------------------------------
    if (! getSpinResponse(gr, op)) {
      gr->sendRetransmitExchangeError(pEfhRunCtx);
      close(gr->snapshot_sock);
      continue;
    }
    //-----------------------------------------------------------------
    break;
  }

  gr->heartbeat_active = true;

  pthread_t heartbeat_thread;
  dev->createThread((std::string("HB_") + std::string(EKA_EXCH_DECODE(gr->exch)) + '_' + std::to_string(gr->id)).c_str(),EkaServiceType::kHeartbeat,spin_heartbeat_thread,(void*)gr,dev->createThreadContext,(uintptr_t*)&heartbeat_thread);

  //-----------------------------------------------------------------
  gr->snapshot_active = true;

  uint64_t sequence = 0;
  while (gr->snapshot_active) { // Accepted Login
    struct batspitch_sequenced_unit_header hdr = {};
    if (recv(gr->snapshot_sock,&hdr,sizeof(hdr),MSG_WAITALL) < (int) sizeof(struct batspitch_sequenced_unit_header)) {
      EKA_LOG("%s:%u: Spin Server connection reset by peer (failed to receive Unit Header)",EKA_EXCH_DECODE(gr->exch),gr->id);
      break;
    }
    int payload_size = hdr.length - sizeof(struct batspitch_sequenced_unit_header);
    uint8_t buf[1536] = {};

    int size = 0;
    if ((size = recv(gr->snapshot_sock,buf,payload_size,MSG_WAITALL)) != payload_size) {
      EKA_WARN("%s:%u: Spin Server connection reset by peer (received %u bytes < expected payload size of %d bytes)",
	       EKA_EXCH_DECODE(gr->exch),gr->id,size,payload_size);
      break;
    }
    //    EKA_LOG("%s:%u: Sequenced Unit Header: Length=%u, Count=%u, Unit=%u, Sequence=%u",EKA_EXCH_DECODE(gr->exch),gr->id,hdr.length,hdr.count,hdr.unit,hdr.sequence);

    sequence = hdr.sequence;

    uint8_t* ptr = &buf[0];
    bool end_of_spin = false;
    for (uint i = 0; i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",EKA_EXCH_DECODE(gr->exch),gr->id,size);
      struct batspitch_dummy_header* msg_hdr = (struct batspitch_dummy_header*) ptr;
      size -= msg_hdr->length;

      end_of_spin = gr->parseMsg(pEfhRunCtx,ptr,sequence,op);
      if (end_of_spin) break;

      ptr += msg_hdr->length;
      //-----------------------------------------------------------------
    }
    if (end_of_spin) break;
    if (size != 0) on_error("%s:%u: after parsing all messages remaining buf size %d != 0",EKA_EXCH_DECODE(gr->exch),gr->id,size);
  }
  //-----------------------------------------------------------------
  dev->credRelease(lease, dev->credContext);
  gr->heartbeat_active = false;
  close(gr->snapshot_sock);
  EKA_LOG("End Of Spin for %s:%u, requestedSpinSequence=%ju, sequence from EKA_BATS_PITCH_MSG::SPIN_FINISHED = %ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,requestedSpinSequence,gr->seq_after_snapshot);
  gr->gapClosed = true;

  return NULL;
}

/* ##################################################################### */
static void sendGapRequest(EkaFhBatsGr* gr, uint64_t start, uint16_t cnt) {
  EkaDev* dev = gr->dev;

  batspitch_gap_request gap_request = {};
  memset(&gap_request,' ',sizeof(gap_request));
  gap_request.hdr.length = sizeof(gap_request);
  gap_request.hdr.count = 1;
  gap_request.hdr.unit = gr->batsUnit;
  gap_request.hdr.sequence = 0;

  gap_request.length   = sizeof(gap_request) - sizeof(gap_request.hdr);
  gap_request.type     = (uint8_t) EKA_BATS_PITCH_MSG::GAP_REQUEST;
  gap_request.sequence = start & 0xFFFFFFFF;
  gap_request.count    = cnt;
  EKA_LOG("%s:%u Sending GRP Request from %ju for %u messages",EKA_EXCH_DECODE(gr->exch),gr->id,start,cnt);

#ifndef FH_LAB
  if(send(gr->snapshot_sock,&gap_request,sizeof(gap_request), 0) < 0) on_error("GRP Request send failed");
#endif
  //  hexDump("GRP Request Message sent",&gap_request,sizeof(gap_request));
  return;
}

/* ##################################################################### */
int getGapResponse(EkaFhBatsGr* gr) {
  EkaDev* dev = gr->dev;
#ifdef FH_LAB
  EKA_LOG("%s:%u FH_LAB DUMMY Gap request Accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
  return 0;
#endif
  batspitch_gap_response gap_response ={};
  gap_response.type = static_cast< uint8_t > (EKA_BATS_PITCH_MSG::GAP_RESPONSE);
  gap_response.status = 'A';
  gap_response.hdr.count = 1;

  if (recv(gr->snapshot_sock,&gap_response,sizeof(gap_response),MSG_WAITALL) <= 0) {
    EKA_WARN("%s%u: connection reset by peer",EKA_EXCH_DECODE(gr->exch),gr->id);
    return -1;

  }

  if (gap_response.type != static_cast< uint8_t > (EKA_BATS_PITCH_MSG::GAP_RESPONSE)) {
    EKA_WARN ("Unexpected GRP resonse type 0x%02x",gap_response.type);
    return -1;
  }
  if (gap_response.hdr.count != 1) {
    EKA_WARN("More than 1 message (%u) come with the Gap Response",gap_response.hdr.count);
    return -1;
  }
  switch (gap_response.status) {
  case 'A':
    EKA_LOG("%s:%u Accepted: Unit=%u, Seq=%u. Cnt=%u",EKA_EXCH_DECODE(gr->exch),gr->id,
	    gap_response.unit, gap_response.sequence, gap_response.count);
    return gap_response.count;
  case 'O': 
    EKA_WARN("Out-of-range");
    break;
  case 'D': 
    EKA_WARN("Daily gap request allocation exhausted");
    break;
  case 'M': 
    EKA_WARN("Minute gap request allocation exhausted");
    break;
  case 'S': 
    EKA_WARN("Second gap request allocation exhausted");
    break;
  case 'C': 
    EKA_WARN("Count request limit for one gap request exceeded");
    break;
  case 'I': 
    EKA_WARN("Invalid Unit specified in request");
    break;
  case 'U': 
    EKA_WARN("Unit is currently unavailable");
    break;
  default:
    EKA_WARN("Unexpected Gap response status: %c",gap_response.status);
  }
  return -1;
}

/* ##################################################################### */
void* getGrpRetransmitData(void* attr) {
  pthread_detach(pthread_self());

  //  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;

  EkaFhBatsGr*   gr            = (EkaFhBatsGr*)((EkaFhThreadAttr*)attr)->gr;
  EkaDev* dev = gr->dev;
  uint64_t   start          = ((EkaFhThreadAttr*)attr)->startSeq;
  uint64_t   end            = ((EkaFhThreadAttr*)attr)->endSeq;
  EKA_LOG("%s:%u Closing Gap by GRP retransmitting %ju .. %ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,start,end);

#ifdef FH_LAB
  EKA_LOG("%s:%u FH_LAB DUMMY Gap closed, gr->seq_after_snapshot = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
  gr->seq_after_snapshot = end + 1;
  gr->gapClosed = true;
#else

  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  //  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;
  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();

  if (end - start > 65000) on_error("Gap %ju is too high (> 65000), start = %ju, end = %ju",end - start, start, end);

  int cnt = -1;
  while (1) {
    gr->snapshot_sock = ekaTcpConnect(gr->snapshot_ip,gr->snapshot_port);
    //-----------------------------------------------------------------
    gr->recovery_sock = ekaUdpConnect(dev,gr->recovery_ip, gr->recovery_port);
    //-----------------------------------------------------------------
    sendLogin(gr);
    //-----------------------------------------------------------------
    getLoginResponse(gr);
    //-----------------------------------------------------------------
    sendGapRequest(gr, start, (uint16_t) (end - start + 1));
    //-----------------------------------------------------------------
    cnt = getGapResponse(gr);
    if (cnt < 0) {
      gr->sendRetransmitExchangeError(pEfhRunCtx);
      close(gr->snapshot_sock);
      close(gr->recovery_sock);
      continue;
    }
    break;
  }
  //-----------------------------------------------------------------
  struct sockaddr_in recovery_addr = {};
  recovery_addr.sin_addr.s_addr = gr->recovery_ip;
  recovery_addr.sin_port = gr->recovery_port;

  bool endReached = false;
  uint64_t sequence = 0;
  while (sequence <= end) {
    socklen_t addrlen = sizeof(struct sockaddr);

    batspitch_sequenced_unit_header hdr = {};
    int size = recvfrom(gr->recovery_sock, &hdr, sizeof(hdr), 0, (struct sockaddr*) &recovery_addr, &addrlen);
    if (size < (int) sizeof(hdr)) on_error("read_size %d < %ju",size,sizeof(hdr));

    if (hdr.unit != gr->batsUnit) continue;

    int payload_size = hdr.length - sizeof(hdr);
    uint8_t buf[1536] = {};
    size = recvfrom(gr->recovery_sock, buf, payload_size, 0, (struct sockaddr*) &recovery_addr, &addrlen);
    if (size < payload_size) on_error("read_size %d < %d", size, payload_size);

    uint8_t* ptr = buf;
    sequence = hdr.sequence;
    for (uint i = 0; i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",EKA_EXCH_DECODE(gr->exch),gr->id,size);
      batspitch_dummy_header* msg_hdr = (batspitch_dummy_header*) ptr;
      size -= msg_hdr->length;

      if (sequence >= start) gr->parseMsg(pEfhRunCtx,ptr,sequence,EkaFhMode::SNAPSHOT);
      sequence++;

      if (sequence > end) { endReached = true; break; }

      ptr += msg_hdr->length;
    }
    if (endReached) break;
    if (size != 0) on_error("%s:%u: after parsing all messages remaining buf size %d != 0",
			    EKA_EXCH_DECODE(gr->exch),gr->id,size);
  }
  gr->seq_after_snapshot = sequence;
  gr->gapClosed = true;
  EKA_LOG("%s:%u: GRP closed: start=%ju, end=%ju, cnt=%d, seq_after_snapshot=%ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,start,end,cnt,gr->seq_after_snapshot);
  //-----------------------------------------------------------------
  close(gr->snapshot_sock);
  close(gr->recovery_sock);
#endif
  return NULL;
}
