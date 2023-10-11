#include "EkaEobi.h"
#include "EkaEobiParser.h"
#include "EkaEobiBook.h"
#include "EkaStrat.h"
#include "EkaEpm.h"
#include "EpmEti8PktTemplate.h"
#include "EkaEpmAction.h"

class EkaDev;
class EkaProd;

int      ekaWriteTob(EkaDev* dev, void* params, uint paramsSize, eka_product_t product_id, eka_side_type_t side);
uint32_t deltaTcpCsum(unsigned short *ptr,int nbytes);
uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);

/* ----------------------------------------------------- */
EkaEobi::EkaEobi(EkaDev* _dev/* , uint8_t _coreId */) : EkaExch(_dev/* ,_coreId */) {
  /* parser = new EkaEobiParser(this); */
  /* if (parser == NULL) on_error ("parser == NULL"); */
  /* strat = new EkaStrat(_dev,SN_EOBI); */
  EKA_LOG("Opening EOBI Exch"/* ,coreId */);

}

/* ----------------------------------------------------- */
EkaParser* EkaEobi::newParser(EkaStrat* _strat) {
  EkaEobiParser* p = new EkaEobiParser(this, _strat);
  if (p == NULL) on_error("new EkaEobiParser == NULL");
  p->strat = _strat;
  return (EkaParser*)p;
}

/* ----------------------------------------------------- */

EkaBook*  EkaEobi::newBook(uint8_t coreId,EkaStrat* _strat, EkaProd* _prod) {
  return (EkaBook*) (new EkaEobiBook(dev,_strat,coreId,_prod));
}

/* ----------------------------------------------------- */
inline uint16_t price2Norm(uint64_t _price, uint64_t step,uint64_t bottom) {
  if (_price == 0) return 0;
  if (unlikely(_price < bottom)) 
    on_error("price %ju < bottom %ju",_price,bottom);
  return (_price - bottom) / step;
}

/* ----------------------------------------------------- */

inline void getDepthData(EkaBook* b, SIDE side, EkaEobi::EobiDepthParams* dst, uint depth) {
  uint32_t accSize = 0;
  for (uint i = 0; i < depth; i ++) {
    dst->entry[i].price = b->getEntryPrice(side,i);
    accSize            += b->getEntrySize(side,i);
    dst->entry[i].size  = accSize;
  }
}

/* ----------------------------------------------------- */

int EkaEobi::onTobChange(MdOut* mdOut, EkaBook* b, SIDE side) {
  EobiHwBookParams tobParams = {};

  EkaProd* prod = b->prod;

  uint64_t tobPrice = static_cast<uint64_t>(b->getEntryPrice(side,0));
  uint64_t step     = prod->getStep();
  uint64_t bottom   = prod->getBottom();

  tobParams.tob.last_transacttime = mdOut->transactTime;
  tobParams.tob.price             = tobPrice;
  tobParams.tob.size              = b->getEntrySize (side,0);
  tobParams.tob.msg_seq_num       = mdOut->sequence;

  tobParams.tob.normprice         = tobPrice == 0 ? 0 : price2Norm(tobPrice,step,bottom);

  tobParams.tob.firePrice = tobPrice;
  
  uint64_t betterPrice = tobPrice == 0 ? 0 :
    side == SIDE::BID ? tobPrice + step : tobPrice - step;

  tobParams.tob.fireBetterPrice = betterPrice;

  getDepthData (b, side,&tobParams.depth,HW_DEPTH_PRICES);

  eka_side_type_t hwSide = side == BID ? eka_side_type_t::BUY : eka_side_type_t::SELL;
#ifdef _EKA_PARSER_PRINT_ALL_ 
  EKA_LOG("TOB changed price=%ju size=%ju normprice=%ju",tobParams.tob.price,tobParams.tob.size,tobParams.tob.normprice);
#endif

#ifdef PCAP_TEST
#else
  ekaWriteTob(mdOut->dev,&tobParams,sizeof(EobiHwBookParams),(eka_product_t)b->getProdId(),hwSide);
#endif
 return 0;
}

