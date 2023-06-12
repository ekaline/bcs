#include "EkaFhMiax.h"
#include "EkaFhMiaxGr.h"
#include "EkaFhMiaxParser.h"
#include "EkaFhRunGroup.h"
#include "EkaFhThreadAttr.h"
#include "EkaUdpChannel.h"

void *getSesmData(void *attr);

/* ############################################## */
EkaFhGroup *EkaFhMiax::addGroup() {
  return new EkaFhMiaxGr();
}
/* ############################################## */

const uint8_t *
EkaFhMiax::getUdpPkt(EkaFhRunGroup *runGr, uint *msgInPkt,
                     int16_t *pktLen, uint64_t *sequence,
                     uint8_t *gr_id, bool *isHeartbeat) {

  uint8_t *pkt = (uint8_t *)runGr->udpCh->get();
  if (pkt == NULL)
    on_error("%s: pkt == NULL", EKA_EXCH_DECODE(exch));

  *gr_id = getGrId(pkt);
  if (*gr_id == 0xFF) {
    runGr->udpCh->next();
    return NULL;
  }

  *pktLen = runGr->udpCh->getPayloadLen();
  *sequence = EKA_GET_MACH_SEQ((pkt));
  *isHeartbeat = EKA_IS_MACH_HEARTBEAT((pkt));
  *msgInPkt = 16; // just a number, greater than amount of
                  // messages in a packet
  return pkt;
}
/* ############################################## */

