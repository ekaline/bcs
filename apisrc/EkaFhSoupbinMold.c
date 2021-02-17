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

#include "EkaFhNasdaqGr.h"
#include "EkaFhThreadAttr.h"

struct soupbin_header {
  uint16_t    length;
  char        type;
} __attribute__((packed));

struct soupbin_login_req {
  struct soupbin_header	header;
  char		       	username[6];
  char		       	password[10];
  char			session[10];
  char			sequence[20];
} __attribute__((packed));

//-----------------------------------------------

static int sendUdpPkt (EkaDev* dev, 
		       EkaFhNasdaqGr* gr, 
		       int sock, 
		       void* sendBuf, 
		       size_t size, 
		       const sockaddr* addr, 
		       const char* msgName) {
  static const int SendMaxReTry = 3;
  int bytesSent = -1;
  for (auto i = 0; i < SendMaxReTry; i++) {
    int bytesSent = sendto(sock,sendBuf,size,0,addr,sizeof(sockaddr));
    if (bytesSent == (int) size) {
      EKA_TRACE("%s:%u %s of %d bytes is sent",
		EKA_EXCH_DECODE(gr->exch),gr->id,msgName,bytesSent);
      return bytesSent;
    } else {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u sendto failed for %s to: %s:%u bytesSent=%d: %s",
	       EKA_EXCH_DECODE(gr->exch),gr->id, msgName,
	       EKA_IP2STR(((sockaddr_in*)addr)->sin_addr.s_addr),
	       be16toh(((sockaddr_in*)addr)->sin_port),
	       bytesSent,
	       strerror(dev->lastErrno));
    }
    sleep(1);
  }
  return bytesSent;
}
//-----------------------------------------------
static int recvUdpPkt (EkaDev* dev, 
		       EkaFhNasdaqGr* gr, 
		       int sock, 
		       void* recvBuf, 
		       size_t size, 
		       sockaddr* addr, 
		       const char* msgName) {
  int receivedBytes = -1;
  static const int MaxIterations = 1000000;
  socklen_t addrlen = sizeof(sockaddr);
  int reTryCnt = 0;
  while (1) {
    //    receivedBytes = recvfrom(sock, recvBuf, size, 0, addr, &addrlen); 
    receivedBytes = recvfrom(sock, recvBuf, size, MSG_DONTWAIT, addr, &addrlen); 
    if (receivedBytes > 0) {
      EKA_TRACE("%s:%u %s of %d bytes is received",EKA_EXCH_DECODE(gr->exch),gr->id,msgName,receivedBytes);
      return receivedBytes;
    }

    if (receivedBytes < 0) {
      dev->lastErrno = errno;
      EKA_WARN("recvfrom MOLD UDP socket failed: %s",strerror(dev->lastErrno));
      return -1;
    }

    if (++ reTryCnt % MaxIterations == 0) {
      EKA_WARN("%s:%u Failed to receive %s: (receivedBytes = %d) after %d iterations. Giving up ...",
    	       EKA_EXCH_DECODE(gr->exch),gr->id,msgName,receivedBytes,reTryCnt);
      return -1;
    }
    /* if (receivedBytes < 0)  */
    /*   on_error("%s:%u Failed receiving %s: receivedBytes = %d", */
    /* 	       EKA_EXCH_DECODE(gr->exch),gr->id,msgName,receivedBytes); */
    /* if (receivedBytes == 0) { */
    /*   EKA_WARN("%s:%u Receiving %s: receivedBytes = %d. Retrying...", */
    /* 	       EKA_EXCH_DECODE(gr->exch),gr->id,msgName,receivedBytes); */
    /*   continue; */
    /* } */
    /* if (receivedBytes <= 0)  */
    /*   EKA_TRACE("%s:%u %s of %d bytes is received",EKA_EXCH_DECODE(gr->exch),gr->id,msgName,receivedBytes); */

  }
  return -1;
}

