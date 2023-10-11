#include "EkaEobi.h"
#include "EkaEobiParser.h"
#include "EkaEobiBook.h"
#include "EOBILayouts.h"
#include "EkaStrat.h"


#define SecurityStatusDecode(x) \
  x == 1 ? "Active" : \
    x == 2 ? "Inactive" : \
    x == 4 ? "Expired" : \
    x == 6 ? "KnockedOut" : \
    x == 7 ? "KnockedOutRevoked" : \
    x == 9 ? "Suspended" : \
    x == 11 ? "PendingDeletion" : \
    x == 12 ? "KnockedOutAndSuspended" : \
    "UNKNOWN"


inline uint EkaEobiParser::processPkt(MdOut* mdOut, 
				      ProcessAction processAction, 
				      uint8_t* pkt, 
				      uint pktPayloadSize) {
  if (pkt == NULL) on_error("pkt == NULL");

  uint8_t* p = pkt;
  uint8_t* pktEnd = pkt + pktPayloadSize;
  mdOut->book      = NULL;
  mdOut->lastEvent = false;

  MessageHeaderCompT* hdr = (MessageHeaderCompT*) p;
  if (hdr->TemplateID != TID_PACKETHEADER) 
    on_error("TemplateID %u != TID_PACKETHEADER",hdr->TemplateID);

  mdOut->transactTime = ((PacketHeaderT*)hdr)->TransactTime;

  p += hdr->BodyLen;

  while (p < pktEnd) {
    p += processMsg(mdOut,processAction,p);
  }

  return pktPayloadSize;
}

  //--------------------------------------------------------

