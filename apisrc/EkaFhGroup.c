#include "EkaFhGroup.h"


void EkaFhGroup::sendFeedUp(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");

  EfhFeedUpMsg efhFeedUpMsg = {
    EfhMsgType::kFeedUp, 
    {exch, id}, 
    ++gapNum 
  };
  pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
}


void EkaFhGroup::sendFeedDown(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");

  EfhFeedDownMsg efhFeedDownMsg = { 
    EfhMsgType::kFeedDown, 
    {exch, id}, 
    ++gapNum 
  };
  pEfhRunCtx->onEfhFeedDownMsgCb(&efhFeedDownMsg, 0, pEfhRunCtx->efhRunUserData);
}
