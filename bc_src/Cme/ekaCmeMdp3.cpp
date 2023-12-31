#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <assert.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>

#include "ekaCmeMdp3.h"
#include "eka_bc_book.h"
#include "eka_bc_data_structs.h"
#include "eka_bc.h"
#include "eka_macros.h"

void hexDump (char *desc, void *addr, int len);
CmeBook* findBook(EkaDev* pEkaDev,uint8_t coreId, uint32_t SecurityId);

//###################################################

static inline std::string events2str (uint8_t eventIndicator) {
  std::string events[8] = {
    "LastTradeMsg",
    "LastVolumeMsg",
    "LastQuoteMsg",
    "LastStatsMsg",
    "LastImpliedMsg",
    "RecoveryMsg",
    "Reserved",
    "EndOfEvent",
  };
  std::string ret = "";
  for (int i = 0; i < 8; i++) {
    if ((eventIndicator & (1 << i)) != 0) ret += events[i] + " ";
  }
  return ret;
}
//###################################################

static inline std::string ts_ns2str(uint64_t ts) {
  char dst[64] = {};
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;
  uint us = res % 1000;
  res = (res - us) / 1000;
  uint ms = res % 1000;
  res = (res - ms) / 1000;
  uint s = res % 60;
  res = (res - s) / 60;
  uint m = res % 60;
  res = (res - m) / 60;
  uint h = res % 24;
  sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns);
  return std::string(dst);
}

//###################################################
void MdEntryIncrementalRefreshBook46::print(uint num, uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  printf ("\n\t%u: price=%ju,size=%u,SecurityId=%u,%u,%u,priceLevel=%u,%s,%s\n",
	  num,
	  p->price,
	  p->size,
	  p->SecurityId,
	  p->seq,
	  p->numOrders,
	  p->priceLevel,
	  updateAction2str(p->MDUpdateAction),
	  entryTypeDecode(p->MDEntryType)
	  );
}
//###################################################
bool MdEntryIncrementalRefreshBook46::isBookUpdate(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  return (p->MDUpdateAction == (uint8_t)0 || p->MDUpdateAction == (uint8_t)1 || p->MDUpdateAction == (uint8_t)2);
}

//###################################################
uint32_t MdEntryIncrementalRefreshBook46::getSecurityId(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  return p->SecurityId;
}
//###################################################
uint64_t MdEntryIncrementalRefreshBook46::getPrice(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  return p->price;
}
//###################################################
uint32_t MdEntryIncrementalRefreshBook46::getSize(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  return p->size;
}
//###################################################
uint32_t MdEntryIncrementalRefreshBook46::getSequence(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  return p->seq;
}//###################################################

CmeBook::ACTION MdEntryIncrementalRefreshBook46::getAction(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  switch (p->MDUpdateAction) {
  case 0: return CmeBook::ACTION::NEW;
  case 1: return CmeBook::ACTION::CHANGE;
  case 2: return CmeBook::ACTION::DELETE;
  default: on_error("Unexpected CmeBook::ACTION %u",p->MDUpdateAction);
  }
  return CmeBook::ACTION::UNDEFINED;
}

//###################################################

//###################################################
CmeBook::SIDE MdEntryIncrementalRefreshBook46::getSide(uint8_t* msgPtr) {
  MsgLayout* p = (MsgLayout*) msgPtr;
  return p->MDEntryType == '0' ? CmeBook::SIDE::BID : CmeBook::SIDE::ASK;
}

//###################################################


//###################################################

