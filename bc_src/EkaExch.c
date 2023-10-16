#if 0
#include "EkaBook.h"
#include "EkaEpm.h"
#include "EkaParser.h"
#include "EkaStrat.h"
#include "EkaTcpSess.h"

/* ----------------------------------------------------- */
int EkaExch::initStrat(StrategyType type) {
  switch (type) {
  case FireLogic :
    fireLogic          = new EkaStrat(this,type);
    if (fireLogic == NULL) on_error("fireLogic == NULL");
    fireLogic->parser  = newParser(fireLogic);
    break;
  case FeedServer :
    feedServer         = new EkaStrat(this,type);
    if (feedServer == NULL) on_error("feedServer == NULL");
    feedServer->parser = newParser(feedServer);
    break;
  case CancelMsgs :
    cancelMsgs         = new EkaStrat(this,type);
    if (cancelMsgs == NULL) on_error("cancelMsgs == NULL");
    break;
  default:
    on_error("Unexpected Strategy type %d",type);
  }
  return 0;
}


/* ----------------------------------------------------- */


uint EkaExch::booksPerCore(uint8_t coreId) {
  uint sum = 0;
  /* for (uint i = 0; i < numBooks; i++) { */
  /*   if (book[i] == NULL) on_error("book[%u] == NULL",i); */
  /*   if (book[i]->getCoreId() == coreId) sum++; */
  /* } */

  // TBD
  return sum;
}
/* ----------------------------------------------------- */


int EkaExch::setFireParams(int sock, void* params) {
  EkaTcpSess* s = dev->findTcpSess(sock);
  uint64_t base_addr = 0x20000 + s->coreId * 0x1000;

  uint64_t* wr_ptr = (uint64_t*) params;
  int iter = getFireParamsSize()/8 + !!(getFireParamsSize()%8); // num of 8Byte words
  for (int z=0; z<iter; ++z) {
    eka_write(dev,base_addr+z*8,*(wr_ptr + z)); //data
  }
  eka_write(dev,0x2f000 + s->coreId * 0x100 ,s->sessId * 0x10000); //desc
  //  EKA_LOG("getFireParamsSize = %u",getFireParamsSize());
  return 0;
}

int EkaExch::prepareFireReportJump(void* dst,void* src) {

  dma_report_t* dmaPtrDst = &((fire_report_jump_t *)dst)->dma_report;
  dma_report_t* dmaPtrSrc = &((fire_report_jump_t *)src)->dma_report;
  //dma report
  dmaPtrDst->type     =          dmaPtrSrc->type;
  dmaPtrDst->subtype  =          dmaPtrSrc->subtype;
  dmaPtrDst->core_id  =          dmaPtrSrc->core_id;
  dmaPtrDst->length   =  be16toh(dmaPtrSrc->length);

  normalized_report_common_t* commonHPtrDst = &((fire_report_jump_t *)dst)->normalized_report.common_header;
  normalized_report_common_t* commonHPtrSrc = &((fire_report_jump_t *)src)->normalized_report.common_header;

  commonHPtrDst->fire_ei        =         commonHPtrSrc->fire_ei;
  commonHPtrDst->fire_session   =         commonHPtrSrc->fire_session;
  commonHPtrDst->strategy_id    =         commonHPtrSrc->strategy_id;
  commonHPtrDst->report_bytes   = be16toh(commonHPtrSrc->report_bytes);
  commonHPtrDst->strategy_subid =         commonHPtrSrc->strategy_subid;
  commonHPtrDst->hw_ts          = be64toh(commonHPtrSrc->hw_ts);
  commonHPtrDst->fire_reason    =         commonHPtrSrc->fire_reason;

  normalized_report_level_t* levelPtrDst = &((fire_report_jump_t *)dst)->normalized_report.ticker;
  normalized_report_level_t* levelPtrSrc = &((fire_report_jump_t *)src)->normalized_report.ticker;

  levelPtrDst->price = be64toh(levelPtrSrc->price);
  levelPtrDst->size  = be64toh(levelPtrSrc->size);
  levelPtrDst->side  =         levelPtrSrc->side;

  levelPtrDst = &((fire_report_jump_t *)dst)->normalized_report.tob_buy;
  levelPtrSrc = &((fire_report_jump_t *)src)->normalized_report.tob_buy;

  levelPtrDst->price = be64toh(levelPtrSrc->price);
  levelPtrDst->size  = be64toh(levelPtrSrc->size);
  levelPtrDst->side  =         levelPtrSrc->side;

  levelPtrDst = &((fire_report_jump_t *)dst)->normalized_report.tob_sell;
  levelPtrSrc = &((fire_report_jump_t *)src)->normalized_report.tob_sell;

  levelPtrDst->price = be64toh(levelPtrSrc->price);
  levelPtrDst->size  = be64toh(levelPtrSrc->size);
  levelPtrDst->side  =         levelPtrSrc->side;

  normalized_report_jump_t* normalizedPtrDst = &((fire_report_jump_t *)dst)->normalized_report;
  normalized_report_jump_t* normalizedPtrSrc = &((fire_report_jump_t *)src)->normalized_report;

  normalizedPtrDst->ts                = be64toh(normalizedPtrSrc->ts);
  normalizedPtrDst->seq =      getFireReportSeq(normalizedPtrSrc->seq);
  normalizedPtrDst->post_ticker_size =  be64toh(normalizedPtrSrc->post_ticker_size);

  // Fire report loop

  EkaRawMessageFire* rawFireDst = &((fire_report_jump_t *)dst)->fireRawReport;
  EkaRawMessageFire* rawFireSrc = &((fire_report_jump_t *)src)->fireRawReport;

  uint fieldSize = dev->hwRawFireSize;
  for (uint i = 0; i < fieldSize; i++) {
    (*rawFireDst)[i] = (*rawFireSrc)[fieldSize - i - 1];
  }

return 0;
}

#endif
