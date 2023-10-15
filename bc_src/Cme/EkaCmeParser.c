#include "EkaCme.h"
#include "EkaCmeParser.h"
#include "EkaCmeBook.h"
#include "EkaStrat.h"

/* inline static void hexDump (const char *desc, void *addr, int len) { */
/*     int i; */
/*     unsigned char buff[17]; */
/*     unsigned char *pc = (unsigned char*)addr; */
/*     if (desc != NULL) printf ("%s:\n", desc); */
/*     if (len == 0) { printf("  ZERO LENGTH\n"); return; } */
/*     if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; } */
/*     for (i = 0; i < len; i++) { */
/*         if ((i % 16) == 0) { */
/*             if (i != 0) printf ("  %s\n", buff); */
/*             printf ("  %04x ", i); */
/*         } */
/*         printf (" %02x", pc[i]); */
/*         if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.'; */
/*         else buff[i % 16] = pc[i]; */
/*         buff[(i % 16) + 1] = '\0'; */
/*     } */
/*     while ((i % 16) != 0) { printf ("   "); i++; } */
/*     printf ("  %s\n", buff); */
/* } */

inline uint EkaCmeParser::processPkt(MdOut* mdOut, 
				     ProcessAction processAction, 
				     uint8_t* pkt, 
				     uint pktPayloadSize) {
  if (pkt == NULL) on_error("pkt == NULL");

  uint8_t* p = pkt;
  uint8_t* pktEnd = pkt + pktPayloadSize;

  p += processPktHdr(mdOut,p);
  mdOut->msgNum = 1;
  while (p < pktEnd) {
    mdOut->book = NULL;
    mdOut->lastEvent = false;
    p += processMsg(mdOut,processAction,p);

    EkaCmeBook* b = static_cast<EkaCmeBook*>(mdOut->book);
    if (b != NULL && mdOut->lastEvent) {
      if(b->hasPendingTOB(BID)) exch->onTobChange(mdOut,b,BID);
      if(b->hasPendingTOB(ASK)) exch->onTobChange(mdOut,b,ASK);
      b->clearPendingTOB();
    }

  }

  return pktPayloadSize;
}

  //--------------------------------------------------------

inline uint EkaCmeParser::processPktHdr(MdOut* mdOut, uint8_t* pktStart) {
  mdOut->transactTime = ((PktHdrLayout*)pktStart)->time;
  mdOut->pktSeq       = ((PktHdrLayout*)pktStart)->seq;
#ifdef _EKA_PARSER_PRINT_ALL_
  printf("transactTime = %ju (%s), pktSeq = %u\n",mdOut->transactTime,ns2str(mdOut->transactTime).c_str(),mdOut->pktSeq);
#endif
  return sizeof(PktHdrLayout);
}
  //--------------------------------------------------------

inline uint EkaCmeParser::processMsg(MdOut* mdOut, ProcessAction processAction, uint8_t* msgStart) {
  uint      msgSize = getMsgSize (msgStart);
  MsgId          id = getMsgId   (msgStart);
  uint16_t blockLen = getBlockLen(msgStart);
  if (msgSize == 0) on_error ("msgSize = 0");
  
  uint8_t* p = msgStart + sizeof(MsgHdrLayout);
#ifdef _EKA_PARSER_PRINT_ALL_
  //  if (unlikely(processAction == ProcessAction::PrintOnly || processAction == ProcessAction::PrintAndUpdateBook)) {
  //printf ("MsgId = %u: ",(uint)id);
    //  }
#endif
  switch (id) {
  case MsgId::MDIncrementalRefreshBook :
    processMDIncrementalRefreshBook46(mdOut,processAction,p,blockLen);
    break;
#ifdef _EKA_PARSER_PRINT_ALL_
  case MsgId::MDIncrementalRefreshTradeSummary : // 48
    processMDIncrementalRefreshTradeSummary48(mdOut,processAction,p,blockLen);
    break;
#endif
  default:
#ifdef _EKA_PARSER_PRINT_ALL_
    printf ("\n");
#endif
    break;
  }
  return msgSize;
}
//--------------------------------------------------------


