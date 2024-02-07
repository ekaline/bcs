#ifndef __TEST_TCP_CTX_H__
#define __TEST_TCP_CTX_H__

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

#include "EkaEfcDataStructs.h"
#include "eka_macros.h"
#include <EkaBcs.h>
// #include <Eka.h>
// #include <Epm.h>
// #include <Exc.h>

extern EkaDev *g_ekaDev;
/* --------------------------------------------- */
extern volatile bool keep_work;
extern volatile bool serverSet;

#define MASK32 0xffffffff

/* --------------------------------------------- */
using namespace EkaBcs;

struct TestTcpSess {
  int coreId;
  const char *srcIp;
  const char *dstIp;
  uint16_t dstPort;
};

struct TestTcpParams {
  const TestTcpSess *tcpSess;
  size_t nTcpSess;
};

/* --------------------------------------------- */
static void tcpChild(int sock, uint port) {
  pthread_setname_np(pthread_self(), "tcpServerChild");
  EKA_LOG("Running TCP Server for sock=%d, port = %u", sock,
          port);
  do {
    char line[1536] = {};
    int rc = recv(sock, line, sizeof(line), 0);
    if (rc > 0) {
      char bufStr[10000] = {};
      hexDump2str("received TCP pkt", line, rc, bufStr,
                  sizeof(bufStr));
      EKA_LOG("%s", bufStr);
      fflush(g_ekaLogFile);
      send(sock, line, rc, 0);
    }
  } while (keep_work);
  EKA_LOG("tcpChild thread is terminated");
  fflush(stdout);
  close(sock);
  return;
}
/* --------------------------------------------- */

static void tcpServer(const char *ip, uint16_t port,
                      int *sock, bool *serverSet) {
  pthread_setname_np(pthread_self(), "tcpServerParent");

  EKA_LOG("Starting TCP server: %s:%u", ip, port);
  int sd = 0;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &one_const,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct linger so_linger = {
      true, // Linger ON
      0     // Abort pending traffic on close()
  };

  if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &so_linger,
                 sizeof(struct linger)) < 0)
    on_error("Cant set SO_LINGER");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = be16toh(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    on_error("failed to bind server sock to %s:%u",
             EKA_IP2STR(addr.sin_addr.s_addr),
             be16toh(addr.sin_port));
  if (listen(sd, 20) != 0)
    on_error("Listen");

  *serverSet = true;

  while (keep_work) {
    int addr_size = sizeof(addr);

    *sock = accept(sd, (struct sockaddr *)&addr,
                   (socklen_t *)&addr_size);
    EKA_LOG("Connected from: %s:%d -- sock=%d\n",
            inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),
            *sock);
    int status =
        fcntl(*sock, F_SETFL,
              fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
    if (status == -1)
      on_error("fcntl error");

    do {
      char line[1536] = {};
      int rc = recv(*sock, line, sizeof(line), 0);
      if (rc > 0) {
        char bufStr[10000] = {};
        hexDump2str("received TCP pkt", line, rc, bufStr,
                    sizeof(bufStr));
        EKA_LOG("%s", bufStr);
        fflush(g_ekaLogFile);
        send(*sock, line, rc, 0);
      }
    } while (keep_work);
  }
  EKA_LOG(" -- closing");
  close(*sock);
  close(sd);
  return;
}
/* --------------------------------------------- */

/* --------------------------------------------- */
class TestTcpSessCtx {
public:
  TestTcpSessCtx(const TestTcpSess *conf) {
    coreId_ = conf->coreId;
    srcIp_ = conf->srcIp;
    dstIp_ = conf->dstIp;
    dstPort_ = conf->dstPort;
  }
  /* --------------------------------------------- */

  ~TestTcpSessCtx() {
    tcpServThr_.join();
    excClose(g_ekaDev, hCon_);
  }
  /* --------------------------------------------- */

  void createTcpServ() {
    bool serverSet = false;
    tcpServThr_ = std::thread(tcpServer, dstIp_, dstPort_,
                              &servSock_, &serverSet);
    while (keep_work && !serverSet)
      std::this_thread::yield();
  }
  /* --------------------------------------------- */

