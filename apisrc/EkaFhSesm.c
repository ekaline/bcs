#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <iostream>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "EkaFhMiaxGr.h"
#include "EkaFhMiaxParser.h"

#include "EfhFastBuffer.h"
#include "EkaFhThreadAttr.h"

using namespace Tom;

static bool sendLogin(EkaFhMiaxGr *gr);
static bool getLoginResponse(EkaFhMiaxGr *gr);
static bool sendRetransmitRequest(EkaFhMiaxGr *gr,
                                  uint64_t start,
                                  uint64_t end);
static bool sendRequest(EkaFhMiaxGr *gr, char refreshType);
static bool sendLogOut(EkaFhMiaxGr *gr);

static EkaFhParseResult procRefreshSesmPkt(
    const EfhRunCtx *pEfhRunCtx, int sock, EkaFhMiaxGr *gr,
    EkaFhMode op,
    EfhFastBuffer<TomSeriesUpdate, MaxVanillaDefinitions>
        *pVanillaDefinitions);

static EkaFhParseResult
procRetransmitSesmPkt(const EfhRunCtx *pEfhRunCtx, int sock,
                      EkaFhMiaxGr *gr, EkaFhMode op,
                      uint64_t end);

/* ################################################# */

static int sendHearBeat(int sock) {
  sesm_header heartbeat = {
      .length = 1, // sizeof(struct sesm_header) -
                   // sizeof(heartbeat.length),
      .type = '1'};

  return send(sock, &heartbeat, sizeof(sesm_header),
              MSG_NOSIGNAL);
}
/* ################################################# */
static bool sesmRefreshCycle(EkaDev *dev,
                             EfhRunCtx *pEfhRunCtx,
                             EkaFhMode op, EkaFhMiaxGr *gr,
                             char sesmRequest,
                             uint64_t start, uint64_t end,
                             const int MaxTrials) {
  EkaFhParseResult parseResult;
  EfhFastBuffer<TomSeriesUpdate, MaxVanillaDefinitions>
      *pVanillaDefinitions = NULL;
  for (auto trial = 0;
       trial < MaxTrials && gr->recovery_active; trial++) {
    EKA_LOG("%s:%d %s cycle: trial: %d / %d",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            EkaFhMode2STR(op), trial, MaxTrials);

    if (op == EkaFhMode::DEFINITIONS) {
      pVanillaDefinitions =
          new EfhFastBuffer<TomSeriesUpdate,
                            MaxVanillaDefinitions>;
      if (!pVanillaDefinitions)
        on_error(
            "Failed creating VanillaDefinitions vector");
    }

    gr->recovery_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (gr->recovery_sock < 0)
      on_error("%s:%u: failed to open TCP socket",
               EKA_EXCH_DECODE(gr->exch), gr->id);

    static const int TimeOut = 10; // seconds
    struct timeval tv = {.tv_sec = TimeOut};
    setsockopt(gr->recovery_sock, SOL_SOCKET, SO_RCVTIMEO,
               &tv, sizeof(tv));

    sockaddr_in remote_addr = {};
    remote_addr.sin_addr.s_addr = gr->recovery_ip;
    remote_addr.sin_port = gr->recovery_port;
    remote_addr.sin_family = AF_INET;
    if (connect(gr->recovery_sock, (sockaddr *)&remote_addr,
                sizeof(sockaddr_in)) != 0) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u Tcp Connect to %s:%u failed: %s",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               EKA_IP2STR(remote_addr.sin_addr.s_addr),
               be16toh(remote_addr.sin_port),
               strerror(dev->lastErrno));

      goto ITERATION_FAIL;
    }

    //-----------------------------------------------------------------
    if (!sendLogin(gr))
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    if (!getLoginResponse(gr))
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    if (!sendRequest(gr, sesmRequest))
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    while (gr->recovery_active) {
      parseResult =
          procRefreshSesmPkt(pEfhRunCtx, gr->recovery_sock,
                             gr, op, pVanillaDefinitions);
      switch (parseResult) {
      case EkaFhParseResult::End:
        goto SUCCESS_END;
      case EkaFhParseResult::NotEnd:
        break;
      case EkaFhParseResult::SocketError:
        goto ITERATION_FAIL;
      default:
        on_error("Unexpected parseResult %d",
                 (int)parseResult);
      }
    }

  ITERATION_FAIL:
    sendLogOut(gr);
    close(gr->recovery_sock);
    if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
      gr->sendRetransmitExchangeError(pEfhRunCtx);
    if (dev->lastErrno != 0)
      gr->sendRetransmitSocketError(pEfhRunCtx);
    if (pVanillaDefinitions) {
      delete pVanillaDefinitions;
      pVanillaDefinitions = NULL;
    }
  }
  return false;

