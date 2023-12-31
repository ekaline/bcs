#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
// #include<resolv.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "eka_macros.h"

volatile bool keep_work = true;

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  fprintf(
      stderr,
      "Ctrl-C detected: keep_work = false, exitting...");
  fflush(stdout);
  fflush(stderr);
  return;
}

void *Child(void *arg) {
  int client = *(int *)arg;

  do {
    char rxBuf[2000] = {};
    ssize_t bytes_read =
        recv(client, rxBuf, sizeof(rxBuf), 0);
    if (bytes_read > 0) {
      hexDump("TCP RX", rxBuf, bytes_read);
      send(client, rxBuf, bytes_read, 0);
    }
  } while (keep_work);
  close(client);

  return arg;
}

/*--------------------------------------------------------------------*/
/*--- main - setup server and await connections (no need to
 * clean  ---*/
/*--- up after terminated children. ---*/
/*--------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  if (argc != 2)
    on_error("TCP server port not specified: argc=%d",
             argc);

  signal(SIGINT, INThandler);

  uint16_t port = atoi(argv[1]);

  printf("Starting echo server on port %d\n", port);

  int sd = -1;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEADDR) failed");

  struct linger so_linger = {
      true, // Linger ON
      0     // Abort pending traffic on close()
  };

  if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &so_linger,
                 sizeof(struct linger)) < 0)
    on_error("Cant set SO_LINGER");

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    on_error("Bind");
  if (listen(sd, 20) != 0)
    on_error("Listen");
  while (keep_work) {
    int client, addr_size = sizeof(addr);
    pthread_t child;

    client = accept(sd, (struct sockaddr *)&addr,
                    (socklen_t *)&addr_size);
    printf("Connected from: %s:%d\n",
           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    if (pthread_create(&child, NULL, Child, &client) != 0)
      perror("Thread creation");
    else
      pthread_detach(child); /* disassociate from parent */
  }
  return 0;
}
