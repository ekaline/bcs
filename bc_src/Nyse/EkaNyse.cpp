#include "EkaNyse.h"
#include "EkaNyseParser.h"
#include "EkaNyseBook.h"
#include "EkaNyseBookSide.h"
#include "EkaStrat.h"

#include "EpmIlink3CancelOrderTemplate.h"
#include "EpmIlink3NewOrderTemplate.h" //PATCH
#include "EkaEpmAction.h"

class EkaDev;
class EkaProd;

int ekaWriteTob(EkaDev* dev, void* params, uint paramsSize, eka_product_t product_id, eka_side_type_t side);
uint32_t deltaTcpCsum(unsigned short *ptr,int nbytes);

/* ----------------------------------------------------- */
EkaNyse::EkaNyse(EkaDev* _dev) : EkaExch(_dev) {
  EKA_LOG("Opening NYSE Exch");
}
/* ----------------------------------------------------- */
EkaParser* EkaNyse::newParser(EkaStrat* _strat) {
  EkaNyseParser* p = new EkaNyseParser(this, _strat);
  if (p == NULL) on_error("new EkaNyseParser == NULL");
  p->strat = _strat;
  return (EkaParser*)p;
}

/* ----------------------------------------------------- */

EkaBook*  EkaNyse::newBook(uint8_t coreId,EkaStrat* _strat, EkaProd* _prod) {
  return (EkaBook*) (new EkaNyseBook(dev,_strat,coreId,_prod));
}

/* ----------------------------------------------------- */
inline uint16_t price2Norm(uint64_t _price, uint64_t step,uint64_t bottom) {
  if (_price == 0) return 0;
  if (unlikely(_price < bottom)) 
    on_error("price %ju < bottom %ju",_price,bottom);
  return (_price - bottom) / step;
}

/* ----------------------------------------------------- */

inline void getDepthData(EkaBook* b, SIDE side, EkaNyse::NyseDepthParams* dst, uint depth) {
  uint32_t accSize = 0;
  for (uint i = 0; i < depth; i ++) {
    dst->entry[i].price = b->getEntryPrice(side,i);
    accSize            += b->getEntrySize(side,i);
    dst->entry[i].size  = accSize;
  }
}

/* ----------------------------------------------------- */

int EkaNyse::onTobChange(MdOut* mdOut, EkaBook* b, SIDE side) {
  NyseHwBookParams tobParams = {};

  EkaProd* prod = b->prod;

  uint64_t tobPrice = static_cast<uint64_t>(b->getEntryPrice(side,0));
  uint64_t step = prod->getStep();
  uint64_t bottom = prod->getBottom();

  tobParams.tob.last_transacttime = mdOut->transactTime;
  tobParams.tob.price             = tobPrice;
  tobParams.tob.size              = b->getEntrySize (side,0);
  tobParams.tob.msg_seq_num       = mdOut->sequence;

  tobParams.tob.normprice         = price2Norm(tobPrice,step,bottom);

  tobParams.tob.firePrice = tobPrice;
  
  uint64_t betterPrice = tobPrice == 0 ? 0 :
    side == SIDE::BID ? tobPrice + step : tobPrice - step;

  tobParams.tob.fireBetterPrice = betterPrice;

  getDepthData (b, side,&tobParams.depth,HW_DEPTH_PRICES);

#ifdef PCAP_TEST
  printf("==================================\nBOOK WRITE TO FPGA:\n");
  //  if (dev->tobCnt++ > 500000)  {
  //    ((EkaNyseBook*)b)->printBook();
    ((EkaNyseBook*)b)->checkIntegrity();
    if (side == BID) ((EkaNyseBook*)b)->bid->printSide();
    if (side == ASK) ((EkaNyseBook*)b)->ask->printSide();
    //  }
#else
    //  eka_side_type_t hwSide = side == BID ? eka_side_type_t::BUY : eka_side_type_t::SELL;
  if (side == BID)
    ekaWriteTob(mdOut->dev,&tobParams,sizeof(NyseHwBookParams),(eka_product_t)b->getProdId(),eka_side_type_t::BUY);
  if (side == ASK)
    ekaWriteTob(mdOut->dev,&tobParams,sizeof(NyseHwBookParams),(eka_product_t)b->getProdId(),eka_side_type_t::SELL);

#endif

 return 0;
}

