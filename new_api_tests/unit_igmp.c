#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <thread>
#include <assert.h>
#include <string>
#include <algorithm>
#include <fstream>

#define on_error(...)   { fprintf(stderr, "FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }
#define on_warning(...) { fprintf(stderr, "WARNING: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); }

#define EKA_IP2STR(x) ((std::to_string((x >> 0) & 0xFF) + '.' + std::to_string((x >> 8) & 0xFF) + '.' + std::to_string((x >> 16) & 0xFF) + '.' + std::to_string((x >> 24) & 0xFF)).c_str())

static bool keep_work = false;

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected\n",__func__);
  fflush(stdout);
  return;
}

void print_usage(char* cmd) {
  printf("USAGE: %s -s <NIC IP to subscribe from> -m <MCAST IP to subscribe to>\n",cmd); 
  return;
}

int main(int argc, char **argv) {
  if (argc != 5) {
    print_usage(argv[0]); 
    return 1;
  }

  int sock_fd;
  if ((sock_fd = socket(AF_INET,SOCK_DGRAM,0)) < 0) on_error("cannot open UDP socket");

  struct ip_mreq mreq = {};
  std::string source = {};
  std::string mcast = {};

  int opt; 
  while((opt = getopt(argc, argv, ":s:m:h")) != -1) {  
    switch(opt) {  
      case 's':  
	source = std::string(optarg);
	mreq.imr_interface.s_addr = inet_addr(source.c_str());
	break;
      case 'm':  
	mcast = std::string(optarg);
	mreq.imr_multiaddr.s_addr = inet_addr(mcast.c_str());
	break;
      case 'h':  
	print_usage(argv[0]);
	return 0;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  if (mreq.imr_interface.s_addr == 0) on_error("Source IP address is not specified");
  if (mreq.imr_multiaddr.s_addr == 0) on_error("MCAST IP address is not specified");

  int const_one = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &const_one, sizeof(int)) < 0) on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &const_one, sizeof(int)) < 0) on_error("setsockopt(SO_REUSEPORT) failed");

  struct sockaddr_in local2bind = {};
  local2bind.sin_family=AF_INET;
  local2bind.sin_addr.s_addr = INADDR_ANY;
  local2bind.sin_port = 0;
  if (bind(sock_fd,(struct sockaddr*) &local2bind, sizeof(struct sockaddr)) < 0) on_error("cannot bind UDP socket to port %u",be16toh(local2bind.sin_port));

  printf("joining SW IGMP from local addr %s to %s\n",EKA_IP2STR(*(uint32_t*)&mreq.imr_interface),EKA_IP2STR(*(uint32_t*)&mreq.imr_multiaddr));

  if (setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) on_warning("fail to join IGMP for MC addr: %s",EKA_IP2STR(*(uint32_t*)&mreq.imr_multiaddr));

  keep_work = true;
  signal(SIGINT, INThandler);

  while (keep_work) {}
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  printf ("%s: leaving %s group at: %02d:%02d:%02d\n",__func__,EKA_IP2STR(*(uint32_t*)&mreq.imr_multiaddr),tm.tm_hour,tm.tm_min,tm.tm_sec);

  return 0;
}

