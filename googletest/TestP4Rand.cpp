#include <random>

#include "TestP4Rand.h"

static bool getRandomBoolean(double probability) {
  // Seed the random number generator
  std::random_device rd;
  std::mt19937 gen(rd());

  // Create a distribution with the specified probability
  std::bernoulli_distribution dist(probability);

  // Generate a random Boolean value based on the
  // probability
  return dist(gen);
}

static void generateValidSecId(char *dst) {}
static void generateInvalidSecId(char *dst) {}

void TestP4Rand::createSecList(
    const void *algoConfigParams) {
  auto secConf = reinterpret_cast<const TestP4SecConf *>(
      algoConfigParams);

  for (auto i = 0; i < secConf->nSec; i++) {
    char secStrId[8] = {};

    if (getRandomBoolean(secConf->percentageValidSecs))
      generateValidSecId(secStrId);
    else
      generateInvalidSecId(secStrId);
    secList_[i] = getBinSecId(secConf->sec[i].id);
  }
  nSec_ = secConf->nSec;
  EKA_LOG("Created List of %ju P4 Securities:", nSec_);
  for (auto i = 0; i < nSec_; i++)
    EKA_LOG("\t\'%.8s\', 0x%jx %c",
            cboeSecIdString(secConf->sec[i].id, 8).c_str(),
            secList_[i], i == nSec_ - 1 ? '\n' : ',');
}
