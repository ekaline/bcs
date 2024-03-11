#include "EkaBcs.h"

using namespace EkaBcs;

MoexSecurityId::MoexSecurityId() {
  std::memset(data_, 0, sizeof(data_));
}

MoexSecurityId::MoexSecurityId(const char *name) {
  std::memcpy(data_, name, sizeof(data_));
}

void MoexSecurityId::getName(void *dst) const {
  memcpy(dst, data_, sizeof(data_));
}

std::string MoexSecurityId::getName() const {
  return std::string(data_, sizeof(data_));
}
std::string MoexSecurityId::getSwapName() const {
  // std::string reversed = std::string(data_);
  // std::reverse(reversed.begin(), reversed.end());

  char reversed[12] = {};
  getSwapName(reversed);

  return std::string(reversed);
}

void MoexSecurityId::getSwapName(void *dst) const {
  char *dst_char = (char *)dst;
  for (int i = 0; i < sizeof(data_); i++) {
    dst_char[i] = data_[sizeof(data_) - i - 1];
  }
}

MoexSecurityId &
MoexSecurityId::operator=(const MoexSecurityId &other) {
  if (this != &other)
    std::memcpy(data_, other.data_, sizeof(data_));
  return *this;
}
