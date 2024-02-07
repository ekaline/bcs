#ifndef __TEST_UDP_CTX_H__
#define __TEST_UDP_CTX_H__
#include <arpa/inet.h>
#include <getopt.h>
#include <iterator>
#include <linux/sockios.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <sys/time.h>

#include "EkaBcs.h"
#include "EkaEfcDataStructs.h"
#include "eka_macros.h"
#include <Efc.h>
#include <Eka.h>
#include <Epm.h>
#include <Exc.h>

static const char *udpSrcIp[] = {
    "10.0.0.10",
    "10.0.0.11",
    "10.0.0.12",
    "10.0.0.13",
};
/* --------------------------------------------- */

class TestUdpConn {
public:
  TestUdpConn(const McGroupParams *mcParams) {
    coreId_ = mcParams->lane;
    mcIp_ = mcParams->mcIp;
    mcPort_ = mcParams->mcUdpPort;

    udpSock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSock_ < 0)
      on_error("failed to open UDP sock");

    mcSrc_.sin_addr.s_addr = inet_addr(udpSrcIp[coreId_]);
    mcSrc_.sin_family = AF_INET;
    mcSrc_.sin_port = 0;

    mcDst_.sin_family = AF_INET;
    mcDst_.sin_addr.s_addr = inet_addr(mcIp_);
    mcDst_.sin_port = be16toh(mcPort_);

    if (bind(udpSock_, (sockaddr *)&mcSrc_,
             sizeof(sockaddr)) != 0) {
      on_error("failed to bind server udpSock to %s:%u",
               EKA_IP2STR(mcSrc_.sin_addr.s_addr),
               be16toh(mcSrc_.sin_port));
    }

    EKA_LOG("udpSock %d of MC %s:%u is binded to %s:%u",
            udpSock_, EKA_IP2STR(mcDst_.sin_addr.s_addr),
            be16toh(mcDst_.sin_port),
            EKA_IP2STR(mcSrc_.sin_addr.s_addr),
            be16toh(mcSrc_.sin_port));
  }
  /* -------------------------------------------- */

  size_t printMcConnParams(char *dst) {
    if (!dst)
      return fprintf(g_ekaLogFile, "%d, %s:%u, ", coreId_,
                     mcIp_, mcPort_);
    else
      return sprintf(dst, "%d, %s:%u, ", coreId_, mcIp_,
                     mcPort_);
  }
  /* -------------------------------------------- */

  ssize_t sendUdpPkt(const void *pkt, size_t pktLen) {
#if 1
    char pktBufStr[8192] = {};
    hexDump2str("UdpPkt", pkt, pktLen, pktBufStr,
                sizeof(pktBufStr));
    EKA_LOG("Sending pkt\n%s\n to %s:%u", pktBufStr,
            EKA_IP2STR(mcDst_.sin_addr.s_addr),
            be16toh(mcDst_.sin_port));
#endif
    auto rc =
        sendto(udpSock_, pkt, pktLen, 0,
               (const sockaddr *)&mcDst_, sizeof(sockaddr));

    if (rc < 0)
      on_error("MC trigger send failed");

    return rc;
  }

private:
  EkaBcLane coreId_;

  const char *srcIp_ = nullptr;
  const char *mcIp_ = nullptr;
  uint16_t mcPort_ = 0;

  int udpSock_ = -1;

  struct sockaddr_in mcSrc_ = {};
  struct sockaddr_in mcDst_ = {};
};
/* --------------------------------------------- */

/* --------------------------------------------- */
class TestUdpCtx {
public:
  TestUdpCtx(const UdpMcParams *mcParams) {
    memcpy(&udpConf_, mcParams, sizeof(udpConf_));

    for (auto i = 0; i < mcParams->nMcGroups; i++) {
      auto lane = mcParams->groups[i].lane;
      auto perCoreidx = nMcCons_[lane];
      udpConn_[lane][perCoreidx] =
          new TestUdpConn(&mcParams->groups[i]);
      if (!udpConn_[lane][perCoreidx])
        on_error("Failed on new TestUdpConn");

      nMcCons_[lane]++;
      totalMcCons_++;
    }
  }

  /* --------------------------------------------- */

  size_t printAllMcParams(char *dst) {
    size_t bufLen = 0;
    for (auto lane = 0; lane < EFC_MAX_CORES; lane++)
      for (auto i = 0; i < nMcCons_[lane]; i++) {
        if (!udpConn_[lane][i])
          on_error("!udpConn_[%d][%d]", lane, i);
        bufLen += sprintf(dst, "\t%2d: ", i);
        bufLen += udpConn_[lane][i]->printMcConnParams(
            dst + bufLen);
      }
    bufLen += sprintf(dst, "\n");
    return bufLen;
  }

  /* --------------------------------------------- */
  void printConf() {
    fprintf(g_ekaLogFile, "\t%ju MC groups:\n",
            totalMcCons_);
    for (size_t lane = 0; lane < EFC_MAX_CORES; lane++)
      for (size_t i = 0; i < nMcCons_[lane]; i++) {
        auto gr = udpConn_[lane][i];
        char strBuf[256] = {};
        gr->printMcConnParams(strBuf);
        fprintf(g_ekaLogFile, "\t\t%s, ", strBuf);
      }
    fprintf(g_ekaLogFile, "\n");
  }
  /* --------------------------------------------- */
public:
  UdpMcParams udpConf_ = {};

  TestUdpConn
      *udpConn_[EFC_MAX_CORES]
               [EFC_PREALLOCATED_P4_ACTIONS_PER_LANE] = {};
  size_t nMcCons_[EFC_MAX_CORES] = {};
  size_t totalMcCons_ = 0;
};

#endif
