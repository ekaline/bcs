#include <random>

#include "TestP4Rand.h"

/* ############################################# */

TestP4Rand::TestP4Rand() {
#if 0
  outputFile_.open("rand/TestP4Rand.json",
                   std::ofstream::out);
#endif
}
/* ############################################# */

TestP4Rand::~TestP4Rand() {
#if 0
outputFile_.close();
#endif
}
/* ############################################# */

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

  std::string charSet = "0123456789"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz";

  // Seed the random number generator
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(
      0, charSet.size() - 1);

  for (auto i = 2; i < 6; i++) {
    int randomIndex = dist(gen);
    dst += charSet[randomIndex];
  }
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
static bool
alreadyExists(std::vector<TestP4CboeSec> &secVec,
              std::string secId) {
  for (auto const &s : secVec)
    if (s.strId == secId)
      return true;
  return false;
}
/* ############################################# */
static std::string
generateUniqValidSecId(std::vector<TestP4CboeSec> &secVec) {
  while (1) {
    auto secId = generateValidSecId();
    if (!alreadyExists(secVec, secId))
      return secId;
#if 0
    EKA_LOG("Regenerating previously existing \'%s\'",
            secId.c_str());
#endif
  }
}
/* ############################################# */

void TestP4Rand::createSecList(
    const void *algoConfigParams) {
  auto secConf = reinterpret_cast<const TestP4SecConf *>(
      algoConfigParams);

  nValidSecs_ = 0;

  for (auto i = 0; i < secConf->nSec; i++) {
    char secStrId[8] = {};

    TestP4CboeSec sec = {};
    sec.valid =
        getRandomBoolean(secConf->percentageValidSecs);
    sec.strId = sec.valid ? generateUniqValidSecId(allSecs_)
                          : generateInValidSecId();
    sec.binId = getBinSecId(sec.strId);
    sec.bidMinPrice = sec.valid
                          ? static_cast<TestP4SecCtxPrice>(
                                1 + rand() % 0x7FFF)
                          : static_cast<TestP4SecCtxPrice>(
                                -1) /* always pass */;
    sec.askMaxPrice = sec.valid
                          ? static_cast<TestP4SecCtxPrice>(
                                1 + rand() % 0x7FFF)
                          : static_cast<TestP4SecCtxPrice>(
                                0) /* always pass */;
    sec.size = 1; // TBD
    allSecs_.push_back(sec);

    secList_[nSec_++] = sec.binId;
    if (sec.valid)
      nValidSecs_++;
  }

  EKA_LOG("Created List of Total %ju (Valid %ju) P4 "
          "Securities:",
          allSecs_.size(), nValidSecs_);
  for (const auto &sec : allSecs_)
    EKA_LOG("\t\'%.8s\', 0x%jx (%s)", sec.strId.c_str(),
            sec.binId, sec.valid ? "VALID" : "INVALID");
}

/* ############################################# */

void TestP4Rand::archiveSecCtxsMd() {
  std::ofstream outputFile("rand/SecCtxsMd.json");
  cereal::JSONOutputArchive archive(outputFile);
  archive(CEREAL_NVP(allSecs_));
  archive(CEREAL_NVP(insertedMd_));
};
/* ############################################# */

/* ############################################# */

void TestP4Rand::generateMdDataPkts(const void *unused) {
  for (const auto &sec : allSecs_) {

    auto side =
        getRandomBoolean(0.5) ? SideT::BID : SideT::ASK;

    TestP4Md md = {
        .secId = sec.strId,
        .side = side,
        .price = side == SideT::BID
                     ? static_cast<TestP4MdPrice>(
                           sec.bidMinPrice * 100 + 1)
                     : static_cast<TestP4MdPrice>(
                           sec.askMaxPrice * 100 - 1),
        .size = sec.size,
        .expectedFire = sec.valid && sec.handle >= 0};
#if 0
    TEST_LOG("%s: sec.bidMinPrice = %u, md.price = %u",
             md.secId.c_str(), sec.bidMinPrice, md.price);
#endif
    insertedMd_.push_back(md);
  }
}
/* ############################################# */
