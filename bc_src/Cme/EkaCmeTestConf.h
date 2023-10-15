#ifndef _EKA_TEST_CONF_H_
#define _EKA_TEST_CONF_H_


struct NwParams {
  uint8_t      srcMac[6];
  const char*  srcIp;
  uint16_t     srcPort;
  const char*  mcIp;
  uint16_t     mcPort;
};

struct MyProd {
  uint64_t     secId;
  uint64_t     midPoint;
  uint64_t     multiplier;
  uint64_t     step;
};

struct BookEntry {
  uint64_t price;
  uint32_t size;
  uint64_t ts;
};


struct BookSide {
  size_t     numEntries;
  BookEntry* entry;
};

struct Book {
  BookSide side[2];
};

typedef enum {BID = 0, ASK = 1} SIDE;

struct PktParams {
  uint64_t firstTs;
  uint64_t incrTs;
  uint64_t firstSeq;
};

/* ---------------------------------- */
//price:792350 000000000 size:2 secid:36742 side:1

const NwParams nwParams = {
  .srcMac   = {0x10, 0x0e, 0x7e, 0xe1, 0x73, 0x57},
  .srcIp    = "205.209.221.75", 
  .srcPort  = 1234,
  .mcIp     = "224.0.31.9",
  .mcPort   = 14318
};

const PktParams pktParams = {
  .firstTs  = 1000000,
  .incrTs   = 100,
  .firstSeq = 1
};

const MyProd myProd = {
  .secId    = 36742,
  .midPoint = 7777777, 
  .multiplier = static_cast<uint64_t>(1e9),
  .step     = static_cast<uint64_t>(25e9)
};

BookEntry bidEntries[] = {
  {static_cast<uint64_t>(792350*myProd.multiplier-1*myProd.step), 6, 666677}, //tob
  {static_cast<uint64_t>(792350*myProd.multiplier-2*myProd.step), 7, 666688},
  {static_cast<uint64_t>(792350*myProd.multiplier-3*myProd.step), 8, 666699},
  {static_cast<uint64_t>(792350*myProd.multiplier-4*myProd.step), 9, 667700}};

BookEntry askEntries[] = {
  {static_cast<uint64_t>(792350*myProd.multiplier+0*myProd.step), 10, 776666}, //tob
  {static_cast<uint64_t>(792350*myProd.multiplier+1*myProd.step), 20, 776677},
  {static_cast<uint64_t>(792350*myProd.multiplier+2*myProd.step), 30, 776688},
  {static_cast<uint64_t>(792350*myProd.multiplier+3*myProd.step), 40, 776699}};

Book book = {{
  {std::size(bidEntries), bidEntries},
  {std::size(askEntries), askEntries}
  }};

#endif
