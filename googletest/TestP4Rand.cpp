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
/* ############################################# */

static std::string generateValidSecId() {
  std::string dst = {};
  dst += '0';
  dst += '0' + rand() % 4;
  for (auto i = 2; i < 6; i++)
    dst += 'a' + rand() % ('z' - 'a' + 1);
  return dst;
}
/* ############################################# */

static std::string generateInValidSecId() {
  std::string dst = {};
  dst += '5'; // makes the sec ID to be invalid
  dst += '0' + rand() % 4;
  for (auto i = 2; i < 6; i++)
    dst += 'a' + rand() % ('z' - 'a' + 1);
  return dst;
}

/* ############################################# */

void TestP4Rand::createSecList(
    const void *algoConfigParams) {
  auto secConf = reinterpret_cast<const TestP4SecConf *>(
      algoConfigParams);

  for (auto i = 0; i < secConf->nSec; i++) {
    char secStrId[8] = {};

    TestP4CboeSec sec = {};
    if (getRandomBoolean(secConf->percentageValidSecs)) {
      sec.valid = true;
      sec.strId = generateValidSecId();
      sec.binId = getBinSecId(sec.strId);
      sec.bidMinPrice = static_cast<FixedPrice>(rand());
      sec.askMaxPrice = static_cast<FixedPrice>(rand());
      sec.size = 1; // TBD
      validSecs_.push_back(sec);
    } else {
      sec.valid = false;
      sec.strId = generateInValidSecId();
      sec.binId = getBinSecId(sec.strId);
      sec.bidMinPrice = -1; // always pass
      sec.askMaxPrice = 0;  // always pass
      sec.size = 1;         // TBD
      inValidSecs_.push_back(sec);
    }
    secList_[nSec_++] = sec.binId;
  }

  EKA_LOG("Created List of %ju Valid P4 Securities:",
          validSecs_.size());
  for (const auto &sec : validSecs_)
    EKA_LOG("\t\'%.8s\', 0x%jx,", sec.strId.c_str(),
            sec.binId);

  EKA_LOG("Created List of %ju InValid P4 Securities:",
          inValidSecs_.size());
  for (const auto &sec : inValidSecs_)
    EKA_LOG("\t\'%.8s\', 0x%jx,", sec.strId.c_str(),
            sec.binId);
}

/* ############################################# */

void TestP4Rand::initializeAllCtxs(
    const TestCaseConfig *unused) {
  int i = 0;
  for (auto &sec : validSecs_) {
    sec.handle = getSecCtxHandle(g_ekaDev, sec.binId);

    SecCtx secCtx = {};
    setSecCtx(&sec, &secCtx);

    EKA_LOG(
        "Setting StaticSecCtx[%ju] \'%s\' secId=0x%016jx,"
        "handle=%jd,bidMinPrice=%u,askMaxPrice=%u,"
        "bidSize=%u,askSize=%u,"
        "versionKey=%u,lowerBytesOfSecId=0x%x",
        i, sec.strId.c_str(), sec.binId, sec.handle,
        secCtx.bidMinPrice, secCtx.askMaxPrice,
        secCtx.bidSize, secCtx.askSize, secCtx.versionKey,
        secCtx.lowerBytesOfSecId);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    if (sec.handle >= 0) {
      auto rc = efcSetStaticSecCtx(g_ekaDev, sec.handle,
                                   &secCtx, 0);
      if (rc != EKA_OPRESULT__OK)
        on_error("failed to efcSetStaticSecCtx");
    }
    i++;
  }
}

/* ############################################# */

void TestP4Rand::generateMdDataPkts(const void *unused) {
  for (const auto &sec : validSecs_) {
    TestP4Md md = {.secId = sec.strId,
                   .side = SideT::BID,
                   .price = static_cast<FixedPrice>(
                       sec.bidMinPrice + 1),
                   .size = sec.size,
                   .expectedFire = true};

    insertedMd_.push_back(md);
  }
}
/* ############################################# */