inline uint EkaCmeParser::processMDIncrementalRefreshTradeSummary48(MdOut* mdOut, 
								    ProcessAction processAction, 
								    uint8_t* msgStart, 
								    uint16_t rootBlockLen) {
  if (msgStart == NULL) on_error("msgStart == NULL");
  if (mdOut == NULL) on_error("mdOut == NULL");
  uint8_t* p = msgStart;
  //  mdOut->lastEvent = ((RefreshBookRoot*)p)->MatchEventIndicator & 0x80; // bit 7 is ON
  mdOut->lastEvent = ((RefreshBookRoot*)p)->MatchEventIndicator & 0x01; // bit 2 "LastTradeMsg" is ON
  uint8_t lastEventRaw = ((RefreshBookRoot*)p)->MatchEventIndicator;
  p += rootBlockLen;

  //  uint16_t blockLen = ((GroupSize*)p)->blockLen;
  uint8_t   entries = ((GroupSize*)p)->numInGroup;
  printf("lastEvent = %u (0x%02x), entries = %u\n",mdOut->lastEvent,lastEventRaw,entries);

  p += sizeof(GroupSize);
  for (uint i = 0; i < entries; i++) {
    EkaCmeBook* b = static_cast<EkaCmeBook*>(strat->findBook(static_cast<GlobalExchSecurityId>(getTickerSecurityId(p))));
    if (b != NULL) {
      mdOut->book = static_cast<EkaBook*>(b);
      mdOut->sequence   = getTickerSequence(p);
      printf("\t%ju: MDIncrementalRefreshTradeSummary48:    %8ju, %c,(0x%x), 0x%x, 0x%x, %8ju, %u\n",
	     mdOut->sequence,
	     b->getSecurityId(),
	     getTickerSide(p) == SIDE::BID ? 'B' :  getTickerSide(p) == SIDE::ASK ? 'A' : 'X',
	     ((TickerEntryLayout*)p)->AggressorSide,
	     ((TickerEntryLayout*)p)->MDUpdateAction,
	     ((TickerEntryLayout*)p)->MDEntryType,
	     getTickerPrice(p),
	     getTickerSize(p));
    }
  }
  return 0;
}

//--------------------------------------------------------


inline uint EkaCmeParser::processMDIncrementalRefreshBook46(MdOut* mdOut, 
							    ProcessAction processAction, 
							    uint8_t* msgStart, 
							    uint16_t rootBlockLen) {
  if (msgStart == NULL) on_error("msgStart == NULL");

  if (mdOut == NULL) on_error("mdOut == NULL");
  uint8_t* p = msgStart;
  //  mdOut->lastEvent = ((RefreshBookRoot*)p)->MatchEventIndicator & 0x80; // bit 7 is ON
  mdOut->lastEvent = ((RefreshBookRoot*)p)->MatchEventIndicator & 0x04; // bit 2 "LastQuoteMsg" is ON

#ifdef _EKA_PARSER_PRINT_ALL_
  uint8_t lastEventRaw = ((RefreshBookRoot*)p)->MatchEventIndicator;
#endif
  p += rootBlockLen;
  uint16_t blockLen = ((GroupSize*)p)->blockLen;
  uint8_t   entries = ((GroupSize*)p)->numInGroup;
#ifdef _EKA_PARSER_PRINT_ALL_
  printf("lastEvent = %u (0x%02x), entries = %u\n",mdOut->lastEvent,lastEventRaw,entries);
#endif
  p += sizeof(GroupSize);
  for (uint i = 0; i < entries; i++) {
    //	printFields();

    EkaCmeBook* b = static_cast<EkaCmeBook*>(strat->findBook(static_cast<GlobalExchSecurityId>(getSecurityId(p))));
    if (b != NULL) {
      mdOut->book = static_cast<EkaBook*>(b);
      mdOut->sequence   = getSequence(p);

      switch (getAction(p)) {
      case ACTION::NEW:
#ifdef _EKA_PARSER_PRINT_ALL_
	printf("\t%ju: IncrementalRefreshBook46:    newPlevel: %8ju, %c,(%c), %2u, %8ju, %u\n",
	       mdOut->sequence,	       
	       b->getSecurityId(),
	       getSide(p) == SIDE::BID ? 'B' :  getSide(p) == SIDE::ASK ? 'A' : 'X',
	       ((MdEntryLayout*) p)->MDEntryType,
	       getPriceLevel(p),
	       getPrice(p),
	       getSize(p));
#endif
	b->newPlevel(mdOut,
		     getSide(p),
		     getPriceLevel(p),
		     getPrice(p),
		     getSize(p));
	break;
      case ACTION::CHANGE:
#ifdef _EKA_PARSER_PRINT_ALL_
	printf("\t%ju: IncrementalRefreshBook46: changePlevel: %8ju, %c,(%c), %2u, %8ju, %u\n",
	       mdOut->sequence,	       
	       b->getSecurityId(),
	       getSide(p) == SIDE::BID ? 'B' :  getSide(p) == SIDE::ASK ? 'A' : 'X',
	       ((MdEntryLayout*) p)->MDEntryType,
	       getPriceLevel(p),
	       getPrice(p),
	       getSize(p));
#endif
	b->changePlevel(mdOut,
			getSide(p),
			getPriceLevel(p),
			getPrice(p),
			getSize(p));
	break;
      case ACTION::DELETE :
#ifdef _EKA_PARSER_PRINT_ALL_
	printf("\t%ju: IncrementalRefreshBook46: deletePlevel: %8ju, %c,(%c), %2u\n",
	       mdOut->sequence,	       
	       b->getSecurityId(),
	       getSide(p) == SIDE::BID ? 'B' :  getSide(p) == SIDE::ASK ? 'A' : 'X',
	       ((MdEntryLayout*) p)->MDEntryType,
	       getPriceLevel(p));
#endif
	b->deletePlevel(mdOut,
			getSide(p),
			getPriceLevel(p));
	break;
      default:
	on_error("Unexpeted ACTION %u",(uint)getAction(p));
      }
    }
    p += blockLen;
  }
  return (p - msgStart);
}
