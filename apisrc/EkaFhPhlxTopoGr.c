#include "EkaFhPhlxTopoGr.h"
#include "EkaFhThreadAttr.h"

void* getMolUdp64Data(void* attr);
void* getSoupBinData(void* attr);


int EkaFhPhlxTopoGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}


int EkaFhPhlxTopoGr::closeIncrementalGap(EfhCtx*          pEfhCtx, 
					 const EfhRunCtx* pEfhRunCtx, 
					 uint64_t         startSeq,
					 uint64_t         endSeq) {

  std::string threadName = std::string("ST_") + 
    std::string(EKA_EXCH_SOURCE_DECODE(exch)) + 
    '_' + 
    std::to_string(id);

  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
					       pEfhRunCtx, 
					       this, 
					       startSeq, 
					       endSeq,  
					       EkaFhMode::RECOVERY);
  if (attr == NULL) on_error("attr = NULL");
    
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    getSoupBinData,  //  getMolUdp64Data,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;

}
