#pragma once

#include <compat/Eka.h>
#include <compat/Epm.h>
#include <tuple>
#include <vector>

#include "Action.h"
#include "Trigger.h"
#include "common.h"

namespace gts::ekaline {

class HeapManager {
 public:
  bool initialize(EkaDev *device) {
    TotalHeapSize = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_PayloadMemorySize);
    MessageAlignment = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_PayloadAlignment);
    if (MessageAlignment == 0) MessageAlignment = 1;
    MessageHeaderSize = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_DatagramOffset);
    MessageTrailerSize = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_RequiredTailPadding);
    current_ = 0;
    return TotalHeapSize > 0 &&
           MessageAlignment > 0 &&
           MessageHeaderSize >= 0 &&
           MessageTrailerSize >= 0;
  }

  u32 offset() const {
    assert(current_ % MessageAlignment == 0);
    return current_;
  }

  u32 insert(u32 size) {
    assert(current_ % MessageAlignment == 0);
    u32 totalSize = MessageHeaderSize + size + MessageTrailerSize;
    u32 alignedSize = alignUpTo(totalSize, MessageAlignment);
    current_ += alignedSize;
    return current_;
  }

 private:
  u32 current_ = 0;

  u32 TotalHeapSize = 0;
  u32 MessageAlignment = -1;
  u32 MessageHeaderSize = -1;
  u32 MessageTrailerSize = -1;
};

class Strategy {
 public:
  using ReportCbFn = EpmFireReportCb;
  using ActionChain = std::vector<Action>;
  using Scenario = std::pair<Trigger, ActionChain>;

  Strategy(ReportCbFn reportCallback, void *cookie, bitsT enableBits)
      : callback_(reportCallback), cookie_(cookie), enableBits_(enableBits) {}

  int createScenario(Trigger trigger, std::vector<Action> &&actions = {}) {
    scenarios_.emplace_back(trigger, std::move(actions));
    return scenarios_.size() - 1;
  }
  bool addAction(u32 scenarioIdx, Action action) {
    assert(scenarioIdx < scenarios_.size());
    if (scenarioIdx >= scenarios_.size())
      return false;
    scenarios_[scenarioIdx].second.push_back(action);
    return true;
  }

  const Scenario &scenario(int idx) const { return scenarios_.at(idx); }
  const Trigger &trigger(int scenarioIdx) const {
    return scenarios_.at(scenarioIdx).first;
  }
  Action &action(int scenarioIdx, int actionIdx) {
    assert((size_t)scenarioIdx < scenarios_.size());
    Scenario &scenario{scenarios_[scenarioIdx]};
    assert((size_t)actionIdx < scenario.second.size());
    return scenario.second[actionIdx];
  }
  const Scenario *getScenarioForAction(int &combinedActionNo) {
    assert(combinedActionNo >= 0);
    int scenarioIdx = 0;
    int combinedActionCount = 0;
    while ((size_t)scenarioIdx < scenarios_.size() &&
           (size_t)combinedActionNo >= combinedActionCount + scenarios_[scenarioIdx].second.size()) {
      combinedActionCount += scenarios_[scenarioIdx].second.size();
      ++scenarioIdx;
    }
    if ((size_t)scenarioIdx < scenarios_.size()) {
      combinedActionNo -= combinedActionCount;
      return &scenarios_.at(scenarioIdx);
    }
    return nullptr;
  }
  bool validate() {
    // TODO
    return false;
  }

  bool placeMessages(HeapManager &heap) {
    for (auto &scenario : scenarios_) {
      ActionChain &actions{scenario.second};
      for (auto &action : actions) {
        action.message().setHeapOffset(heap.offset());
        if (!heap.insert(action.message().size()))
          return false;
      }
    }
    return true;
  }
  std::tuple<EpmStrategyParams, std::vector<EpmTriggerParams>, std::vector<EpmAction>, bitsT>
  build() const {
    std::vector<EpmTriggerParams> epmTriggers;
    std::vector<EpmAction> epmActions;
    // Copy all triggers and the actions they directly invoke
    for (auto &scenario : scenarios_) {
      epmTriggers.push_back(scenario.first.build());
      assert(scenario.second.size() > 0);
      epmActions.push_back(scenario.second[0].build());
      epmActions.back().nextAction = EPM_LAST_ACTION;
    }
    // Copy the rest of the action chains (the first action from each chain has been handled above)
    for (int idx = 0; idx < (int)scenarios_.size(); ++idx) {
      int lastActionInChainIdx = idx;
      auto &actionChain = scenarios_[idx].second;
      for (int inChainIdx = 1; inChainIdx < (int)actionChain.size(); ++inChainIdx) {
        int thisActionIdx = epmActions.size();
        epmActions[lastActionInChainIdx].nextAction = thisActionIdx;
        lastActionInChainIdx = thisActionIdx;
        epmActions.push_back(actionChain[inChainIdx].build());
      }
      epmActions.back().nextAction = EPM_LAST_ACTION;
    }

    EpmStrategyParams epmStrategy{
        .numActions = (int)epmActions.size(),
        .triggerParams = epmTriggers.data(), //malloc(scenarios_.size() * sizeof(EpmTriggerParams)),
        .numTriggers = epmTriggers.size(),
        .reportCb = callback_,
        .cbCtx = cookie_
    };
    return std::tuple<EpmStrategyParams, std::vector<EpmTriggerParams>, std::vector<EpmAction>, bitsT>{
        epmStrategy, std::move(epmTriggers), std::move(epmActions), enableBits_
    };
  }

 private:
  ReportCbFn  callback_;
  void       *cookie_;
  bitsT       enableBits_;
  std::vector<Scenario> scenarios_;
};

}  // namespace gts::ekaline