#ifndef _EKA_DEV_H
#define _EKA_DEV_H

// SHURIK
#include <atomic>
#include <mutex>
#include <thread>

#include "Efc.h"
#include "Efh.h"
#include "Eka.h"
#include "EkaBcs.h"
#include "EkaHwInternalStructs.h"
#include "eka_hw_conf.h"

#include "eka_sn_addr_space.h"

class EkaEfc;
class EkaEpm;
class EkaCore;
class EkaTcpSess;
class EkaSnDev;
class EkaUserReportQ;
class EkaHwCaps;
class EkaUserChannel;
class EkaIgmp;
class EkaWc;

class EkaMdRecvHandler;

using namespace EkaBcs;

class EkaDev {

public:
  EkaDev(const EkaDevInitCtx *initCtx);
  ~EkaDev();
  int clearHw();

  inline void eka_write(uint64_t addr, uint64_t val) {
#ifdef _VERILOG_SIM
    fprintf(g_ekaVerilogSimFile,
            "efh_write(20'h%jx,64'h%jx);\n", addr, val);
#endif
    uint32_t index = addr / 8;
    if (index >= snDevNumberOfUserLogicRegisters)
      on_error("invalid eka_write to 0x%jx", addr);
    *(snDevUserLogicRegistersPtr + index) = val;
  }

  inline uint64_t eka_read(uint64_t addr) {
    uint32_t index = addr / 8;
    if (index >= snDevNumberOfUserLogicRegisters)
      on_error("invalid eka_read from to 0x%jx", addr);
    return *(snDevUserLogicRegistersPtr + index);
  }

  bool openEpm();
  bool initEpmTx();
  bool checkAndSetEpmTx();

