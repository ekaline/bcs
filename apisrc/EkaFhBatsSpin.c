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

#include "EkaFhParserCommon.h"
#include "EkaFhBatsParser.h"
#include "EkaFhBatsGr.h"
#include "EkaFhThreadAttr.h"

using namespace Bats;

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
static EfhExchangeErrorCode getGapResponse(EkaDev*        dev,
					   int            sock,
					   uint8_t        batsUnit,
					   EkaSource      exch,
					   EkaLSI         gr,
					   volatile bool* recovery_active);
/* ##################################################################### */
static int sendHearBeat(EkaDev* dev,int sock, uint8_t batsUnit) {
  sequenced_unit_header heartbeat = {
    .length   = sizeof(sequenced_unit_header),
    .count    = 0,
    .unit     = batsUnit,
    .sequence = 0
  };
  return send(sock,&heartbeat,sizeof(heartbeat), MSG_NOSIGNAL);
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
  LoginRequest loginMsg = {};
  memset(&loginMsg,' ',sizeof(loginMsg));
  loginMsg.hdr.length   = sizeof(loginMsg);
  loginMsg.hdr.count    = 1;
  loginMsg.hdr.unit     = batsUnit;
  loginMsg.hdr.sequence = 0;

  loginMsg.length       = sizeof(loginMsg) - sizeof(loginMsg.hdr);
  loginMsg.type         = (uint8_t)MsgId::LOGIN_REQUEST;

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
  if(send(sock,&loginMsg,sizeof(loginMsg), MSG_NOSIGNAL) < 0) {
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

static EfhExchangeErrorCode getLoginResponse(EkaDev*     dev,
			     int         sock,
			     const char* loginType,
			     EkaSource   exch,
			     EkaLSI      id) {
  LoginResponse login_response ={};
  if (recv(sock,&login_response,sizeof(login_response),MSG_WAITALL) <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: %s connection reset by peer after Login "
	     "(failed to receive Login Response): %s",
	     EKA_EXCH_DECODE(exch),id,loginType,
	     strerror(dev->lastErrno));
    return EfhExchangeErrorCode::kRequestNotServed;
  }
  if (login_response.type != MsgId::LOGIN_RESPONSE) 
    on_error ("%s:%u: Unexpected %s Login response type 0x%02x",
	      EKA_EXCH_DECODE(exch),id,loginType,login_response.type);

  switch (login_response.status) {
  case 'N' :
    EKA_WARN("%s:%u: %s rejected login (\'N\') "
	     "Not authorized (Invalid Username/Password)",
	     EKA_EXCH_DECODE(exch),id,loginType);
    return EfhExchangeErrorCode::kInvalidUserPasswd;
  case 'B' :
    EKA_WARN("%s:%u: %s rejected login (\'B\') Session in use",
	     EKA_EXCH_DECODE(exch),id,loginType);
    return EfhExchangeErrorCode::kSessionInUse;
  case 'S' :
    EKA_WARN("%s:%u: %s rejected login (\'S\') Invalid Session",
	     EKA_EXCH_DECODE(exch),id,loginType);
    return EfhExchangeErrorCode::kInvalidSession;
  case 'A' :
    if (login_response.hdr.count != 1) 
      EKA_WARN("More than 1 message (%u) come with the Login Response",
	       login_response.hdr.count);
    EKA_LOG("%s:%u %s Login Accepted",EKA_EXCH_DECODE(exch),id,loginType);
    return EfhExchangeErrorCode::kNoError;
  default :
    on_error("%s:%u: Unknown %s Login response status \'%c\'",
	     EKA_EXCH_DECODE(exch),id,loginType,login_response.status);
  }
  return EfhExchangeErrorCode::kNoError;  
}
/* ##################################################################### */
static bool getSpinImageSeq(EkaFhBatsGr* gr, int sock, int64_t* imageSequence) {
  EkaDev* dev = gr->dev;

  static const int MaxLocalTrials = 4;
  for (auto localTrial = 0; localTrial < MaxLocalTrials && gr->snapshot_active; localTrial++) {
    sequenced_unit_header hdr = {};
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
      auto msgHdr {reinterpret_cast<const DummyHeader*>(ptr)};
      if ((MsgId)msgHdr->type == MsgId::SPIN_IMAGE_AVAILABLE) {
	EKA_LOG("%s:%u Spin Image Available at Sequence = %u",
		EKA_EXCH_DECODE(gr->exch),gr->id,((const SpinImageAvailable*)ptr)->sequence);
	*imageSequence = ((const SpinImageAvailable*)ptr)->sequence;
	return true;
      }
      ptr += msgHdr->length;
      //-----------------------------------------------------------------
    }
  }
  gr->lastExchErr = EfhExchangeErrorCode::kRequestNotServed;
  EKA_WARN("%s:%u Spin Image Available not received after %d trials",
	   EKA_EXCH_DECODE(gr->exch),gr->id,MaxLocalTrials);

  return false;  

}
/* ##################################################################### */
static EfhExchangeErrorCode getSpinResponse(EkaDev*   dev, 
					    int       sock, 
					    EkaFhMode op, 
					    EkaSource exch,
					    EkaLSI    id,
					    volatile bool* snapshot_active) {

  static const int MaxLocalTrials = 4;
  for (auto localTrial = 0; localTrial < MaxLocalTrials && *snapshot_active;
       localTrial++) {
    sequenced_unit_header hdr = {};
    int size = recv(sock,&hdr,sizeof(hdr),MSG_WAITALL);
    if (size < (int) sizeof(hdr)) {
      dev->lastErrno = errno;
      EKA_LOG("%s:%u: Spin Server connection reset by peer "
	      "(failed to receive Unit Header): %s",
	      EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
      return EfhExchangeErrorCode::kRequestNotServed;
    }
    int payloadSize = hdr.length - sizeof(hdr);
    uint8_t buf[1536] = {};
    size = recv(sock,buf,payloadSize,MSG_WAITALL);
    if (size != payloadSize) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: Spin Server connection reset by peer "
	       "(received %u bytes < expected payload size of %d bytes): %s",
	       EKA_EXCH_DECODE(exch),id,size,payloadSize,strerror(dev->lastErrno));
      return EfhExchangeErrorCode::kRequestNotServed;
    }
    uint8_t* ptr = &buf[0];
    for (uint i = 0; *snapshot_active && i < hdr.count; i ++) {
      if (size <= 0) on_error("%s:%u: remaining buff size = %d",EKA_EXCH_DECODE(exch),id,size);
      auto msgHdr {reinterpret_cast<const DummyHeader*>(ptr)};

      if ((MsgId)msgHdr->type == MsgId::SNAPSHOT_RESPONSE ||
	  (MsgId)msgHdr->type == MsgId::DEFINITIONS_RESPONSE
	  ) {
	switch (((const SpinResponse*)ptr)->status) {
	case 'A' : // Accepted
	  EKA_LOG("%s:%u Spin %s Request Accepted: Count=%u, Sequence=%u",
		  EKA_EXCH_DECODE(exch),id,
		  op == EkaFhMode::DEFINITIONS ? "Definitions" : "Snapshot",
		  ((const SpinResponse*)ptr)->count,((const SpinResponse*)ptr)->sequence);
	  return EfhExchangeErrorCode::kNoError;
	case 'O' : 
	  EKA_WARN("%s:%u: Spin request rejected with (\'O\') - "
		   "Out of Range (Sequence requested is greater than "
		   "Sequence available by the next spin)",
		   EKA_EXCH_DECODE(exch),id);
	  return EfhExchangeErrorCode::kInvalidSequenceRange;
	case 'S' : 
	  EKA_WARN("%s:%u: Spin request rejected with (\'S\') - "
		   "Spin already in progress",
		   EKA_EXCH_DECODE(exch),id);
	  on_error("%s:%u: Spin request rejected with (\'S\') - "
		   "Spin already in progress",
		   EKA_EXCH_DECODE(exch),id);
	  return EfhExchangeErrorCode::kOperationAlreadyInProgress;
	default  : 
	  on_error("%s:%u: Spin request rejected with unknown status |%c|",
		   EKA_EXCH_DECODE(exch),id,((const SpinResponse*)ptr)->status);
	  return EfhExchangeErrorCode::kRequestNotServed;
	}
      }
      ptr += msgHdr->length;
      //-----------------------------------------------------------------
    }
  }

  EKA_WARN("%s:%u Spin Reponse not received after %d trials",
	   EKA_EXCH_DECODE(exch),id,MaxLocalTrials);

  return EfhExchangeErrorCode::kRequestNotServed;  
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

  SpinRequest msg = {};
  msg.hdr.length = sizeof(msg);
  msg.hdr.count = 1;
  msg.hdr.unit = batsUnit;
  msg.hdr.sequence = 0;

  msg.length   = sizeof(msg) - sizeof(msg.hdr);
  msg.type     = op == EkaFhMode::DEFINITIONS ? (uint8_t) MsgId::DEFINITIONS_REQUEST : 
    (uint8_t) MsgId::SNAPSHOT_REQUEST;
  msg.sequence = op == EkaFhMode::DEFINITIONS ? 0 : spinImageSequence;
  EKA_LOG("%s:%u Sending %s Request, msg.type = 0x%02x, msg.sequence = %u",
	  EKA_EXCH_DECODE(exch),
	  id, 
	  EkaFhMode2STR(op),
	  msg.type,
	  msg.sequence
	  );

  if(send(sock,&msg,sizeof(msg), MSG_NOSIGNAL) < 0) {
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
  sequenced_unit_header hdr = {};
  int size = recv(sock,&hdr,sizeof(hdr),MSG_WAITALL);
  if (size < (int) sizeof(hdr)) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Spin Server connection reset by peer "
	     "(failed to receive Unit Header): %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }
  int payloadSize = hdr.length - sizeof(hdr);
  uint8_t buf[1536] = {};

  size = recv(sock,buf,payloadSize,MSG_WAITALL);
  if (size != payloadSize) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: Spin Server connection reset by peer "
	     "(received %u  < expected %d): %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     size,payloadSize,strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }

  uint64_t sequence = hdr.sequence;

  uint8_t* ptr = &buf[0];

  for (uint i = 0; *snapshot_active && i < hdr.count; i ++) {
    if (size <= 0) on_error("%s:%u: remaining buff size = %d",
			    EKA_EXCH_DECODE(gr->exch),gr->id,size);
    auto msgHdr {reinterpret_cast<const DummyHeader*>(ptr)};
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

  static const int TimeOut = 5; // seconds

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
  gr->lastExchErr = getLoginResponse(dev,
				     sock,
				     "Spin",
				     gr->exch,
				     gr->id);
  if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
    goto ITERATION_FAIL;
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
  gr->lastExchErr = getSpinResponse(dev, 
				    sock, 
				    op,
				    gr->exch,
				    gr->id,
				    &gr->snapshot_active);
  if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
    goto ITERATION_FAIL;
  //-----------------------------------------------------------------
  while (gr->snapshot_active) { // Accepted Login
    now = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>
	(now - lastHeartBeatTime).count() > 900) {
      sendHearBeat(dev,sock,gr->batsUnit);
      lastHeartBeatTime = now;
      EKA_TRACE("%s:%u: Heartbeat sent",
		EKA_EXCH_DECODE(gr->exch),gr->id);
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
    EKA_WARN("%s:%u GRP read failed from: %s:%u r=%d, errno = %jd: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(gr->recovery_ip),
	     be16toh   (gr->recovery_port),
	     size,
	     dev->lastErrno,
	     strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }

  auto hdr { reinterpret_cast<const sequenced_unit_header*>(buf)};
  EKA_LOG("%s:%u: UdpPkt accepted: unit=%u, sequence=%u, "
	  "msgCnt=%u, length=%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,hdr->unit,hdr->sequence,
	  hdr->count,hdr->length);
  if (hdr->unit != gr->batsUnit) return EkaFhParseResult::NotEnd;
  size -= sizeof(*hdr);
  uint8_t* ptr = &buf[0] + sizeof(*hdr);
  uint64_t sequence = hdr->sequence;

  for (uint i = 0; *recovery_active && i < hdr->count; i ++) {
    if (size <= 0) on_error("%s:%u: size = %d",
			    EKA_EXCH_DECODE(gr->exch),gr->id,size);

    auto msgHdr {reinterpret_cast<const DummyHeader*>(ptr)};

    if (sequence >= start)
      gr->parseMsg(pEfhRunCtx,ptr,sequence,EkaFhMode::RECOVERY);
    sequence++;

    if (sequence == end) {
      gr->seq_after_snapshot = sequence + 1;
      EKA_LOG("%s:%u: GAP closed by GRP: sequence %ju == end %ju "
	      "seq_after_snapshot = %ju",
	      EKA_EXCH_DECODE(gr->exch),gr->id,sequence,end,
	      gr->seq_after_snapshot);
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

  static const int TimeOut = 10; // seconds

  struct timeval tv = {
    .tv_sec = TimeOut
  }; 
  setsockopt(tcpSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  //-----------------------------------------------------------------
  int udpSock = ekaUdpMcConnect(dev,gr->recovery_ip, gr->recovery_port);
  if (udpSock < 0) on_error("%s:%u: failed to open UDP socket",
			    EKA_EXCH_DECODE(gr->exch),gr->id);
  static const int udpTimeOut = 5; // seconds

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

  uint64_t currStart = start;
  uint64_t currEnd = 0;
  
  for (auto i = 0; i < LocalAttemps; i++) {
    int size = recvfrom(udpSock,buf,sizeof(buf),MSG_WAITALL,NULL,NULL);
    if (size > 0) {
      success = true;
      break;
    }
  }
  if (! success) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u failed to receive GRP MC pkt after %d attempts: %s",
	     EKA_EXCH_DECODE(gr->exch),gr->id,LocalAttemps,
	     strerror(dev->lastErrno));
    gr->lastExchErr = EfhExchangeErrorCode::kConnectionProblem;
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
    gr->lastExchErr = EfhExchangeErrorCode::kConnectionProblem;
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
		  gr->id)) {
    gr->lastExchErr = EfhExchangeErrorCode::kConnectionProblem;
    goto ITERATION_FAIL;
  }
  //-----------------------------------------------------------------
  gr->lastExchErr = getLoginResponse(dev,
				     tcpSock,
				     "GRP",
				     gr->exch,
				     gr->id);
  if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
    goto ITERATION_FAIL;
  //-----------------------------------------------------------------

  while (gr->recovery_active) {
    currEnd = std::min(currStart + gr->GrpReqLimit,end);
    //-----------------------------------------------------------------
  
    if (! sendGapRequest(dev, 
			 tcpSock, 
			 gr->batsUnit,
			 currStart,
			 currEnd,
			 gr->exch,
			 gr->id)) {
      gr->lastExchErr = EfhExchangeErrorCode::kConnectionProblem;
      goto ITERATION_FAIL;
    }
    //-----------------------------------------------------------------
    gr->lastExchErr = getGapResponse(dev, 
				     tcpSock, 
				     gr->batsUnit,
				     gr->exch,
				     gr->id,
				     &gr->recovery_active);
    if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    while (gr->recovery_active) { 
      now = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>
	  (now - lastHeartBeatTime).count() > 4) {
	sendHearBeat(dev,tcpSock,gr->batsUnit);
	lastHeartBeatTime = now;
	EKA_TRACE("%s:%u: Heartbeat sent",
		  EKA_EXCH_DECODE(gr->exch),gr->id);
      }

      parseResult = procGrp(pEfhRunCtx,
			    udpSock,
			    gr,
			    start,
			    end,
			    &gr->recovery_active);

      switch (parseResult) {
      case EkaFhParseResult::End:
	goto NEXT_ITERATION;
      case EkaFhParseResult::NotEnd:
	break;
      case EkaFhParseResult::SocketError:
	goto ITERATION_FAIL;
      default:
	on_error("Unexpected parseResult %d",(int)parseResult);
      }
    }
  NEXT_ITERATION:  
    if (currEnd == end) {
      success = true;
      goto END;
    }
    currStart = currEnd;
  }
  
 ITERATION_FAIL:
  success = false;

 END:
  close(tcpSock);
  close(udpSock);
  return success;

}
/* ##################################################################### */

void* getSpinData(void* attr) {
#ifdef FH_LAB
  pthread_detach(pthread_self());
  return NULL;
#endif
  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhBatsGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");

  gr->recoveryThreadDone = false;

  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
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
  gr->credentialAcquire(EkaCredentialType::kSnapshot,
			gr->auth_user, sizeof(gr->auth_user), &lease);

  //-----------------------------------------------------------------
  gr->snapshot_active = true;
  const int MaxTrials = 4;
  bool success = false;
  //-----------------------------------------------------------------
  for (auto trial = 0; trial < MaxTrials && gr->snapshot_active; trial++) {
    EKA_LOG("%s:%d %s cycle: trial: %d / %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	    trial,MaxTrials);

    success = spinCycle(pEfhRunCtx,op,gr,MaxTrials);
    if (success) break;
    if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
      gr->sendRetransmitExchangeError(pEfhRunCtx);
    if (dev->lastErrno   != 0)
      gr->sendRetransmitSocketError(pEfhRunCtx);
  }
  //-----------------------------------------------------------------
  int rc = gr->credentialRelease(lease);
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
  gr->recoveryThreadDone = true;

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

  const GapRequest gap_request = {
    .hdr = {
      .length   = sizeof(gap_request),
      .count    = 1,
      .unit     = batsUnit,
      .sequence = 0
    },
    .length   = (uint8_t)(sizeof(gap_request) - sizeof(sequenced_unit_header)),
    .type     = (uint8_t) MsgId::GAP_REQUEST,
    .unit     = batsUnit,
    .sequence = (uint32_t)start,
    .count    = (uint16_t) (end - start)
  };

  EKA_LOG("%s:%u Sending GRP Request for unit=%u, start sequence=%u, count=%d",
	  EKA_EXCH_DECODE(exch),id,
	  gap_request.unit,
	  gap_request.sequence,
	  gap_request.count);

  int size = send(sock,&gap_request,sizeof(gap_request), MSG_NOSIGNAL);
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
static EfhExchangeErrorCode getGapResponse(EkaDev*        dev,
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
		     sizeof(sequenced_unit_header),
		     MSG_WAITALL);
    if (bytes <= 0) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: connection reset by peer: %s",
	       EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
      return EfhExchangeErrorCode::kRequestNotServed;
    }
    sequenced_unit_header* hdr = (sequenced_unit_header*)&buf[0];

    bytes = recv(sock,
		 &buf[sizeof(sequenced_unit_header)],
		 hdr->length - sizeof(sequenced_unit_header),
		 MSG_WAITALL);

    if (bytes <= 0) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u: connection reset by peer: %s",
	       EKA_EXCH_DECODE(exch),id,strerror(dev->lastErrno));
      return EfhExchangeErrorCode::kRequestNotServed;
    }

    EKA_LOG("%s:%u: TcpPkt accepted: unit=%u, sequence=%u, "
	    "msgCnt=%u, length=%u",
    	    EKA_EXCH_DECODE(exch),id,hdr->unit,hdr->sequence,
	    hdr->count,hdr->length);

    if (hdr->unit != 0 && hdr->unit != batsUnit) {
      EKA_WARN("%s:%u: hdr->unit %u != batsUnit %u",
	       EKA_EXCH_DECODE(exch),id,hdr->unit, batsUnit);
      return EfhExchangeErrorCode::kRequestNotServed;
    }

    uint8_t* msg = &buf[sizeof(sequenced_unit_header)];

    for (auto i = 0; *recovery_active && i < hdr->count; i ++) {
      uint8_t msgType = ((DummyHeader*)msg)->type;
      uint8_t msgLen  = ((DummyHeader*)msg)->length;

      if (msgType != MsgId::GAP_RESPONSE) {
	EKA_LOG("%s:%u: Ignoring Msg 0x%02x at pkt %d, msg %d",
		EKA_EXCH_DECODE(exch),id,msgType, j,i);
	msg += msgLen;
	continue;
      }
      auto gap_response {reinterpret_cast<const GapResponse*>(msg)};
      if (gap_response->unit != 0 && gap_response->unit != batsUnit) {
	EKA_WARN("%s:%u: msgType = 0x%x, "
		 "gap_response->unit %u != batsUnit %u",
		 EKA_EXCH_DECODE(exch),id,msgType,
		 gap_response->unit, batsUnit);
	continue;
      }

      EKA_LOG("%s:%u Gap Response Accepted: Unit=%u, Seq=%u, "
	      "Count=%u, Status=\'%c\'",
	      EKA_EXCH_DECODE(exch),id,
	      gap_response->unit, gap_response->sequence,
	      gap_response->count,gap_response->status);

      switch (gap_response->status) {
      case 'A':
	EKA_LOG("%s:%u Accepted: Unit=%u, Seq=%u. Cnt=%u",
		EKA_EXCH_DECODE(exch),id,
		gap_response->unit, gap_response->sequence,
		gap_response->count);
	return EfhExchangeErrorCode::kNoError;
      case 'O': 
	EKA_WARN("%s:%u: Out of range (ahead of sequence or too far behind)",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kInvalidSequenceRange;
      case 'D': 
	EKA_WARN("%s:%u: Daily gap request allocation exhausted",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kServiceLimitExhausted;
      case 'M': 
	EKA_WARN("%s:%u: Minute gap request allocation exhausted",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kServiceLimitExhausted;
      case 'S': 
	EKA_WARN("%s:%u: Second gap request allocation exhausted",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kServiceLimitExhausted;
      case 'C': 
	EKA_WARN("%s:%u: Count request limit for one gap request exceeded",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kCountLimitExceeded;
      case 'I': 
	EKA_WARN("%s:%u: Invalid Unit specified in request",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kInvalidFieldFormat;
      case 'U': 
	EKA_WARN("%s:%u: Unit is currently unavailable",
		 EKA_EXCH_DECODE(exch),id);
	return EfhExchangeErrorCode::kServiceCurrentlyUnavailable;
      default:
	on_error("%s:%u: Unexpected Gap response status: \'%c\'",
		 EKA_EXCH_DECODE(exch),id,gap_response->status);
      }
      EKA_WARN("%s:%u: Gap response not received after processing %d packets",
	       EKA_EXCH_DECODE(exch),id,Packets2Try);
      return EfhExchangeErrorCode::kRequestNotServed;
    }
  }

  return EfhExchangeErrorCode::kRequestNotServed;
}

/* ##################################################################### */
void* getGrpRetransmitData(void* attr) {
  pthread_detach(pthread_self());
#ifdef FH_LAB
  return NULL;
#endif

  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhBatsGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");
  gr->recoveryThreadDone = false;
  
  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
  uint64_t     start      = params->startSeq;
  uint64_t     end        = params->endSeq;
  delete params;

  if (gr == NULL) on_error("gr == NULL");
  EkaDev* dev = gr->dev;
  if (dev == NULL) on_error("dev == NULL");

  if (! gr->grpSet)
    on_error("%s:%u: GRP credentials not set",
	     EKA_EXCH_DECODE(gr->exch),gr->id);

  if (end - start > 65000) 
    on_error("%s:%u: Gap %ju is too high (> 65000), "
	     "start = %ju, end = %ju",
	     EKA_EXCH_DECODE(gr->exch),gr->id, end - start,
	     start, end);

  uint16_t cnt = (uint16_t)(end - start);
  EKA_LOG("%s:%u: GRP recovery for %ju..%ju, cnt = %u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,start,end,cnt);

  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  gr->credentialAcquire(EkaCredentialType::kRecovery, gr->grpUser,
			sizeof(gr->grpUser), &lease);

  //-----------------------------------------------------------------
  gr->recovery_active = true;
  const int MaxTrials = 1;
  bool success = false;
  //-----------------------------------------------------------------
  for (auto trial = 0; trial < MaxTrials && gr->recovery_active; trial++) {
    EKA_LOG("%s:%d GRP cycle: trial: %d / %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,trial,MaxTrials);

    success = grpCycle(pEfhRunCtx, gr, start, end);
    if (success) break;
    if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
      gr->sendRetransmitExchangeError(pEfhRunCtx,true);
    if (dev->lastErrno   != 0)
      gr->sendRetransmitSocketError(pEfhRunCtx,true);
  }
  //-----------------------------------------------------------------
  int rc = gr->credentialRelease(lease);
  if (rc != 0) on_error("%s:%u Failed to credRelease",
			EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------
  gr->recovery_active = false;
  gr->seq_after_snapshot = end;

  if (success) {
    EKA_LOG("%s:%u: GRP closed: start=%ju, end=%ju, cnt=%d, "
	    "seq_after_snapshot=%ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,start,end,cnt,
	    gr->seq_after_snapshot);
  } else {
    gr->lastExchErr = EfhExchangeErrorCode::kConnectionProblem;
    EKA_WARN("%s:%u GRP Failed after %d trials",
	     EKA_EXCH_DECODE(gr->exch),gr->id,MaxTrials);
  }
  gr->gapClosed = true;
  gr->recoveryThreadDone = true;

  return NULL;
}


