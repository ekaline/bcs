#include "EkaFhNom.h"

#include "EkaFhNasdaq.h"
#include "EkaFhNasdaqGr.h"
#include "EkaFhNomGr.h"
#include "EkaFhRunGroup.h"
#include "EkaFhThreadAttr.h"
#include "EkaUdpChannel.h"

EkaFhGroup *EkaFhNom::addGroup() {
  return new EkaFhNomGr();
}

/* #########################################################
 */
EkaOpResult EkaFhNom::runGroups(EfhCtx *pEfhCtx,
                                const EfhRunCtx *pEfhRunCtx,
                                uint8_t runGrId) {
  EkaFhRunGroup *runGr = dev->runGr[runGrId];
  if (!runGr)
    on_error("!runGr");

  EKA_DEBUG("Initializing %s Run Group "
            "%u: %s GROUPS",
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
            "Main Thread for %s Run "
            "Group %u: %s GROUPS "
            "~~~~~~~~~~~~~",
            EKA_EXCH_DECODE(exch), runGr->runId,
            runGr->list2print);

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
  uint64_t firstDropSeq = 0;
#endif

  terminated = false;

  while (runGr->thread_active &&
         !runGr->stoppedByExchange) {
    if (!runGr->udpCh->has_data()) {
      if (runGr->checkNoMd)
        runGr->checkGroupsNoMd(pEfhRunCtx);
      continue;
    }
    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;

    auto pkt =
        getUdpPkt(runGr, &msgInPkt, &sequence, &gr_id);
    if (!pkt)
      continue;

    auto gr{dynamic_cast<EkaFhNomGr *>(b_gr[gr_id])};
    if (!gr)
      on_error("! b_gr[%u]", gr_id);

    gr->resetNoMdTimer();

    /* // unsequenced or heartbeat */
    /* if (sequence == 0 || msgInPkt ==
     * 0) */
    /*   goto SKIP_PKT; */

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    bool dropMe = false;
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
      if (sequence - firstDropSeq ==
          _EFH_TEST_GAP_INJECT_DELTA_)
        dropMe = true;
      break;
    default:
      dropMe = false;
    }
    if (dropMe) {
      EKA_WARN("\n---------------\n"
               "%s:%u: TEST GAP INJECTED: "
               "(INTERVAL = %d, DELTA = "
               "%d): sequence "
               "%ju with %u messages dropped"
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
    gr->processUdpPkt(pEfhRunCtx, pkt, msgInPkt, sequence);
    goto SKIP_PKT;
#endif
    //----------------------------------------
    switch (gr->state) {
      //------------------------------------------
    case EkaFhGroup::GrpState::INIT: // waiting for Snapshot
      if (lockSnapshotGap.test_and_set(
              std::memory_order_acquire))
        break; // only1s group can get
               // snapshot at a tim

      EKA_LOG("%s:%u 1st MC msq "
              "sequence=%ju",
              EKA_EXCH_DECODE(exch), gr_id, sequence);
      gr->pushUdpPkt2Q(pkt, msgInPkt, sequence);

      gr->invalidateBook();
      gr->gapClosed = false;

      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;

      gr->closeSnapshotGap(pEfhCtx, pEfhRunCtx, 1, 0);

      break;
      //----------------------------------------
    case EkaFhGroup::GrpState::NORMAL:
      if (sequence == gr->expected_sequence) { // NORMAL
        if (!msgInPkt)
          break;
        if (sequence % gr->StaleDataSampleRate == 0) {
          auto msg =
              EkaNwParser::MoldUdp64::getFirstMsg(pkt);
          if (!msg)
            on_error("!msg");
          auto exchTS = NomFeed::getTs(msg);

          auto now = std::chrono::system_clock::now();
          auto midnight = dev->midnightSystemClock;
          auto sampleNs = static_cast<uint64_t>(
              std::chrono::duration_cast<
                  std::chrono::nanoseconds>(now - midnight)
                  .count());

          if (sampleNs - exchTS >
              gr->StaleDataNanosecThreshold) {
            EKA_WARN("%s:%u: Stale data: "
                     "sampleNs %s - exchNs "
                     "%s > %ju",
                     EKA_EXCH_DECODE(exch), id,
                     ts_ns2str(sampleNs).c_str(),
                     ts_ns2str(exchTS).c_str(),
                     gr->StaleDataNanosecThreshold);

            gr->state = EkaFhGroup::GrpState::INIT;
            gr->sendFeedDownStaleData(pEfhRunCtx);
            break;
          }
        }

        runGr->stoppedByExchange = gr->processUdpPkt(
            pEfhRunCtx, pkt, msgInPkt, sequence);
        break;
      }
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       */
      // BACK-IN-TIME to be ignored
      if (sequence < gr->expected_sequence)
        break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       */
      // GAP: sequence >
      // gr->expected_sequence
      EKA_LOG("%s:%u Gap at NORMAL:  "
              "expected_sequence=%ju, "
              "sequence=%ju, gap=%ju",
              EKA_EXCH_DECODE(exch), gr_id,
              gr->expected_sequence, sequence,
              sequence - gr->expected_sequence);

      gr->gapClosed = false;
      gr->sendFeedDown(pEfhRunCtx);
      gr->pushUdpPkt2Q(pkt, msgInPkt, sequence);

      gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
      gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx,
                              gr->expected_sequence,
                              sequence);

      break;
      //----------------------------------------
    case EkaFhGroup::GrpState::RETRANSMIT_GAP:
    case EkaFhGroup::GrpState::SNAPSHOT_GAP:
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       */
      // Waiting for recovery
      gr->pushUdpPkt2Q(pkt, msgInPkt, sequence);

      if (!gr->gapClosed)
        break;
      /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       */
      // Gap closed
      if (gr->state == EkaFhGroup::GrpState::SNAPSHOT_GAP)
        lockSnapshotGap.clear();

      gr->expected_sequence = gr->seq_after_snapshot;

      EKA_LOG("%s:%u: %s Closed, switching "
              "to fetch from Q: "
              "expected_sequence = %ju",
              EKA_EXCH_DECODE(exch), gr->id,
              gr->printGrpState(), gr->seq_after_snapshot);

      if (gr->processFromQ(pEfhRunCtx) <
          0) { // gap in the Q
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
      //----------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED "
               "GrpState %u",
               EKA_EXCH_DECODE(exch), gr->id,
               (uint)gr->state);
      break;
    }

#ifdef FH_LAB
  SKIP_PKT:
#endif
    runGr->udpCh->next();
  }
  EKA_LOG("%s RunGroup %u EndOfSession",
          EKA_EXCH_DECODE(exch), runGrId);

  runGr->sendFeedCloseAll(pEfhRunCtx);

#ifdef EKA_NOM_LATENCY_CHECK
  const char *fileName = "NOM_latencies.csv";
  FILE *latenciesFile = fopen(fileName, "w");
  if (!latenciesFile)
    on_error("cannot open %s", fileName);
  for (auto const &p : latencies) {
    fprintf(latenciesFile, "%c,%9ju\n", p.first, p.second);
  }
  fclose(latenciesFile);
#endif
  terminated = true;

  return EKA_OPRESULT__OK;
}