int parseCmeMsg(EkaDev* dev, uint8_t coreId, Mdp3MsgHdr* m, bool printOutGlob, bool bookUpd) {
  bool printOut = false;
  uint16_t blockLen = m->blockLen;
  uint8_t entries = 0;
  uint8_t* p = (uint8_t*) m + sizeof(Mdp3MsgHdr);
  //  if (printOut) printf ("%s : ",printTemplateName(m->templateId));
  switch (m->templateId) {
    //-------------------------------------------
  case Mdp3TemplateId::MDIncrementalRefreshBook : { // id=46
    printOut = true;
    //    uint8_t MatchEventIndicator= ((RefreshBookRoot*)p)->MatchEventIndicator;
    uint64_t TransactTime = ((RefreshBookRoot*)p)->TransactTime;
    p += blockLen;

    blockLen = ((NumInGroup*)p)->blockLen;
    entries = ((NumInGroup*)p)->numInGroup;

    p += sizeof(NumInGroup);
    //    if (printOut) printf ("%s,%s, %u MdEntries:%c",ts_ns2str(TransactTime).c_str(),events2str(MatchEventIndicator).c_str(),entries, entries ? ' ' : '\n');

    for (uint i = 0; i < entries; i++) {
      //      if (printOut) MdEntryIncrementalRefreshBook46::print(i,p);
      /* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& */

      if (bookUpd && MdEntryIncrementalRefreshBook46::isBookUpdate(p)) {
	uint32_t mdSecurityId = MdEntryIncrementalRefreshBook46::getSecurityId(p);

	CmeBook* b = findBook(dev,(uint8_t)coreId,mdSecurityId);
	if (b != NULL) {
	  CmeBook::ACTION mdAction    = MdEntryIncrementalRefreshBook46::getAction(p);
	  CmeBook::SIDE   mdSide      = MdEntryIncrementalRefreshBook46::getSide(p);
	  uint64_t        mdPrice     = MdEntryIncrementalRefreshBook46::getPrice(p);
	  uint32_t        mdSize      = MdEntryIncrementalRefreshBook46::getSize(p);
	  uint32_t        mdSequence  = MdEntryIncrementalRefreshBook46::getSequence(p);

	  CmeBook::UPDATE_RESULT tobChanged = b->update(mdAction,mdSide,mdPrice,mdSize);
	  switch (tobChanged) {
	  case CmeBook::UPDATE_RESULT::EMPTY_BOOK :
	    mdPrice = 0;
	    mdSize  = 0;
	  case CmeBook::UPDATE_RESULT::TOB_UPDATE : {
	    hw_book_params_t tobParams = {};
	    b->generateTobUpdate(&tobParams.tob,mdSide,TransactTime,mdSequence,mdPrice,mdSize);
	    b->generateDepthUpdate(&tobParams.depth,HW_DEPTH_PRICES,mdSide);
	    eka_side_type_t hwSide = mdSide == CmeBook::SIDE::BID ? eka_side_type_t::BUY : eka_side_type_t::SELL;
	    eka_write_tob (dev,&tobParams,(eka_product_t)b->prodId,hwSide);
	  }
	    break;
	  case CmeBook::UPDATE_RESULT::NO_UPDATE :
	    break;
	  default:
	    on_error("Unexpected tobChanged %u",(uint)tobChanged);
	  }
	}
      }
    /* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& */
    p += blockLen;
  }
  printOut = false;

  blockLen = ((NumInGroup8*)p)->blockLen;
  entries = ((NumInGroup8*)p)->numInGroup;
  if (printOut) printf ("%u OrderEntries: ",entries);
  p += sizeof(NumInGroup8);
  for (uint i = 0; i < entries; i++) {
    if (printOut) printf ("%u: %ju,%ju,%u,%u,%s;",i,((OrderEntry*)p)->orderId,((OrderEntry*)p)->MdOrderPriority,((OrderEntry*)p)->Qty,((OrderEntry*)p)->RefId,orderUpdateAction2str(((OrderEntry*)p)->OrderUpdAction));
    p += blockLen;
  }

}
    break;
    //-------------------------------------------

  case Mdp3TemplateId::MDIncrementalRefreshOrderBook : { // id=47
    uint64_t TransactTime = ((RefreshBookRoot*)p)->TransactTime;
    uint8_t MatchEventIndicator= ((RefreshBookRoot*)p)->MatchEventIndicator;
    if (printOut) printf ("%ju,%s, ",TransactTime,events2str(MatchEventIndicator).c_str());
    p += blockLen;

    blockLen = ((NumInGroup*)p)->blockLen;
    entries = ((NumInGroup*)p)->numInGroup;
    if (printOut) printf ("%ju,%s\n",TransactTime,events2str(MatchEventIndicator).c_str());
    p += sizeof(NumInGroup);

    //    if (printOut) printf ("Group of %u entries, %u bytes each\n",entries,blockLen);
    for (uint i = 0; i < entries; i++) {
      if (printOut) printf ("\tMdEntry %u: %ju,orderPriority=%ju,price=%ju,%u,SecurityId=%u,%s,%s\n",
			    i,
			    ((MdEntryIncrementalRefreshOrderBook47*)p)->orderId,
			    ((MdEntryIncrementalRefreshOrderBook47*)p)->orderPriority,
			    ((MdEntryIncrementalRefreshOrderBook47*)p)->price,
			    ((MdEntryIncrementalRefreshOrderBook47*)p)->size,
			    ((MdEntryIncrementalRefreshOrderBook47*)p)->SecurityId,
			    updateAction2str(((MdEntryIncrementalRefreshOrderBook47*)p)->MDUpdateAction),
			    entryTypeDecode(((MdEntryIncrementalRefreshOrderBook47*)p)->MDEntryType)
			    );
      p += blockLen;
    }
  }
    break;
    //-------------------------------------------

  case Mdp3TemplateId::MDIncrementalRefreshTradeSummary : {  // id=48
    uint64_t TransactTime = ((RefreshBookRoot*)p)->TransactTime;
    uint8_t MatchEventIndicator= ((RefreshBookRoot*)p)->MatchEventIndicator;
    if (printOut) printf ("%ju,%s, ",TransactTime,events2str(MatchEventIndicator).c_str());
    p += blockLen;
    
    blockLen = ((NumInGroup*)p)->blockLen;
    entries = ((NumInGroup*)p)->numInGroup;
    if (printOut) printf ("%u MdEntries: ",entries);
    p += sizeof(NumInGroup);

    for (uint i = 0; i < entries; i++) {

      p += blockLen;
    }

    blockLen = ((NumInGroup8*)p)->blockLen;
    entries = ((NumInGroup8*)p)->numInGroup;
    if (printOut) printf ("%u OrderEntries: ",entries);
    p += sizeof(NumInGroup8);
    for (uint i = 0; i < entries; i++) {
      if (printOut) printf ("%u: %ju,%ju,%u,%u,%s;",
	      i,
	      ((OrderEntry*)p)->orderId,
	      ((OrderEntry*)p)->MdOrderPriority,
	      ((OrderEntry*)p)->Qty,
	      ((OrderEntry*)p)->RefId,
	      orderUpdateAction2str(((OrderEntry*)p)->OrderUpdAction)
	      );
      p += blockLen;
    }

  }
    break;
    //-------------------------------------------

  case Mdp3TemplateId::MDIncrementalRefreshVolume : { // id=37
    uint64_t TransactTime = ((RefreshBookRoot*)p)->TransactTime;
    uint8_t MatchEventIndicator= ((RefreshBookRoot*)p)->MatchEventIndicator;
    if (printOut) printf ("%ju,%s, ",TransactTime,events2str(MatchEventIndicator).c_str());
    p += blockLen;

    blockLen = ((NumInGroup*)p)->blockLen;
    entries = ((NumInGroup*)p)->numInGroup;
    if (printOut) printf ("%ju,%s\n",TransactTime,events2str(MatchEventIndicator).c_str());
    p += sizeof(NumInGroup);

    //    if (printOut) printf ("Group of %u entries, %u bytes each\n",entries,blockLen);
    for (uint i = 0; i < entries; i++) {
      if (printOut) printf ("\tMdEntry %u: %u,SecurityId=%u,seq=%u,%s,%s\n",
	      i,
	      ((MdEntryIncrementalRefreshVolume37*)p)->size,
	      ((MdEntryIncrementalRefreshVolume37*)p)->SecurityId,
	      ((MdEntryIncrementalRefreshVolume37*)p)->seq,
	      updateAction2str(((MdEntryIncrementalRefreshVolume37*)p)->MDUpdateAction),
	      entryTypeDecode(((MdEntryIncrementalRefreshVolume37*)p)->MDEntryType)
	      );
      p += blockLen;
    }
  }
    break;
    //-------------------------------------------

  case Mdp3TemplateId::MDIncrementalRefreshSessionStatistics : { // id=51
    uint64_t TransactTime = ((RefreshBookRoot*)p)->TransactTime;
    uint8_t MatchEventIndicator= ((RefreshBookRoot*)p)->MatchEventIndicator;
    if (printOut) printf ("%ju,%s, ",TransactTime,events2str(MatchEventIndicator).c_str());
    p += blockLen;

    blockLen = ((NumInGroup*)p)->blockLen;
    entries = ((NumInGroup*)p)->numInGroup;
    if (printOut) printf ("%ju,%s\n",TransactTime,events2str(MatchEventIndicator).c_str());
    p += sizeof(NumInGroup);

    if (printOut) printf ("Group of %u entries, %u bytes each\n",entries,blockLen);
    for (uint i = 0; i < entries; i++) {
      if (printOut) printf ("\tMdEntry %u: %ju,SecurityId=%u,%u,%u,%s,%s,%u\n",
	      i,
	      ((MdEntryIncrementalRefreshSessionStatistics51*)p)->price,
	      ((MdEntryIncrementalRefreshSessionStatistics51*)p)->SecurityId,
	      ((MdEntryIncrementalRefreshSessionStatistics51*)p)->seq,
	      ((MdEntryIncrementalRefreshSessionStatistics51*)p)->OpenCloseSettlFlag,
	      updateAction2str(((MdEntryIncrementalRefreshSessionStatistics51*)p)->MDUpdateAction),
	      entryTypeDecode(((MdEntryIncrementalRefreshSessionStatistics51*)p)->MDEntryType),
	      ((MdEntryIncrementalRefreshSessionStatistics51*)p)->MDEntrySize
	      );
      p += blockLen;
    }
  }
    break;
    //-------------------------------------------
  default :
    return -1;
    break;
  }
  if (printOut) printf ("\n");
  return (p - (uint8_t*)m);
}

