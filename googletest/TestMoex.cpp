#include "TestMoex.h"


void TestMoex::generateMdDataPkts(
    const void *mdInjectParams) {
}
/* ############################################# */

void TestMoex::sendData() {
#if 0
  for (const auto &md : insertedMd_) {
    setSessionCntr(dev_, tcpCtx_->tcpSess_[0]->hCon_,
                        md.appSeq);

    sendPktToAll(md.preloadedPkt, md.pktLen,
                 md.expectedFire);
  }
#endif
}

/* ############################################# */