//-----------------------------------------------
/* ##################################################################### */

static int sendHearBeat(int sock) {
  soupbin_header heartbeat = {
    .length		= be16toh(1),
    .type		= 'R'
  };

  return send(sock,&heartbeat,sizeof(heartbeat), 0);
}

/* ##################################################################### */
void* soupbin_heartbeat_thread(void* attr) {
  pthread_detach(pthread_self());
  EkaFhNasdaqGr* gr = (EkaFhNasdaqGr*) attr;
  EkaDev* dev = gr->dev;
  gr->heartbeatThreadDone = false;
  struct soupbin_header heartbeat = {};
  heartbeat.length		= htons(1);
  heartbeat.type		= 'R';
  EKA_TRACE("%s:%u: sending heartbeats to sock = %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  while(gr->heartbeat_active) {
    /* EKA_TRACE("%s:%u sending Glimpse hearbeat on sock %d, recovery_sequence = %ju, currUdpSequence=%ju", */
    /* 	      EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock,gr->recovery_sequence,gr->q == NULL ? 0 : gr->q->currUdpSequence); */
    if(send(gr->snapshot_sock,&heartbeat,sizeof(struct soupbin_header), 0) < 0) 
      on_error("%s:%u: heartbeat send failed, gr->snapshot_sock=%d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
    usleep(900000);
  }
  gr->heartbeatThreadDone = true;

  EKA_LOG("%s:%u: Glimpse hearbeat thread terminated on sock %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  return NULL;
}

static bool sendLogin (EkaFhNasdaqGr* gr, uint64_t start_sequence) {
  EkaDev* dev = gr->dev;

  struct soupbin_header header = {};
  struct soupbin_login_req login_message = {};
  login_message.header.length = htons(sizeof(struct soupbin_login_req)-sizeof(header.length));
  login_message.header.type		= 'L';
  memset(login_message.session,' ',sizeof(login_message.session));
  //  memcpy(login_message.sequence, "                   1", sizeof(login_message.sequence));

  char tmp_start_sequence_str[21];
  sprintf(tmp_start_sequence_str,"%20ju",start_sequence);
  memcpy(login_message.sequence,tmp_start_sequence_str,20);
  memcpy(login_message.username, gr->auth_user,   sizeof(login_message.username));
  memcpy(login_message.password, gr->auth_passwd, sizeof(login_message.password));
#if 1
  EKA_LOG("%s:%u: sending login message: user[6]=|%s|, passwd[10]=|%s|, session[10]=|%s|, sequence[20]=|%s|",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  std::string(login_message.username,sizeof(login_message.username)).c_str(),
	  std::string(login_message.password,sizeof(login_message.password)).c_str(),
	  std::string(login_message.session, sizeof(login_message.session) ).c_str(),
	  std::string(login_message.sequence,sizeof(login_message.sequence)).c_str()
	  );
#endif	
  if(send(gr->snapshot_sock,&login_message,sizeof(login_message), 0) < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u Login send failed, gr->snapshot_sock = %d: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock,strerror(dev->lastErrno));
    return false;
  }
  return true;
}

static void sendLogout (EkaFhNasdaqGr* gr) {
  EkaDev* dev = gr->dev;
  struct soupbin_header logout_request = {};
  logout_request.length		= htons(1);
  logout_request.type		= 'O';
  if(send(gr->snapshot_sock,&logout_request,sizeof(logout_request) , 0) < 0) {
    EKA_WARN("%s:%u Logout send failed, gr->snapshot_sock = %d: ",
	     EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock,strerror(errno));
  } else {
    EKA_LOG("%s:%u Logout sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  }
  return;
}
/* ##################################################################### */

static bool getLoginResponse(EkaFhNasdaqGr* gr) {
  EkaDev* dev = gr->dev;

  soupbin_header soupbin_hdr ={};
  if (recv(gr->snapshot_sock,&soupbin_hdr,sizeof(soupbin_header),MSG_WAITALL) <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u Glimpse connection reset by peer after Login (failed to receive SoupbinHdr), gr->snapshot_sock = %d",
	     EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
    return false;
  }
  char soupbin_buf[100] = {};
  if (recv(gr->snapshot_sock,soupbin_buf,be16toh(soupbin_hdr.length) - sizeof(soupbin_hdr.type),MSG_WAITALL) <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u failed to receive Soupbin message from Glimpse, gr->snapshot_sock = %d",
	     EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
    return false;
  }
  char* session_id = soupbin_buf;

  switch (soupbin_hdr.type) {
  case 'J' : {
    const char* rejectReason = 
      soupbin_buf[0] == 'A' ? "Not Authorized. There was an invalid username and password combination in the Login Request Message." :
      soupbin_buf[0] == 'S' ? "Session not available. The Requested Session in the Login Request Packet was either invalid or not available." :
      "Unknown reason";

    on_error("Glimpse rejected login (\'J\') message with code: \'%c\', reject reason: \'%s\'",
	     soupbin_buf[0], rejectReason);
  }

  case 'H' :
    EKA_WARN("Soupbin Heartbeat arrived before login");
    return false;

  case 'A' : {
    char first_seq[20] = {};
    memcpy(first_seq,session_id+10,sizeof(first_seq));
    gr->recovery_sequence = strtoul(first_seq, NULL, 10);
  
    EKA_LOG("%s:%u Login accepted for session_id=\'%s\', first_seq= \'%s\' (=%ju)",
	    EKA_EXCH_DECODE(gr->exch),gr->id,session_id,first_seq,gr->recovery_sequence);
  }
    break;

  default:
    EKA_WARN("Unknown Soupbin message type \'%c\' arrived after Login request",soupbin_hdr.type);
    return false;
  }

  return true;  
}
/* ##################################################################### */
static EkaFhParseResult procSoupbinPkt(const EfhRunCtx* pEfhRunCtx, 
				       EkaFhNasdaqGr*   gr,
				       uint64_t         end_sequence,
				       EkaFhMode        op) {
  EkaDev* dev = gr->dev;
  soupbin_header hdr ={};
  int r = recv(gr->snapshot_sock,&hdr,sizeof(hdr),MSG_WAITALL);
  if (r <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Soupbin Server connection reset by peer (failed to receive SoupbinHdr), r = %d: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     r,
	     strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }
  uint8_t soupbin_buf[2000] = {};
  int payloadLen = be16toh(hdr.length) - sizeof(hdr.type);
  if (payloadLen < 0) {
    hexDump("SoupbinHdr",&hdr,sizeof(hdr));
    on_error("payloadLen = %d, hdr.length = %d",
	     payloadLen,be16toh(hdr.length));
  }
  r = recv(gr->snapshot_sock,
	   soupbin_buf,
	   payloadLen,
	   MSG_WAITALL);
  if (r < payloadLen) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u failed to receive SoupbinBuf: received %d, expected %d: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     r,
	     payloadLen,
	     strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }
  bool sequencedPkt = false;
  bool lastMsg      = false;
  /* ------------ */
  switch (hdr.type) {
  case 'H' : // Hearbeat
    if (gr->feed_ver == EfhFeedVer::kPHLX && 
	(op == EkaFhMode::DEFINITIONS  || end_sequence == 1) 
	&& ++gr->hearbeat_ctr == 5) { 
      return EkaFhParseResult::End;
    }
    break;

    /* ------------ */
  case 'Z' : // End of Session Packet
    EKA_WARN("%s:%u Soupbin closed the session with Z (End of Session Packet)",
	     EKA_EXCH_DECODE(gr->exch),gr->id);
    return EkaFhParseResult::End;

    /* ------------ */
  case '+' : // Debug Packet
    EKA_LOG("%s:%u Soupbin Debug Msg: \'%s\'",
	    EKA_EXCH_DECODE(gr->exch),gr->id,
	    (unsigned char*)&soupbin_buf[0]);
    break;
    /* ------------ */
  case 'S' : // Sequenced Packet
    sequencedPkt = true;
    /* ------------ */
  default:
    lastMsg = gr->parseMsg(pEfhRunCtx,soupbin_buf,gr->recovery_sequence,op);

    if (lastMsg) {
      gr->seq_after_snapshot = gr->recovery_sequence + 1;
      EKA_LOG("%s:%u After lastMsg message: seq_after_snapshot = %ju, recovery_sequence = %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,soupbin_buf,gr->seq_after_snapshot,gr->recovery_sequence);
      return EkaFhParseResult::End;
    }

    if (sequencedPkt) gr->recovery_sequence++;
    if (end_sequence != 0 && gr->recovery_sequence >= end_sequence) {
      gr->seq_after_snapshot = gr->recovery_sequence + 1;
      EKA_LOG("%s:%u Snapshot Gap is closed: recovery_sequence == end_sequence %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,end_sequence);
      return EkaFhParseResult::End;
    }
    /* ------------ */
  } // switch (hdr.type)
  return EkaFhParseResult::NotEnd;
}
/* ##################################################################### */
static bool soupbinCycle(EkaDev*        dev, 
			   EfhRunCtx*     pEfhRunCtx, 
			   EkaFhMode      op,
			   EkaFhNasdaqGr* gr, 
			   uint64_t       start_sequence,
			   uint64_t       end_sequence,
			   const int      MaxTrials
			 ) {
  EKA_LOG("%s:%u %s start_sequence=%ju, end_sequence=%ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	  start_sequence,end_sequence);

  EkaFhParseResult parseResult;
  auto lastHeartBeatTime = std::chrono::high_resolution_clock::now();
  std::chrono::high_resolution_clock::time_point now;
  gr->hearbeat_ctr = 0;

  gr->snapshot_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (gr->snapshot_sock < 0) on_error("%s:%u: failed to open TCP socket",
				      EKA_EXCH_DECODE(gr->exch),gr->id);

  static const int TimeOut = 1; // seconds
  struct timeval tv = {
    .tv_sec = TimeOut
  }; 
  setsockopt(gr->snapshot_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in remote_addr = {};
  remote_addr.sin_addr.s_addr = gr->snapshot_ip;
  remote_addr.sin_port        = gr->snapshot_port;
  remote_addr.sin_family      = AF_INET;
  if (connect(gr->snapshot_sock,(sockaddr*)&remote_addr,sizeof(sockaddr_in)) != 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u Tcp Connect to %s:%u failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(remote_addr.sin_addr.s_addr),
	     be16toh   (remote_addr.sin_port),
	     strerror(dev->lastErrno));

    goto ITERATION_FAIL;
  }
  //-----------------------------------------------------------------
  if (! sendLogin(gr, start_sequence)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (! getLoginResponse(gr) ) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  while (gr->snapshot_active) {
    now = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartBeatTime).count() > 900) {
      sendHearBeat(gr->snapshot_sock);
      lastHeartBeatTime = now;
      /* if (isTradingHours(9,30,16,00)) */
      /* 	EKA_TRACE("%s:%u: Heartbeat: %s start=%10ju, curr=%10ju, end=%10ju", */
      /* 		  EKA_EXCH_DECODE(gr->exch),gr->id, */
      /* 		  EkaFhMode2STR(op), */
      /* 		  start_sequence, */
      /* 		  gr->recovery_sequence, */
      /* 		  end_sequence); */
    }

    parseResult = procSoupbinPkt(pEfhRunCtx,gr,end_sequence,op);
    switch (parseResult) {
    case EkaFhParseResult::End:
      goto SUCCESS_END;
    case EkaFhParseResult::NotEnd:
      break;
    case EkaFhParseResult::SocketError:
      start_sequence = gr->recovery_sequence;
      goto ITERATION_FAIL;
    default:
      on_error("Unexpected parseResult %d",(int)parseResult);
    }
  }
  //-----------------------------------------------------------------
  goto SUCCESS_END;

 ITERATION_FAIL:    
  sendLogout(gr);
  close(gr->snapshot_sock);
  return false;

 SUCCESS_END:
  sendLogout(gr);
  close(gr->snapshot_sock);
  return true;
}

/* ##################################################################### */

void* getSoupBinData(void* attr) {
#ifdef FH_LAB
  return NULL;
#endif

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhNasdaqGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");

  EfhCtx*    pEfhCtx        = params->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = params->pEfhRunCtx;
  uint64_t   start_sequence = params->startSeq;
  uint64_t   end_sequence   = params->endSeq;
  EkaFhMode  op             = params->op;

  delete params;

  if (op != EkaFhMode::DEFINITIONS) pthread_detach(pthread_self());

  EkaDev* dev = pEfhCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  if (dev->fh[pEfhCtx->fhId] == NULL) 
    on_error("dev->fh[pEfhCtx->fhId] == NULL for pEfhCtx->fhId = %u",pEfhCtx->fhId);
  if (gr == NULL) on_error("gr == NULL");

  EKA_LOG("%s:%u %s : start_sequence=%ju, end_sequence=%ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  EkaFhMode2STR(op),
	  start_sequence,end_sequence
	  );
  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  gr->credentialAcquire(gr->auth_user,sizeof(gr->auth_user),&lease);

  //-----------------------------------------------------------------
  const int MaxTrials = 4;
  bool success = false;
  gr->snapshot_active = true;
  //-----------------------------------------------------------------
  for (auto trial = 0; trial < MaxTrials && gr->snapshot_active; trial++) {
    EKA_LOG("%s:%d %s cycle: trial: %d / %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),trial,MaxTrials);

    success = soupbinCycle(dev,
			   pEfhRunCtx,
			   op,
			   gr,
			   start_sequence,
			   end_sequence,
			   MaxTrials);
    if (success) break;
    gr->sendRetransmitSocketError(pEfhRunCtx);    
  }
  //-----------------------------------------------------------------
  int rc = dev->credRelease(lease, dev->credContext);
  if (rc != 0) on_error("%s:%u Failed to credRelease",
			EKA_EXCH_DECODE(gr->exch),gr->id);
  EKA_LOG("%s:%u Soupbin Credentials Released",
	  EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------
  gr->snapshot_active = false;

  if (success) {
    EKA_LOG("%s:%u End Of %s, seq_after_snapshot = %ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,
	    EkaFhMode2STR(op),gr->seq_after_snapshot);

    gr->gapClosed = true;
  } else {
    EKA_WARN("%s:%u soupbinCycle Failed after %d trials. Exiting...",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
    on_error("%s:%u Failed after %d trials. Exiting...",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
  }
  gr->snapshotThreadDone = true;
  return NULL;
}


void* getMolUdp64Data(void* attr) {
  pthread_detach(pthread_self());

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhNasdaqGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");

  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
  uint64_t     start      = params->startSeq;
  uint64_t     end        = params->endSeq;
  delete params;

  EkaDev* dev = gr->dev;
  assert (dev != NULL);
  int udpSock = socket(AF_INET,SOCK_DGRAM,0);
  if (udpSock < 0) 
    on_error("%s:%u: Failed to open socket: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(errno));

  
  int const_one = 1;
  if (setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, &const_one, sizeof(int)) < 0) {
    dev->lastErrno = errno;
    gr->sendRetransmitSocketError(pEfhRunCtx);
    on_error("setsockopt(SO_REUSEADDR) failed");
  }

  if (setsockopt(udpSock, SOL_SOCKET, SO_REUSEPORT, &const_one, sizeof(int)) < 0) {
    dev->lastErrno = errno;
    gr->sendRetransmitSocketError(pEfhRunCtx);
    on_error("setsockopt(SO_REUSEPORT) failed");
  }

  sockaddr_in local2bind = {};
  local2bind.sin_family      = AF_INET;
  local2bind.sin_addr.s_addr = INADDR_ANY; //gr->recovery_ip;
  local2bind.sin_port        = gr->recovery_port;

  if (bind(udpSock,(sockaddr*) &local2bind, sizeof(sockaddr)) < 0) {
    dev->lastErrno = errno;
    gr->sendRetransmitSocketError(pEfhRunCtx);
    on_error("bind UDP socket failed");
  }
  EKA_LOG("%s:%u: Udp recovery socket is binded to: %s:%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  EKA_IP2STR(local2bind.sin_addr.s_addr),be16toh(local2bind.sin_port));


  ifreq ifr = {};
  memset(&ifr, 0, sizeof(ifreq));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), dev->genIfName);
  ioctl(udpSock, SIOCGIFINDEX, &ifr);
  if (setsockopt(udpSock, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(ifreq)) < 0) {
  //  if (setsockopt(udpSock, SOL_SOCKET, SO_BINDTODEVICE, dev->genIfName, strlen(dev->genIfName)+1) < 0) {
    EKA_WARN("%s:%u: setsockopt SO_BINDTODEVICE failed binding to %s (len=%d), errno=%d (%s)",
	     EKA_EXCH_DECODE(gr->exch),gr->id,dev->genIfName,strlen(dev->genIfName)+1,errno,strerror(errno));
  } else {
    EKA_LOG("%s:%u: Mold UDP sock is binded to %s (len=%d)",
	     EKA_EXCH_DECODE(gr->exch),gr->id,dev->genIfName,strlen(dev->genIfName)+1);
  }

  static const int TimeOut = 1; // seconds
  struct timeval tv = {
    .tv_sec = TimeOut
  }; 
  setsockopt(udpSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  mold_hdr mold_request = {};
  memcpy(&mold_request.session_id,(uint8_t*)gr->session_id,10);
  uint64_t cnt2ask = end - start + 1;
  uint64_t seq2ask = start;

  struct sockaddr_in mold_recovery_addr = {};
  mold_recovery_addr.sin_family      = AF_INET; // IPv4 
  mold_recovery_addr.sin_addr.s_addr = gr->recovery_ip;
  mold_recovery_addr.sin_port        = gr->recovery_port; 

  gr->recovery_active = true;
  uint64_t sequence = 0;
  while (gr->recovery_active && cnt2ask > 0) {
    char buf[1500] = {};
    mold_request.sequence = be64toh(seq2ask);
    // 200 is just a number: a Mold pkt always contains less than 200 messages
    uint16_t cnt2ask4mold = cnt2ask > 200 ? 200 : cnt2ask & 0xFFFF; 
    mold_request.message_cnt = be16toh(cnt2ask4mold);
    EKA_TRACE("%s:%u: Sending Mold request to: %s:%u, session_id = %s, seq=%ju (0x%jx), cnt=%u",
    	      EKA_EXCH_DECODE(gr->exch),gr->id,
    	      EKA_IP2STR(*(uint32_t*)&mold_recovery_addr.sin_addr),be16toh(mold_recovery_addr.sin_port),
    	      mold_request.session_id + '\0',
    	      be64toh(mold_request.sequence),be64toh(mold_request.sequence),
    	      be16toh(mold_request.message_cnt)
    	      );
    static const int MaxMoldReTry = 5;
    static const int MaxSendReTry = 2;
    static const int MaxReadReTry = 10;
    bool moldRcvSuccess = false;
    /* ----------------------------------------------------- */
    for (auto i = 0; i < MaxMoldReTry; i++) {
      /* ----------------------------------------------------- */
      for (auto j = 0; j < MaxSendReTry; j++) {
	int r = sendto(udpSock,
		       &mold_request,
		       sizeof(mold_request),
		       0,
		       (const sockaddr*)&mold_recovery_addr,
		       sizeof(sockaddr));
	if (r == sizeof(mold_request)) {
	  EKA_TRACE("%s:%u Mold request sent: %s:%u r=%d",
		    EKA_EXCH_DECODE(gr->exch),gr->id,
		    EKA_IP2STR(mold_recovery_addr.sin_addr.s_addr),
		    be16toh   (mold_recovery_addr.sin_port),
		    r);
	  break;
	} else {      
	  dev->lastErrno = errno;
	  EKA_WARN("%s:%u Mold request send failed to: %s:%u r=%d: %s",
		   EKA_EXCH_DECODE(gr->exch),gr->id,
		   EKA_IP2STR(mold_recovery_addr.sin_addr.s_addr),
		   be16toh   (mold_recovery_addr.sin_port),
		   r,
		   strerror(dev->lastErrno));
	  sleep(0);
	  continue;
	}
      }
      /* ----------------------------------------------------- */
      for (auto k = 0; k < MaxReadReTry; k++) {
	int r = recvfrom(udpSock,buf,sizeof(buf),MSG_WAITALL,NULL,NULL);
	if (r > 0) {
	  moldRcvSuccess = true;
	  break;
	}
	if (r == 0) {
	  sleep(0);
	  continue;
	}
	// r < 0
	if (errno == EAGAIN) {
	  EKA_WARN("%s:%u Mold request receive failed from: %s:%u after TimeOut %d, r=%d: %s",
		   EKA_EXCH_DECODE(gr->exch),gr->id,
		   EKA_IP2STR(mold_recovery_addr.sin_addr.s_addr),
		   be16toh   (mold_recovery_addr.sin_port),
		   TimeOut,
		   r,
		   strerror(errno));
	  continue;
	}
	
	dev->lastErrno = errno;

	EKA_WARN("%s:%u Mold request receive failed from: %s:%u r=%d: %s",
		 EKA_EXCH_DECODE(gr->exch),gr->id,
		 EKA_IP2STR(mold_recovery_addr.sin_addr.s_addr),
		 be16toh   (mold_recovery_addr.sin_port),
		 r,
		 strerror(dev->lastErrno));
	moldRcvSuccess = false;
	break;
      }
      /* ----------------------------------------------------- */
    }
    if (! moldRcvSuccess) {
      gr->sendRetransmitSocketError(pEfhRunCtx);
      on_error("%s:%u Mold request failed after %d attempts: %s",
	       EKA_EXCH_DECODE(gr->exch),gr->id,
	       MaxMoldReTry,strerror(dev->lastErrno));
    }

    //-----------------------------------------------
    uint indx = sizeof(mold_hdr); // pointer to the start of 1st message in the packet
    uint16_t message_cnt = EKA_MOLD_MSG_CNT(buf);
    sequence = EKA_MOLD_SEQUENCE(buf);
    for (uint msg=0; msg < message_cnt; msg++) {
      uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(buf[indx]));
      //-----------------------------------------------------------------
      if (sequence >= start) gr->parseMsg(pEfhRunCtx,(unsigned char*)&buf[indx+2],sequence++,EkaFhMode::MCAST);
      //-----------------------------------------------------------------
      indx += msg_len + sizeof(msg_len); 
    }
    //-----------------------------------------------
    cnt2ask -= message_cnt;
    seq2ask += message_cnt;
  } // while loop
  gr->seq_after_snapshot = sequence;
  EKA_LOG("%s:%u: Mold recovery finished: next expected_sequence = %ju",
	  EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
  gr->gapClosed = true;

  close(udpSock);

  return NULL;
}

void* getMolUdpPlxOrdData(void* attr) {
#ifdef FH_LAB
  pthread_detach(pthread_self());
  return NULL;
#endif

  pthread_detach(pthread_self());

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhNasdaqGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");

  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
  uint64_t     start      = params->startSeq;
  uint64_t     end        = params->endSeq;
  delete params;

  EkaDev* dev = gr->dev;
  assert (dev != NULL);
  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",
					EKA_EXCH_DECODE(gr->exch),gr->id);
  if ((gr->recovery_sock = socket(AF_INET,SOCK_DGRAM,0)) == -1) 
    on_error("Failed to open socket for %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id);

  PhlxMoldHdr mold_request = {};
  memcpy(&mold_request.session_id,(uint8_t*)gr->session_id,10);
  uint64_t cnt2ask = end - start + 1;
  uint64_t seq2ask = start;

  struct sockaddr_in mold_recovery_addr = {};
  mold_recovery_addr.sin_family      = AF_INET;
  mold_recovery_addr.sin_addr.s_addr = gr->recovery_ip;
  mold_recovery_addr.sin_port = gr->recovery_port; 

  gr->recovery_active = true;
  uint64_t sequence = 0;
  while (gr->recovery_active && cnt2ask > 0) {
    char buf[1500] = {};

    mold_request.sequence = seq2ask;
    // 200 is just a number: a Mold pkt always contains less than 200 messages
    uint16_t cnt2ask4mold = cnt2ask > 200 ? 200 : cnt2ask & 0xFFFF; 
    mold_request.message_cnt = cnt2ask4mold;
    EKA_TRACE("%s:%u: Sending Mold request to: %s:%u, session_id = %s, seq=%ju, cnt=%u",
    	      EKA_EXCH_DECODE(gr->exch),gr->id,
    	      EKA_IP2STR(*(uint32_t*)&mold_recovery_addr.sin_addr),be16toh(mold_recovery_addr.sin_port),
    	      mold_request.session_id + '\0',
    	      mold_request.sequence,
    	      mold_request.message_cnt
    	      );

    int attempt = 0;
    if (sendUdpPkt (dev, gr, gr->recovery_sock, &mold_request, sizeof(mold_request), (const sockaddr*) &mold_recovery_addr, "Mold request") <= 0) {
      dev->lastErrno = errno;
      gr->sendRetransmitSocketError(pEfhRunCtx);
      return NULL;
    }
    while (1) {
      int r = recvUdpPkt (dev, gr, gr->recovery_sock, buf,           sizeof(buf),          (sockaddr*)       &mold_recovery_addr, "Mold response");
      if (r > 0) break;
      if (attempt++ == gr->MoldLocalRetryAttempts) {
	EKA_WARN("Mold UDP socket is not responding after %d attempts",gr->MoldLocalRetryAttempts);
	gr->sendRetransmitSocketError(pEfhRunCtx);
      }
      sleep(0);
    }

    //-----------------------------------------------
    uint indx = sizeof(PhlxMoldHdr); // pointer to the start of 1st message in the packet
    uint16_t message_cnt = EKA_PHLX_MOLD_MSG_CNT(buf);
    sequence = EKA_PHLX_MOLD_SEQUENCE(buf);
    for (uint msg=0; msg < message_cnt; msg++) {
      uint16_t msg_len = (uint16_t) *(uint16_t*)&(buf[indx]);
      //-----------------------------------------------------------------
      if (sequence >= start) gr->parseMsg(pEfhRunCtx,(unsigned char*)&buf[indx+2],sequence++,EkaFhMode::MCAST);
      //-----------------------------------------------------------------
      indx += msg_len + sizeof(msg_len); 
    }
    //-----------------------------------------------
    cnt2ask -= message_cnt;
    seq2ask += message_cnt;
  } // while loop
  gr->seq_after_snapshot = sequence;
  EKA_LOG("%s:%u: Mold recovery finished: next expected_sequence = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
  gr->gapClosed = true;

  close(gr->recovery_sock);
  gr->recovery_sock = -1;
  return NULL;
}