inline uint EkaEobiParser::processMsg(MdOut* mdOut, ProcessAction processAction, uint8_t* msgStart) {
  MessageHeaderCompT* hdr = (MessageHeaderCompT*) msgStart;

#ifdef _EKA_PARSER_PRINT_ALL_
  EKA_LOG ("MsgId = %u: ",(uint)hdr->TemplateID);
  printf ("===============================\n");
#endif
  EkaEobiBook* b = NULL;
  SIDE eventSide = NONE;
  switch (hdr->TemplateID) {
    /* ------------------------------ */
  case TID_ORDERADD : {
    OrderAddT* msg = (OrderAddT*) hdr;
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    //    EKA_LOG ("msg->SecurityID = %u: ",(uint)msg->SecurityID);
    if (b == NULL) return hdr->BodyLen;

    b->orderAdd(msg->OrderDetails.Side,
		msg->OrderDetails.Price,
		msg->OrderDetails.DisplayQty);

    eventSide = b->decodeSide(msg->OrderDetails.Side);
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("ORDERADD: %ju,%c,%ju,%u",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    eventSide == BID ? 'B' : 'A',
	    msg->OrderDetails.Price,
	    msg->OrderDetails.DisplayQty
	    );
#endif
  }
    break;
    /* ------------------------------ */
  case TID_ORDERMODIFY : {
    OrderModifyT* msg = (OrderModifyT*) hdr;
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    if (b == NULL) return hdr->BodyLen;
    
    b->orderModify(msg->OrderDetails.Side,
		   msg->PrevPrice,
		   msg->PrevDisplayQty,
		   msg->OrderDetails.Price,
		   msg->OrderDetails.DisplayQty);
    eventSide = b->decodeSide(msg->OrderDetails.Side);    
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("ORDERMODIFY: %ju,%c,%ju,%u,%ju,%u",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    eventSide == BID ? 'B' : 'A',
	    msg->PrevPrice,
	    msg->PrevDisplayQty,
	    msg->OrderDetails.Price,
	    msg->OrderDetails.DisplayQty
	    );
#endif
  }
    break;
    /* ------------------------------ */
  case TID_ORDERMODIFYSAMEPRIO : {
    OrderModifySamePrioT* msg = (OrderModifySamePrioT*) hdr;
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    if (b == NULL) return hdr->BodyLen;
    
    b->orderModifySamePrio(msg->OrderDetails.Side,
			   msg->PrevDisplayQty,
			   msg->OrderDetails.Price,
			   msg->OrderDetails.DisplayQty);
    eventSide = b->decodeSide(msg->OrderDetails.Side);    
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("ORDERMODIFYSAMEPRIO: %ju,%c,%ju,%u,%u",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    eventSide == BID ? 'B' : 'A',
	    msg->OrderDetails.Price,
	    msg->PrevDisplayQty,
	    msg->OrderDetails.DisplayQty
	    );
#endif
  }
    break;
    /* ------------------------------ */
  case TID_ORDERDELETE : {
    OrderDeleteT* msg = (OrderDeleteT*) hdr;
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    if (b == NULL) return hdr->BodyLen;
    
    b->orderDelete(msg->OrderDetails.Side,
		   msg->OrderDetails.Price,
		   msg->OrderDetails.DisplayQty);
    eventSide = b->decodeSide(msg->OrderDetails.Side);   
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("ORDERDELETE: %ju,%c,%ju,%u",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    eventSide == BID ? 'B' : 'A',
	    msg->OrderDetails.Price,
	    msg->OrderDetails.DisplayQty
	    );
#endif
  }
    break;
    /* ------------------------------ */
  case TID_ORDERMASSDELETE : {
    OrderMassDeleteT* msg = (OrderMassDeleteT*) hdr;
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("ORDERMASSDELETE: %ju", static_cast<GlobalExchSecurityId>(msg->SecurityID));
#endif
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    if (b == NULL) return hdr->BodyLen;
    b->orderMassDelete();
    eventSide = BOTH;
  }
    break;
    /* ------------------------------ */
  case TID_PARTIALORDEREXECUTION : {
    PartialOrderExecutionT* msg = (PartialOrderExecutionT*) hdr;
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    if (b == NULL) return hdr->BodyLen;
    b->orderPartialExecution(msg->Side,
			     msg->Price,
			     msg->LastQty);
    eventSide = b->decodeSide(msg->Side);   
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("PARTIALORDEREXECUTION: %ju,%c,%ju,%u",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    eventSide == BID ? 'B' : 'A',
	    msg->Price,
	    msg->LastQty
	    );
#endif
  }
    break;
    /* ------------------------------ */
  case TID_FULLORDEREXECUTION : {
    FullOrderExecutionT* msg = (FullOrderExecutionT*) hdr;
    b = (EkaEobiBook*)strat->findBook(static_cast<GlobalExchSecurityId>(msg->SecurityID));
    if (b == NULL) return hdr->BodyLen;
    b->orderFullExecution(msg->Side,
			  msg->Price,
			  msg->LastQty);
    eventSide = b->decodeSide(msg->Side);   
#ifdef _EKA_PARSER_PRINT_ALL_
    EKA_LOG("FULLORDEREXECUTION: %ju,%c,%ju,%u",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    eventSide == BID ? 'B' : 'A',
	    msg->Price,
	    msg->LastQty
	    );
#endif 
  }
    break;
    /* ------------------------------ */

  case TID_HEARTBEAT :
  case TID_INSTRUMENTSTATECHANGE :
  case TID_PRODUCTSTATECHANGE :
  case TID_EXECUTIONSUMMARY :
  case TID_AUCTIONBBO :
  case TID_AUCTIONCLEARINGPRICE :
    return hdr->BodyLen;

  default:
    EKA_WARN("Unexpected Message %u",hdr->TemplateID);
    return hdr->BodyLen;
  }
  if (b == NULL) on_error("b == NULL");
#ifdef _EKA_PARSER_PRINT_ALL_ 
  b->printBook();
#endif 

  switch (eventSide) {
  case ASK : 
    if (b->testTobChange(ASK,mdOut->transactTime))
      exch->onTobChange(mdOut,b,ASK);
    break;
  case BID :
    if (b->testTobChange(BID,mdOut->transactTime))
      exch->onTobChange(mdOut,b,BID);
    break;
  case SIDE::BOTH :
    //    EKA_LOG("    exch->onTobChange(mdOut,b,ASK);");
    exch->onTobChange(mdOut,b,ASK);

    //    EKA_LOG("    exch->onTobChange(mdOut,b,BID);");
    exch->onTobChange(mdOut,b,BID);
    break;
  default:
    break;
  }

  return hdr->BodyLen;
}
//--------------------------------------------------------

inline uint EkaEobiParser::printPkt(uint8_t* pkt, 
				    uint pktPayloadSize) {
  if (pkt == NULL) on_error("pkt == NULL");

  uint8_t* p = pkt;
  uint8_t* pktEnd = pkt + pktPayloadSize;

  MessageHeaderCompT* hdr = (MessageHeaderCompT*) p;
  if (hdr->TemplateID != TID_PACKETHEADER) 
    on_error("TemplateID %u != TID_PACKETHEADER",hdr->TemplateID);

  printf ("%u: ",((PacketHeaderT*)hdr)->ApplSeqNum);

  p += hdr->BodyLen;

  while (p < pktEnd) {
    p += printMsg(p);
  }
  printf("\n");

  return pktPayloadSize;
}

