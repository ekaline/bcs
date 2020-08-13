#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "eka_fh.h"
#include "eka_fh_xdp_messages.h"
#include "eka_data_structs.h"
#include "EkaDev.h"
#include "Efh.h"

void hexDump (const char* desc, void *addr, int len);

bool FhXdpGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  switch (((XdpMsgHdr*)m)->MsgType) {
    //-----------------------------------------------------------------------------
  case EKA_XDP_MSG_TYPE::REFRESH_QUOTE :
    if (op != EkaFhMode::RECOVERY) break;
    //-----------------------------------------------------------------------------
  case EKA_XDP_MSG_TYPE::QUOTE : {
    XdpQuote* msg = (XdpQuote*)m;
    fh_b_security* s = book->find_security(msg->SeriesIndex);
    if (s == NULL) {
      if (! book->subscribe_all) return false;
      s = book->subscribe_security(msg->SeriesIndex & 0x00000000FFFFFFFF,0,0,0,0);
    }

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
      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kOpeningRotation;
     break;
    case '3' : // (Trading Halted)
      s->trading_action = EfhTradeStatus::kHalted;
      break;
    case '4' : // (Pre-open)
      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kPreopen;
      break;
    case '5' : // (Rotation, legal width quote pending)
      s->option_open    = true;
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

  case EKA_XDP_MSG_TYPE::TRADE : {
    XdpTrade* msg = (XdpTrade*) m;
    fh_b_security* s = book->find_security(msg->SeriesIndex);
    if (s == NULL) return false;

    const EfhTradeMsg efhTradeMsg = {
      { EfhMsgType::kTrade,
	{exch,(EkaLSI)id}, // group
	0,  // underlyingId
	(uint64_t) msg->SeriesIndex,
	sequence,
	msg->time.SourceTime * static_cast<uint64_t>(SEC_TO_NANO) + msg->time.SourceTimeNS,
	gapNum },
      msg->Price,
      msg->Volume,
      EfhTradeCond::kReg
    };
    pEfhRunCtx->onEfhTradeMsgCb(&efhTradeMsg, s->efhUserData, pEfhRunCtx->efhRunUserData);
  }
    break;
    //-----------------------------------------------------------------------------

  case EKA_XDP_MSG_TYPE::SERIES_STATUS : {
    XdpSeriesStatus* msg = (XdpSeriesStatus*) m;
    fh_b_security* s = book->find_security(msg->SeriesIndex);
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

