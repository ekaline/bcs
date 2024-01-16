#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define on_error(...)                                      \
  {                                                        \
    fprintf(stderr, "FATAL ERROR: %s@%s:%d: ", __func__,   \
            __FILE__, __LINE__);                           \
    fprintf(stderr, __VA_ARGS__);                          \
    fprintf(stderr, "\n");                                 \
    perror("");                                            \
    fflush(stdout);                                        \
    fflush(stderr);                                        \
    exit(1);                                               \
  }
#define TEST_LOG(...)                                      \
  {                                                        \
    fprintf(stderr, "%s@%s:%d: ", __func__, __FILE__,      \
            __LINE__);                                     \
    fprintf(stderr, __VA_ARGS__);                          \
    fprintf(stderr, "\n");                                 \
  }

#define EKA_IP2STR(x)                                      \
  ((std::to_string((x >> 0) & 0xFF) + '.' +                \
    std::to_string((x >> 8) & 0xFF) + '.' +                \
    std::to_string((x >> 16) & 0xFF) + '.' +               \
    std::to_string((x >> 24) & 0xFF))                      \
       .c_str())

void print_usage(char *cmd) {
  printf("USAGE: %s -m [MC IP] -p [MC Port] -i [Local IP "
         "to bind]\n",
         cmd);
  exit(1);
}
volatile bool keep_work = true;

int sock_fd = -1;

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  printf("Ctrl-C detected, quitting\n");
  keep_work = false;
  shutdown(sock_fd, SHUT_RD);
}

int main(int argc, char *argv[]) {
  uint32_t localIP = 0;
  uint32_t mcIP = 0;
  uint16_t mcPort = 0;

  signal(SIGINT, INThandler);

  /* ------------------------------------------------------------------
   */
  int opt;
  while ((opt = getopt(argc, argv, ":i:p:m:h")) != -1) {
    switch (opt) {
    case 'm':
      mcIP = inet_addr(optarg);
      printf("mcIP = %s\n", EKA_IP2STR(mcIP));
      break;
    case 'i':
      localIP = inet_addr(optarg);
      printf("localIP = %s\n", EKA_IP2STR(localIP));
      break;
    case 'p':
      mcPort = atoi(optarg);
      printf("localIf.Port: = %u\n", mcPort);
      break;
    case 'h':

    default:
      print_usage(argv[0]);
      return 1;
      break;
    }
  }
  /* ------------------------------------------------------------------
   */

  if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    on_error("cannot open UDP socket");

  int const_one = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
                 &const_one, sizeof(int)) < 0)
    on_error("%s: setsockopt(SO_REUSEADDR) failed\n",
             __func__);
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT,
                 &const_one, sizeof(int)) < 0)
    on_error("%s: setsockopt(SO_REUSEPORT) failed\n",
             __func__);

  struct sockaddr_in local2bind = {};
  local2bind.sin_family = AF_INET;
  local2bind.sin_addr.s_addr = INADDR_ANY;
  local2bind.sin_port = be16toh(mcPort);

  if (bind(sock_fd, (struct sockaddr *)&local2bind,
           sizeof(struct sockaddr)) < 0)
    on_error("cannot bind UDP socket to port %d\n",
             be16toh(local2bind.sin_port));

  struct ip_mreq mreq = {};
  mreq.imr_interface.s_addr = localIP;
  mreq.imr_multiaddr.s_addr = mcIP;
  TEST_LOG("joining IGMP %s from local addr %s ",
           EKA_IP2STR(mreq.imr_multiaddr.s_addr),
           EKA_IP2STR(mreq.imr_interface.s_addr));

  if (setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                 &mreq, sizeof(mreq)) < 0)
    on_error("fail to join IGMP  %s:%d\n",
             inet_ntoa(mreq.imr_multiaddr),
             be16toh(local2bind.sin_port));

  uint64_t pktCnt = 0;
  while (keep_work) {
    char buffer[1536] = {};
    socklen_t addrlen = sizeof(struct sockaddr);
    struct sockaddr_in from = {};
    int rc = recvfrom(sock_fd, buffer, sizeof(buffer), 0,
                      (struct sockaddr *)&from, &addrlen);
    if (rc < 0 && errno != EAGAIN)
      on_error("ERROR READING FROM SOCKET");

    printf("%s:%u : \n", EKA_IP2STR(from.sin_addr.s_addr),
           be16toh(from.sin_port));
#if 0
    if (++pktCnt % 10 == 0)
      TEST_LOG("received %ju packets", pktCnt);
#endif
  }
  close(sock_fd);

  return 0;
}