  void connect() {
    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(dstIp_);

    serverAddr.sin_port = be16toh(dstPort_);

    excSock_ = excSocket(g_ekaDev, coreId_, 0, 0, 0);
    if (excSock_ < 0)
      on_error("failed on excSocket()");
    hCon_ = excConnect(g_ekaDev, excSock_,
                       (sockaddr *)&serverAddr,
                       sizeof(sockaddr_in));
    if (hCon_ < 0)
      on_error("excConnect to %s:%u",
               EKA_IP2STR(serverAddr.sin_addr.s_addr),
               be16toh(serverAddr.sin_port));

    EKA_LOG("TCP connected %s --> %s:%u", srcIp_,
            EKA_IP2STR(serverAddr.sin_addr.s_addr),
            be16toh(serverAddr.sin_port));
  }
  /* --------------------------------------------- */

  void set1stSeqNum(uint64_t seq) {
    ekaBcSetSessionCntr(g_ekaDev, hCon_, seq);
    return;
  }
  /* --------------------------------------------- */

  void sendTestPkt() {
    char pkt[1500] = {};
    sprintf(pkt,
            "\n\nThis is 1st TCP packet sent from FPGA TCP "
            "client to Kernel TCP server: %s --> %s:%u\n\n",
            srcIp_, dstIp_, dstPort_);

    EKA_LOG("Sending %s", pkt);

    excSend(g_ekaDev, hCon_, pkt, strlen(pkt), 0);
    int bytes_read = 0;
    char rxBuf[2000] = {};
    bytes_read = recv(servSock_, rxBuf, sizeof(rxBuf), 0);
    if (bytes_read > 0)
      EKA_LOG("\n%s", rxBuf);
  }
  /* --------------------------------------------- */

  /* --------------------------------------------- */

public:
  ExcSocketHandle excSock_;
  ExcConnHandle hCon_;

  int coreId_;
  const char *srcIp_;
  const char *dstIp_;
  uint16_t dstPort_;

  int servSock_ = -1;

  std::thread tcpServThr_;
};

/* --------------------------------------------- */
/* --------------------------------------------- */

class TestTcpCtx {
public:
  TestTcpCtx(const TestTcpParams *tcpParams) {
    EKA_LOG("Creating TestTcpCtx with %ju connections",
            tcpParams->nTcpSess);
    nTcpSess_ = tcpParams->nTcpSess;
    for (auto i = 0; i < nTcpSess_; i++) {
      tcpSess_[i] =
          new TestTcpSessCtx(&tcpParams->tcpSess[i]);
      if (!tcpSess_[i])
        on_error("failed on new TestTcpSessCtx()");
    }
  }
  /* --------------------------------------------- */
  ~TestTcpCtx() {
    EKA_LOG("Closing %ju TCP connections", nTcpSess_);
    for (auto i = 0; i < nTcpSess_; i++)
      if (tcpSess_[i])
        delete tcpSess_[i];
  }
  /* --------------------------------------------- */

  void connectAll() {
    for (auto i = 0; i < nTcpSess_; i++) {
      if (!tcpSess_[i])
        on_error("!tcpSess_[%d]", i);

      tcpSess_[i]->createTcpServ();
      tcpSess_[i]->connect();
      tcpSess_[i]->set1stSeqNum(i * 1000); // Place holder
    }
  }
  /* --------------------------------------------- */

  void printConf() {
    fprintf(g_ekaLogFile, "\t%ju TCP sessions:\n",
            nTcpSess_);
    for (auto i = 0; i < nTcpSess_; i++) {
      auto gr = tcpSess_[i];
      fprintf(g_ekaLogFile, "\t\t%d, %s --> %s:%u, ",
              gr->coreId_, gr->srcIp_, gr->dstIp_,
              gr->dstPort_);
    }
    fprintf(g_ekaLogFile, "\n");
  }

  static const int MaxSess = 32;
  size_t nTcpSess_ = 0;
  TestTcpSessCtx *tcpSess_[MaxSess] = {};
};

#endif
