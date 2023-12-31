#include "EkaNyse.h"
#include "EkaNyseParser.h"
#include "EkaNyseBook.h"
#include "EkaStrat.h"

inline uint EkaNyseParser::processPkt(MdOut* mdOut, 
				      ProcessAction processAction, // not used
				      uint8_t* pkt, 
				      uint pktPayloadSize) {
  if (pkt == NULL) on_error("pkt == NULL");

  uint8_t* p = pkt;

  uint16_t pktSize = ((XdpPktHdr*)pkt)->PktSize;
  uint8_t  numMsgs = ((XdpPktHdr*)pkt)->NumberMsgs;

  if (pktPayloadSize != pktSize)
    on_error("pktPayloadSize (%u) != PktSize (%u)",pktPayloadSize,pktSize);

#ifdef _EKA_PARSER_PRINT_ALL_
  uint32_t seq     = ((XdpPktHdr*)pkt)->SeqNum;
  uint64_t ts      = ((XdpPktHdr*)pkt)->time.SourceTime * static_cast<uint64_t>(SEC_TO_NANO) + ((XdpPktHdr*)pkt)->time.SourceTimeNS;
  printf("%s,%u,%u,%u\n",ts_ns2str(ts).c_str(),seq,numMsgs,pktSize); fflush(stdout);
#endif

  p += sizeof(XdpPktHdr);

  for (uint i = 0; i < numMsgs; i++) {
    mdOut->book = NULL;
    uint16_t msgSize = ((XdpMsgHdr*)p)->MsgSize;
    uint16_t msgType = ((XdpMsgHdr*)p)->MsgType;
    EkaNyseBook* b = NULL;
    SIDE side = NONE;

#ifdef _EKA_PARSER_PRINT_ALL_
    printf("\t%s: ",msgType2str(msgType).c_str()); fflush(stdout);
#endif

    switch ((XDP_MSG_TYPE)msgType) {
      /* +++++++++++++++++++++++++++++++++++++++++ */
    case XDP_MSG_TYPE::AddOrder : {
      XdpAddOrder* msg = (XdpAddOrder*)p;
#ifdef _EKA_PARSER_PRINT_ALL_
      printf("%u,%c,%u,%u,%u",msg->SymbolIndex,msg->Side,msg->OrderID,msg->Price,msg->Volume);
#endif

      ExchSecurityId securityId = msg->SymbolIndex;

      b = reinterpret_cast<EkaNyseBook*>(strat->findBook(securityId));
      if (b == NULL) break;
      
      side      = decodeSide(msg->Side);
      Price price     = msg->Price; // DONT FORGET to ADD div PriceScale!!!
      Size  size      = msg->Volume;
      OrderID orderId = msg->OrderID;

      b->addOrder(orderId,side,price,size);
    }
      break;
      /* +++++++++++++++++++++++++++++++++++++++++ */
    case XDP_MSG_TYPE::ModifyOrder : {
      XdpModifyOrder* msg = (XdpModifyOrder*)p;
#ifdef _EKA_PARSER_PRINT_ALL_
      printf("%u,%c,%u,%u,%u",msg->SymbolIndex,msg->Side,msg->OrderID,msg->Price,msg->Volume);
#endif
      ExchSecurityId securityId = msg->SymbolIndex;

      b = reinterpret_cast<EkaNyseBook*>(strat->findBook(securityId));
      if (b == NULL) break;
      
      side      = decodeSide(msg->Side);
      Price price     = msg->Price; // DONT FORGET to ADD div PriceScale!!!
      Size  size      = msg->Volume;
      OrderID orderId = msg->OrderID;

      b->modifyOrder(orderId,side,price,size);
    }
      break;
      /* +++++++++++++++++++++++++++++++++++++++++ */
    case XDP_MSG_TYPE::DeleteOrder : {
      XdpDeleteOrder* msg = (XdpDeleteOrder*)p;
#ifdef _EKA_PARSER_PRINT_ALL_
      printf("%u,%c,%u",msg->SymbolIndex,msg->Side,msg->OrderID);
#endif
      ExchSecurityId securityId = msg->SymbolIndex;

      b = reinterpret_cast<EkaNyseBook*>(strat->findBook(securityId));
      if (b == NULL) break;
      
      side      = decodeSide(msg->Side);
      OrderID orderId = msg->OrderID;

      b->deleteOrder(orderId,side);
    }
      break;
    default:
      break;
    }
    //    if (b != NULL && b->pendingTob()) exch->onTobChange(mdOut,b,side);
    if (b != NULL) exch->onTobChange(mdOut,b,side);

    p += msgSize;
#ifdef _EKA_PARSER_PRINT_ALL_
    printf("\n");
#endif
  }
  if (pkt + pktPayloadSize != p)
    on_error("pkt (%p) + pktPayloadSize (%u) (=%ju) !== p (%p)",
	     pkt,pktPayloadSize,(uint64_t)pkt + pktPayloadSize, p);
  return pktPayloadSize;
}
