#pragma once

#include <compat/Eka.h>
#include <compat/Epm.h>
#include <sstream>
#include <stdint.h>
#include <string>

namespace gts {

using s8  = int8_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

using bitsT = epm_enablebits_t;

void doLog(file, format, ...) {
  va_arg args;
  va_start(args, format);
  vfprintf(file, format, va_args);
  va_end(args);
}

#define ERROR(fmt, ...) do { \
  doLog(stderr, "[ERROR] %s:%d " fmt, __FILE__, __LINE__, __VA_ARGS__); \
} while (0)
#define WARN(fmt, ...) do { \
  doLog(stderr, "[WARNING] %s:%d " fmt, __FILE__, __LINE__, __VA_ARGS__); \
} while (0)


template <typename T>
T alignUpTo(T value, T alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

template <typename T>
std::string toHexString(T t) {
  std::stringstream stream{"0x"};
  stream << std::hex << t;
  return stream.str();
}

namespace ekaline {

bool operator==(const EpmTriggerParams &a, const EpmTriggerParams &b) {
  return a.coreId == b.coreId &&
         a.mcUdpPort == b.mcUdpPort &&
         std::string(a.mcIp).compare(b.mcIp) == 0;
}
bool operator!=(const EpmTriggerParams &a, const EpmTriggerParams &b) {
  return !(a == b);
}

static bool isResultOk(EkaOpResult result) {
  return result == EKA_OPRESULT__OK || result == EKA_OPRESULT__ALREADY_INITIALIZED;
}

}  // namespace ekaline
}  // namespace gts

// Defined in the top-level namespace due to ADL (Epm* are defined in this ns).
inline std::ostream &operator<<(std::ostream &out, const EpmTriggerParams &trg) {
  return out << "EpmTrigger{ phyPort:" << trg.coreId << ", UDP: " << trg.mcIp << ":" << trg.mcUdpPort << " }";
}
inline std::ostream &operator<<(std::ostream &out, const EpmTriggerAction &action) {
  std::string_view value;
  switch (action) {
    case Unknown:
      value = "Unknown"; break;
    case Sent:
      value = "Sent"; break;
    case InvalidToken:
      value = "InvalidToken"; break;
    case InvalidStrategy:
      value = "InvalidStrategy"; break;
    case InvalidAction:
      value = "InvalidAction"; break;
    case DisabledAction:
      value = "DisabledAction"; break;
    case SendError:
      value = "SendError"; break;
  }
  return out << value;
}
inline std::ostream &operator<<(std::ostream &out, const EpmFireReport &r) {
  out << "EpmFire{trigger: " << (r.local ? "local" : "HW") << ", action: " << r.action
      << ", strategy_id: " << r.strategyId << ", action_id: " << r.actionId
      << "\n\tactionTrigger: " << "_" << ", result: " << r.error
      <<"\n\taction masks {pre " << gts::toHexString(r.preLocalEnable) << ", post " << gts::toHexString(r.postLocalEnable)
      << " }, startegy masks {pre" << r.preStratEnable << ", post " << r.postStratEnable << " }"
      << "\n\tuser cookie: " << r.user << " }";
  return out;
}
