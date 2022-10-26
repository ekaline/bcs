#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhXdpGr.h"
#include "EkaFhXdpParser.h"

using namespace Xdp;

bool EkaFhXdpGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op,
				 std::chrono::high_resolution_clock::time_point startTime) {
  switch (((XdpMsgHdr*)m)->MsgType) {
    //-----------------------------------------------------------------------------
  case MSG_TYPE::REFRESH_QUOTE :
    if (op != EkaFhMode::RECOVERY) break;
    //-----------------------------------------------------------------------------
  case MSG_TYPE::QUOTE : {
    XdpQuote* msg = (XdpQuote*)m;
    FhSecurity* s = book->findSecurity(msg->SeriesIndex);
    if (s == NULL) return false;

    /* s->seconds       = msg->time.SourceTime; */
    /* s->nanoseconds   = msg->time.SourceTimeNS; */

    s->bid_price     = msg->BidPrice;
    s->bid_size      = msg->BidVolume;
    s->bid_cust_size = msg->BidCustomerVolume;

    s->ask_price     = msg->AskPrice;
    s->ask_size      = msg->AskVolume;
    s->ask_cust_size = msg->AskCustomerVolume;

    switch (msg->QuoteCondition) {
    case '1' : // (Regular Trading)
      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kNormal;
      break;
    case '2' : // (Rotation) 
      //      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kOpeningRotation;
     break;
    case '3' : // (Trading Halted)
      s->trading_action = EfhTradeStatus::kHalted;
      break;
    case '4' : // (Pre-open)
      //      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kPreopen;
      break;
    case '5' : // (Rotation, legal width quote pending)
      //      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kOpeningRotation;
      break;
    default:
      hexDump("Bad Msg",m,sizeof(XdpQuote));
      on_error("Unexpected QuoteCondition: \'%c\' (0x%x)",msg->QuoteCondition,msg->QuoteCondition);
    }
    book->generateOnQuote (pEfhRunCtx, s, sequence, msg->time.SourceTime * SEC_TO_NANO + msg->time.SourceTimeNS, gapNum);
  }
    break;
    //-----------------------------------------------------------------------------

  case MSG_TYPE::TRADE : {
    XdpTrade* xdpMsg = (XdpTrade*) m;
    FhSecurity* s = book->findSecurity(xdpMsg->SeriesIndex);
    if (s == NULL) return false;

    EfhTradeMsg msg{};
    msg.header.msgType = EfhMsgType::kTrade;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) xdpMsg->SeriesIndex;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = xdpMsg->time.SourceTime * static_cast<uint64_t>(SEC_TO_NANO) + xdpMsg->time.SourceTimeNS;
    msg.header.gapNum         = gapNum;

    msg.price       = xdpMsg->Price;
    msg.size        = xdpMsg->Volume;
    msg.tradeStatus = s->trading_action;
    msg.tradeCond   = EfhTradeCond::kREG;

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
  }
    break;
    //-----------------------------------------------------------------------------

  case MSG_TYPE::SERIES_STATUS : {
    XdpSeriesStatus* msg = (XdpSeriesStatus*) m;
    FhSecurity* s = book->findSecurity(msg->SeriesIndex);
    if (s == NULL) return false;
    switch (msg->SecurityStatus) {
    case 'O' :
      s->option_open = true;
      s->trading_action = EfhTradeStatus::kNormal;
      break;
    case 'X' :
      s->option_open = false;
      s->trading_action = EfhTradeStatus::kClosed;
      break;
    case 'S' :
      s->trading_action = EfhTradeStatus::kHalted;
      break;
     case 'U' :
      s->trading_action = EfhTradeStatus::kNormal;
      break;
    default:
      return false;
    }
    book->generateOnQuote (pEfhRunCtx, s, sequence, msg->time.SourceTime * SEC_TO_NANO + msg->time.SourceTimeNS, gapNum);
  }
    break;

    //-----------------------------------------------------------------------------

  default:
    return false;
  }

  return false;
}

