#include "EkaFhBats.h"
#include "EkaFhBatsGr.h"
#include "EkaFhBatsParser.h"
#include "EkaFhRunGroup.h"
#include "EkaFhThreadAttr.h"
#include "EkaUdpChannel.h"

using namespace Bats;

void *getSpinData(void *attr);

/* ############################################### */
EkaFhGroup *EkaFhBats::addGroup() {
  return new EkaFhBatsGr();
}

/* ############################################### */
const uint8_t *EkaFhBats::getUdpPkt(EkaFhRunGroup *runGr,
                                    uint *msgInPkt,
                                    uint64_t *sequence,
                                    uint8_t *gr_id) {
  auto pkt = runGr->udpCh->get();
  if (!pkt)
    on_error("%s: pkt == NULL", EKA_EXCH_DECODE(exch));

  uint msgCnt = EKA_BATS_MSG_CNT((pkt));
  uint8_t grId = getGrId(pkt);

  if (grId == 0xFF || (!runGr->isMyGr(grId)) ||
      b_gr[grId] == NULL || b_gr[grId]->q == NULL) {
    runGr->udpCh->next();
    return NULL;
  }

  *msgInPkt = msgCnt;
  *sequence = EKA_BATS_SEQUENCE((pkt));
  *gr_id = grId;
  return pkt;
}
/* ############################################### */

uint8_t EkaFhBats::getGrId(const uint8_t *pkt) {
  for (uint8_t i = 0; i < groups; i++) {
    auto gr = dynamic_cast<EkaFhBatsGr *>(b_gr[i]);
    if (!gr)
      on_error("b_gr[%u] = NULL", i);

    if (gr->mcast_port == EKA_UDPHDR_DST((pkt - 8)) &&
        EKA_BATS_UNIT(pkt) == gr->batsUnit)
      return i;
  }
  return 0xFF;
}

/* ############################################### */