int EkaNyse::ekaDownloadProductParams(eka_product_t product_id) {
  uint sw_product_index = fireLogic->findProd(product_id,__func__);
  uint hw_product_index = fireLogic->findProdHw(product_id,__func__);

  if (hw_product_index >= EKA_MAX_BOOK_PRODUCTS) 
    on_error ("jump strategy is supported only on book product (not on %d)",(int)hw_product_index);

  EkaProd* prod = fireLogic->prod[sw_product_index];
  if (! prod->is_book) return 1;

  volatile struct hw_product_param_entry_t product_param = {};

  product_param.max_book_spread = (uint8_t)prod->max_book_spread;
  product_param.buy_size        = (uint8_t)prod->buy_size;
  product_param.sell_size       = (uint8_t)prod->sell_size;
  product_param.step            = prod->hwstep;
  product_param.ei_if           = prod->ei_if;
  product_param.ei_session      = prod->ei_session;

  //  product_param.ei_seq_num      = prod->ei_seq_num;
  //  uint642ascii8((char *)&product_param.ei_seq_num,prod->ei_seq_num);  not in eobi

  if (product_param.buy_size > 15 || product_param.sell_size > 15) 
    on_error("too high buy_size %u or sell_size %u",product_param.buy_size, product_param.sell_size);

  //  EKA_LOG("product_id = %s (%u), seq = 0x%jx", EKA_RESOLVE_PROD(dev->hwFeedVer,product_id),product_id,product_param.ei_seq_num);

  uint64_t base_addr = 0x83000;
  uint64_t* wr_ptr = (uint64_t*) &product_param;
  int iter = sizeof(struct hw_product_param_entry_t)/8 + !!(sizeof(struct hw_product_param_entry_t)%8); // num of 8Byte words
  for (int z=0; z<iter; z++) {
    //    printf ("%s: writing 0x%016jx to 0x%016jx\n",__func__, *wr_ptr,base_addr+hw_product_index*iter*8+z*8);
    eka_write(dev,base_addr+hw_product_index*iter*8+z*8,*wr_ptr++); //data
  }

  /* printf ("%s: for product %d, (sw:%d) (hw:%d)\n",__func__, (int)product_id,sw_product_index,hw_product_index); */
  /* printf ("%s: dev midpoint %u hw midpint %u 0x%x\n",__func__,prod->midpoint,product_param.midpoint,product_param.midpoint); */
  /* printf ("%s: iter %d sizeofstruct %lu sizeofproduct_param %lu\n",__func__,iter,sizeof(struct hw_product_param_entry_t),sizeof(product_param)); */

  return 0;

}

/* ----------------------------------------------------- */
template <class O, class I> inline O normalizePriceDelta(I price) {
  if (price == (I)(-1))  return (O)-1;
  return (O)(price);
}

/* ----------------------------------------------------- */
template <class O, class I> inline O normalizeSize(I size) {
  if (size == (I)(-1))  return (O)-1;
  return (O)(size);
}

/* ----------------------------------------------------- */
template <class O, class I> inline O normalizeTimeDelta(I timeDelta) {
  if (timeDelta == (I)(-1))  return (O)-1;

  if (timeDelta % 1000)      on_error ("%s: time delta must have whole number of microseconds, (not %ju nanoseconds) -> ",__func__,timeDelta);
  if (timeDelta > 64000000)  on_error ("%s: time delta is limited to 64ms (not %ju ns) -> ",__func__,timeDelta);

  return (O)(timeDelta/1000);
}

