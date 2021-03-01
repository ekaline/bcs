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
#include <string>

#include "EkaFhBatsParser.h"
#include "EkaFhBatsGr.h"
#include "EkaFhThreadAttr.h"

int ekaTcpConnect(uint32_t ip, uint16_t port);
//int ekaUdpConnect(EkaDev* dev, uint32_t ip, uint16_t port);
int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);
static bool sendGapRequest(EkaDev*   dev,
			   int       sock,
			   uint8_t   batsUnit,
			   uint64_t  start,
			   uint64_t  end,
			   EkaSource exch,
			   EkaLSI    id
			   );
static bool getGapResponse(EkaDev*        dev,
			   int            sock,
			   uint8_t        batsUnit,
			   EkaSource      exch,
			   EkaLSI         gr,
			   volatile bool* recovery_active);
/* ##################################################################### */
static int sendHearBeat(EkaDev* dev,int sock, uint8_t batsUnit) {
  batspitch_sequenced_unit_header heartbeat = {
    .length   = sizeof(batspitch_sequenced_unit_header),
    .count    = 0,
    .unit     = batsUnit,
    .sequence = 0
  };
  return send(sock,&heartbeat,sizeof(heartbeat), 0);
}

/* ##################################################################### */

static bool sendLogin (EkaDev*     dev,
		       int         sock,
		       const char* loginType,
		       uint8_t     batsUnit,
		       const char* userName,
		       const char* passwd,
		       const char* sessionSubID,
		       EkaSource   exch,
		       EkaLSI      id
		       ) {
  batspitch_login_request loginMsg = {};
  memset(&loginMsg,' ',sizeof(loginMsg));
  loginMsg.hdr.length   = sizeof(loginMsg);
  loginMsg.hdr.count    = 1;
  loginMsg.hdr.unit     = batsUnit;
  loginMsg.hdr.sequence = 0;

  loginMsg.length       = sizeof(loginMsg) - sizeof(loginMsg.hdr);
  loginMsg.type         = (uint8_t)EKA_BATS_PITCH_MSG::LOGIN_REQUEST;

  memcpy(loginMsg.username,       userName,     sizeof(loginMsg.username));
  memcpy(loginMsg.password,       passwd,       sizeof(loginMsg.password));
  memcpy(loginMsg.session_sub_id, sessionSubID, sizeof(loginMsg.session_sub_id));

  EKA_LOG("%s:%u: %s LoginMsg: unit=%u, session_sub_id[4]=|%s|, user[4]=|%s|, passwd[10]=|%s|",
	  EKA_EXCH_DECODE(exch),id,
	  loginType,
	  loginMsg.hdr.unit,
	  std::string((const char*)&loginMsg.session_sub_id,sizeof(loginMsg.session_sub_id)).c_str(),
	  std::string((const char*)&loginMsg.username,      sizeof(loginMsg.username)).c_str(),
	  std::string((const char*)&loginMsg.password,      sizeof(loginMsg.password)).c_str()
	  );
  if(send(sock,&loginMsg,sizeof(loginMsg), 0) < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: %s Login send failed: %s",
	     EKA_EXCH_DECODE(exch),id,loginType,strerror(dev->lastErrno));
    return false;
  }

  EKA_LOG("%s:%u %s Login sent",EKA_EXCH_DECODE(exch),id,loginType);
  //  hexDump("Spin Login Message sent",&loginMsg,sizeof(loginMsg));
  return true;
}
/* ##################################################################### */

