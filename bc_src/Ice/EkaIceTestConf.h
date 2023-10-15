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
//	Trade: secid 6084245, price 57115, size 1, side 1 (B)
//	Trade: secid 6084245, price 57115, size 1, side 1 (B) ts 1587456951044354030

const NwParams nwParams = {
  .srcMac   = {0x10, 0x0e, 0x7e, 0xe1, 0x73, 0x57},
  .srcIp    = "205.209.221.75", 
  .srcPort  = 1234,
  .mcIp     = "224.0.126.6",
  .mcPort   = 20006
};

const PktParams pktParams = {
  .firstTs  = 1000000,
  .incrTs   = 100,
  .firstSeq = 1
};

const MyProd myProd = {
  .secId    = 6084246,
  .midPoint = 7777777, 
  .step     = 10
};

BookEntry bidEntries[] = {
  {static_cast<uint64_t>(57104), 5, 666666}, //TOB
  {static_cast<uint64_t>(57103), 6, 666677},
  {static_cast<uint64_t>(57102), 7, 666688},
  {static_cast<uint64_t>(57101), 8, 666699},
  {static_cast<uint64_t>(57100), 9, 667700}};

BookEntry askEntries[] = {
  {static_cast<uint64_t>(57112), 10, 776666}, //TOB
  {static_cast<uint64_t>(57113), 20, 776677},
  {static_cast<uint64_t>(57114), 30, 776688},
  {static_cast<uint64_t>(57115), 40, 776699}};

Book book = {{
  {std::size(bidEntries), bidEntries},
  {std::size(askEntries), askEntries}
  }};

#endif