int EkaNyse::jumpParamsApi2Internal(const jump_params_t* apiParams, volatile void* _dst) {
  volatile hw_jump_params_t* dst = (volatile hw_jump_params_t*)_dst;
  dst->bit_params = ((uint8_t)apiParams->strat_en) << BP_ENABLE;
  if (! apiParams->strat_en) return 1;

  dst->bit_params |= ((uint8_t)apiParams->boc_en) << BP_BOC;

  dst->min_price_delta = normalizePriceDelta <decltype(dst->min_price_delta),decltype(apiParams->min_price_delta)> (apiParams->min_price_delta);
  dst->min_ticker_size = normalizeSize       <decltype(dst->min_ticker_size),decltype(apiParams->min_ticker_size)> (apiParams->min_ticker_size);
  dst->max_post_size   = normalizeSize       <decltype(dst->max_post_size)  ,decltype(apiParams->max_post_size)>   (apiParams->max_post_size  );
  dst->max_tob_size    = normalizeSize       <decltype(dst->max_tob_size)   ,decltype(apiParams->max_tob_size)>    (apiParams->max_tob_size   );
  dst->min_tob_size    = normalizeSize       <decltype(dst->min_tob_size)   ,decltype(apiParams->min_tob_size)   > (apiParams->min_tob_size   );
  dst->buy_size        = normalizeSize       <decltype(dst->buy_size)       ,decltype(apiParams->buy_size)>        (apiParams->buy_size       );
  dst->sell_size       = normalizeSize       <decltype(dst->sell_size)      ,decltype(apiParams->sell_size)>       (apiParams->sell_size      );  

  return 1;
}

int EkaNyse::rjumpParamsApi2Internal(const reference_jump_params_t* apiParams, volatile void* _dst) {
  volatile hw_rjump_params_t* dst = (volatile hw_rjump_params_t*)_dst;
  dst->bit_params = ((uint8_t)apiParams->strat_en) << BP_ENABLE;
  if (! apiParams->strat_en) return 1;

  dst->bit_params |= ((uint8_t)apiParams->boc_en) << BP_BOC;

  dst->time_delta_ns        = normalizeTimeDelta  <decltype(dst->time_delta_ns)       ,decltype(apiParams->time_delta_ns         )> (apiParams->time_delta_ns         );
  dst->tickersize_lots      = normalizeSize       <decltype(dst->tickersize_lots)     ,decltype(apiParams->tickersize_lots       )> (apiParams->tickersize_lots       );
  dst->min_tob_size         = normalizeSize       <decltype(dst->min_tob_size)        ,decltype(apiParams->min_tob_size          )> (apiParams->min_tob_size          );
  dst->max_tob_size         = normalizeSize       <decltype(dst->max_tob_size)        ,decltype(apiParams->max_tob_size          )> (apiParams->max_tob_size          );
  dst->max_opposit_tob_size = normalizeSize       <decltype(dst->max_opposit_tob_size),decltype(apiParams->max_opposit_tob_size  )> (apiParams->max_opposit_tob_size  );
  dst->min_spread           = normalizeSize       <decltype(dst->min_spread)          ,decltype(apiParams->min_spread            )> (apiParams->min_spread            );
  dst->buy_size             = normalizeSize       <decltype(dst->buy_size)            ,decltype(apiParams->buy_size              )> (apiParams->buy_size              );
  dst->sell_size            = normalizeSize       <decltype(dst->sell_size)           ,decltype(apiParams->sell_size             )> (apiParams->sell_size             );

  return 1;
}

int EkaNyse::sendDate2Hw(EkaDev* dev) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); 
  uint64_t current_time_ns = ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 + (uint64_t)(t.tv_nsec)); //UTC
  //  current_time_ns += static_cast<uint64_t>(6*60*60) * static_cast<uint64_t>(1e9); //+6h UTC time
  int current_time_seconds = current_time_ns/(1000*1000*1000);
  time_t tmp = current_time_seconds;
  struct tm lt;
  (void) localtime_r(&tmp, &lt);
  char result[32] = {};
  strftime(result, sizeof(result), "%Y%m%d-%H:%M:%S.000", &lt); //20191206-20:17:32.131 
  uint64_t* wr_ptr = (uint64_t*) &result;
  for (int z=0; z<3; z++) {
    eka_write(dev,0xf0300+z*8,*wr_ptr++); //data, ascii, last write commits the change
  }
  return 1;
}
/* ----------------------------------------------------- */

EpmTemplate* EkaNyse::addCancelMsgTemplate(uint templateId) {
  return new EpmIlink3CancelOrderTemplate(templateId,"EpmIlink3CancelOrderTemplate"); //PATCH
}
/* ----------------------------------------------------- */
EpmTemplate* EkaNyse::addNewOrderMsgTemplate(uint templateId) {
  return new EpmIlink3NewOrderTemplate(templateId,"EpmIlink3NewOrderTemplate"); //PATCH
}
/* ----------------------------------------------------- */

