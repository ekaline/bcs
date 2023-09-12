#include <assert.h>
#include <cmath>
#include <endian.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EkaFhBatsParser.h"
#include "EkaFhBatsTransaction.h"
#include "EkaFhParserCommon.h"
#include "EkaOsiSymbol.h"
#ifdef _PCAP_TEST_
#include "batsPcapParse.h"
#else
#include "EkaFhBatsGr.h"
#endif

static void eka_print_msg(FILE *md_file, uint8_t *m, int gr,
                          uint64_t sequence, uint64_t ts);
std::string ts_ns2str(uint64_t ts);

using namespace Bats;

bool EkaFhBatsGr::parseMsg(
    const EfhRunCtx *pEfhRunCtx, const unsigned char *m,
    uint64_t sequence, EkaFhMode op,
    std::chrono::high_resolution_clock::time_point
        startTime) {
  MsgId enc = (MsgId)m[1];
  //  EKA_LOG("%s:%u: 0x%02x",EKA_EXCH_DECODE(exch),id,enc);

#ifdef EFH_INTERNAL_LATENCY_CHECK
  book->latencyStat = {};
  book->latencyStat.msgType = (char)enc;
  auto start = std::chrono::high_resolution_clock::now();
#endif

  uint64_t msg_timestamp = 0;

  std::chrono::high_resolution_clock::time_point
      msgStartTime{};
#if EFH_TIME_CHECK_PERIOD
  if (sequence % EFH_TIME_CHECK_PERIOD == 0) {
    msgStartTime =
        std::chrono::high_resolution_clock::now();
  }
#endif

  if (op == EkaFhMode::SNAPSHOT &&
      enc == MsgId::SYMBOL_MAPPING)
    return false;

  if (msgHasTimestamp(enc))
    msg_timestamp =
        seconds + ((const GenericHeader *)m)->time;

  bool tsChanged = (grTs_ != msg_timestamp);
  grTs_ = msg_timestamp;

  if (tsChanged)
    flushTobBuf(pEfhRunCtx);

    /* if (state == GrpState::NORMAL) */
    /*   checkTimeDiff(dev->deltaTimeLogFile,dev->midnightSystemClock,msg_timestamp,sequence);
     */

#ifdef _PCAP_TEST_
  eka_print_msg(parser_log, (uint8_t *)m, id, sequence,
                msg_timestamp);
  return false;
#else
  FhSecurity *s = NULL;

  if (fh->print_parsed_messages)
    sendMdCb(pEfhRunCtx, m, id, sequence, msg_timestamp);
  if (fh->noTob && enc != MsgId::TIME &&
      enc != MsgId::DEFINITIONS_FINISHED &&
      enc != MsgId::SYMBOL_MAPPING &&
      enc != MsgId::SPIN_FINISHED)
    return false;

  switch (enc) {
    //--------------------------------------------------------------
  case MsgId::DEFINITIONS_FINISHED: {
    EKA_LOG("%s:%u: DEFINITIONS_FINISHED",
            EKA_EXCH_DECODE(exch), id);
    if (op == EkaFhMode::DEFINITIONS)
      return true;
    on_error("%s:%u DEFINITIONS_FINISHED accepted at non "
             "Definitions (%u) phase",
             EKA_EXCH_DECODE(exch), id, (uint)op);
  }
    //--------------------------------------------------------------
  case MsgId::SPIN_FINISHED: {
    seq_after_snapshot =
        ((const SpinFinished *)m)->sequence + 1;
    EKA_LOG(
        "%s:%u: SPIN_FINISHED: seq_after_snapshot = %ju",
        EKA_EXCH_DECODE(exch), id, seq_after_snapshot);
    return true;
  }
    //--------------------------------------------------------------
  case MsgId::SYMBOL_MAPPING:
    return process_Definition(pEfhRunCtx, m, sequence, op);
    //--------------------------------------------------------------
  case MsgId::TRANSACTION_BEGIN: {

    return false;
  }
    //--------------------------------------------------------------
  case MsgId::TRANSACTION_END: {

    return false;
  }
    //--------------------------------------------------------------
  case MsgId::END_OF_SESSION: {
    //    market_open = false;
    return true;
  }
    //--------------------------------------------------------------
  case MsgId::TIME: {
    seconds = ((const GenericHeader *)m)->time *
              (uint64_t)SEC_TO_NANO;
    return false;
  }
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_SHORT:
    if (productMask & PM_ComplexBook)
      s = process_AddOrderShort<FhSecurity,
                                add_order_short_complex>(m);
    else
      s = process_AddOrderShort<FhSecurity,
                                add_order_short>(m);
    if (!s)
      return false;
    break;
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_LONG:
    if (productMask & PM_ComplexBook)
      s = process_AddOrderLong<FhSecurity,
                               add_order_long_complex>(m);
    else
      s = process_AddOrderLong<FhSecurity, add_order_long>(
          m);
    if (!s)
      return false;
    break;
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_EXPANDED:
    if (productMask & PM_ComplexBook)
      s = process_AddOrderExpanded<
          FhSecurity, add_order_expanded_complex>(m);
    else
      s = process_AddOrderExpanded<FhSecurity,
                                   add_order_expanded>(m);
    if (!s)
      return false;
    break;
    //--------------------------------------------------------------
  case MsgId::ORDER_EXECUTED: {
    auto message{
        reinterpret_cast<const order_executed *>(m)};
    OrderIdT order_id = message->order_id;
    SizeT delta_size = message->executed_size;
    FhOrder *o = book->findOrder(order_id);
    if (!o)
      return false;
    s = (FhSecurity *)o->plevel->s;

    EfhTradeMsg msg{};
    msg.header.msgType = EfhMsgType::kTrade;
    msg.header.group.source = exch;
    msg.header.group.localId = id;
    msg.header.underlyingId = 0;
    msg.header.securityId = (uint64_t)s->secId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp = msg_timestamp;
    msg.header.gapNum = gapNum;

    msg.price = o->plevel->price;
    msg.size = message->executed_size;
    msg.tradeStatus = s->trading_action;
    msg.tradeCond = getTradeCond(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData,
                                pEfhRunCtx->efhRunUserData);

    book->setSecurityPrevState(s);

    if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o, delta_size);
      o->plevel->deductSize(o->type, delta_size);
    }

    break;
  }
    //--------------------------------------------------------------
  case MsgId::ORDER_EXECUTED_AT_PRICE_SIZE: {
    auto message{reinterpret_cast<
        const order_executed_at_price_size *>(m)};
    OrderIdT order_id = message->order_id;
    SizeT delta_size = message->executed_size;
    FhOrder *o = book->findOrder(order_id);
    if (o == NULL)
      return false;
    s = (FhSecurity *)o->plevel->s;

    EfhTradeMsg msg{};
    msg.header.msgType = EfhMsgType::kTrade;
    msg.header.group.source = exch;
    msg.header.group.localId = id;
    msg.header.underlyingId = 0;
    msg.header.securityId = (uint64_t)s->secId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp = msg_timestamp;
    msg.header.gapNum = gapNum;

    msg.price = (uint32_t)message->price;
    msg.size = message->executed_size;
    msg.tradeStatus = s->trading_action;
    msg.tradeCond = getTradeCond(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData,
                                pEfhRunCtx->efhRunUserData);

    book->setSecurityPrevState(s);

    if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o, delta_size);
      o->plevel->deductSize(o->type, delta_size);
    }

    break;
  }
    //--------------------------------------------------------------
  case MsgId::REDUCED_SIZE_LONG: {
    reduced_size_long *message = (reduced_size_long *)m;
    OrderIdT order_id = message->order_id;
    SizeT delta_size = message->canceled_size;
    FhOrder *o = book->findOrder(order_id);
    if (o == NULL)
      return false;
    s = (FhSecurity *)o->plevel->s;
    book->setSecurityPrevState(s);

    if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o, delta_size);
      o->plevel->deductSize(o->type, delta_size);
    }

    break;
  }
    //--------------------------------------------------------------
  case MsgId::REDUCED_SIZE_SHORT: {
    reduced_size_short *message = (reduced_size_short *)m;
    OrderIdT order_id = message->order_id;
    SizeT delta_size = message->canceled_size;
    FhOrder *o = book->findOrder(order_id);
    if (o == NULL)
      return false;
    assert(o->plevel != NULL);
    assert(o->plevel->s != NULL);
    s = (FhSecurity *)o->plevel->s;
    book->setSecurityPrevState(s);

    if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o, delta_size);
      o->plevel->deductSize(o->type, delta_size);
    }

    break;
  }
    //--------------------------------------------------------------
  case MsgId::ORDER_MODIFY_SHORT:
    if (productMask & PM_ComplexBook)
      s = process_OrderModifyShort<
          FhSecurity, order_modify_short_complex>(m);
    else
      s = process_OrderModifyShort<FhSecurity,
                                   order_modify_short>(m);
    if (!s)
      return false;
    break;
    //--------------------------------------------------------------
  case MsgId::ORDER_MODIFY_LONG:
    if (productMask & PM_ComplexBook)
      s = process_OrderModifyLong<
          FhSecurity, order_modify_long_complex>(m);
    else
      s = process_OrderModifyLong<FhSecurity,
                                  order_modify_long>(m);
    if (!s)
      return false;
    break;
    //--------------------------------------------------------------
  case MsgId::ORDER_DELETE: {
    order_delete *message = (order_delete *)m;

    OrderIdT order_id = message->order_id;
    FhOrder *o = book->findOrder(order_id);
    if (o == NULL)
      return false;
    s = (FhSecurity *)o->plevel->s;

    book->setSecurityPrevState(s);
    book->deleteOrder(o);
    break;
  }

    //--------------------------------------------------------------
  case MsgId::TRADE_SHORT:
    if (productMask & PM_ComplexTrades)
      s = process_TradeShort<FhSecurity,
                             TradeShort_complex>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    else
      s = process_TradeShort<FhSecurity, TradeShort>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    return false;
    //--------------------------------------------------------------
  case MsgId::TRADE_LONG:
    if (productMask & PM_ComplexTrades)
      s = process_TradeLong<FhSecurity, TradeLong_complex>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    else
      s = process_TradeLong<FhSecurity, TradeLong>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    return false;
    //--------------------------------------------------------------
  case MsgId::TRADE_EXPANDED:
    if (productMask & PM_ComplexTrades)
      s = process_TradeExpanded<FhSecurity,
                                TradeExpanded_complex>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    else
      s = process_TradeExpanded<FhSecurity, TradeExpanded>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    return false;
    //--------------------------------------------------------------
  case MsgId::TRADING_STATUS: {
    auto message{
        reinterpret_cast<const trading_status *>(m)};
    SecurityIdT security_id = symbol2secId(message->symbol);
    s = book->findSecurity(security_id);
    if (s == NULL)
      return false;
    book->setSecurityPrevState(s);

    s->trading_action = tradeAction(
        s->trading_action, message->trading_status);
    break;
  }
  //--------------------------------------------------------------
  case MsgId::OPTIONS_AUCTION_UPDATE: { // 0xD1
    if (productMask & PM_ComplexBook) {
      return false; // Complex imbalances not yet supported
    }
    auto message{
        reinterpret_cast<const OptionsAuctionUpdate *>(m)};
    SecurityIdT security_id =
        expSymbol2secId(message->symbol);
    s = book->findSecurity(security_id);
    if (s == nullptr)
      return false;

    if (message->referencePrice !=
        message->indicativePrice) {
      EKA_WARN("%s:%d: Skipping unexpected reference or "
               "indicative price on `%.8s`: ref=%" PRIu64
               ", ind=%" PRIu64,
               EKA_EXCH_DECODE(exch), id, message->symbol,
               message->referencePrice,
               message->indicativePrice);
      return false;
    }

    const int64_t price =
        getEfhPrice(message->indicativePrice);
    const uint32_t rawBidSize = message->buyContracts;
    const uint32_t rawAskSize = message->sellContracts;
    const uint32_t size = std::min(rawBidSize, rawAskSize);

    bool hasBid = false;
    bool hasAsk = false;
    switch (message->openingCondition) {
    case 'O': // Would open
      hasBid = hasAsk = true;
      break;
    case 'B': // Needs more buyers
      hasAsk = true;
      break;
    case 'S': // Needs more sellers
      hasBid = true;
      break;
    case 'Q': // Needs quote to open
    case 'C': // Crossed composite market
    default:
      if (size > 0) {
        EKA_WARN("%s:%d: Skipping unexpected positive size "
                 "on `%.8s`: openingCondition='%c', "
                 "bidSize=%u, askSize=%u",
                 EKA_EXCH_DECODE(exch), id, message->symbol,
                 message->openingCondition, rawBidSize,
                 rawAskSize);
        return false;
      }
    }

    EfhImbalanceMsg msg{};
    msg.header.msgType = EfhMsgType::kImbalance;
    msg.header.group.source = exch;
    msg.header.group.localId = id;
    msg.header.underlyingId = 0;
    msg.header.securityId = security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp = msg_timestamp;
    msg.header.gapNum = gapNum;

    msg.tradeStatus = s->trading_action;
    msg.bidSide.price = hasBid ? price : 0;
    msg.bidSide.size = hasBid ? size : 0;
    msg.askSide.price = hasAsk ? price : 0;
    msg.askSide.size = hasAsk ? size : 0;

    if (pEfhRunCtx->onEfhImbalanceMsgCb == NULL)
      on_error("pEfhRunCtx->onEfhImbalanceMsgCb == NULL");
    pEfhRunCtx->onEfhImbalanceMsgCb(
        &msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }
  //--------------------------------------------------------------
  case MsgId::AUCTION_NOTIFICATION: { // 0xAD
    if (productMask & PM_ComplexAuction)
      s = process_AuctionNotification<
          FhSecurity, AuctionNotification_complex>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    else
      s = process_AuctionNotification<FhSecurity,
                                      AuctionNotification>(
          pEfhRunCtx, sequence, msg_timestamp, m);
    if (s == NULL)
      return false;
    break;
  }
    //--------------------------------------------------------------
  case MsgId::AUCTION_CANCEL: { // 0xAE
    auto message{
        reinterpret_cast<const AuctionCancel *>(m)};

    AuctionIdT auctionId = message->auctionId;
    auto auctionSearch = auctionMap.find(auctionId);
    if (auctionSearch == auctionMap.end())
      return false;

    SecurityIdT security_id = auctionSearch->second;
    //    auctionMap.erase(auctionId);
    auctionMap.erase(auctionSearch);
    s = book->findSecurity(security_id);
    if (!s) {
      EKA_WARN("WARNING: auctionId %ju with no security_id",
               auctionId);
      return false;
    }

    EfhAuctionUpdateMsg msg{};
    msg.header.msgType = EfhMsgType::kAuctionUpdate;
    msg.header.group.source = exch;
    msg.header.group.localId = id;
    msg.header.underlyingId = 0;
    msg.header.securityId = security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp = msg_timestamp;
    msg.header.gapNum = gapNum;
    numToStrBuf(msg.auctionId, auctionId);
    msg.updateType = EfhAuctionUpdateType::kDelete;
    msg.auctionType = EfhAuctionType::kUnknown;

    pEfhRunCtx->onEfhAuctionUpdateMsgCb(
        &msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
    break;
  }
    //--------------------------------------------------------------
  case MsgId::
      COMPLEX_INSTRUMENT_DEFINITION_EXPANDED: { // 0x9A
    auto root{reinterpret_cast<
        const ComplexInstrumentDefinitionExpanded_root *>(
        m)};

    EfhComplexDefinitionMsg msg{};
    msg.header.msgType = EfhMsgType::kComplexDefinition;
    msg.header.group.source = exch;
    msg.header.group.localId = id;
    msg.header.underlyingId =
        expSymbol2secId(root->ComplexInstrumentUnderlying);
    msg.header.securityId =
        symbol2secId(root->ComplexInstrumentId);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp = 0;
    msg.header.gapNum = gapNum;

    msg.commonDef.securityType = EfhSecurityType::kComplex;
    msg.commonDef.exchange = EKA_GRP_SRC2EXCH(exch);

    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate =
        0; // FIXME: for completeness only, could be "today"
    msg.commonDef.expiryTime = 0;
    msg.commonDef.contractSize = 0;

    copySymbol(msg.commonDef.underlying,
               root->ComplexInstrumentUnderlying);
    copySymbol(msg.commonDef.classSymbol,
               root->ComplexInstrumentUnderlying);
    copySymbol(msg.commonDef.exchSecurityName,
               root->ComplexInstrumentId);

    msg.numLegs = root->LegCount;

    const auto *leg{reinterpret_cast<
        const ComplexInstrumentDefinitionExpanded_leg *>(
        m + sizeof(*root))};
    for (uint i = 0; i < msg.numLegs; i++) {
      msg.legs[i].securityId =
          expSymbol2secId(leg[i].LegSymbol);
      msg.legs[i].type =
          getComplexSecurityType(leg[i].LegSecurityType);
      msg.legs[i].side = leg[i].LegRatio > 0
                             ? EfhOrderSide::kBid
                             : EfhOrderSide::kAsk;
      msg.legs[i].ratio = std::abs(leg[i].LegRatio);
    }
    if (pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL)
      on_error("pEfhRunCtx->onEfhComplexDefinitionMsgCb == "
               "NULL");
    pEfhRunCtx->onEfhComplexDefinitionMsgCb(
        &msg, (EfhSecUserData)0,
        pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------

  default:
    //    EKA_LOG("Ignored Message type:
    //    enc=\'0x%02x\'",enc);
    return false;
  }
  if (s == NULL)
    on_error(
        "Uninitialized Security ptr after message 0x%x",
        (uint8_t)enc);

  // s->option_open = market_open;
  s->option_open = true;

  bool secChanged = (s != prevSec_);
  prevSec_ = s;

  if (secChanged)
    flushTobBuf(pEfhRunCtx);

  if (!book->isEqualState(s)) {
#ifdef EFH_INTERNAL_LATENCY_CHECK
    auto finish = std::chrono::high_resolution_clock::now();
    book->latencyStat.totalLatencyNs =
        (uint64_t)std::chrono::duration_cast<
            std::chrono::nanoseconds>(finish - start)
            .count();
    //    if (duration_ns > 5000)
    //        EKA_WARN("WARNING: \'%c\' processing took %ju
    //        ns",
    //                 enc, duration_ns);
    latencies.push_back(book->latencyStat);
#endif
    if (useTransactions && op == EkaFhMode::MCAST &&
        trCtx_) {
      trCtx_->pushTob(s, sequence, msg_timestamp);
    } else {
      book->generateOnQuote(pEfhRunCtx, s, sequence,
                            msg_timestamp, gapNum,
                            msgStartTime);
    }
  }

  return false;
}

int EkaFhBatsGr::sendMdCb(const EfhRunCtx *pEfhRunCtx,
                          const uint8_t *m, int gr,
                          uint64_t sequence, uint64_t ts) {
  if (pEfhRunCtx->onEfhMdCb == NULL)
    on_error("pEfhRunCtx->onMd == NULL");

  MsgId enc = (MsgId)m[1];

  uint8_t msgBuf[2000] = {};
  auto hdr{reinterpret_cast<EfhMdHeader *>(msgBuf)};

  hdr->mdRawMsgType =
      static_cast<decltype(hdr->mdRawMsgType)>(enc);
  hdr->group.source = exch;
  hdr->group.localId = id;
  hdr->sequenceNumber = sequence;
  hdr->timeStamp = ts;

  switch (enc) {
  case MsgId::ADD_ORDER_LONG: {
    auto srcMsg{
        reinterpret_cast<const add_order_long *>(m)};
    auto dstMsg{reinterpret_cast<MdNewOrder *>(msgBuf)};

    hdr->mdMsgType = EfhMdType::NewOrder;
    hdr->mdMsgSize = sizeof(MdNewOrder);
    hdr->securityId = symbol2secId(srcMsg->symbol);

    dstMsg->attr = srcMsg->flags;
    dstMsg->orderId = srcMsg->order_id;
    dstMsg->side = efhMsgSideDecode(srcMsg->side);
    dstMsg->price = srcMsg->price / EFH_PRICE_SCALE;
    dstMsg->size = srcMsg->size;

    pEfhRunCtx->onEfhMdCb(hdr, (EfhRunUserData)parser_log);
  } break;

  case MsgId::ADD_ORDER_SHORT: {
    auto srcMsg{
        reinterpret_cast<const add_order_short *>(m)};
    auto dstMsg{reinterpret_cast<MdNewOrder *>(msgBuf)};

    hdr->mdMsgType = EfhMdType::NewOrder;
    hdr->mdMsgSize = sizeof(MdNewOrder);
    hdr->securityId = symbol2secId(srcMsg->symbol);

    dstMsg->attr = srcMsg->flags;
    dstMsg->orderId = srcMsg->order_id;
    dstMsg->side = efhMsgSideDecode(srcMsg->side);
    dstMsg->price = static_cast<decltype(dstMsg->price)>(
        srcMsg->price * 100 / EFH_PRICE_SCALE);
    dstMsg->size =
        static_cast<decltype(dstMsg->size)>(srcMsg->size);

    pEfhRunCtx->onEfhMdCb(hdr, (EfhRunUserData)parser_log);
  } break;

  case MsgId::ADD_ORDER_EXPANDED: {
    auto srcMsg{
        reinterpret_cast<const add_order_expanded *>(m)};
    auto dstMsg{reinterpret_cast<MdNewOrder *>(msgBuf)};

    hdr->mdMsgType = EfhMdType::NewOrder;
    hdr->mdMsgSize = sizeof(MdNewOrder);
    hdr->securityId = expSymbol2secId(srcMsg->exp_symbol);

    dstMsg->attr = srcMsg->flags;
    dstMsg->orderId = srcMsg->order_id;
    dstMsg->side = efhMsgSideDecode(srcMsg->side);
    dstMsg->price = srcMsg->price / EFH_PRICE_SCALE;
    dstMsg->size = srcMsg->size;

    pEfhRunCtx->onEfhMdCb(hdr, (EfhRunUserData)parser_log);
  } break;

  default:
    break;
  }

#endif // _PCAP_TEST_
  return 0;
}

static void eka_print_msg(FILE *md_file, uint8_t *m, int gr,
                          uint64_t sequence, uint64_t ts) {
  MsgId enc = (MsgId)m[1];
  //  fprintf(md_file,"%s,%ju,%s(0x%x),",ts_ns2str(ts).c_str(),sequence,EKA_BATS_PITCH_MSG_DECODE(enc),(uint8_t)enc);

  switch (enc) {
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_LONG:
    fprintf(md_file, "SN:%ju,", sequence);
    fprintf(md_file,
            "SID:0x%02x%02x%02x%02x%02x%02x%02x%02x,%c,P:%"
            "8ju,S:%8u\n",
            //	     EKA_PRINT_BATS_SYMBOL(((add_order_long*)m)->symbol),
            0, 0, ((add_order_long *)m)->symbol[0],
            ((add_order_long *)m)->symbol[1],
            ((add_order_long *)m)->symbol[2],
            ((add_order_long *)m)->symbol[3],
            ((add_order_long *)m)->symbol[4],
            ((add_order_long *)m)->symbol[5],

            ((add_order_long *)m)->side,
            ((add_order_long *)m)->price,
            ((add_order_long *)m)->size);

    /* fprintf(md_file,"%ju,%c,%u,%s (%u),%ju,0x%x\n", */
    /* 	    ((add_order_long*)m)->order_id, */
    /* 	    ((add_order_long*)m)->side, */
    /* 	    ((add_order_long*)m)->size, */
    /* 	    EKA_PRINT_BATS_SYMBOL(((add_order_long*)m)->symbol),
     */
    /* 	    bats_symbol2optionid(((add_order_long*)m)->symbol,6),
     */
    /* 	    ((add_order_long*)m)->price, */
    /* 	    ((add_order_long*)m)->flags */
    /* 	    ); */
    break;

    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_SHORT:
    fprintf(md_file, "SN:%ju,", sequence);
    fprintf(md_file,
            "SID:0x%02x%02x%02x%02x%02x%02x%02x%02x,%c,P:%"
            "8u,S:%8u\n",
            //	     EKA_PRINT_BATS_SYMBOL(((add_order_short*)m)->symbol),
            0, 0, ((add_order_short *)m)->symbol[0],
            ((add_order_short *)m)->symbol[1],
            ((add_order_short *)m)->symbol[2],
            ((add_order_short *)m)->symbol[3],
            ((add_order_short *)m)->symbol[4],
            ((add_order_short *)m)->symbol[5],

            ((add_order_short *)m)->side,
            ((add_order_short *)m)->price * 100 /
                EFH_PRICE_SCALE,
            ((add_order_short *)m)->size);

    /* fprintf(md_file,"%ju,%c,%u,%s (%u),%u,0x%x\n", */
    /* 	    ((add_order_short*)m)->order_id, */
    /* 	    ((add_order_short*)m)->side, */
    /* 	    ((add_order_short*)m)->size, */
    /* 	    EKA_PRINT_BATS_SYMBOL(((add_order_short*)m)->symbol),
     */
    /* 	    bats_symbol2optionid(((add_order_short*)m)->symbol,6),
     */
    /* 	    ((add_order_short*)m)->price, */
    /* 	    ((add_order_short*)m)->flags */
    /* 	    ); */
    break;

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::ORDER_EXECUTED: */
    /*   fprintf(md_file,"%ju,%u,%ju,%c\n", */
    /* 	    ((order_executed*)m)->order_id, */
    /* 	    ((order_executed*)m)->executed_size, */
    /* 	    ((order_executed*)m)->execution_id, */
    /* 	    ((order_executed*)m)->trade_condition */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::ORDER_EXECUTED_AT_PRICE_SIZE: */
    /*   fprintf(md_file,"%ju,%u,%u,%ju,%ju,%c\n", */
    /* 	    ((order_executed_at_price_size*)m)->order_id, */
    /* 	    ((order_executed_at_price_size*)m)->executed_size,
     */
    /* 	    ((order_executed_at_price_size*)m)->remaining_size,
     */
    /* 	    ((order_executed_at_price_size*)m)->execution_id,
     */
    /* 	    ((order_executed_at_price_size*)m)->price, */
    /* 	    ((order_executed_at_price_size*)m)->trade_condition
     */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::REDUCED_SIZE_LONG: */
    /*   fprintf(md_file,"%ju,%u\n", */
    /* 	    ((reduced_size_long*)m)->order_id, */
    /* 	    ((reduced_size_long*)m)->canceled_size */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::REDUCED_SIZE_SHORT: */
    /*   fprintf(md_file,"%ju,%u\n", */
    /* 	    ((reduced_size_short*)m)->order_id, */
    /* 	    ((reduced_size_short*)m)->canceled_size */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::ORDER_MODIFY_LONG: */
    /*   fprintf(md_file,"%ju,%u,%ju,0x%x\n", */
    /* 	    ((order_modify_long*)m)->order_id, */
    /* 	    ((order_modify_long*)m)->size, */
    /* 	    ((order_modify_long*)m)->price, */
    /* 	    ((order_modify_long*)m)->flags */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::ORDER_MODIFY_SHORT: */
    /*   fprintf(md_file,"%ju,%u,%u,0x%x\n", */
    /* 	    ((order_modify_short*)m)->order_id, */
    /* 	    ((order_modify_short*)m)->size, */
    /* 	    ((order_modify_short*)m)->price, */
    /* 	    ((order_modify_short*)m)->flags */
    /* 	    ); */
    /*   break; */
    /*   //--------------------------------------------------------------
     */
    /* case MsgId::ORDER_DELETE: */
    /*   fprintf(md_file,"%ju\n", */
    /* 	    ((order_delete*)m)->order_id */
    /* 	    ); */
    /*   break; */
    /*   //--------------------------------------------------------------
     */
    /* case MsgId::TRADE_LONG: */
    /*   fprintf(md_file,"%ju,%c,%u,%s (%u),%ju,%ju,%c\n",
     */
    /* 	    ((trade_long*)m)->order_id, */
    /* 	    ((trade_long*)m)->side, */
    /* 	    ((trade_long*)m)->size, */
    /* 	    EKA_PRINT_BATS_SYMBOL(((trade_long*)m)->symbol),
     */
    /* 	    bats_symbol2optionid(((trade_long*)m)->symbol,6),
     */
    /* 	    ((trade_long*)m)->price, */
    /* 	    ((trade_long*)m)->execution_id, */
    /* 	    ((trade_long*)m)->trade_condition */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::TRADE_SHORT: */
    /*   fprintf(md_file,"%ju,%c,%u,%s (%u),%u,%ju,%c\n", */
    /* 	    ((trade_short*)m)->order_id, */
    /* 	    ((trade_short*)m)->side, */
    /* 	    ((trade_short*)m)->size, */
    /* 	    EKA_PRINT_BATS_SYMBOL(((trade_short*)m)->symbol),
     */
    /* 	    bats_symbol2optionid(((trade_short*)m)->symbol,6),
     */
    /* 	    ((trade_short*)m)->price, */
    /* 	    ((trade_short*)m)->execution_id, */
    /* 	    ((trade_short*)m)->trade_condition */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */
    /* case MsgId::TRADING_STATUS: */
    /*   fprintf(md_file,"%s (%u),%c,%c\n", */
    /* 	    EKA_PRINT_BATS_SYMBOL(((trading_status*)m)->symbol),
     */
    /* 	    bats_symbol2optionid(((trading_status*)m)->symbol,6),
     */
    /* 	    ((trading_status*)m)->trading_status, */
    /* 	    ((trading_status*)m)->gth_trading_status */
    /* 	    ); */
    /*   break; */

    /*   //--------------------------------------------------------------
     */

  default:
    //    fprintf(md_file,"\n");
    break;
  }
}

bool EkaFhBatsGr::process_Definition(
    const EfhRunCtx *pEfhRunCtx, const unsigned char *m,
    uint64_t sequence, EkaFhMode op) {
  auto message{reinterpret_cast<const SymbolMapping *>(m)};

  EkaOsiSymbolData osi;
  if (!osi.parseFromSymbol(message->osi_symbol)) {
    EKA_ERROR("%s:%d: Skipping option def `%.21s` (`%.6s`) "
              "as the OSI symbol is not valid!",
              EKA_EXCH_DECODE(exch), id,
              message->osi_symbol, message->symbol);
    return false;
  }

  EfhOptionDefinitionMsg msg{};
  msg.header.msgType = EfhMsgType::kOptionDefinition;
  msg.header.group.source = exch;
  msg.header.group.localId = id;
  msg.header.underlyingId =
      expSymbol2secId(message->underlying);
  msg.header.securityId = symbol2secId(message->symbol);
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp = 0;
  msg.header.gapNum = gapNum;

  msg.commonDef.securityType = EfhSecurityType::kOption;
  msg.commonDef.exchange = EKA_GRP_SRC2EXCH(exch);
  msg.commonDef.isPassive =
      isDefinitionPassive(EfhSecurityType::kOption);
  msg.commonDef.underlyingType = EfhSecurityType::kStock;
  msg.commonDef.expiryDate = osi.getYYYYMMDD();
  msg.strikePrice = osi.efhStrikePrice;
  msg.optionType = osi.optionType;
  msg.optionStyle = EfhOptionStyle::kAmerican;
  msg.segmentId = batsUnit;

  copySymbol(msg.commonDef.underlying, message->underlying);
  osi.copyRoot(msg.commonDef.classSymbol,
               message->osi_symbol);
  copySymbol(msg.commonDef.exchSecurityName,
             message->symbol);
  memcpy(&msg.commonDef.opaqueAttrA, message->symbol, 6);

  pEfhRunCtx->onEfhOptionDefinitionMsgCb(
      &msg, (EfhSecUserData)0, pEfhRunCtx->efhRunUserData);
  return false;
}

template <class SecurityT, class OrderMsgT>
SecurityT *
EkaFhBatsGr::process_AddOrderShort(const uint8_t *m) {
  auto *message = reinterpret_cast<const OrderMsgT *>(m);
  SecurityIdT security_id = symbol2secId(message->symbol);

  SecurityT *s = book->findSecurity(security_id);
  if (!s)
    return NULL;
  book->setSecurityPrevState(s);

  OrderIdT order_id = message->order_id;
  SizeT size = message->size;
  PriceT price =
      message->price * 100 /
      EFH_PRICE_SCALE; // Short Price representation
  SideT side = sideDecode(message->side);

  book->addOrder(s, order_id,
                 addFlags2orderType(message->flags), price,
                 size, side);
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *
EkaFhBatsGr::process_AddOrderLong(const uint8_t *m) {
  auto *message = reinterpret_cast<const OrderMsgT *>(m);
  SecurityIdT security_id = symbol2secId(message->symbol);

  SecurityT *s = book->findSecurity(security_id);
  if (!s)
    return NULL;
  book->setSecurityPrevState(s);

  OrderIdT order_id = message->order_id;
  SizeT size = message->size;
  PriceT price = message->price / EFH_PRICE_SCALE;
  SideT side = sideDecode(message->side);

  /* if (checkPriceLengh(message->price,EFH_PRICE_SCALE)) */
  /*   EKA_WARN("%s %s seq=%ju Long price(%jd) exceeds
   * 32bit", */
  /* 	     std::string(message->symbol,sizeof(message->symbol)).c_str(),
   */
  /* 	     ts_ns2str(msg_timestamp).c_str(), */
  /* 	     sequence, */
  /* 	     message->price); */
  book->addOrder(s, order_id,
                 addFlags2orderType(message->flags), price,
                 size, side);
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *
EkaFhBatsGr::process_AddOrderExpanded(const uint8_t *m) {
  auto *message = reinterpret_cast<const OrderMsgT *>(m);
  SecurityIdT security_id =
      expSymbol2secId(message->exp_symbol);

  SecurityT *s = book->findSecurity(security_id);
  if (!s)
    return NULL;
  book->setSecurityPrevState(s);

  OrderIdT order_id = message->order_id;
  SizeT size = message->size;
  PriceT price = message->price / EFH_PRICE_SCALE;
  SideT side = sideDecode(message->side);

  /* if (checkPriceLengh(message->price,EFH_PRICE_SCALE)) */
  /*   EKA_WARN("%s %s seq=%ju Long price(%jd) exceeds
   * 32bit", */
  /* 	     std::string(message->symbol,sizeof(message->symbol)).c_str(),
   */
  /* 	     ts_ns2str(msg_timestamp).c_str(), */
  /* 	     sequence, */
  /* 	     message->price); */
  book->addOrder(
      s, order_id,
      addFlagsCustomerIndicator2orderType(
          message->flags, message->customer_indicator),
      price, size, side);
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *
EkaFhBatsGr::process_OrderModifyShort(const uint8_t *m) {
  auto *message = reinterpret_cast<const OrderMsgT *>(m);

  OrderIdT order_id = message->order_id;
  SizeT size = message->size;
  PriceT price =
      message->price * 100 /
      EFH_PRICE_SCALE; // Short Price representation

  FhOrder *o = book->findOrder(order_id);
  if (!o)
    return NULL;
  SecurityT *s = (FhSecurity *)o->plevel->s;
  book->setSecurityPrevState(s);

  book->modifyOrder(
      o, price, size); // modify order for price and size
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *
EkaFhBatsGr::process_OrderModifyLong(const uint8_t *m) {
  auto *message = reinterpret_cast<const OrderMsgT *>(m);

  OrderIdT order_id = message->order_id;
  SizeT size = message->size;
  PriceT price = message->price / EFH_PRICE_SCALE;

  /* if (checkPriceLengh(message->price,EFH_PRICE_SCALE)) */
  /*   EKA_WARN("ORDER_MODIFY_LONG: %s seq=%ju Long
   * price(%ju) exceeds 32bit", */
  /* 	     ts_ns2str(msg_timestamp).c_str(), */
  /* 	     sequence, */
  /* 	     message->price); */

  FhOrder *o = book->findOrder(order_id);
  if (!o)
    return NULL;
  SecurityT *s = (FhSecurity *)o->plevel->s;
  book->setSecurityPrevState(s);

  book->modifyOrder(
      o, price, size); // modify order for price and size
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *EkaFhBatsGr::process_TradeShort(
    const EfhRunCtx *pEfhRunCtx, uint64_t sequence,
    uint64_t msg_timestamp, const uint8_t *m) {
  auto message{reinterpret_cast<const TradeShort *>(m)};
  SecurityIdT security_id = symbol2secId(message->symbol);

  SecurityT *s = book->findSecurity(security_id);
  if (!s)
    return NULL;

  PriceT price =
      message->price * 100 /
      EFH_PRICE_SCALE; // Short Price representation
  SizeT size = message->size;

  EfhTradeMsg msg{};
  msg.header.msgType = EfhMsgType::kTrade;
  msg.header.group.source = exch;
  msg.header.group.localId = id;
  msg.header.underlyingId = 0;
  msg.header.securityId = (uint64_t)security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp = msg_timestamp;
  msg.header.gapNum = gapNum;

  msg.price = price;
  msg.size = size;
  msg.tradeStatus = s->trading_action;
  msg.tradeCond = getTradeCond(message->trade_condition);

  pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData,
                              pEfhRunCtx->efhRunUserData);
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *EkaFhBatsGr::process_TradeLong(
    const EfhRunCtx *pEfhRunCtx, uint64_t sequence,
    uint64_t msg_timestamp, const uint8_t *m) {
  auto message{reinterpret_cast<const TradeLong *>(m)};
  SecurityIdT security_id = symbol2secId(message->symbol);

  SecurityT *s = book->findSecurity(security_id);
  if (!s)
    return NULL;

  PriceT price = message->price / EFH_PRICE_SCALE;
  SizeT size = message->size;

  /* if (checkPriceLengh(message->price,EFH_PRICE_SCALE)) */
  /*   EKA_WARN("%s %s seq=%ju Long price(%ju) exceeds
   * 32bit", */
  /* 	       std::string(message->symbol,sizeof(message->symbol)).c_str(),
   */
  /* 	       ts_ns2str(msg_timestamp).c_str(), */
  /* 	       sequence, */
  /* 	       message->price); */

  EfhTradeMsg msg{};
  msg.header.msgType = EfhMsgType::kTrade;
  msg.header.group.source = exch;
  msg.header.group.localId = id;
  msg.header.underlyingId = 0;
  msg.header.securityId = (uint64_t)security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp = msg_timestamp;
  msg.header.gapNum = gapNum;

  msg.price = price;
  msg.size = size;
  msg.tradeStatus = s->trading_action;
  msg.tradeCond = getTradeCond(message->trade_condition);

  pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData,
                              pEfhRunCtx->efhRunUserData);
  return s;
}

template <class SecurityT, class OrderMsgT>
SecurityT *EkaFhBatsGr::process_TradeExpanded(
    const EfhRunCtx *pEfhRunCtx, uint64_t sequence,
    uint64_t msg_timestamp, const uint8_t *m) {
  auto message{reinterpret_cast<const OrderMsgT *>(m)};
  SecurityIdT security_id =
      expSymbol2secId(message->symbol);

  SecurityT *s = book->findSecurity(security_id);
  if (!s)
    return NULL;

  PriceT price = message->price / EFH_PRICE_SCALE;
  SizeT size = message->size;

  /* if (checkPriceLengh(message->price,EFH_PRICE_SCALE)) */
  /*   EKA_WARN("%s %s seq=%ju Long price(%ju) exceeds
   * 32bit", */
  /* 	       std::string(message->symbol,sizeof(message->symbol)).c_str(),
   */
  /* 	       ts_ns2str(msg_timestamp).c_str(), */
  /* 	       sequence, */
  /* 	       message->price); */

  EfhTradeMsg msg{};
  msg.header.msgType = EfhMsgType::kTrade;
  msg.header.group.source = exch;
  msg.header.group.localId = id;
  msg.header.underlyingId = 0;
  msg.header.securityId = (uint64_t)security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp = msg_timestamp;
  msg.header.gapNum = gapNum;

  msg.price = price;
  msg.size = size;
  msg.tradeStatus = s->trading_action;
  msg.tradeCond = getTradeCond(message->trade_condition);

  pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData,
                              pEfhRunCtx->efhRunUserData);
  return s;
}

inline EfhAuctionType
getAuctionType(const AuctionNotification &message,
               bool useAIM) {
  return getVanillaAuctionType(message.auctionType, useAIM);
}

inline EfhAuctionType
getAuctionType(const AuctionNotification_complex &message,
               bool) {
  return getComplexAuctionType(message.auctionType);
}

inline int64_t
getAuctionPrice(const AuctionNotification &message) {
  return static_cast<int64_t>(message.price);
}

inline int64_t getAuctionPrice(
    const AuctionNotification_complex &message) {
  if (message.side == 'S') {
    return -message.price;
  } else {
    return message.price;
  }
}

template <class SecurityT, class AuctionMsgT>
SecurityT *EkaFhBatsGr::process_AuctionNotification(
    const EfhRunCtx *pEfhRunCtx, uint64_t sequence,
    uint64_t msg_timestamp, const uint8_t *m) {
  auto message{reinterpret_cast<const AuctionMsgT *>(m)};
  SecurityIdT security_id = symbol2secId(message->symbol);
  SecurityT *s = book->findSecurity(security_id);
  if (s == nullptr)
    return nullptr;
  AuctionIdT auctionId = message->auctionId;
  auctionMap[auctionId] = security_id;
  const bool useAIM = exch == EkaSource::kC1_PITCH;

  EfhAuctionUpdateMsg msg{};
  msg.header.msgType = EfhMsgType::kAuctionUpdate;
  msg.header.group.source = exch;
  msg.header.group.localId = id;
  msg.header.underlyingId = 0;
  msg.header.securityId = security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp = msg_timestamp;
  msg.header.gapNum = gapNum;

  numToStrBuf(msg.auctionId, auctionId);
  msg.auctionType = getAuctionType(*message, useAIM);

  msg.updateType = EfhAuctionUpdateType::kNew;
  msg.side = getSide(message->side);
  msg.capacity = getRfqCapacity(message->customerIndicator);
  msg.quantity = message->contracts;
  msg.price = getAuctionPrice(*message);
  msg.endTimeNanos = seconds + message->auctionEndOffset;

  pEfhRunCtx->onEfhAuctionUpdateMsgCb(
      &msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
  return s;
}