int EkaEobi::ekaDownloadProductParams(eka_product_t product_id) {
  uint sw_product_index = fireLogic->findProd(product_id,__func__);
  uint hw_product_index = fireLogic->findProdHw(product_id,__func__);

  if (hw_product_index >= EKA_MAX_BOOK_PRODUCTS) 
    on_error ("jump strategy is supported only on book product (not on %d)",(int)hw_product_index);

  EkaProd* prod        = fireLogic->prod[sw_product_index];

  if (! prod->is_book) return 1;

  volatile struct hw_product_param_entry_t product_param = {};
  volatile struct hw_session_param_entry_t session_param = {};

  if (prod->buy_size%10000)   on_error ("%s: any size parameter must be multiple of 10000 (not %ju) -> ",__func__,prod->buy_size);
  if (prod->sell_size%10000)  on_error ("%s: any size parameter must be multiple of 10000 (not %ju) -> ",__func__,prod->sell_size);

  product_param.max_book_spread = (uint8_t)prod->max_book_spread;
  product_param.buy_size        = (uint8_t)(prod->buy_size  / 10000);
  product_param.sell_size       = (uint8_t)(prod->sell_size / 10000);
  product_param.step            = prod->hwstep;
  product_param.ei_if           = prod->ei_if;
  product_param.ei_session      = prod->ei_session;

  //  product_param.ei_seq_num    = prod->ei_seq_num; TBD

  //  if (product_param.buy_size > 15 || product_param.sell_size > 15) 
  //    on_error("too high buy_size %ju or sell_size %ju",product_param.buy_size, product_param.sell_size);

  //  EKA_LOG("product_id = %s (%u), seq = 0x%jx", EKA_RESOLVE_PROD(dev->hwFeedVer,product_id),product_id,product_param.ei_seq_num);

  uint64_t base_addr = 0x83000;
  uint64_t* wr_ptr        = (uint64_t*) &product_param;
  uint64_t* wr_ptr_shadow = (uint64_t*) &fireLogic->hw_product_param_shadow[hw_product_index];    

  int iter = sizeof(struct hw_product_param_entry_t)/8 + !!(sizeof(struct hw_product_param_entry_t)%8); // num of 8Byte words
  for (int z=0; z<iter; z++) {
    if (*(wr_ptr+z) != *(wr_ptr_shadow+z)) {
      /* EKA_LOG("CHAN %s (%d): z=%02d, old=0x%016jx, new=0x%016jx,has sw_product_index %d hw_product_index", */
      /* 	      EKA_RESOLVE_PROD(dev->hwFeedVer,prod->product_id), */
      /* 	      product_id, */
      /* 	      z, */
      /* 	      *(wr_ptr_shadow+z), */
      /* 	      *(wr_ptr+z), */
      /* 	      sw_product_index, */
      /* 	      hw_product_index */
      /* 	      ); */
      eka_write(dev,base_addr+hw_product_index*iter*8+z*8,*(wr_ptr + z)); //data
      *(wr_ptr_shadow+z) = *(wr_ptr+z);
    } else {
      /* EKA_LOG("SAME %s (%d): z=%02d, old=0x%016jx, new=0x%016jx,has sw_product_index %d hw_product_index", */
      /* 	      EKA_RESOLVE_PROD(dev->hwFeedVer,prod->product_id), */
      /* 	      product_id, */
      /* 	      z, */
      /* 	      *(wr_ptr_shadow+z), */
      /* 	      *(wr_ptr+z), */
      /* 	      sw_product_index, */
      /* 	      hw_product_index */
      /* 	      ); */
    }
  }

  session_param.ei_seq_num        = prod->ei_seq_num;
  base_addr = 0x86000;
  wr_ptr = (uint64_t*) &session_param;
  wr_ptr_shadow = (uint64_t*) &dev->exch->fireLogic->hw_session_param_shadow[product_param.ei_session+product_param.ei_if*EKA_MAX_BOOK_PRODUCTS];    

  iter = sizeof(struct hw_session_param_entry_t)/8 + !!(sizeof(struct hw_session_param_entry_t)%8); // num of 8Byte words
  for (int z=0; z<iter; z++) {
    if (*(wr_ptr+z) != *(wr_ptr_shadow+z)) {
      /* EKA_LOG("CHAN %s (%d): z=%02d, old=0x%016jx, new=0x%016jx,has sw_product_index %d hw_product_index", */
      /* 	      EKA_RESOLVE_PROD(dev->hwFeedVer,prod->product_id), */
      /* 	      product_id, */
      /* 	      z, */
      /* 	      *(wr_ptr_shadow+z), */
      /* 	      *(wr_ptr+z), */
      /* 	      sw_product_index, */
      /* 	      hw_product_index */
      /* 	      ); */
      eka_write(dev,base_addr+(product_param.ei_session+product_param.ei_if*EKA_MAX_BOOK_PRODUCTS)*iter*8+z*8,*(wr_ptr + z)); //data
      *(wr_ptr_shadow+z) = *(wr_ptr+z);
    } else {
      /* EKA_LOG("SAME %s (%d): z=%02d, old=0x%016jx, new=0x%016jx,has sw_product_index %d hw_product_index", */
      /* 	      EKA_RESOLVE_PROD(dev->hwFeedVer,prod->product_id), */
      /* 	      product_id, */
      /* 	      z, */
      /* 	      *(wr_ptr_shadow+z), */
      /* 	      *(wr_ptr+z), */
      /* 	      sw_product_index, */
      /* 	      hw_product_index */
      /* 	      ); */
    }

  }

  /* printf ("%s: for product %d, (sw:%d) (hw:%d)\n",__func__, (int)product_id,sw_product_index,hw_product_index); */
  /* printf ("%s: dev midpoint %u hw midpint %u 0x%x\n",__func__,prod->midpoint,product_param.midpoint,product_param.midpoint); */
  /* printf ("%s: iter %d sizeofstruct %lu sizeofproduct_param %lu\n",__func__,iter,sizeof(struct hw_product_param_entry_t),sizeof(product_param)); */
  return 0;

}

