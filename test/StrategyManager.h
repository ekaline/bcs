#pragma once

#include <compat/Efc.h>
#include <compat/Eka.h>
#include <compat/Epm.h>
#include <string>
#include <vector>

#include "common.h"
#include "Strategy.h"

namespace gts::ekaline {

class StrategyManager {
 public:
  static constexpr int MIN_STRATEGY_ACTIONS = 256;

  struct EpmStrategyBuilder {
    using bitsT = epm_enablebits_t;

    bool build(EkaDev *device, std::vector<Strategy> &strategies) {
      HeapManager heap;
      heap.initialize(device);
      for (auto &strategy : strategies) {
        bool result = strategy.placeMessages(heap);
        if (!result)
          return false;
        // Bad
      }

//    auto appendMovedVector = template<typename T> [](std::vector<T> &dst, std::vector<T> &&src) {
//      auto oldDstSize = dst.size();
//      dst.resize(oldDstSize + src.size());
//      for (size_t idx = 0; idx < src.size(); ++idx)
//      std::swap(dst[oldDstSize + idx], src[idx]);
//    };
      for (auto &strategy : strategies) {
        auto tuple{strategy.build()};
        epmStrategies.push_back(std::get<0>(tuple));
        epmTriggers.push_back(std::move(std::get<1>(tuple)));
        epmActions.push_back(std::move(std::get<2>(tuple)));
//      appendMovedVector(epmTriggers, std::get<1>(tuple));
//      appendMovedVector(epmActions, std::get<2>(tuple));

        enableBits.push_back(std::get<3>(tuple));
      }
      return true;
    }

    std::vector<EpmStrategyParams> epmStrategies;
    std::vector<std::vector<EpmTriggerParams>> epmTriggers;
    std::vector<std::vector<EpmAction>> epmActions;
    std::vector<bitsT> enableBits;
  };

  bool deployStrategies(EkaDev *device, EkaCoreId phyPort, EfcCtx *efcCtx, std::vector<Strategy> &strategies) {
    WARN("deploy strategies (dev, port %d, , {} * %d, , )", phyPort, strategies.size());
    EpmStrategyBuilder builder;
    if (!builder.build(device, strategies)) {
      ERROR("Failed to serialize EPM strategies for ekaline API");
      return false;
    } else {
      int strategyCount = (int)strategies.size();
      int epmStrategyCount = (int)builder.epmStrategies.size();
      INFO("%d strategies, %d epm ones\n\tfirst callback: <fn %p, cts %p>",
           strategyCount, epmStrategyCount,
           epmStrategyCount ? builder.epmStrategies[0].reportCb : nullptr,
           epmStrategyCount ? builder.epmStrategies[0].cbCtx : nullptr);
    }

    if (0) {
      if (!isResultOk(epmEnableController(device, phyPort, false))) {
        ERROR("Failed to disable EPM for port %d", phyPort);
        return false;
      }
    }

    int strategyIdx = 0;
    for (auto &strategy: builder.epmStrategies) {
      if (strategy.numActions < MIN_STRATEGY_ACTIONS) {
        WARN("Strategy %d has %d actions, bumping up to %d", strategyIdx, strategy.numActions, MIN_STRATEGY_ACTIONS);
        strategy.numActions = MIN_STRATEGY_ACTIONS;
      }
    }
    auto result = epmInitStrategies(device, builder.epmStrategies.data(), builder.epmStrategies.size());
    if (!isResultOk(result)) {
      ERROR("Failed to epmInitStrategies(_, _, %ld, _) with %d", builder.epmStrategies.size(), result);
      return false;
    }

    EfcStratGlobCtx efcStratGlobCtx = {
        .enable_strategy      = 1,
        .report_only          = 0,
        .watchdog_timeout_sec = 60,
    };
    result = efcInitStrategy(efcCtx, &efcStratGlobCtx);
    if (!isResultOk(result)) {
      ERROR("Failed to init (default?) strategy; error = %d", result);
      return false;
    }

    bool failed = false;
    for (int strategyIdx = 0; !failed && (strategyIdx < (int)builder.epmStrategies.size()); ++strategyIdx) {
      Strategy &strategy = strategies[strategyIdx];
      auto &strategyActions{builder.epmActions[strategyIdx]};
      WARN("Strategy[%d, %d]: actions", strategyIdx, strategyActions.size());
      for (int actionIdx = 0; actionIdx < (int)strategyActions.size(); ++actionIdx) {
        EpmAction &epmAction{strategyActions[actionIdx]};
        int actionIndexInScenario = actionIdx;
        const Strategy::Scenario *scenario = strategy.getScenarioForAction(actionIndexInScenario);
        if (!scenario) {
          ERROR("Failed to retrieve scenario #%d in action #%d", actionIndexInScenario, strategyIdx);
          return false;
        }
        const Message &message{scenario->second.at(actionIndexInScenario).message()};
        WARN("Uploading payload (device, idx %d, ofs %d, size %d, _)",
             strategyIdx, message.heapOffset(), message.size());
        usleep(200);
        EkaOpResult result = epmPayloadHeapCopy(device, strategyIdx,
                                                message.heapOffset(), message.size(), message.data());
        if (!isResultOk(result)) {
          ERROR("Failed to upload payload (device, idx %d, ofs %d, size %d, _) with %d",
                strategyIdx, message.heapOffset(),message.size(), (int)result);
          failed = true;
          break;
        }
        if (!isResultOk(epmSetAction(device, strategyIdx, actionIdx, &epmAction))) {
          ERROR("Failed to set { action %d, for strategy %d, type %d, ofs %d, , size %d, , }",
                actionIdx, strategyIdx, (int)epmAction.type, epmAction.offset, epmAction.length);
          failed = true;
          break;
        } else {
          INFO("EPM action set (strategy %d, action %d, {type %d, token 0x%lx, ofs %d, len %d, valid %d, enable 0x%x})",
               strategyIdx, actionIdx, epmAction.type, epmAction.token, epmAction.offset, epmAction.length,
               epmAction.actionFlags, epmAction.enable);
        }
      }
      if (!isResultOk(epmSetStrategyEnableBits(device, strategyIdx, builder.enableBits[strategyIdx]))) {
        ERROR("Failed to enable strategy %d, bits %s", strategyIdx, toHexString(builder.enableBits[strategyIdx]).c_str());
        failed = true;
        break;
      } else {
        INFO("strategy enable bits set (strategy %d, bits %s)", strategyIdx, toHexString(builder.enableBits[strategyIdx]).c_str());
      }
    }

    bool deployed = false;
    if (!failed) {
      deployed = true;//isResultOk(epmEnableController(device, phyPort, true));
      if (!deployed)
        ERROR("Failed to epmEnableController(dev, port %d, enable true)", phyPort);
    } else {
      INFO("EPM controller enabled on port %d", phyPort);
    }
    return deployed;
  }
};

}  // namespace gts::ekaline