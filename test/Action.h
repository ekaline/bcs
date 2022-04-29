#pragma once

#include <arpa/inet.h>
#include <compat/Eka.h>
#include <compat/Epm.h>
#include <compat/Exc.h>
#include <ostream>
#include <string>

#include "common.h"

namespace gts::ekaline {

class Message {
 public:
  Message(const std::string payload, int heapOffset = -1)
      : payload_(payload), heapOffset_(heapOffset) {}

  u32 heapOffset() const { return heapOffset_; }
  void setHeapOffset(int offset) { heapOffset_ = offset; }
  u32 size() const { return payload_.size(); }
  void const *data() const { return payload_.data(); }
  const std::string &payload() const { return payload_; }

 private:
  std::string payload_;
  u32 heapOffset_;
};
std::ostream &operator<<(std::ostream &out, const Message &msg) {
  out << "Message{" << msg.size() << "@" << msg.heapOffset() << " : '"
      <<  msg.payload() << "'}";
  return out;
}

class Action {
 public:
  static constexpr epm_token_t
  ACTION_TOKEN = 0xf001cafedeadbabe;

  Action(EpmActionType type, ExcConnHandle connection, Message message,
         u32 flags, bitsT enableBits, bitsT actionMask, bitsT strategyMask, uintptr_t cookie)
      : type_(type), connection_(connection), message_(message), flags_(flags),
        enableBits_(enableBits), actionMask_(actionMask), strategyMask_(strategyMask) {}

  Message &message() { return message_; }

  const Message &message() const { return message_; }

  EpmAction build() const {
    EpmAction action{
        .type = type_,
        .token = ACTION_TOKEN,
        .hConn = connection_,
        .offset = message_.heapOffset(),
        .length = message_.size(),
        .actionFlags = flags_,
        .nextAction = -1,
        .enable = enableBits_,
        .postLocalMask = actionMask_,
        .postStratMask = strategyMask_,
        .user = cookie_
    };
    return action;
  }

 private:
  EpmActionType type_;
  ExcConnHandle connection_;
  Message message_;
  u32 flags_;
  bitsT enableBits_;
  bitsT actionMask_;
  bitsT strategyMask_;
  uintptr_t cookie_;
};

}  // namespace gts::ekaline