/* ----------------------------------------------------- */
template <class O, class I> inline O normalizePriceDelta(I price) {
  if (price == (I)(-1))  return (O)-1;
  if (price/100000000 > 42)  on_error ("%s: price delta parameter must be less than 42*100000000 (not %ju) -> ",__func__,price);
  return (O)(price);
}

/* ----------------------------------------------------- */
template <class O, class I> inline O normalizeSize(I size) {
  if (size == (I)(-1))  return (O)-1;
  if (size%10000)  on_error ("%s: any size parameter must be multiple of 10000 (not %u) -> ",__func__,size);
  return (O)(size / 10000);
}

/* ----------------------------------------------------- */
template <class O, class I> inline O normalizeTimeDelta(I timeDelta) {
  if (timeDelta == (I)(-1))  return (O)-1;

  if (timeDelta % 1000)      on_error ("%s: time delta must have whole number of microseconds, (not %ju nanoseconds) -> ",__func__,timeDelta);
  if (timeDelta > 64000000)  on_error ("%s: time delta is limited to 64ms (not %ju ns) -> ",__func__,timeDelta);

  return (O)(timeDelta/1000);
}

int EkaEobi::jumpParamsApi2Internal(const jump_params_t* apiParams, volatile void* _dst) {
  volatile hw_jump_params_t* dst = (volatile hw_jump_params_t*)_dst;
  dst->bit_params = ((uint8_t)apiParams->strat_en) << BP_ENABLE;
  if (! apiParams->strat_en) return 1;

  dst->bit_params |= ((uint8_t)apiParams->boc_en) << BP_BOC;

  dst->min_price_delta = normalizePriceDelta <decltype(dst->min_price_delta),decltype(apiParams->min_price_delta)> (apiParams->min_price_delta);
  dst->min_ticker_size = normalizeSize       <decltype(dst->min_ticker_size),decltype(apiParams->min_ticker_size)> (apiParams->min_ticker_size);
  dst->max_post_size   = normalizeSize       <decltype(dst->max_post_size)  ,decltype(apiParams->max_post_size)  > (apiParams->max_post_size  );
  dst->max_tob_size    = normalizeSize       <decltype(dst->max_tob_size)   ,decltype(apiParams->max_tob_size)   > (apiParams->max_tob_size   );
  dst->min_tob_size    = normalizeSize       <decltype(dst->min_tob_size)   ,decltype(apiParams->min_tob_size)   > (apiParams->min_tob_size   );
  dst->buy_size        = normalizeSize       <decltype(dst->buy_size)       ,decltype(apiParams->buy_size)       > (apiParams->buy_size       );
  dst->sell_size       = normalizeSize       <decltype(dst->sell_size)      ,decltype(apiParams->sell_size)      > (apiParams->sell_size      );  

  return 1;
}

