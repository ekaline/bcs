#pragma once

#include <compat/Eka.h>
#include <compat/Epm.h>
#include <string>
#include <vector>

#include "common.h"
#include "Strategy.h"

namespace gts::ekaline {

class StrategyManager {
 public:
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

  bool deployStrategies(EkaDev *device, EkaCoreId phyPort, std::vector<Strategy> &strategies) {
    WARN("deploy strategies (dev, port ", phyPort, ", {} * ", strategies.size(), ")");
    EpmStrategyBuilder builder;
    if (!builder.build(device, strategies)) {
      ERROR("Failed to serialize EPM strategies for ekaline API");
      return false;
    }

    if (!isResultOk(epmEnableController(device, phyPort, false))) {
      // Warn
    }

    auto result = epmInitStrategies(device, builder.epmStrategies.data(), builder.epmStrategies.size());
    if (!isResultOk(result)) {
      ERROR("Failed to epmInitStrategies(_, _, ", builder.epmStrategies.size(), ") with ", result);
      return false;
    }

    bool failed = false;
    for (int strategyIdx = 0; !failed && (strategyIdx < (int)builder.epmStrategies.size()); ++strategyIdx) {
      Strategy &strategy = strategies[strategyIdx];
      auto &strategyActions{builder.epmActions[strategyIdx]};
      WARN("Strategy[", strategyIdx, "]: ", strategyActions.size(), " actions");
      for (int actionIdx = 0; actionIdx < (int)strategyActions.size(); ++actionIdx) {
        EpmAction &epmAction{strategyActions[actionIdx]};
        int actionIndexInScenario = actionIdx;
        const Strategy::Scenario *scenario = strategy.getScenarioForAction(actionIndexInScenario);
        if (!scenario) {
          ERROR("Failed to retrieve scenario #", actionIndexInScenario, " in action #", strategyIdx);
          return false;
        }
        const Message &message{scenario->second.at(actionIndexInScenario).message()};
        WARN("Uploading payload (device, idx ", strategyIdx, ", ofs ", message.heapOffset(),
             ", size ", message.size(), ") with ", (int)result);
        usleep(100);
        EkaOpResult result = epmPayloadHeapCopy(device, strategyIdx, message.heapOffset(), message.size(), message.data());
        if (!isResultOk(result)) {
          ERROR("Failed to upload payload (device, idx ", strategyIdx, ", ofs ", message.heapOffset(),
                ", size ", message.size(), ") with ", (int)result);
          failed = true;
          break;
        }
        if (!isResultOk(epmSetAction(device, strategyIdx, actionIdx, &epmAction))) {
          ERROR("Failed to set { action ", actionIdx, ", for strategy ", strategyIdx, ", type ", (int)epmAction.type,
                ", ofs ", epmAction.offset, ", size ", epmAction.length, "}");
          failed = true;
          break;
        }
      }
      if (!isResultOk(epmSetStrategyEnableBits(device, strategyIdx, builder.enableBits[strategyIdx]))) {
        ERROR("Failed to enable strategy ", strategyIdx, ", bits ", toHexString(builder.enableBits[strategyIdx]).c_str());
        failed = true;
        break;
      }
    }

    bool deployed = false;
    if (!failed) {
      deployed = isResultOk(epmEnableController(device, phyPort, true));
      if (!deployed)
        ERROR("Failed to epmEnableController(dev, port ", phyPort, ", enable true)");
    }
    return deployed;
  }
};

}  // namespace gts::ekaline