  EkaTcpSess *findTcpSess(uint32_t ipSrc, uint16_t udpSrc,
                          uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess *findTcpSess(int sock);
  EkaTcpSess *findTcpSess(ExcConnHandle hCon);
  EkaTcpSess *getControlTcpSess(EkaCoreId coreId);
  EkaCoreId findCoreByMacSa(const uint8_t *macSa);

  int configurePort(const EkaCoreInitCtx *pCoreInit);
  uint8_t getNumFh();

  static const uint MAX_FEED_HANDLERS = 32;
  static const int8_t MAX_CORES = 4;
  static const uint MAX_RUN_GROUPS = 32;
  static const uint MAX_SESS_PER_CORE = 31;
  static const uint CONTROL_SESS_ID = MAX_SESS_PER_CORE;
  static const uint TOTAL_SESSIONS_PER_CORE =
      MAX_SESS_PER_CORE + 1;
  static const uint NETWORK_PATH_MTU = 1500;
  static const uint MAX_ETH_FRAME_SIZE =
      1536; // NOTE: must be multiple of 64
  static const uint MAX_UDP_CHANNELS = 32;

  static const uint MAX_SEC_CTX = 768 * 1024;
  static const uint MAX_CTX_THREADS = 16;
  static const uint MAX_SESSION_CTX_PER_CORE = 128;

  EkaDev *dev = NULL; // pointer to myself
  EkaSnDev *snDev = NULL;

  // constexpr double EKA_FPGA_FREQUENCY = 10.0 * 66 / 64 /
  // 64 * 1000;

  bool epmEnabled = false;
  EkaUserChannel *epmReport = NULL;
  EkaUserChannel *epmFeedback = NULL;
  EkaUserChannel *lwipRx = NULL;

  uint8_t hwEnabledCores = 0;

  EkaCore *core[MAX_CORES] = {};

  EkaEpm *epm = NULL;
  EpmThreadState epmThr[MAX_CTX_THREADS] = {};

  bool use_vlan = false;

  volatile bool exc_active = false;

  int64_t lwip_sem;
  volatile uint8_t *txPktBuf = NULL;

  pthread_mutex_t tcp_send_mutex;
  pthread_mutex_t tcp_socket_open_mutex;

  std::mutex addTcpSessMtx;
  std::mutex lwipConnectMtx;
  // std::mutex efhGrInitMtx;
  std::mutex igmpJoinMtx;

  struct EfcCtx *pEfcCtx = NULL;
  struct EfcRunCtx *pEfcRunCtx = NULL;
  // struct EfhRunCtx *pEfhRunCtx = NULL;

  void *credContext = NULL;
  void *createThreadContext = NULL;

  volatile bool servThreadActive = false;
  std::thread servThread;
  volatile bool servThreadTerminated = true;

  volatile bool tcpRxThreadActive = false;
  std::thread tcpRxThread;
  volatile bool tcpRxThreadTerminated = true;

  volatile bool fireReportThreadActive = false;
  std::thread fireReportThread;
  volatile bool fireReportThreadTerminated = true;

  volatile bool exc_inited = false;
  volatile bool lwip_inited = false;

  char genIfName[20] = {'U', 'N', 'S', 'E', 'T'};
  uint32_t genIfIp = 0; // INADDR_ANY;

  std::mutex
      mtx; // mutex to protect concurrent dev->numRunGr++

  EkaIgmp *ekaIgmp = NULL;

  EkaLogCallback logCB;
  void *logCtx = NULL;

  EkaAcquireCredentialsFn credAcquire;
  EkaReleaseCredentialsFn credRelease;
  EkaThreadCreateFn createThread;

  volatile int statNumUdpSess[MAX_CORES] = {};
  volatile uint32_t
      statMcGrCore[EKA_MAX_UDP_SESSIONS_PER_CORE]
                  [MAX_CORES] = {};

  EkaHwCaps *ekaHwCaps = NULL;

  int64_t lastErrno = 0;

  std::chrono::system_clock::time_point midnightSystemClock;

  EkaWc *ekaWc = nullptr;

  EkaEfc *efc = nullptr;

  FILE *deltaTimeLogFile = NULL;

  std::atomic<uint64_t> globalFastPathBytes = 0;
  std::atomic<uint64_t> globalDummyBytes = 0;

  volatile uint64_t *snDevUserLogicRegistersPtr = NULL;
  volatile uint64_t *snDevWCPtr = NULL;
  const uint32_t snDevNumberOfUserLogicRegisters = uint32_t(
      0x100000 /* BAR2_REGS_SIZE */ / sizeof(uint64_t));

  AffinityConfig affinityConf = {-1, -1, -1, -1, -1};

  EkaMdRecvHandler *mdRecvH[2] = {};
};

/* ########################################################################
 */

inline void eka_write(EkaDev *dev, uint64_t addr,
                      uint64_t val) {
#ifdef _VERILOG_SIM
  fprintf(g_ekaVerilogSimFile,
          "efh_write(20'h%jx,64'h%jx);\n", addr, val);
  fflush(g_ekaVerilogSimFile);
#endif

  if (dev == NULL)
    fprintf(stderr, "dev == NULL");

  return dev->eka_write(addr, val);
}
#if 1
inline uint64_t eka_read(EkaDev *dev, uint64_t addr) {
  return dev->eka_read(addr);
}
#endif

inline bool eka_is_all_zeros(const void *buf,
                             ssize_t size) {
  uint8_t *b = (uint8_t *)buf;
  for (int i = 0; i < size; i++)
    if (b[i] != 0)
      return false;
  return true;
}

inline void __attribute__((optimize(2)))
copyBuf2Hw(EkaDev *dev, uint64_t dstAddr, uint64_t *srcAddr,
           uint msgSize) {
  //  EKA_LOG("dstAddr=0x%jx, srcAddr=%p,
  //  msgSize=%u",dstAddr,srcAddr,msgSize);
  uint words2write = msgSize / 8 + !!(msgSize % 8);
  for (uint w = 0; w < words2write; w++)
    eka_write(dev, dstAddr + w * 8, *srcAddr++);
}

inline void copyHw2Buf(EkaDev *dev, void *bufAddr,
                       uint64_t srcAddr, uint bufSize) {
  uint64_t *dstAddr = (uint64_t *)bufAddr;
  //  EKA_LOG("dstAddr=%p, srcAddr=0x%jx,
  //  bufSize=%u",dstAddr,srcAddr,msgSize);
  uint words2read = bufSize / 8 + !!(bufSize % 8);
  for (uint w = 0; w < words2read; w++)
    *(dstAddr + w) = eka_read(dev, srcAddr + w * 8);
}

inline void bufSwap4(uint32_t *dst, uint32_t *src,
                     uint size) {
  uint words2write = size / 4 + !!(size % 4);
  for (uint w = 0; w < words2write; w++) {
    (*dst++) = be32toh(*src++);
  }
}

inline void copyBuf2Hw_swap4(EkaDev *dev, uint64_t dstAddr,
                             uint64_t *srcAddr,
                             uint msgSize) {
  //  EKA_LOG("dstAddr=0x%jx, srcAddr=%p,
  //  msgSize=%u",dstAddr,srcAddr,msgSize);
  uint words2write = msgSize / 8 + !!(msgSize % 8);
  for (uint w = 0; w < words2write; w++) {
    uint32_t dataLO = *(uint32_t *)srcAddr;
    uint32_t dataHI = *((uint32_t *)srcAddr + 1);
    uint32_t dataLO_swapped = be32toh(dataLO);
    uint32_t dataHI_swapped = be32toh(dataHI);
    uint64_t res = (((uint64_t)dataHI_swapped) << 32) |
                   (uint64_t)dataLO_swapped;

    eka_write(dev, dstAddr + w * 8, res);
    srcAddr++;
  }
}

// --------------------------------------------------------

inline void
atomicIndirectBufWrite(EkaDev *dev, uint64_t addr,
                       uint8_t bank, uint8_t threadId,
                       uint32_t idx, uint8_t target_table) {
  union scratchpad_table_desc {
    uint64_t lt_desc;
    struct ltd {
      uint8_t src_bank;
      uint8_t src_thread;
      uint32_t target_idx : 24;
      uint8_t pad[2];
      uint8_t
          target_table; // bit[0]: buy/sell book, bit[7:1]:
                        // security index when subscribing
    } __attribute__((packed)) ltd;
  } __attribute__((packed));

  union scratchpad_table_desc desc = {};
  desc.ltd.src_bank = bank;
  desc.ltd.src_thread = threadId;
  desc.ltd.target_idx = idx;
  desc.ltd.target_table = target_table;

  eka_write(dev, addr, desc.lt_desc);
}

inline void indirectWrite(EkaDev *dev, uint64_t addr,
                          uint64_t data) {
  eka_write(dev, 0xf0100, addr);
  eka_write(dev, 0xf0108, data);
}

inline uint64_t indirectRead(EkaDev *dev, uint64_t addr) {
  eka_write(dev, 0xf0100, addr);
  return eka_read(dev, 0xf0108);
}

inline void checkScratchPadAddr(uint64_t addr) {
  if (addr >= SW_SCRATCHPAD_BASE + SW_SCRATCHPAD_SIZE)
    on_error(
        "Out of Scratchpad address space: %jx > "
        "SW_SCRATCHPAD_BASE %jx + SW_SCRATCHPAD_SIZE %jx",
        addr, (uint64_t)SW_SCRATCHPAD_BASE,
        (uint64_t)SW_SCRATCHPAD_SIZE);
}

// ---------------------------------------------------

#endif