int EkaEobi::rjumpParamsApi2Internal(const reference_jump_params_t* apiParams, volatile void* _dst) {
  volatile hw_rjump_params_t* dst = (volatile hw_rjump_params_t*)_dst;
  dst->bit_params = ((uint8_t)apiParams->strat_en) << BP_ENABLE;
  if (! apiParams->strat_en) return 1;

  dst->bit_params |= ((uint8_t)apiParams->boc_en) << BP_BOC;

  dst->time_delta_ns        = normalizeTimeDelta  <decltype(dst->time_delta_ns)       ,decltype(apiParams->time_delta_ns         )> (apiParams->time_delta_ns         );
  dst->tickersize_lots      = normalizeSize       <decltype(dst->tickersize_lots)     ,decltype(apiParams->tickersize_lots       )> (apiParams->tickersize_lots       );
  dst->min_tob_size         = normalizeSize       <decltype(dst->min_tob_size)        ,decltype(apiParams->min_tob_size          )> (apiParams->min_tob_size          );
  dst->max_tob_size         = normalizeSize       <decltype(dst->max_tob_size)        ,decltype(apiParams->max_tob_size          )> (apiParams->max_tob_size          );
  dst->max_opposit_tob_size = normalizeSize       <decltype(dst->max_opposit_tob_size),decltype(apiParams->max_opposit_tob_size  )> (apiParams->max_opposit_tob_size  );
  dst->buy_size             = normalizeSize       <decltype(dst->buy_size)            ,decltype(apiParams->buy_size              )> (apiParams->buy_size              );
  dst->sell_size            = normalizeSize       <decltype(dst->sell_size)           ,decltype(apiParams->sell_size             )> (apiParams->sell_size             );

  dst->min_spread           = (decltype(dst->min_spread))(apiParams->min_spread);

  return 1;
}

int EkaEobi::sendDate2Hw(EkaDev* dev) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); 
  uint64_t current_time_ns = ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 + (uint64_t)(t.tv_nsec)); //UTC
  //  current_time_ns += static_cast<uint64_t>(6*60*60) * static_cast<uint64_t>(1e9); //+6h UTC time

  eka_write(dev,0xf0300+0*8,current_time_ns); //data, last write commits the change
  eka_write(dev,0xf0300+1*8,0x0); //data, last write commits the change
  eka_write(dev,0xf0300+2*8,0x0); //data, last write commits the change

  return 1;
}

/* ----------------------------------------------------- */

int EkaEobi::createCancelMsg(uint8_t* dst, void* params) {
  
  memcpy(dst,params,sizeof(DeleteOrderSingleRequestT));

  DeleteOrderSingleRequestT* cancelMsg = (DeleteOrderSingleRequestT*)dst;
  
  cancelMsg->OrderID = 0;
  cancelMsg->ClOrdID = 0;

  cancelMsg->MsgSeqNum = 0;

  return sizeof(DeleteOrderSingleRequestT);
}
/* ----------------------------------------------------- */

int EkaEobi::updateCancelMsg(uint prodIdx, uint8_t* msg, void* params) {
  EkaProd* prod = dev->exch->cancelMsgs->prod[prodIdx];
  if (prod == NULL) on_error("prod[%u] == NULL",prodIdx);

  uint8_t* payload = msg + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr);
  ((DeleteOrderSingleRequestT*)payload)->OrderID = ((EpmEurexCancelCriticalParams*)params)->OrderID;  
  ((DeleteOrderSingleRequestT*)payload)->ClOrdID = ((EpmEurexCancelCriticalParams*)params)->ClOrdID;

  uint64_t firstFieldOffs = (uint64_t)((uint8_t*)&((DeleteOrderSingleRequestT*)payload)->OrderID - msg);

  bool notAligned = firstFieldOffs % 8 != 0;
  uint updateSize = 
    sizeof(((DeleteOrderSingleRequestT*)payload)->OrderID) +
    sizeof(((DeleteOrderSingleRequestT*)payload)->ClOrdID) +
    8 * notAligned;

  uint64_t srcWordOffs = firstFieldOffs / 8 * 8;

  uint8_t  threadId = 0; //TBD
  uint64_t dstLogicalAddr = prod->cancelAction->heapAddr;

  copyIndirectBuf2HeapHw_swap4(dev,dstLogicalAddr + srcWordOffs,(uint64_t*)(msg + srcWordOffs),threadId,updateSize); 

  prod->Csum4trigger = prod->cancelMsgChckSum;

  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((DeleteOrderSingleRequestT*)payload)->OrderID,
				     sizeof(((DeleteOrderSingleRequestT*)payload)->OrderID));
  prod->Csum4trigger += deltaTcpCsum((uint16_t*)&((DeleteOrderSingleRequestT*)payload)->ClOrdID,
				     sizeof(((DeleteOrderSingleRequestT*)payload)->ClOrdID));

  return 0;
}

/* ----------------------------------------------------- */

EpmTemplate* EkaEobi::addCancelMsgTemplate(uint templateId) {
  return new EpmEti8PktTemplate(templateId,"EpmEti8PktTemplate");
}

EpmTemplate* EkaEobi::addNewOrderMsgTemplate(uint templateId) {
  return new EpmIlink3NewOrderTemplate(templateId,"EpmIlink3NewOrderTemplate"); //PATCH
}
/* ----------------------------------------------------- */