int EkaNyse::createCancelMsg(uint8_t* dst, void* params) {
  
  memcpy(dst,params,sizeof(Ilink3OrderCancelRequestT));

  Ilink3OrderCancelRequestT* cancelMsg = (Ilink3OrderCancelRequestT*)dst;
  
  cancelMsg->OrderID = 0;
  cancelMsg->SeqNum = 0;
  memset(cancelMsg->CIOrdID,0,sizeof(cancelMsg->CIOrdID));
  cancelMsg->OrderRequestID = 0;
  cancelMsg->SendingTimeEpoch = 0;
  cancelMsg->Side = 0;

  return sizeof(Ilink3OrderCancelRequestT);
}
/* ----------------------------------------------------- */

int EkaNyse::createNewOrder(uint8_t* dst, void* params) {
  
  memcpy(dst,params,sizeof(Ilink3NewOrderT));

  Ilink3NewOrderT* newOrder = (Ilink3NewOrderT*)dst;
  
  newOrder->Price = 0;
  newOrder->OrderQty = 0;
  newOrder->Side = 0;
  newOrder->SeqNum = 0;
  memset(newOrder->CIOrdID,0,sizeof(newOrder->CIOrdID));
  newOrder->OrderRequestID = 0;
  newOrder->SendingTimeEpoch = 0;

  return sizeof(Ilink3NewOrderT);
}
/* ----------------------------------------------------- */

int EkaNyse::updateCancelMsg(uint prodIdx, uint8_t* msg, void* params) {
  EkaProd* prod = dev->exch->cancelMsgs->prod[prodIdx];
  if (prod == NULL) on_error("prod[%u] == NULL",prodIdx);

  uint8_t* payload = msg + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr);

  //update SW critical fields
  ((Ilink3OrderCancelRequestT*)payload)->OrderID = ((EpmIlink3CancelCriticalParams*)params)->OrderID;  
  memcpy(((Ilink3OrderCancelRequestT*)payload)->CIOrdID,((EpmIlink3CancelCriticalParams*)params)->CIOrdID,sizeof(((EpmIlink3CancelCriticalParams*)params)->CIOrdID));
  ((Ilink3OrderCancelRequestT*)payload)->OrderRequestID    = ((EpmIlink3CancelCriticalParams*)params)->OrderRequestID;  
  ((Ilink3OrderCancelRequestT*)payload)->Side    = ((EpmIlink3CancelCriticalParams*)params)->Side;  


  //update checksum
  prod->Csum4trigger = prod->cancelMsgChckSum;

  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3OrderCancelRequestT*)payload)->OrderID,
						   sizeof(((Ilink3OrderCancelRequestT*)payload)->OrderID));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3OrderCancelRequestT*)payload)->CIOrdID,
						   sizeof(((Ilink3OrderCancelRequestT*)payload)->CIOrdID));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3OrderCancelRequestT*)payload)->OrderRequestID,
						   sizeof(((Ilink3OrderCancelRequestT*)payload)->OrderRequestID));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3OrderCancelRequestT*)payload)->Side,
						   sizeof(((Ilink3OrderCancelRequestT*)payload)->Side));


  //update HW OrderID
  // common to all updates
  uint8_t  threadId = 0;
  uint64_t dstLogicalAddr = prod->cancelAction->heapAddr;
  uint64_t firstFieldOffs;
  bool notAligned;
  uint updateSize;
  uint64_t srcWordOffs;

  // field 1
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3OrderCancelRequestT*)payload)->OrderID - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3OrderCancelRequestT*)payload)->OrderID) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  EKA_LOG (">>>>>>>>>>>>>> OrderID = 0x%jx",((Ilink3OrderCancelRequestT*)payload)->OrderID);

  // field 2
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3OrderCancelRequestT*)payload)->CIOrdID - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3OrderCancelRequestT*)payload)->CIOrdID) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  hexDump(">>>>>>>>>>>>>> ClOrdId", ((Ilink3OrderCancelRequestT*)payload)->CIOrdID, sizeof(((Ilink3OrderCancelRequestT*)payload)->CIOrdID));

  // field 3
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3OrderCancelRequestT*)payload)->OrderRequestID - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3OrderCancelRequestT*)payload)->OrderRequestID) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  EKA_LOG (">>>>>>>>>>>>>> OrderRequestID = 0x%jx",((Ilink3OrderCancelRequestT*)payload)->OrderRequestID);

  // field 4
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3OrderCancelRequestT*)payload)->Side - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3OrderCancelRequestT*)payload)->Side) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  EKA_LOG (">>>>>>>>>>>>>> Side = 0x%jx",((Ilink3OrderCancelRequestT*)payload)->Side);

  return 0;
}