//--------------------------------------------------------
inline char decodeSide4print(uint8_t _side) {
  if (_side == 1) return 'B';
  if (_side == 2) return 'A';
  on_error("unexpected side = 0x%x",_side);
}

//--------------------------------------------------------

uint EkaEobiParser::printMsg(uint8_t* msgStart) {
  MessageHeaderCompT* hdr = (MessageHeaderCompT*) msgStart;

  switch (hdr->TemplateID) {
    /* ------------------------------ */
  case TID_ORDERADD : {
    OrderAddT* msg = (OrderAddT*) hdr;
    printf("\tORDERADD: %ju,%c,%ju,%jd",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    decodeSide4print(msg->OrderDetails.Side),
	    msg->OrderDetails.Price,
	    msg->OrderDetails.DisplayQty
	    );
  }
    break;
    /* ------------------------------ */
  case TID_ORDERMODIFY : {
    OrderModifyT* msg = (OrderModifyT*) hdr;
    printf("\tORDERMODIFY: %ju,%c,%ju,%jd,%ju,%jd",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    decodeSide4print(msg->OrderDetails.Side),
	    msg->PrevPrice,
	    msg->PrevDisplayQty,
	    msg->OrderDetails.Price,
	    msg->OrderDetails.DisplayQty
	    );
  }
    break;
    /* ------------------------------ */
  case TID_ORDERMODIFYSAMEPRIO : {
    OrderModifySamePrioT* msg = (OrderModifySamePrioT*) hdr;
    printf("\tORDERMODIFYSAMEPRIO: %ju,%c,%ju,%jd,%jd",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    decodeSide4print(msg->OrderDetails.Side),
	    msg->OrderDetails.Price,
	    msg->PrevDisplayQty,
	    msg->OrderDetails.DisplayQty
	    );
  }
    break;
    /* ------------------------------ */
  case TID_ORDERDELETE : {
    OrderDeleteT* msg = (OrderDeleteT*) hdr;
    printf("\tORDERDELETE: %ju,%c,%ju,%jd",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    decodeSide4print(msg->OrderDetails.Side),
	    msg->OrderDetails.Price,
	    msg->OrderDetails.DisplayQty
	    );
  }
    break;
    /* ------------------------------ */
  case TID_ORDERMASSDELETE : {
    OrderMassDeleteT* msg = (OrderMassDeleteT*) hdr;
    printf("\tORDERMASSDELETE: %ju", static_cast<GlobalExchSecurityId>(msg->SecurityID));
  }
    break;
    /* ------------------------------ */
  case TID_PARTIALORDEREXECUTION : {
    PartialOrderExecutionT* msg = (PartialOrderExecutionT*) hdr;
    printf("\tPARTIALORDEREXECUTION: %ju,%c,%ju,%jd",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    decodeSide4print(msg->Side),
	    msg->Price,
	    msg->LastQty
	    );
  }
    break;
    /* ------------------------------ */
  case TID_FULLORDEREXECUTION : {
    FullOrderExecutionT* msg = (FullOrderExecutionT*) hdr;
    printf("\tFULLORDEREXECUTION: %ju,%c,%ju,%jd",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	    decodeSide4print(msg->Side),
	    msg->Price,
	    msg->LastQty
	    );
  }
    break;
    /* ------------------------------ */
  case TID_INSTRUMENTSTATECHANGE : {
    InstrumentStateChangeT*  msg = (InstrumentStateChangeT*) hdr;
    printf("\tINSTRUMENTSTATECHANGE: %ju,%s,%d,%d,%d,%d,%d,%d",
	    (static_cast<GlobalExchSecurityId>(msg->SecurityID)),
	   SecurityStatusDecode(msg->SecurityStatus),
	   msg->SecurityStatus,
	   msg->SecurityTradingStatus,
	   msg->MarketCondition,
	   msg->FastMarketIndicator,
	   msg->SecurityTradingEvent,
	   msg->SoldOutIndicator
	   );
  }
    break;
  case TID_PRODUCTSTATECHANGE : {
    ProductStateChangeT*  msg = (ProductStateChangeT*) hdr;
    printf("\tPRODUCTSTATECHANGE:TradSesStatus=%d, MarketCondition=%d,FastMarketIndicator=%d",
	   msg->TradSesStatus,
	   msg->MarketCondition,
	   msg->FastMarketIndicator
	   );
  }
    break;
  default:
    EKA_LOG("Unexpected Message %u",hdr->TemplateID);
  }
  return hdr->BodyLen;
}
//--------------------------------------------------------