EkaOpResult
EkaFhBats::runGroups(EfhCtx *pEfhCtx,
                     const EfhRunCtx *pEfhRunCtx,
                     uint8_t runGrId) {
  auto runGr = dev->runGr[runGrId];
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

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
  uint64_t firstDropSeq = 0;
  bool dropMe = false;
#endif

#if EFH_MONITOR_BOOK_STATS
  uint64_t monitorCounter = 0;
#endif

  active = true;
  while (runGr->thread_active &&
         !runGr->stoppedByExchange) {
    //-----------------------------------------
#if EFH_MONITOR_BOOK_STATS
    if (++monitorCounter % 10000000000UL == 0) {
      for (uint8_t i = 0; i < runGr->numGr; i++) {
        b_gr[runGr->groupList[i]]->printBookState();
      }
    }
#endif
    //-----------------------------------------

    if (!runGr->udpCh->has_data()) {
#ifndef _DONT_SEND_MDTIMEOUT
      if (runGr->checkNoMd)
        runGr->checkGroupsNoMd(pEfhRunCtx);
#endif
      continue;
    }

    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;

    auto pkt =
        getUdpPkt(runGr, &msgInPkt, &sequence, &gr_id);
    if (!pkt)
      continue;

    auto gr = dynamic_cast<EkaFhBatsGr *>(b_gr[gr_id]);
    if (!gr)
      on_error("b_gr[%u] = NULL", gr_id);

#ifndef _DONT_SEND_MDTIMEOUT
    gr->resetNoMdTimer();
#endif

    std::chrono::high_resolution_clock::time_point
        startTime{};
#if EFH_TIME_CHECK_PERIOD
    if (sequence % EFH_TIME_CHECK_PERIOD == 0) {
      startTime = std::chrono::high_resolution_clock::now();
    }
#endif
    if (sequence == 0) // unsequenced packet
      goto SKIP_PKT;

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    dropMe = false;
    switch (gr->state) {
    case EkaFhGroup::GrpState::NORMAL:
      if (sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
        dropMe = true;
        firstDropSeq = sequence;
      }
      break;
    case EkaFhGroup::GrpState::SNAPSHOT_GAP:
      break;
    case EkaFhGroup::GrpState::RETRANSMIT_GAP:
      if (sequence - firstDropSeq <=
          _EFH_TEST_GAP_INJECT_DELTA_)
        dropMe = true;
      break;
    default:
      dropMe = false;
    }
    if (dropMe) {
      EKA_WARN("\n---------------\n"
               "%s:%u: TEST GAP INJECTED: (INTERVAL = %d, "
               "DELTA = %d): "
               "sequence %ju with %u messages dropped"
               "\n---------------",
               EKA_EXCH_DECODE(exch), gr_id,
               _EFH_TEST_GAP_INJECT_INTERVAL_,
               _EFH_TEST_GAP_INJECT_DELTA_, sequence,
               msgInPkt);
      goto SKIP_PKT;
    }
#endif

#ifdef FH_LAB
    gr->state = EkaFhGroup::GrpState::NORMAL;
    gr->processUdpPkt(pEfhRunCtx, pkt, msgInPkt, sequence,
                      startTime);
    goto SKIP_PKT;
#endif

    //-----------------------------------------
    switch (gr->state) {
      //------------------------------------------
    case EkaFhGroup::GrpState::INIT: // waiting for Snapshot
      if (lockSnapshotGap.test_and_set(
              std::memory_order_acquire))
        break; // only1s group can get snapshot at a time

      EKA_LOG("%s:%u 1st MC msq sequence=%ju",
              EKA_EXCH_DECODE(exch), gr_id, sequence);
      gr->pushUdpPkt2Q(pkt, msgInPkt, sequence);

      gr->invalidateBook();
      gr->gapClosed = false;

      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;

      gr->closeSnapshotGap(pEfhCtx, pEfhRunCtx, 0, 0);

      break;
      //-----------------------------------------
    case EkaFhGroup::GrpState::NORMAL:
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // NORMAL
      if (sequence == gr->expected_sequence) {
        runGr->stoppedByExchange = gr->processUdpPkt(
            pEfhRunCtx, pkt, msgInPkt, sequence, startTime);
        break;
      }
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // BACK-IN-TIME to be ignored
      if (sequence < gr->expected_sequence)
        break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // GAP: sequence > gr->expected_sequence
      EKA_LOG("%s:%u Gap at NORMAL: expected_sequence=%ju, "
              "sequence=%ju, gap=%jd",
              EKA_EXCH_DECODE(exch), gr_id,
              gr->expected_sequence, sequence,
              sequence - gr->expected_sequence);

      gr->gapClosed = false;
      gr->sendFeedDown(pEfhRunCtx);
      gr->pushUdpPkt2Q(pkt, msgInPkt, sequence);

      gr->state = EkaFhGroup::GrpState::INIT;

      break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

      //----------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP:
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // Waiting for recovery
      gr->pushUdpPkt2Q(pkt, msgInPkt, sequence);

      if (!gr->gapClosed)
        break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
      // Gap closed
      lockSnapshotGap.clear();

      /* if (gr->lastExchErr !=
       * EfhExchangeErrorCode::kNoError) { */
      /* 	EKA_LOG("%s:%u: GRP Recovery failed, trying
       * Spin Snapshot", */
      /* 					EKA_EXCH_DECODE(exch),gr->id);
       */
      /* 	gr->gapClosed = false; */
      /* 	gr->state = EkaFhGroup::GrpState::INIT; */
      /* 	break; */
      /* } */

      gr->expected_sequence = gr->seq_after_snapshot;

      EKA_LOG(
          "%s:%u: %s Closed, switching to fetch from Q: "
          "expected_sequence = %ju",
          EKA_EXCH_DECODE(exch), gr->id,
          gr->printGrpState(), gr->seq_after_snapshot);

      if (gr->processFromQ(pEfhRunCtx) <
          0) { // gap in the Q
        EKA_LOG("%s:%u: gap during %s recovery, "
                "Snapshot recovery to be redone!",
                EKA_EXCH_DECODE(exch), gr->id,
                gr->printGrpState());
        gr->state = EkaFhGroup::GrpState::INIT;
        break;
      }
      EKA_LOG(
          "%s:%u: %s GAP Closed - expected_sequence=%ju",
          EKA_EXCH_DECODE(exch), gr->id,
          gr->printGrpState(), gr->expected_sequence);

      gr->state = EkaFhGroup::GrpState::NORMAL;
      gr->sendFeedUp(pEfhRunCtx);

      break;
      //----------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
               EKA_EXCH_DECODE(exch), gr->id,
               (uint)gr->state);
      break;
    }

  SKIP_PKT:
    runGr->udpCh->next();
  }
  EKA_LOG("%s RunGroup %u EndOfSession",
          EKA_EXCH_DECODE(exch), runGrId);
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}
/* ############################################### */

EkaOpResult
EkaFhBats::getDefinitions(EfhCtx *pEfhCtx,
                          const EfhRunCtx *pEfhRunCtx,
                          const EkaGroup *group) {
  auto attr = new EkaFhThreadAttr(
      pEfhCtx, pEfhRunCtx, b_gr[group->localId], 1, 0,
      EkaFhMode::DEFINITIONS);
  if (!attr)
    on_error("attr = NULL");
  getSpinData(attr);

  return EKA_OPRESULT__OK;
}