int EkaNyse::updateNewOrder(uint prodIdx, uint8_t* msg, void* params) {
  EkaProd* prod = dev->exch->fireLogic->prod[prodIdx];
  if (prod == NULL) on_error("prod[%u] == NULL",prodIdx);
  if (prod->newOrderAction == NULL) on_error("prod->newOrderAction == NULL");
  uint8_t* payload = msg + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr);

  //update SW critical fields
  ((Ilink3NewOrderT*)payload)->Price    = ((EpmIlink3NewOrderCriticalParams*)params)->Price;  
  ((Ilink3NewOrderT*)payload)->OrderQty = ((EpmIlink3NewOrderCriticalParams*)params)->OrderQty;
  ((Ilink3NewOrderT*)payload)->Side     = ((EpmIlink3NewOrderCriticalParams*)params)->Side;  
  memcpy(((Ilink3NewOrderT*)payload)->CIOrdID,((EpmIlink3NewOrderCriticalParams*)params)->CIOrdID,sizeof(((EpmIlink3NewOrderCriticalParams*)params)->CIOrdID));
  ((Ilink3NewOrderT*)payload)->OrderRequestID    = ((EpmIlink3NewOrderCriticalParams*)params)->OrderRequestID;  

  //update checksum
  prod->Csum4trigger = prod->newOrderChckSum;

  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3NewOrderT*)payload)->Price,
						   sizeof(((Ilink3NewOrderT*)payload)->Price));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3NewOrderT*)payload)->OrderQty,
						   sizeof(((Ilink3NewOrderT*)payload)->OrderQty));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3NewOrderT*)payload)->Side,
						   sizeof(((Ilink3NewOrderT*)payload)->Side));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3NewOrderT*)payload)->CIOrdID,
						   sizeof(((Ilink3NewOrderT*)payload)->CIOrdID));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((Ilink3NewOrderT*)payload)->OrderRequestID,
						   sizeof(((Ilink3NewOrderT*)payload)->OrderRequestID));


  //update HW Price
  // common to all updates
  uint8_t  threadId = 0;
  uint64_t dstLogicalAddr = prod->newOrderAction->heapAddr;
  uint64_t firstFieldOffs;
  bool notAligned;
  uint updateSize;
  uint64_t srcWordOffs;

  // field 1
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3NewOrderT*)payload)->Price - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3NewOrderT*)payload)->Price) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  EKA_LOG (">>>>>>>>>>>>>> Price = 0x%jx",((Ilink3NewOrderT*)payload)->Price);

  // field 2
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3NewOrderT*)payload)->OrderQty - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3NewOrderT*)payload)->OrderQty) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  hexDump(">>>>>>>>>>>>>> OrderQty", ((Ilink3NewOrderT*)payload)->OrderQty, sizeof(((Ilink3NewOrderT*)payload)->OrderQty));

  // field 3
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3NewOrderT*)payload)->Side - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3NewOrderT*)payload)->Side) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  EKA_LOG (">>>>>>>>>>>>>> Side = 0x%jx",((Ilink3NewOrderT*)payload)->Side);

  // field 4
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3NewOrderT*)payload)->OrderRequestID - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3NewOrderT*)payload)->OrderRequestID) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  EKA_LOG (">>>>>>>>>>>>>> OrderRequestID = 0x%jx",((Ilink3OrderCancelRequestT*)payload)->OrderRequestID);

  // field 5
  firstFieldOffs = (uint64_t)((uint8_t*)&((Ilink3NewOrderT*)payload)->CIOrdID - msg);
  notAligned     = firstFieldOffs % 8 != 0;
  updateSize     = 
    sizeof(((Ilink3NewOrderT*)payload)->CIOrdID) +
    8 * notAligned;
  srcWordOffs = firstFieldOffs / 8 * 8;
  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 
  //  hexDump(">>>>>>>>>>>>>> ClOrdId", ((Ilink3OrderCancelRequestT*)payload)->CIOrdID, sizeof(((Ilink3OrderCancelRequestT*)payload)->CIOrdID));

  return 0;
}