SUCCESS_END:
  sendLogOut(gr);
  close(gr->recovery_sock);
  if (pVanillaDefinitions &&
      pVanillaDefinitions->getSize() > 0) {
    EKA_LOG("%s:%u: Sending out buffered %ju Definitions",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            pVanillaDefinitions->getSize());
    for (size_t i = 0; i < pVanillaDefinitions->getSize();
         i++) {
      auto def = pVanillaDefinitions->pop();
      gr->parseMsg(pEfhRunCtx,
                   reinterpret_cast<const uint8_t *>(def),
                   0, op);
    }
    delete pVanillaDefinitions;
  }
  return true;
}

static bool
sesmRetransmitCycle(EkaDev *dev, EfhRunCtx *pEfhRunCtx,
                    EkaFhMode op, EkaFhMiaxGr *gr,
                    char sesmRequest, uint64_t start,
                    uint64_t end, const int MaxTrials) {
  EkaFhParseResult parseResult;
  for (auto trial = 0;
       trial < MaxTrials && gr->recovery_active; trial++) {
    EKA_LOG("%s:%d %s cycle: trial: %d / %d",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            EkaFhMode2STR(op), trial, MaxTrials);

    gr->recovery_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (gr->recovery_sock < 0)
      on_error("%s:%u: failed to open TCP socket",
               EKA_EXCH_DECODE(gr->exch), gr->id);

    static const int TimeOut = 10; // seconds
    struct timeval tv = {.tv_sec = TimeOut};
    setsockopt(gr->recovery_sock, SOL_SOCKET, SO_RCVTIMEO,
               &tv, sizeof(tv));

    sockaddr_in remote_addr = {};
    remote_addr.sin_addr.s_addr = gr->recovery_ip;
    remote_addr.sin_port = gr->recovery_port;
    remote_addr.sin_family = AF_INET;
    if (connect(gr->recovery_sock, (sockaddr *)&remote_addr,
                sizeof(sockaddr_in)) != 0) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u Tcp Connect to %s:%u failed: %s",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               EKA_IP2STR(remote_addr.sin_addr.s_addr),
               be16toh(remote_addr.sin_port),
               strerror(dev->lastErrno));

      goto ITERATION_FAIL;
    }

    //-----------------------------------------------------------------
    if (!sendLogin(gr))
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    if (!getLoginResponse(gr))
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    if (!sendRetransmitRequest(gr, start, end))
      goto ITERATION_FAIL;
    //-----------------------------------------------------------------
    while (gr->recovery_active) {
      parseResult = procRetransmitSesmPkt(
          pEfhRunCtx, gr->recovery_sock, gr, op, end);
      switch (parseResult) {
      case EkaFhParseResult::End:
        goto SUCCESS_END;
      case EkaFhParseResult::NotEnd:
        break;
      case EkaFhParseResult::SocketError:
        goto ITERATION_FAIL;
      default:
        on_error("Unexpected parseResult %d",
                 (int)parseResult);
      }
    }

  ITERATION_FAIL:
    sendLogOut(gr);
    close(gr->recovery_sock);
    if (gr->lastExchErr != EfhExchangeErrorCode::kNoError)
      gr->sendRetransmitExchangeError(pEfhRunCtx);
    if (dev->lastErrno != 0)
      gr->sendRetransmitSocketError(pEfhRunCtx);
  }
  return false;

SUCCESS_END:
  sendLogOut(gr);
  close(gr->recovery_sock);

  return true;
}

/* ###########################################################
 */

static bool sendLogin(EkaFhMiaxGr *gr) {
  EkaDev *dev = gr->dev;

  //--------------- SESM Login Request -------------------
  struct sesm_login_req sesm_login_msg = {};
  sesm_login_msg.header.length =
      sizeof(struct sesm_login_req) -
      sizeof(sesm_login_msg.header.length);
  sesm_login_msg.header.type = 'L';
  memcpy(sesm_login_msg.version, "1.1  ",
         sizeof(sesm_login_msg.version));
  memcpy(sesm_login_msg.username, gr->auth_user,
         sizeof(sesm_login_msg.username));
  memcpy(sesm_login_msg.computer_id, gr->auth_passwd,
         sizeof(sesm_login_msg.computer_id));
  memcpy(sesm_login_msg.app_protocol, "TOM2.2  ",
         sizeof(sesm_login_msg.app_protocol));
  sesm_login_msg.session = 0;
  sesm_login_msg.sequence = 0;
  EKA_LOG(
      "%s:%u sending SESM login message: "
      "sesm_login_msg.version=%s, credentials: |%s|:|%s| "
      "taken from config |%s|:|%s|",
      EKA_EXCH_DECODE(gr->exch), gr->id,
      std::string(sesm_login_msg.version,
                  sizeof(sesm_login_msg.version))
          .c_str(),
      std::string(sesm_login_msg.username,
                  sizeof(sesm_login_msg.username))
          .c_str(),
      std::string(sesm_login_msg.computer_id,
                  sizeof(sesm_login_msg.computer_id))
          .c_str(),
      std::string(gr->auth_user, sizeof(gr->auth_user))
          .c_str(),
      std::string(gr->auth_passwd, sizeof(gr->auth_passwd))
          .c_str());
  int r = send(gr->recovery_sock, &sesm_login_msg,
               sizeof(struct sesm_login_req), MSG_NOSIGNAL);
  if (r < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: SESM Login send failed: %s",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             strerror(dev->lastErrno));
    return false;
  }

  EKA_LOG("%s:%u Sesm Login sent",
          EKA_EXCH_DECODE(gr->exch), gr->id);

  return true;
}
/* ###########################################################
 */