EkaOpResult
EkaFhMiax::runGroups(EfhCtx *pEfhCtx,
                     const EfhRunCtx *pEfhRunCtx,
                     uint8_t runGrId) {
  EkaFhRunGroup *runGr = dev->runGr[runGrId];
  if (!runGr)
    on_error("!runGr");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
            EKA_EXCH_DECODE(exch), runGr->runId,
            runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  for (uint8_t i = 0; i < runGr->numGr; i++) {
    auto gr = b_gr[runGr->groupList[i]];
    gr->sendFeedDownInitial(pEfhRunCtx);
    gr->gapClosed = false;
    gr->state = EkaFhGroup::GrpState::INIT;
  }

  EKA_DEBUG("\n~~~~~~~~~~ "
            "Main Thread for %s Run Group %u: %s GROUPS "
            "~~~~~~~~~~~~~",
            EKA_EXCH_DECODE(exch), runGr->runId,
            runGr->list2print);

  while (runGr->thread_active &&
         !runGr->stoppedByExchange) {
    //-----------------------------------------
    if (runGr->drainQ(pEfhRunCtx))
      continue;

    //-----------------------------------------
    if (!runGr->udpCh->has_data()) {
      if (runGr->checkNoMd)
        runGr->checkGroupsNoMd(pEfhRunCtx);
      continue;
    }
    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;
    int16_t pktLen = 0;
    bool isHeartbeat = false;

    const uint8_t *pkt =
        getUdpPkt(runGr, &msgInPkt, &pktLen, &sequence,
                  &gr_id, &isHeartbeat);
    if (!pkt)
      continue;

    auto gr{dynamic_cast<EkaFhMiaxGr *>(b_gr[gr_id])};
    if (!gr)
      on_error("! b_gr[%u]", gr_id);
    gr->resetNoMdTimer();

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (gr->state == EkaFhGroup::GrpState::NORMAL &&
        sequence != 0 &&
        sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN(
          "%s:%u: TEST GAP INJECTED: "
          "(GAP_INJECT_INTERVAL = %d): pkt sequence %ju "
          "with unknown number of messages dropped",
          EKA_EXCH_DECODE(exch), gr_id,
          _EFH_TEST_GAP_INJECT_INTERVAL_, sequence);
      runGr->udpCh->next();
      continue;
    }
#endif

    /* if (isHeartbeat) { */
    /*   runGr->udpCh->next();  */
    /*   continue; */
    /* } */

    //-----------------------------------------
    switch (gr->state) {
      //-----------------------------------------
    case EkaFhGroup::GrpState::INIT:
      if (lockSnapshotGap.test_and_set(
              std::memory_order_acquire))
        break; // only1s group can get
               // snapshot at a tim
      gr->firstMcSeq = sequence;

      EKA_LOG("%s:%u firstMcSeq=%ju", EKA_EXCH_DECODE(exch),
              gr_id, gr->firstMcSeq);
      gr->pushUdpPkt2Q(pkt, pktLen);

      gr->invalidateBook();
      gr->gapClosed = false;

      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;

      gr->closeSnapshotGap(pEfhCtx, pEfhRunCtx, 1,
                           sequence);

      break;
      //-----------------------------------------
    case EkaFhGroup::GrpState::NORMAL:
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      if (sequence == 0)
        break; // unsequenced packet
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      if (sequence == gr->expected_sequence) { // NORMAL
        runGr->stoppedByExchange =
            gr->processUdpPkt(pEfhRunCtx, pkt, pktLen);
        break;
      }
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      if (sequence < gr->expected_sequence) {
        if (isHeartbeat) {
          gr->expected_sequence = sequence + 1;
          break;
        }
        EKA_WARN("%s:%u BACK-IN-TIME WARNING: "
                 "sequence %ju < expected_sequence %ju",
                 EKA_EXCH_DECODE(exch), gr_id, sequence,
                 gr->expected_sequence);
        gr->sendBackInTimeEvent(pEfhRunCtx, sequence);
        gr->expected_sequence = sequence;
        break;
      }
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      if (sequence > gr->expected_sequence) { // GAP
        EKA_LOG("%s:%u Gap at NORMAL:  "
                "expected_sequence=%ju, "
                "sequence=%ju, gap=%ju",
                EKA_EXCH_DECODE(exch), gr_id,
                gr->expected_sequence, sequence,
                sequence - gr->expected_sequence);

        gr->gapClosed = false;
        gr->sendFeedDown(pEfhRunCtx);
        gr->pushUdpPkt2Q(pkt, pktLen);

        gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
        gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx,
                                gr->expected_sequence,
                                sequence + msgInPkt);
      }
      break;
      //-----------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP:
      // Waiting for recovery
      gr->pushUdpPkt2Q(pkt, pktLen);

      if (!gr->gapClosed)
        break;

      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // Snapshot Gap closed
      lockSnapshotGap.clear();
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      if (gr->seq_after_snapshot < gr->firstMcSeq) {
        // Snapshot older than very first MC msg
        // need further incremental recovery
        EKA_LOG(
            "%s:%u seq_after_snapshot %ju < firstMcSeq %ju"
            "incrementally filling gap by retransmit:"
            "%ju .. %ju gap = %jd",
            EKA_EXCH_DECODE(exch), gr_id,
            gr->seq_after_snapshot, gr->firstMcSeq,
            gr->seq_after_snapshot + 1, gr->firstMcSeq - 1,
            gr->firstMcSeq - gr->seq_after_snapshot);

        gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
        gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx,
                                gr->seq_after_snapshot,
                                gr->firstMcSeq);
        break;
      }
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // No need further incremental recovery
      gr->expected_sequence = gr->seq_after_snapshot + 1;

      EKA_LOG("%s:%u: %s Closed, switching "
              "to fetch from Q: "
              "expected_sequence = %ju",
              EKA_EXCH_DECODE(exch), gr->id,
              gr->printGrpState(), gr->seq_after_snapshot);

      if (gr->processFromQ(pEfhRunCtx) < 0) { // gap in Q
        EKA_LOG("%s:%u: gap during %s "
                "recovery, "
                "Snapshot recovery to "
                "be redone!",
                EKA_EXCH_DECODE(exch), gr->id,
                gr->printGrpState());
        gr->state = EkaFhGroup::GrpState::INIT;
        break;
      }
      EKA_LOG("%s:%u: %s GAP Closed - "
              "expected_sequence=%ju",
              EKA_EXCH_DECODE(exch), gr->id,
              gr->printGrpState(), gr->expected_sequence);

      gr->state = EkaFhGroup::GrpState::NORMAL;
      gr->sendFeedUp(pEfhRunCtx);
      break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

    case EkaFhGroup::GrpState::RETRANSMIT_GAP:
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // Waiting for recovery
      gr->pushUdpPkt2Q(pkt, pktLen);

      if (!gr->gapClosed)
        break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // Gap closed

      gr->expected_sequence = gr->seq_after_snapshot + 1;

      EKA_LOG("%s:%u: %s Closed, switching "
              "to fetch from Q: "
              "expected_sequence = %ju",
              EKA_EXCH_DECODE(exch), gr->id,
              gr->printGrpState(), gr->seq_after_snapshot);

      if (gr->processFromQ(pEfhRunCtx) < 0) { // gap in Q
        EKA_LOG("%s:%u: gap during %s "
                "recovery, "
                "Snapshot recovery to "
                "be redone!",
                EKA_EXCH_DECODE(exch), gr->id,
                gr->printGrpState());
        gr->state = EkaFhGroup::GrpState::INIT;
        break;
      }
      EKA_LOG("%s:%u: %s GAP Closed - "
              "expected_sequence=%ju",
              EKA_EXCH_DECODE(exch), gr->id,
              gr->printGrpState(), gr->expected_sequence);

      gr->state = EkaFhGroup::GrpState::NORMAL;
      gr->sendFeedUp(pEfhRunCtx);

      break;
      //-----------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
               EKA_EXCH_DECODE(exch), gr->id,
               (uint)gr->state);
      break;
    }
    runGr->udpCh->next();
  }
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}

/* ############################################## */

EkaOpResult
EkaFhMiax::getDefinitions(EfhCtx *pEfhCtx,
                          const EfhRunCtx *pEfhRunCtx,
                          const EkaGroup *group) {
  EkaFhThreadAttr *attr = new EkaFhThreadAttr(
      pEfhCtx, pEfhRunCtx, b_gr[(uint8_t)group->localId], 1,
      0, EkaFhMode::DEFINITIONS);
  getSesmData(attr);
  return EKA_OPRESULT__OK;
}