static bool getLoginResponse(EkaDev*     dev,
			     int         sock,
			     const char* loginType,
			     EkaSource   exch,
			     EkaLSI      id) {
  batspitch_login_response login_response ={};
  if (recv(sock,&login_response,sizeof(login_response),MSG_WAITALL) <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: %s connection reset by peer after Login (failed to receive Login Response): %s",
	     EKA_EXCH_DECODE(exch),id,loginType,strerror(dev->lastErrno));
    return false;
  }
  if (login_response.type != EKA_BATS_PITCH_MSG::LOGIN_RESPONSE) 
    on_error ("%s:%u: Unexpected %s Login response type 0x%02x",
	      EKA_EXCH_DECODE(exch),id,loginType,login_response.type);

  switch (login_response.status) {
  case 'N' :
    dev->lastExchErr = EfhExchangeErrorCode::kInvalidUserPasswd;
    EKA_WARN("%s:%u: %s rejected login (\'N\') Not authorized (Invalid Username/Password)",
	     EKA_EXCH_DECODE(exch),id,loginType);
    return false;
  case 'B' :
    dev->lastExchErr = EfhExchangeErrorCode::kSessionInUse;
    EKA_WARN("%s:%u: %s rejected login (\'B\') Session in use",
	     EKA_EXCH_DECODE(exch),id,loginType);
    return false;
  case 'S' :
    dev->lastExchErr = EfhExchangeErrorCode::kInvalidSession;
    EKA_WARN("%s:%u: %s rejected login (\'S\') Invalid Session",
	     EKA_EXCH_DECODE(exch),id,loginType);
    return false;
  case 'A' :
    if (login_response.hdr.count != 1) 
      EKA_WARN("More than 1 message (%u) come with the Login Response",login_response.hdr.count);
    EKA_LOG("%s:%u %s Login Accepted",EKA_EXCH_DECODE(exch),id,loginType);
    return true;
  default :
    on_error("%s:%u: Unknown %s Login response status \'%c\'",
	     EKA_EXCH_DECODE(exch),id,loginType,login_response.status);
  }
  return false;  
}
/* ##################################################################### */
static bool getSpinImageSeq(EkaFhBatsGr* gr, int sock, int64_t* imageSequence) {
  EkaDev* dev = gr->dev;

  static const int MaxLocalTrials = 4;
  for (auto localTrial = 0; localTrial < MaxLocalTrials && gr->snapshot_active; localTrial++) {
    batspitch_sequenced_unit_header hdr = {};
    const int r = recv(sock,&hdr,sizeof(hdr),MSG_WAITALL);
    if (r < (int) sizeof(hdr)) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: Spin Server connection reset by peer (failed to receive Unit Header) %s",
	       EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
      return false; 
    }
    int payloadSize = hdr.length - sizeof(hdr);
    uint8_t buf[1536] = {};
    int size = recv(sock,buf,payloadSize,MSG_WAITALL);
    if (size != payloadSize) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: Spin Server connection reset by peer (received %u < expected %d): %s",
	       EKA_EXCH_DECODE(gr->exch),gr->id,size,payloadSize,strerror(dev->lastErrno));
      return false; 
    }
    uint8_t* ptr = &buf[0];
    for (uint i = 0; gr->snapshot_active && i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",
			      EKA_EXCH_DECODE(gr->exch),gr->id,size);
      const batspitch_dummy_header* msgHdr = (const batspitch_dummy_header*) ptr;
      if ((EKA_BATS_PITCH_MSG)msgHdr->type == EKA_BATS_PITCH_MSG::SPIN_IMAGE_AVAILABLE) {
	EKA_LOG("%s:%u Spin Image Available at Sequence = %u",
		EKA_EXCH_DECODE(gr->exch),gr->id,((batspitch_spin_image_available*)ptr)->sequence);
	*imageSequence = ((batspitch_spin_image_available*)ptr)->sequence;
	return true;
      }
      ptr += msgHdr->length;
      //-----------------------------------------------------------------
    }
  }
  dev->lastExchErr = EfhExchangeErrorCode::kRequestNotServed;
  EKA_WARN("%s:%u Spin Image Available not received after %d trials",
	   EKA_EXCH_DECODE(gr->exch),gr->id,MaxLocalTrials);

  return false;  

}
/* ##################################################################### */
static bool getSpinResponse(EkaDev*   dev, 
			    int       sock, 
			    EkaFhMode op, 
			    EkaSource exch,
			    EkaLSI    id,
			    volatile bool* snapshot_active) {

  static const int MaxLocalTrials = 4;
  for (auto localTrial = 0; localTrial < MaxLocalTrials && *snapshot_active; localTrial++) {
    batspitch_sequenced_unit_header hdr = {};
    int size = recv(sock,&hdr,sizeof(hdr),MSG_WAITALL);
    if (size < (int) sizeof(hdr)) {
      dev->lastErrno = errno;
      EKA_LOG("%s:%u: Spin Server connection reset by peer (failed to receive Unit Header): %s",
	      EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
      return false;
    }
    int payloadSize = hdr.length - sizeof(hdr);
    uint8_t buf[1536] = {};
    size = recv(sock,buf,payloadSize,MSG_WAITALL);
    if (size != payloadSize) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: Spin Server connection reset by peer (received %u bytes < expected payload size of %d bytes): %s",
	       EKA_EXCH_DECODE(exch),id,size,payloadSize,strerror(dev->lastErrno));
      return false;
    }
    uint8_t* ptr = &buf[0];
    for (uint i = 0; *snapshot_active && i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",EKA_EXCH_DECODE(exch),id,size);
      const batspitch_dummy_header* msg_hdr = (const batspitch_dummy_header*) ptr;
      if ((EKA_BATS_PITCH_MSG)msg_hdr->type == EKA_BATS_PITCH_MSG::SNAPSHOT_RESPONSE ||
	  (EKA_BATS_PITCH_MSG)msg_hdr->type == EKA_BATS_PITCH_MSG::DEFINITIONS_RESPONSE
	  ) {
	switch (((batspitch_spin_response*)ptr)->status) {
	case 'A' : // Accepted
	  EKA_LOG("%s:%u Spin %s Request Accepted: Count=%u, Sequence=%u",
		  EKA_EXCH_DECODE(exch),id,
		  op == EkaFhMode::DEFINITIONS ? "Definitions" : "Snapshot",
		  ((batspitch_spin_response*)ptr)->count,((batspitch_spin_response*)ptr)->sequence);
	  return true;
	case 'O' : 
	  dev->lastExchErr = EfhExchangeErrorCode::kInvalidSequenceRange;
	  EKA_WARN("%s:%u: Spin request rejected with (\'O\') - Out of range",
		   EKA_EXCH_DECODE(exch),id);
	  return false;
	case 'S' : 
	  dev->lastExchErr = EfhExchangeErrorCode::kOperationAlreadyInProgress;
	  EKA_WARN("%s:%u: Spin request rejected with (\'S\') - Spin already in progress",
		   EKA_EXCH_DECODE(exch),id);
	  return false;
	default  : 
	  on_error("%s:%u: Spin request rejected with unknown status |%c|",
		   EKA_EXCH_DECODE(exch),id,((batspitch_spin_response*)ptr)->status);
	  return false;
	}
      }
      ptr += msg_hdr->length;
      //-----------------------------------------------------------------
    }
  }
  dev->lastExchErr = EfhExchangeErrorCode::kRequestNotServed;
  EKA_WARN("%s:%u Spin Reponse not received after %d trials",
	   EKA_EXCH_DECODE(exch),id,MaxLocalTrials);

  return false;  
}
/* ##################################################################### */
static bool sendSpinRequest(EkaDev*   dev, 
			    int       sock, 
			    EkaFhMode op, 
			    uint64_t  spinImageSequence,
			    uint8_t   batsUnit,
			    EkaSource exch,
			    EkaLSI    id
			    ) {
  if (spinImageSequence >= (1ULL << 32)) 
    on_error("%s:%u: spinImageSequence %ju exceeds 32bit",
	     EKA_EXCH_DECODE(exch),id,spinImageSequence);

  batspitch_spin_request msg = {};
  msg.hdr.length = sizeof(msg);
  msg.hdr.count = 1;
  msg.hdr.unit = batsUnit;
  msg.hdr.sequence = 0;

  msg.length   = sizeof(msg) - sizeof(msg.hdr);
  msg.type     = op == EkaFhMode::DEFINITIONS ? (uint8_t) EKA_BATS_PITCH_MSG::DEFINITIONS_REQUEST : 
    (uint8_t) EKA_BATS_PITCH_MSG::SNAPSHOT_REQUEST;
  msg.sequence = op == EkaFhMode::DEFINITIONS ? 0 : spinImageSequence;
  EKA_LOG("%s:%u Sending %s Request, msg.type = 0x%02x, msg.sequence = %u",
	  EKA_EXCH_DECODE(exch),
	  id, 
	  EkaFhMode2STR(op),
	  msg.type,
	  msg.sequence
	  );

  if(send(sock,&msg,sizeof(msg), 0) < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Spin Request send failed: %s",
	     EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
    return false;
  }
  //  hexDump("Spin Request Message sent",&msg,sizeof(msg));
  return true;
}

