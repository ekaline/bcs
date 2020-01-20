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
#include "eka_dev.h"
#include "Efh.h"


bool FhXdpGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  switch (((XdpMsgHdr*)m)->MsgType) {
    //-----------------------------------------------------------------------------
  case EKA_XDP_MSG_TYPE::REFRESH_QUOTE :
    if (op != EkaFhMode::RECOVERY) break;
    //-----------------------------------------------------------------------------
  case EKA_XDP_MSG_TYPE::QUOTE : {
    fh_b_security* s = book->find_security(((XdpQuote*)m)->SeriesIndex);
    if (s == NULL) return false;

    /* if ( */
    /* 	((XdpQuote*)m)->time.SourceTime  <  s->seconds ||  */
    /* 	(((XdpQuote*)m)->time.SourceTime == s->seconds && ((XdpQuote*)m)->time.SourceTimeNS < s->nanoseconds) */
    /* 	) return false; // Back-in-time from Recovery */

    /* s->seconds       = ((XdpQuote*)m)->time.SourceTime; */
    /* s->nanoseconds   = ((XdpQuote*)m)->time.SourceTimeNS; */

    s->bid_price     = ((XdpQuote*)m)->BidPrice;
    s->bid_size      = ((XdpQuote*)m)->BidVolume;
    s->bid_cust_size = ((XdpQuote*)m)->BidCustomerVolume;

    s->ask_price     = ((XdpQuote*)m)->AskPrice;
    s->ask_size      = ((XdpQuote*)m)->AskVolume;
    s->ask_cust_size = ((XdpQuote*)m)->AskCustomerVolume;

    switch (((XdpQuote*)m)->QuoteCondition) {
    case '1' : // (Regular Trading)
      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kNormal;
      break;
    case '2' : // (Rotation) -- TBD
      break;
    case '3' : // (Trading Halted)
      s->trading_action = EfhTradeStatus::kHalted;
      break;
    case '4' : // (Pre-open)
      s->option_open    = false;
      s->trading_action = EfhTradeStatus::kPreopen;
      break;
    default:
      on_error("Unexpected QuoteCondition: \'%c\'",((XdpQuote*)m)->QuoteCondition);
    }
    uint64_t ns = ((XdpQuote*)m)->time.SourceTime;
    book->generateOnQuote (pEfhRunCtx, s, sequence, (ns << 32) | ((XdpQuote*)m)->time.SourceTimeNS, gapNum);
  }
    break;
    //-----------------------------------------------------------------------------

  case EKA_XDP_MSG_TYPE::TRADE : {
    fh_b_security* s = book->find_security(((XdpTrade*)m)->SeriesIndex);
    if (s == NULL) return false;

    uint64_t ns = ((XdpQuote*)m)->time.SourceTime;
    const EfhTradeMsg msg = {
      { EfhMsgType::kTrade,
	{exch,(EkaLSI)id}, // group
	0,  // underlyingId
	(uint64_t) ((XdpTrade*)m)->SeriesIndex,
	sequence,
	(ns << 32) | ((XdpTrade*)m)->time.SourceTimeNS,
	gapNum },
      ((XdpTrade*)m)->Price,
      ((XdpTrade*)m)->Volume,
      EfhTradeCond::kReg
    };
    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
  }
    break;
    //-----------------------------------------------------------------------------

  case EKA_XDP_MSG_TYPE::SERIES_STATUS : {
    fh_b_security* s = book->find_security(((XdpTrade*)m)->SeriesIndex);
    if (s == NULL) return false;
    switch (((XdpSeriesStatus*)m)->SecurityStatus) {
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
    uint64_t ns = ((XdpQuote*)m)->time.SourceTime;
    book->generateOnQuote (pEfhRunCtx, s, sequence, (ns << 32) | ((XdpQuote*)m)->time.SourceTimeNS, gapNum);
  }
    //-----------------------------------------------------------------------------

  default:
    return false;
  }

  return false;
}