static bool getLoginResponse(EkaFhMiaxGr *gr) {
  EkaDev *dev = gr->dev;

  sesm_login_response sesm_response_msg = {};
  const int r =
      recv(gr->recovery_sock, &sesm_response_msg,
           sizeof(sesm_login_response), MSG_WAITALL);
  if (r < 0) {
    dev->lastErrno = errno;
    EKA_WARN(
        "%s:%u: failed to receive SESM login response: %s",
        EKA_EXCH_DECODE(gr->exch), gr->id,
        strerror(dev->lastErrno));
    return false;
  } else if (r == 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: unexpected request server socket EOF "
             "(expected login SESM response): %s",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             strerror(dev->lastErrno));
    return false;
  }
  if (sesm_response_msg.header.type == 'G') {
    gr->lastExchErr =
        EfhExchangeErrorCode::kRequestNotServed;
    EKA_WARN("SESM sent GoodBye Packet with reason: \'%c\'",
             sesm_response_msg.status);
    return false;
  }
  if (sesm_response_msg.header.type != 'R') {
    gr->lastExchErr = EfhExchangeErrorCode::kUnknown;
    EKA_WARN(
        "sesm_response_msg.header.type \'%c\' != \'R\' ",
        sesm_response_msg.header.type);
    return false;
  }

  switch (sesm_response_msg.status) {
  case 'X':
    gr->lastExchErr =
        EfhExchangeErrorCode::kInvalidUserPasswd;
    EKA_WARN(
        "%s:%u: SESM Login Response: \'X\' -- Rejected: "
        "Invalid Username/Computer ID combination",
        EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  case 'S':
    gr->lastExchErr =
        EfhExchangeErrorCode::kServiceCurrentlyUnavailable;
    EKA_WARN("%s:%u: SESM Login Response: \'S\' -- "
             "Rejected: Requested session is not available",
             EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  case 'N':
    gr->lastExchErr =
        EfhExchangeErrorCode::kInvalidSequenceRange;
    EKA_WARN(
        "%s:%u: SESM Login Response: \'N\' -- Rejected: "
        "Invalid start sequence number requested",
        EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  case 'I':
    gr->lastExchErr = EfhExchangeErrorCode::
        kInvalidSessionProtocolVersion;
    EKA_WARN(
        "%s:%u: SESM Login Response: \'I\' -- Rejected: "
        "Incompatible Session protocol version",
        EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  case 'A':
    gr->lastExchErr = EfhExchangeErrorCode::
        kInvalidApplicationProtocolVersion;
    EKA_WARN(
        "%s:%u: SESM Login Response: \'A\' -- Rejected: "
        "Incompatible Application protocol version",
        EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  case 'L':
    gr->lastExchErr =
        EfhExchangeErrorCode::kOperationAlreadyInProgress;
    EKA_WARN(
        "%s:%u: SESM Login Response: \'L\' -- Rejected: "
        "Request rejected because client already logged in",
        EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  case ' ':
    EKA_LOG("%s:%u: SESM Login accepted. Highest sequence "
            "available=%ju",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            sesm_response_msg.sequence);
    return true;
  default:
    gr->lastExchErr = EfhExchangeErrorCode::kUnknown;
    EKA_WARN("%s:%u: Unknown SESM Login Response:  \'%c\'",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             sesm_response_msg.status);
    return false;
  }
}

/* ###########################################################
 */
static bool sendRequest(EkaFhMiaxGr *gr, char refreshType) {
  EkaDev *dev = gr->dev;

  struct miax_request def_request_msg = {};
  def_request_msg.header.length =
      sizeof(struct miax_request) -
      sizeof(def_request_msg.header.length);
  def_request_msg.header.type = 'U';  // Unsequenced
  def_request_msg.request_type = 'R'; // Refresh
  def_request_msg.refresh_type = refreshType;

  //  hexDump("MIAX definitions request",(char*)
  //  &def_request_msg,sizeof(struct miax_request));
  int r = send(gr->recovery_sock, &def_request_msg,
               sizeof(def_request_msg), MSG_NOSIGNAL);
  if (r <= 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: SESM Request send failed: %s",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             strerror(dev->lastErrno));
    return false;
  }

  EKA_LOG("%s:%u: \'%c\' Request sent",
          EKA_EXCH_DECODE(gr->exch), gr->id, refreshType);
  return true;
}

/* ###########################################################
 */
static bool sendRetransmitRequest(EkaFhMiaxGr *gr,
                                  uint64_t start,
                                  uint64_t end) {
  EkaDev *dev = gr->dev;
  sesm_retransmit_req retransmit_req = {};
  retransmit_req.header.length =
      sizeof(retransmit_req) -
      sizeof(retransmit_req.header.length);
  retransmit_req.header.type = 'A';
  retransmit_req.start = start;
  retransmit_req.end = end;

  int r = send(gr->recovery_sock, &retransmit_req,
               sizeof(retransmit_req), MSG_NOSIGNAL);
  if (r <= 0) {
    dev->lastErrno = errno;
    EKA_WARN(
        "%s:%u: SESM Retransmit Request send failed: %s",
        EKA_EXCH_DECODE(gr->exch), gr->id,
        strerror(dev->lastErrno));
    return false;
  }
  EKA_LOG("%s:%u: Retransmit Request sent for %ju .. %ju",
          EKA_EXCH_DECODE(gr->exch), gr->id, start, end);
  return true;
}

/* #######################################################
 */
static bool sendLogOut(EkaFhMiaxGr *gr) {
  EkaDev *dev = gr->dev;

  //--------------- SESM Logout Request -------------------
  struct sesm_logout_req sesm_logout_msg = {};
  sesm_logout_msg.header.length =
      sizeof(sesm_logout_req) -
      sizeof(sesm_logout_msg.header.length);
  sesm_logout_msg.header.type = 'X';
  sesm_logout_msg.reason =
      ' '; // Graceful Logout (Done for now)

  if (send(gr->recovery_sock, &sesm_logout_msg,
           sizeof(sesm_logout_req), MSG_NOSIGNAL) < 0) {
    dev->lastErrno = errno;
    EKA_WARN("%s:%u: SESM Logout send failed",
             EKA_EXCH_DECODE(gr->exch), gr->id);
    return false;
  }

  EKA_LOG("%s:%u: SESM Logout sent",
          EKA_EXCH_DECODE(gr->exch), gr->id);
  return true;
}

/* #######################################################
 */
static EkaFhParseResult procRefreshSesmPkt(
    const EfhRunCtx *pEfhRunCtx, int sock, EkaFhMiaxGr *gr,
    EkaFhMode op,
    EfhFastBuffer<TomSeriesUpdate, MaxVanillaDefinitions>
        *pVanillaDefinitions) {
  EkaDev *dev = gr->dev;
  sesm_header sesm_hdr = {};
  int r = recv(sock, &sesm_hdr, sizeof(sesm_header),
               MSG_WAITALL);
  if (r <= 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      EKA_WARN(
          "%s:%u failed to receive SESM header: r=%d: %s",
          EKA_EXCH_DECODE(gr->exch), gr->id, r,
          strerror(dev->lastErrno));
      return EkaFhParseResult::NotEnd;
    }
    dev->lastErrno = errno;
    EKA_WARN(
        "%s:%u failed to receive SESM header: r=%d: %s",
        EKA_EXCH_DECODE(gr->exch), gr->id, r,
        strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }
  uint8_t msg[1536] = {};
  uint8_t *m = msg;

  int payloadLen = sesm_hdr.length - sizeof(sesm_hdr.type);
  if (payloadLen < 0)
    on_error(
        "sesm_hdr.length %d < sizeof(sesm_hdr.type) %jd",
        sesm_hdr.length, sizeof(sesm_hdr.type));
  if (payloadLen > 0) {
    r = recv(sock, msg, payloadLen, MSG_WAITALL);
    if (r <= 0 || r != payloadLen) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u failed to receive SESM payload of %d "
               "bytes: r=%d: errno=%d, \'%s\'",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               payloadLen, r, errno, strerror(errno));

      return EkaFhParseResult::SocketError;
    }
  }
  uint64_t sequence = 0;

  switch ((EKA_SESM_TYPE)sesm_hdr.type) {
    //---------------------------------------
  case EKA_SESM_TYPE::GoodBye:
    EKA_LOG("%s:%u Sesm Refresh Server sent GoodBye with "
            "reason "
            "\'%c\' : %s",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            ((const sesm_goodbye *)m)->reason,
            ((const sesm_goodbye *)m)->text);
    return EkaFhParseResult::ProtocolError;
    //---------------------------------------

  case EKA_SESM_TYPE::ServerHeartbeat:
    EKA_LOG("%s:%u Sesm Server Heartbeat received",
            EKA_EXCH_DECODE(gr->exch), gr->id);
    return EkaFhParseResult::NotEnd;
    //---------------------------------------

  case EKA_SESM_TYPE::EndOfSession:
    EKA_LOG("%s:%u %s End-of-Session message. "
            "seq_after_snapshot = %ju",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            EkaFhMode2STR(op), gr->seq_after_snapshot);
    return EkaFhParseResult::End;
    //---------------------------------------

  case EKA_SESM_TYPE::SyncComplete:
    EKA_LOG("%s:%u SESM server sent SyncComplete message",
            EKA_EXCH_DECODE(gr->exch), gr->id);
    return EkaFhParseResult::End;

    //---------------------------------------

  case EKA_SESM_TYPE::TestPacket:
    EKA_LOG("%s:%u Sesm Server sent Test Packet: %s",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            (const char *)m);
    return EkaFhParseResult::NotEnd;
    //---------------------------------------

  case EKA_SESM_TYPE::Sequenced: {
    // Sequenced should not happen at Refresh
    sequence = *(uint64_t *)m;
    m += sizeof(sequence);
    auto msgType =
        reinterpret_cast<const TomCommon *>(m)->Type;

    EKA_WARN("%s:%u Unexpected Sequenced Packet at Refresh "
             "%s: sequence = %ju, Msg = \'%c\'",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             EkaFhMode2STR(op), sequence, msgType);
    gr->seq_after_snapshot = sequence;
    if (payloadLen == sizeof(sequence)) {
      EKA_WARN("%s:%u SESM Sequenced packet with no msg",
               EKA_EXCH_DECODE(gr->exch), gr->id);
      return EkaFhParseResult::NotEnd;
    }

    if (static_cast<TOM_MSG>(msgType) ==
        TOM_MSG::SeriesUpdate) {
      if (pVanillaDefinitions)
        pVanillaDefinitions->push(
            reinterpret_cast<const TomSeriesUpdate *>(m));
      break;
    }
    if (gr->parseMsg(pEfhRunCtx, m, sequence, op)) {
      EKA_WARN("%s:%u %s: Sequenced End Of Refresh: "
               "sequence = %ju",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               EkaFhMode2STR(op), sequence);
      return EkaFhParseResult::End;
    }

  } break;
    //---------------------------------------

  case EKA_SESM_TYPE::UnSequenced: {
    auto unsequencedHdr =
        reinterpret_cast<const sesm_unsequenced *>(m);
    auto unsequencedPktType = unsequencedHdr->type;
    auto unsequencedPktSeq = unsequencedHdr->sequence;

    switch (unsequencedPktType) {
      //++++++++++++++++++++++++++++++++++
    case 'R': { // Unsequenced ToM Refresh
      m += sizeof(*unsequencedHdr);

      auto pktPayloadLen =
          payloadLen - sizeof(*unsequencedHdr);

      if (!pktPayloadLen)
        on_error("%s:%u %s: pktPayloadLen = 0 "
                 "for unsequencedPktType = \'%c\'",
                 EKA_EXCH_DECODE(gr->exch), gr->id,
                 EkaFhMode2STR(op), unsequencedPktType);
      auto msgType =
          reinterpret_cast<const TomCommon *>(m)->Type;

      // Definitions
      if (static_cast<TOM_MSG>(msgType) ==
          TOM_MSG::SeriesUpdate) {
        if (pVanillaDefinitions)
          pVanillaDefinitions->push(
              reinterpret_cast<const TomSeriesUpdate *>(m));
        break;
      }

#if 0
      if (unsequencedPktSeq != 0 &&
          unsequencedPktSeq <= gr->seq_after_snapshot)
        EKA_DEBUG("%s:%u %s ignoring msgType = \'%c\' : "
                  "unsequencedPktSeq %ju < "
                  "seq_after_snapshot %ju",
                  EKA_EXCH_DECODE(gr->exch), gr->id,
                  EkaFhMode2STR(op), msgType,
                  unsequencedPktSeq,
                  gr->seq_after_snapshot);
#endif
      if (unsequencedPktSeq > gr->seq_after_snapshot)
        gr->seq_after_snapshot = unsequencedPktSeq;

      if (gr->parseMsg(pEfhRunCtx, m, unsequencedPktSeq,
                       op)) {
        EKA_LOG("%s:%u %s End Of Refresh "
                "msgType = \'%c\' : "
                "seq_after_snapshot = %ju",
                EKA_EXCH_DECODE(gr->exch), gr->id,
                EkaFhMode2STR(op), msgType,
                gr->seq_after_snapshot);
        return EkaFhParseResult::End;
      }
    } break;
      //++++++++++++++++++++++++++++++++++
    case 'E': { // Unsequenced End Of Request
      auto msgType =
          reinterpret_cast<const TomCommon *>(m)->Type;

      EKA_LOG("%s:%u %s SesM type = EndOfRequest \'E\': "
              "msgType = \'%c\'"
              "seq_after_snapshot = %ju",
              EKA_EXCH_DECODE(gr->exch), gr->id,
              EkaFhMode2STR(op), msgType,
              gr->seq_after_snapshot);
      if (payloadLen == 2)
        return EkaFhParseResult::End;

      if (gr->parseMsg(pEfhRunCtx, m, 0, op))
        return EkaFhParseResult::End;

      on_error("%s:%u %s SesM type = EndOfRequest, "
               "msgType = \'%c\'",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               EkaFhMode2STR(op), msgType);
    } break;
      //++++++++++++++++++++++++++++++++++
    default: // of switch (unsequencedPktType)
      on_error("%s:%u Unexpected UnSequenced Packet type "
               "\'%c\'",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               unsequencedPktType);
      //++++++++++++++++++++++++++++++++++
    }
  } break;
    //---------------------------------------

  default: // of switch ((EKA_SESM_TYPE)sesm_hdr.type)
    EKA_WARN("%s:%u Unexpected sesm_hdr.type: \'%c\'",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             sesm_hdr.type);
    on_error("%s:%u Unexpected sesm_hdr.type: \'%c\'",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             sesm_hdr.type);
    return EkaFhParseResult::ProtocolError;
  }
  return EkaFhParseResult::NotEnd;
}
/* ################################################### */
static EkaFhParseResult
procRetransmitSesmPkt(const EfhRunCtx *pEfhRunCtx, int sock,
                      EkaFhMiaxGr *gr, EkaFhMode op,
                      uint64_t end) {
  EkaDev *dev = gr->dev;
  sesm_header sesm_hdr = {};

RETRY_GETTING_SESM_HDR:
  int r = recv(sock, &sesm_hdr, sizeof(sesm_header),
               MSG_WAITALL);
  if (r <= 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      EKA_WARN("%s:%u SLOW TCP: "
               "failed to receive SESM header: r=%d: %s",
               EKA_EXCH_DECODE(gr->exch), gr->id, r,
               strerror(dev->lastErrno));
      goto RETRY_GETTING_SESM_HDR;
      return EkaFhParseResult::NotEnd;
    }
    dev->lastErrno = errno;
    EKA_WARN(
        "%s:%u failed to receive SESM header: r=%d: %s",
        EKA_EXCH_DECODE(gr->exch), gr->id, r,
        strerror(dev->lastErrno));
    return EkaFhParseResult::SocketError;
  }
  uint8_t msg[1536] = {};
  uint8_t *m = msg;

  int payloadLen = sesm_hdr.length - sizeof(sesm_hdr.type);
  if (payloadLen < 0)
    on_error(
        "sesm_hdr.length %d < sizeof(sesm_hdr.type) %jd",
        sesm_hdr.length, sizeof(sesm_hdr.type));
  if (payloadLen > 0) {
    r = recv(sock, msg, payloadLen, MSG_WAITALL);
    if (r <= 0 || r != payloadLen) {
      dev->lastErrno = errno;
      EKA_WARN("%s:%u failed to receive SESM payload of %d "
               "bytes: r=%d: errno=%d, \'%s\'",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               payloadLen, r, errno, strerror(errno));

      return EkaFhParseResult::SocketError;
    }
  }
  uint64_t sequence = 0;

  switch ((EKA_SESM_TYPE)sesm_hdr.type) {
    /* ------------------------------------------------- */
  case EKA_SESM_TYPE::GoodBye:
    EKA_LOG("%s:%u Sesm Server sent GoodBye with reason "
            "\'%c\' : %s",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            ((sesm_goodbye *)m)->reason,
            ((sesm_goodbye *)m)->text);
    if (((sesm_goodbye *)m)->reason == 'A') {
      EKA_WARN(
          "%s:%u SesM terminated incremental recovery "
          "before reaching last requested sequence %ju",
          EKA_EXCH_DECODE(gr->exch), gr->id, end);
      gr->seq_after_snapshot = end;

      return EkaFhParseResult::End;
    }
    return EkaFhParseResult::ProtocolError;

    /* -------------------------------------------------
     */

  case EKA_SESM_TYPE::ServerHeartbeat:
    EKA_LOG("%s:%u Sesm Server Heartbeat: "
            "sequence=%ju, required=%ju",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            gr->seq_after_snapshot, end);
    return EkaFhParseResult::NotEnd;
    /* ------------------------------------------------- */

  case EKA_SESM_TYPE::EndOfSession:
    EKA_LOG("%s:%u %s End-of-Session message. "
            "seq_after_snapshot = %ju",
            EKA_EXCH_DECODE(gr->exch), gr->id,
            EkaFhMode2STR(op), gr->seq_after_snapshot);
    return EkaFhParseResult::End;
    /* ------------------------------------------------- */

  case EKA_SESM_TYPE::SyncComplete:
    EKA_LOG("%s:%u SESM server sent SyncComplete message",
            EKA_EXCH_DECODE(gr->exch), gr->id);
    return EkaFhParseResult::End;
    /* ------------------------------------------------- */

  case EKA_SESM_TYPE::TestPacket:
    EKA_LOG("%s:%u Sesm Server sent Test Packet: %s",
            EKA_EXCH_DECODE(gr->exch), gr->id, (char *)m);
    return EkaFhParseResult::NotEnd;
    /* ------------------------------------------------- */

  case EKA_SESM_TYPE::Sequenced:
    sequence = *(uint64_t *)m;
    m += sizeof(sequence);
    if (payloadLen == sizeof(sequence)) {
      on_error("%s:%u SESM Sequenced packet with no msg",
               EKA_EXCH_DECODE(gr->exch), gr->id);
      return EkaFhParseResult::NotEnd;
    }

    gr->seq_after_snapshot = sequence;
    if (gr->parseMsg(pEfhRunCtx, m, sequence, op)) {
      EKA_LOG("%s:%u %s Unexpected EndOfSession: "
              "seq_after_snapshot = %ju",
              EKA_EXCH_DECODE(gr->exch), gr->id,
              EkaFhMode2STR(op), gr->seq_after_snapshot);
      return EkaFhParseResult::End;
    }

    if (gr->seq_after_snapshot == end) {
      EKA_LOG("%s:%u %s: Retransmit Gap closed: "
              "seq_after_snapshot == end = "
              "%ju: ",
              EKA_EXCH_DECODE(gr->exch), gr->id,
              EkaFhMode2STR(op), gr->seq_after_snapshot);
      return EkaFhParseResult::End;
    }

    break;
    /* ------------------------------------------------- */

  case EKA_SESM_TYPE::UnSequenced: {
    char unsequencedType = ((sesm_unsequenced *)m)->type;
    switch (unsequencedType) {
      /* --------------------------------------------- */
    case 'R':
      on_error("%s:%u %s Unexpected Unsequenced Pkt: "
               "seq_after_snapshot = %ju",
               EKA_EXCH_DECODE(gr->exch), gr->id,
               EkaFhMode2STR(op), gr->seq_after_snapshot);
      /* --------------------------------------------- */
    case 'E':
      EKA_LOG("%s:%u %s Unexpected EndOfSession: "
              "seq_after_snapshot = %ju",
              EKA_EXCH_DECODE(gr->exch), gr->id,
              EkaFhMode2STR(op), gr->seq_after_snapshot);
      return EkaFhParseResult::End;
      break;
      /* --------------------------------------------- */

    default:
      on_error(
          "%s:%u Unexpected UnSequenced Packet type \'%c\'",
          EKA_EXCH_DECODE(gr->exch), gr->id,
          unsequencedType);
    }
  } break;
    /* ------------------------------------------------- */

  default:
    EKA_WARN("%s:%u Unexpected sesm_hdr.type: \'%c\'",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             sesm_hdr.type);
    on_error("%s:%u Unexpected sesm_hdr.type: \'%c\'",
             EKA_EXCH_DECODE(gr->exch), gr->id,
             sesm_hdr.type);
    return EkaFhParseResult::ProtocolError;
  }
  return EkaFhParseResult::NotEnd;
}
/* #################################################### */
void *getSesmData(void *attr) {
#ifdef FH_LAB
  return NULL;
#endif

  auto params{reinterpret_cast<EkaFhThreadAttr *>(attr)};
  auto gr{reinterpret_cast<EkaFhMiaxGr *>(params->gr)};
  if (gr == NULL)
    on_error("gr == NULL");

  EfhCtx *pEfhCtx = params->pEfhCtx;
  EfhRunCtx *pEfhRunCtx = params->pEfhRunCtx;
  uint64_t start = params->startSeq;
  uint64_t end = params->endSeq;
  EkaFhMode op = params->op;
  delete params;

  if (op != EkaFhMode::DEFINITIONS)
    pthread_detach(pthread_self());

  EkaDev *dev = gr->dev;
  if (dev == NULL)
    on_error("dev == NULL");
  if (dev->fh[pEfhCtx->fhId] == NULL)
    on_error("dev->fh[pEfhCtx->fhId] == NULL for "
             "pEfhCtx->fhId = %u",
             pEfhCtx->fhId);
  if (gr == NULL)
    on_error("gr == NULL");

  EKA_LOG("%s:%u %s", EKA_EXCH_DECODE(gr->exch), gr->id,
          EkaFhMode2STR(op));

  //-----------------------------------------------------------------
  EkaCredentialLease *lease;
  gr->credentialAcquire(EkaCredentialType::kSnapshot,
                        gr->auth_user,
                        sizeof(gr->auth_user), &lease);

  //-----------------------------------------------------------------
  const int MaxTrials = 4;
  bool success = false;
  gr->recovery_active = true;

  switch (op) {
  case EkaFhMode::DEFINITIONS:
    EKA_LOG("%s:%u DEFINITIONS", EKA_EXCH_DECODE(gr->exch),
            gr->id);
    success = sesmRefreshCycle(dev, pEfhRunCtx, op, gr, 'P',
                               0, 0, MaxTrials);
    break;
  case EkaFhMode::SNAPSHOT: {
    char snapshotRequests[] = {
        'S', // System State Refresh
        'U', // Underlying Trading Status Refresh
        /* 'P', // Simple Series Update Refresh */
        'Q'}; // Simple Top of Market Refresh
    for (int i = 0; i < (int)(sizeof(snapshotRequests) /
                              sizeof(snapshotRequests[0]));
         i++) {
      EKA_LOG("%s:%u SNAPSHOT \'%c\' Phase",
              EKA_EXCH_DECODE(gr->exch), gr->id,
              snapshotRequests[i]);
      success = sesmRefreshCycle(dev, pEfhRunCtx, op, gr,
                                 snapshotRequests[i], start,
                                 end, MaxTrials);
    }
    break;
  }
  case EkaFhMode::RECOVERY:
    EKA_LOG("%s:%u RECOVERY %ju .. %ju",
            EKA_EXCH_DECODE(gr->exch), gr->id, start, end);
    success =
        sesmRetransmitCycle(dev, pEfhRunCtx, op, gr, ' ',
                            start, end, MaxTrials);
    break;
  default:
    on_error("%s:%u Unexpected op = %d",
             EKA_EXCH_DECODE(gr->exch), gr->id, (int)op);
  }
  //-------------------------------------------------------

  int rc = gr->credentialRelease(lease);
  if (rc != 0)
    on_error("%s:%u Failed to credRelease",
             EKA_EXCH_DECODE(gr->exch), gr->id);
  EKA_LOG("%s:%u Sesm Credentials Released",
          EKA_EXCH_DECODE(gr->exch), gr->id);
  //-------------------------------------------------------

  //-------------------------------------------------------
  if (!success) {
    EKA_WARN("%s:%u Failed after %d trials. Exiting...",
             EKA_EXCH_DECODE(gr->exch), gr->id, MaxTrials);
    on_error("%s:%u Failed after %d trials. Exiting...",
             EKA_EXCH_DECODE(gr->exch), gr->id, MaxTrials);
  }
  //-------------------------------------------------------
  gr->gapClosed = true;
  //-------------------------------------------------------
  EKA_LOG("%s:%u End Of %s, seq_after_snapshot = %ju",
          EKA_EXCH_DECODE(gr->exch), gr->id,
          EkaFhMode2STR(op), gr->seq_after_snapshot);

  gr->recovery_active = false;

  return NULL;
}

/* void* heartBeatThread(EkaDev* dev, EkaFhMiaxGr* gr, int
 * sock) { */
/*   sesm_header heartbeat = { */
/*     .length		= 1, //sizeof(struct sesm_header) -
 * sizeof(heartbeat.length), */
/*     .type		= '1' */
/*   }; */

/*   EKA_LOG("gr=%u to sock = %d",gr->id,sock); */

/*   while(gr->heartbeat_active) { */
/*     EKA_LOG("sending SESM hearbeat for gr=%u",gr->id); */
/*     if(send(sock,&heartbeat,sizeof(sesm_header), 0) < 0)
 * on_error("heartbeat send failed"); */
/*     usleep(900000); */
/*   } */
/*   EKA_LOG("SESM hearbeat thread terminated for
 * gr=%u",gr->id); */
/*   return NULL; */
/* } */