/* ##################################################################### */
static EkaFhParseResult procSpin(const EfhRunCtx* pEfhRunCtx, 
				 int              sock, 
				 EkaFhBatsGr*     gr,
				 EkaFhMode        op,
				 volatile bool*   snapshot_active) {
  EkaDev* dev = gr->dev;
  batspitch_sequenced_unit_header hdr = {};
  int size = recv(sock,&hdr,sizeof(hdr),MSG_WAITALL);
  if (size < (int) sizeof(hdr)) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Spin Server connection reset by peer (failed to receive Unit Header): %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }
  int payloadSize = hdr.length - sizeof(hdr);
  uint8_t buf[1536] = {};

  size = recv(sock,buf,payloadSize,MSG_WAITALL);
  if (size != payloadSize) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Spin Server connection reset by peer (received %u  < expected %d): %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,size,payloadSize,strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }

  uint64_t sequence = hdr.sequence;

  uint8_t* ptr = &buf[0];

  for (uint i = 0; *snapshot_active && i < hdr.count; i ++) {
    if (size <= 0) on_error("%s:%u: remaining buff size = %d",
			    EKA_EXCH_DECODE(gr->exch),gr->id,size);
    auto msgHdr {reinterpret_cast<const batspitch_dummy_header*>(ptr)};
    size -= msgHdr->length;

    if (gr->parseMsg(pEfhRunCtx,ptr,sequence++,op))
      return EkaFhParseResult::End;
    ptr += msgHdr->length;
    //-----------------------------------------------------------------
  }
  return EkaFhParseResult::NotEnd;
}
/* ##################################################################### */
static bool spinCycle(EfhRunCtx*   pEfhRunCtx, 
		      EkaFhMode    op,
		      EkaFhBatsGr* gr, 
		      const int    MaxTrials) {
  EkaDev* dev = gr->dev;
  int64_t requestedSpinSequence = 0;
  EkaFhParseResult parseResult;

  auto lastHeartBeatTime = std::chrono::high_resolution_clock::now();
  std::chrono::high_resolution_clock::time_point now;

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) on_error("%s:%u: failed to open TCP socket",
			 EKA_EXCH_DECODE(gr->exch),gr->id);

  static const int TimeOut = 1; // seconds
  struct timeval tv = {
    .tv_sec = TimeOut
  }; 
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in remote_addr = {};
  remote_addr.sin_addr.s_addr = gr->snapshot_ip;
  remote_addr.sin_port        = gr->snapshot_port;
  remote_addr.sin_family      = AF_INET;
  EKA_LOG("%s:%u Tcp Connecting to %s:%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  EKA_IP2STR(remote_addr.sin_addr.s_addr),
	  be16toh   (remote_addr.sin_port));

  if (connect(sock,(sockaddr*)&remote_addr,sizeof(sockaddr_in)) != 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u Tcp Connect to %s:%u failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(remote_addr.sin_addr.s_addr),
	     be16toh   (remote_addr.sin_port),
	     strerror(dev->lastErrno));

    goto ITERATION_FAIL;
  }
  //-----------------------------------------------------------------
  if (! sendLogin(dev,
		  sock,
		  "Spin",
		  gr->batsUnit,
		  gr->auth_user,
		  gr->auth_passwd,
		  gr->sessionSubID,
		  gr->exch,
		  gr->id)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (! getLoginResponse(dev,
			 sock,
			 "Spin",
			 gr->exch,
			 gr->id)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (op == EkaFhMode::SNAPSHOT) {
    if (! getSpinImageSeq(gr,
			  sock,
			  &requestedSpinSequence)) goto ITERATION_FAIL;
  }
  //-----------------------------------------------------------------
  if (! sendSpinRequest(dev, 
			sock, 
			op, 
			requestedSpinSequence,
			gr->batsUnit,
			gr->exch,
			gr->id)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (! getSpinResponse(dev, 
			sock, 
			op, 
			gr->exch,
			gr->id,
			&gr->snapshot_active)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  while (gr->snapshot_active) { // Accepted Login
    now = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartBeatTime).count() > 900) {
      sendHearBeat(dev,sock,gr->batsUnit);
      lastHeartBeatTime = now;
      EKA_TRACE("%s:%u: Heartbeat sent",EKA_EXCH_DECODE(gr->exch),gr->id);
    }

    parseResult = procSpin(pEfhRunCtx,
			   sock,
			   gr,
			   op,
			   &gr->snapshot_active);
    switch (parseResult) {
    case EkaFhParseResult::End:
      goto SUCCESS_END;
    case EkaFhParseResult::NotEnd:
      break;
    case EkaFhParseResult::SocketError:
      goto ITERATION_FAIL;
    default:
      on_error("Unexpected parseResult %d",(int)parseResult);
    }
  }

 ITERATION_FAIL:
  close(sock);
  return false;

 SUCCESS_END:
  close(sock);
  return true;
}

/* ##################################################################### */

static EkaFhParseResult procGrp(const EfhRunCtx* pEfhRunCtx, 
				int              sock, 
				EkaFhBatsGr*     gr,
				uint64_t         start,
				uint64_t         end,
				volatile bool*   recovery_active) {

  EkaDev* dev = gr->dev;
  uint8_t     buf[1500]  = {};
  int size = recvfrom(sock, buf, sizeof(buf), MSG_WAITALL, NULL, NULL);

  if (size == 0) return EkaFhParseResult::NotEnd;
  if (size < 0) {
    if (errno == EAGAIN) {
      EKA_WARN("%s:%u GPR UDP read failed from: %s:%u r=%d %s",
	       EKA_EXCH_DECODE(gr->exch),gr->id,
	       EKA_IP2STR(gr->recovery_ip),
	       be16toh   (gr->recovery_port),
	       size,
	       strerror(errno));
      return EkaFhParseResult::NotEnd;
    }
    dev->lastErrno = errno;
    EKA_WARN("%s:%u GRP read failed from: %s:%u r=%d, errno = %d: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(gr->recovery_ip),
	     be16toh   (gr->recovery_port),
	     size,
	     dev->lastErrno,
	     strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }

  auto hdr { reinterpret_cast<const batspitch_sequenced_unit_header*>(buf)};
  EKA_LOG("%s:%u: UdpPkt accepted: unit=%u, sequence=%u, msgCnt=%u, length=%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,hdr->unit,hdr->sequence,hdr->count,hdr->length);
  if (hdr->unit != gr->batsUnit) return EkaFhParseResult::NotEnd;
  size -= sizeof(*hdr);
  uint8_t* ptr = &buf[0] + sizeof(*hdr);
  uint64_t sequence = hdr->sequence;

  for (uint i = 0; *recovery_active && i < hdr->count; i ++) {
    if (size <= 0) on_error("%s:%u: size = %d",
			    EKA_EXCH_DECODE(gr->exch),gr->id,size);

    auto msgHdr {reinterpret_cast<const batspitch_dummy_header*>(ptr)};

    if (sequence >= start) gr->parseMsg(pEfhRunCtx,ptr,sequence,EkaFhMode::RECOVERY);
    sequence++;

    if (sequence == end) {
      EKA_LOG("%s:%u: GAP closed by GRP: sequence %ju == end %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,sequence,end);
      return EkaFhParseResult::End; 
    }

    ptr  += msgHdr->length;
    size -= msgHdr->length;
  }
  return EkaFhParseResult::NotEnd; 
}

/* ##################################################################### */
static bool grpCycle(EfhRunCtx*   pEfhRunCtx, 
		     EkaFhBatsGr* gr, 
		     uint64_t     start,
		     uint64_t     end) {
  EkaDev* dev = gr->dev;
  EkaFhParseResult parseResult;

  //-----------------------------------------------------------------
  auto lastHeartBeatTime = std::chrono::high_resolution_clock::now();
  std::chrono::high_resolution_clock::time_point now;
  //-----------------------------------------------------------------
  int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
  if (tcpSock < 0) on_error("%s:%u: failed to open TCP socket",
			    EKA_EXCH_DECODE(gr->exch),gr->id);

  static const int TimeOut = 1; // seconds
  struct timeval tv = {
    .tv_sec = TimeOut
  }; 
  setsockopt(tcpSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  //-----------------------------------------------------------------
  int udpSock = ekaUdpMcConnect(dev,gr->recovery_ip, gr->recovery_port);
  if (udpSock < 0) on_error("%s:%u: failed to open UDP socket",
			    EKA_EXCH_DECODE(gr->exch),gr->id);
  static const int udpTimeOut = 1; // seconds
  struct timeval udpTv = {
    .tv_sec = udpTimeOut
  }; 
  setsockopt(udpSock, SOL_SOCKET, SO_RCVTIMEO, &udpTv, sizeof(udpTv));
  //-----------------------------------------------------------------
  sockaddr_in remote_addr = {};
  remote_addr.sin_addr.s_addr = gr->grpIp;
  remote_addr.sin_port        = gr->grpPort;
  remote_addr.sin_family      = AF_INET;
  //-----------------------------------------------------------------
  EKA_LOG("%s:%u: getting 1 GRP UDP MC pkt to ensure MC joined",
	  EKA_EXCH_DECODE(gr->exch),gr->id);

  bool success = false;
  uint8_t     buf[1500]  = {};
  static const int LocalAttemps = 4;
  for (auto i = 0; i < LocalAttemps; i++) {
    int size = recvfrom(udpSock, buf, sizeof(buf), MSG_WAITALL, NULL, NULL);
    if (size > 0) {
      success = true;
      break;
    }
  }
  if (! success) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u failed to receive GRP MC pkt after %d attempts: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,LocalAttemps,strerror(dev->lastErrno));
    goto ITERATION_FAIL;
  }
  //-----------------------------------------------------------------
  EKA_LOG("%s:%u Tcp Connecting to %s:%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  EKA_IP2STR(remote_addr.sin_addr.s_addr),
	  be16toh   (remote_addr.sin_port));

  if (connect(tcpSock,(sockaddr*)&remote_addr,sizeof(sockaddr_in)) != 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u Tcp Connect to GRP %s:%u failed: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(remote_addr.sin_addr.s_addr),
	     be16toh   (remote_addr.sin_port),
	     strerror(dev->lastErrno));
    goto ITERATION_FAIL;
  }
  //-----------------------------------------------------------------
  if (! sendLogin(dev,
		  tcpSock,
		  "GRP",
		  gr->batsUnit,
		  gr->grpUser,
		  gr->grpPasswd,
		  gr->grpSessionSubID,
		  gr->exch,
		  gr->id)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (! getLoginResponse(dev,
			 tcpSock,
			 "GRP",
			 gr->exch,
			 gr->id)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (! sendGapRequest(dev, 
		       tcpSock, 
		       gr->batsUnit,
		       start,
		       end,
		       gr->exch,
		       gr->id)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  if (! getGapResponse(dev, 
		       tcpSock, 
		       gr->batsUnit,
		       gr->exch,
		       gr->id,
		       &gr->recovery_active)) goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  while (gr->recovery_active) { 
    now = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartBeatTime).count() > 4) {
      sendHearBeat(dev,tcpSock,gr->batsUnit);
      lastHeartBeatTime = now;
      EKA_TRACE("%s:%u: Heartbeat sent",EKA_EXCH_DECODE(gr->exch),gr->id);
    }

    parseResult = procGrp(pEfhRunCtx,
			  udpSock,
			  gr,
			  start,
			  end,
			  &gr->recovery_active);

    switch (parseResult) {
    case EkaFhParseResult::End:
      goto SUCCESS_END;
    case EkaFhParseResult::NotEnd:
      break;
    case EkaFhParseResult::SocketError:
      goto ITERATION_FAIL;
    default:
      on_error("Unexpected parseResult %d",(int)parseResult);
    }
  }

 ITERATION_FAIL:
  close(tcpSock);
  close(udpSock);
  return false;

 SUCCESS_END:
  close(tcpSock);
  close(udpSock);
  return true;
}
/* ##################################################################### */

void* getSpinData(void* attr) {
#ifdef FH_LAB
  pthread_detach(pthread_self());
  return NULL;
#endif

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
  EkaFhBatsGr* gr         = (EkaFhBatsGr*)params->gr;
  EkaFhMode    op         = params->op;
  delete params;

  if (op != EkaFhMode::DEFINITIONS) pthread_detach(pthread_self());

  EkaDev* dev = gr->dev;

  EKA_LOG("%s:%u: Spin %s at %s:%d",
	  EKA_EXCH_DECODE(gr->exch),gr->id, 
	  EkaFhMode2STR(op),
	  EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));
  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  gr->credentialAcquire(gr->auth_user,sizeof(gr->auth_user),&lease);

  //-----------------------------------------------------------------
  gr->snapshot_active = true;
  const int MaxTrials = 4;
  bool success = false;
  //-----------------------------------------------------------------
  for (auto trial = 0; trial < MaxTrials && gr->snapshot_active; trial++) {
    EKA_LOG("%s:%d %s cycle: trial: %d / %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),trial,MaxTrials);

    success = spinCycle(pEfhRunCtx,op,gr,MaxTrials);
    if (success) break;

    if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
    if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);

  }
  //-----------------------------------------------------------------
  int rc = dev->credRelease(lease, dev->credContext);
  if (rc != 0) on_error("%s:%u Failed to credRelease",
			EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------
  gr->snapshot_active = false;

  if (success) {
    gr->gapClosed = true;
    EKA_LOG("%s:%u: End Of Spin: seq_after_snapshot = %ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
  } else {
    EKA_WARN("%s:%u Spin Failed after %d trials. Exiting...",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
    on_error("%s:%u Spin Failed after %d trials. Exiting...",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
  }

  return NULL;
}

/* ##################################################################### */
static bool sendGapRequest(EkaDev*   dev,
			   int       sock,
			   uint8_t   batsUnit,
			   uint64_t  start,
			   uint64_t  end,
			   EkaSource exch,
			   EkaLSI    id
) {

  const batspitch_gap_request gap_request = {
    .hdr = {
      .length   = sizeof(batspitch_gap_request),
      .count    = 1,
      .unit     = batsUnit,
      .sequence = 0
    },
    .length   = (uint8_t)(sizeof(batspitch_gap_request) - sizeof(batspitch_sequenced_unit_header)),
    .type     = (uint8_t) EKA_BATS_PITCH_MSG::GAP_REQUEST,
    .unit     = batsUnit,
    .sequence = (uint32_t)start,
    .count    = (uint16_t) (end - start)
  };

  EKA_LOG("%s:%u Sending GRP Request for unit=%u, start sequence=%ju, count=%ju",
	  EKA_EXCH_DECODE(exch),id,
	  gap_request.unit,
	  gap_request.sequence,
	  gap_request.count);

  int size = send(sock,&gap_request,sizeof(gap_request), 0);
  if(size <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: GRP Request send failed (sent %d bytes): %s",
	     EKA_EXCH_DECODE(exch),id,size,strerror(dev->lastErrno));
    return false;
  }
  //  hexDump("GRP Request Message sent",&gap_request,sizeof(gap_request));
  return true;
}

/* ##################################################################### */
static bool getGapResponse(EkaDev*        dev,
			   int            sock,
			   uint8_t        batsUnit,
			   EkaSource      exch,
			   EkaLSI         id,
			   volatile bool* recovery_active) {
  static const int Packets2Try = 4;

  for (auto j = 0; *recovery_active && j < Packets2Try; j++) {
    uint8_t buf[1500] = {};

    int bytes = recv(sock,
		     buf,
		     sizeof(batspitch_sequenced_unit_header),
		     MSG_WAITALL);
    if (bytes <= 0) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: connection reset by peer: %s",
	       EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
      return false;
    }
    batspitch_sequenced_unit_header* hdr = (batspitch_sequenced_unit_header*)&buf[0];

    bytes = recv(sock,
		 &buf[sizeof(batspitch_sequenced_unit_header)],
		 hdr->length - sizeof(batspitch_sequenced_unit_header),
		 MSG_WAITALL);

    if (bytes <= 0) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: connection reset by peer: %s",
	       EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
      return false;
    }

    EKA_LOG("%s:%u: TcpPkt accepted: unit=%u, sequence=%u, msgCnt=%u, length=%u",
    	    EKA_EXCH_DECODE(exch),id,hdr->unit,hdr->sequence,hdr->count,hdr->length);

    if (hdr->unit != 0 && hdr->unit != batsUnit) {
      dev->lastExchErr = EfhExchangeErrorCode::kUnexpectedResponse;
      EKA_WARN("%s:%u: hdr->unit %u != batsUnit %u",
	       EKA_EXCH_DECODE(exch),id,hdr->unit, batsUnit);
      return false;
    }

    uint8_t* msg = &buf[sizeof(batspitch_sequenced_unit_header)];

    for (auto i = 0; *recovery_active && i < hdr->count; i ++) {
      uint8_t msgType = ((batspitch_dummy_header*)msg)->type;
      uint8_t msgLen  = ((batspitch_dummy_header*)msg)->length;

      if (msgType != EKA_BATS_PITCH_MSG::GAP_RESPONSE) {
	EKA_LOG("%s:%u: Ignoring Msg 0x%02x at pkt %d, msg %d",
		EKA_EXCH_DECODE(exch),id,msgType, j,i);
	msg += msgLen;
	continue;
      }
      batspitch_gap_response* gap_response = (batspitch_gap_response*)msg;
      if (gap_response->unit != 0 && gap_response->unit != batsUnit) {
	EKA_WARN("%s:%u: msgType = 0x%x, gap_response->unit %u != batsUnit %u",
		 EKA_EXCH_DECODE(exch),id,msgType,gap_response->unit, batsUnit);
	continue;
      }

      EKA_LOG("%s:%u Gap Response Accepted: Unit=%u, Seq=%u, Count=%u, Status=\'%c\'",
	      EKA_EXCH_DECODE(exch),id,
	      gap_response->unit, gap_response->sequence, gap_response->count,gap_response->status);

      switch (gap_response->status) {
      case 'A':
	EKA_LOG("%s:%u Accepted: Unit=%u, Seq=%u. Cnt=%u",
		EKA_EXCH_DECODE(exch),id,
		gap_response->unit, gap_response->sequence, gap_response->count);
	return true;
      case 'O': 
	dev->lastExchErr = EfhExchangeErrorCode::kInvalidSequenceRange;
	EKA_WARN("%s:%u: Out-of-range",EKA_EXCH_DECODE(exch),id);
	break;
      case 'D': 
	dev->lastExchErr = EfhExchangeErrorCode::kServiceLimitExhausted;
	EKA_WARN("%s:%u: Daily gap request allocation exhausted",EKA_EXCH_DECODE(exch),id);
	break;
      case 'M': 
	dev->lastExchErr = EfhExchangeErrorCode::kServiceLimitExhausted;
	EKA_WARN("%s:%u: Minute gap request allocation exhausted",EKA_EXCH_DECODE(exch),id);
	break;
      case 'S': 
	dev->lastExchErr = EfhExchangeErrorCode::kServiceLimitExhausted;
	EKA_WARN("%s:%u: Second gap request allocation exhausted",EKA_EXCH_DECODE(exch),id);
	break;
      case 'C': 
	dev->lastExchErr = EfhExchangeErrorCode::kCountLimitExceeded;
	EKA_WARN("%s:%u: Count request limit for one gap request exceeded",EKA_EXCH_DECODE(exch),id);
	break;
      case 'I': 
	dev->lastExchErr = EfhExchangeErrorCode::kInvalidFieldFormat;
	EKA_WARN("%s:%u: Invalid Unit specified in request",EKA_EXCH_DECODE(exch),id);
	break;
      case 'U': 
	dev->lastExchErr = EfhExchangeErrorCode::kServiceCurrentlyUnavailable;
	EKA_WARN("%s:%u: Unit is currently unavailable",EKA_EXCH_DECODE(exch),id);
	break;
      default:
	on_error("%s:%u: Unexpected Gap response status: \'%c\'",EKA_EXCH_DECODE(exch),id,gap_response->status);
      }
      return false;
    }
  }
  dev->lastExchErr = EfhExchangeErrorCode::kRequestNotServed;
  EKA_WARN("%s:%u: Gap response not received after processing %d packets",
	   EKA_EXCH_DECODE(exch),id,Packets2Try);
  return false;
}

/* ##################################################################### */
void* getGrpRetransmitData(void* attr) {
  pthread_detach(pthread_self());
#ifdef FH_LAB
  return NULL;
#endif

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
  EkaFhBatsGr* gr         = (EkaFhBatsGr*)params->gr;
  uint64_t     start      = params->startSeq;
  uint64_t     end        = params->endSeq;
  delete params;

  if (gr == NULL) on_error("gr == NULL");
  EkaDev* dev = gr->dev;
  if (dev == NULL) on_error("dev == NULL");

  if (! gr->grpSet) on_error("%s:%u: GRP credentials not set",
			 EKA_EXCH_DECODE(gr->exch),gr->id);

  if (end - start > 65000) 
    on_error("%s:%u: Gap %ju is too high (> 65000), start = %ju, end = %ju",
	     EKA_EXCH_DECODE(gr->exch),gr->id, end - start, start, end);

  uint16_t cnt = (uint16_t)(end - start);
  EKA_LOG("%s:%u: GRP recovery for %ju..%ju, cnt = %u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,start,end,cnt);

  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  gr->credentialAcquire(gr->grpUser,sizeof(gr->grpUser),&lease);

  //-----------------------------------------------------------------
  gr->recovery_active = true;
  const int MaxTrials = 4;
  bool success = false;
  //-----------------------------------------------------------------
  for (auto trial = 0; trial < MaxTrials && gr->recovery_active; trial++) {
    EKA_LOG("%s:%d GRP cycle: trial: %d / %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,trial,MaxTrials);

    success = grpCycle(pEfhRunCtx, gr, start, end);
    if (success) break;
    if (dev->lastExchErr != EfhExchangeErrorCode::kNoError) gr->sendRetransmitExchangeError(pEfhRunCtx);
    if (dev->lastErrno   != 0)                              gr->sendRetransmitSocketError(pEfhRunCtx);

  }
  //-----------------------------------------------------------------
  int rc = dev->credRelease(lease, dev->credContext);
  if (rc != 0) on_error("%s:%u Failed to credRelease",
			EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------
  gr->recovery_active = false;
  gr->seq_after_snapshot = end;

  if (success) {
    gr->gapClosed = true;
    EKA_LOG("%s:%u: GRP closed: start=%ju, end=%ju, cnt=%d, seq_after_snapshot=%ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,start,end,cnt,gr->seq_after_snapshot);
  } else {
    EKA_WARN("%s:%u GRP Failed after %d trials. Exiting...",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
    on_error("%s:%u GRP Failed after %d trials. Exiting...",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
  }

  return NULL;
}


