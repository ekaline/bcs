/* echo-thread.c
 *
 * Copyright (c) 2000 Sean Walton and Macmillan Publishers.  Use may be in
 * whole or in part in accordance to the General Public License (GPL).
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

/*****************************************************************************/
/*** echo-thread.c                                                         ***/
/***                                                                       ***/
/*** An echo server using threads.                                         ***/
/*****************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <thread>

#include "eka_macros.h"

volatile static bool keep_work;

void Child(int sock, uint port) {
  int bytes_read = -1;
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    bytes_read = recv(sock, line, sizeof(line), 0);
    //    printf ("%u: %s\n",sock,line);
    //    fflush(stdout);
    send(sock, line, bytes_read, 0);
  } while (bytes_read > 0 && keep_work);
  printf("%u: bytes_read = %d -- closing\n",sock,bytes_read);
  fflush(stdout);
  close(sock);
  keep_work = false;
  return;
}

int main(void) {
  keep_work = true;
  int sd;
  struct sockaddr_in addr;

  char hostname[1024];
  gethostname(hostname, 1024);
  if ((strcmp(hostname, "xn03") != 0) && (strcmp(hostname, "xn02") != 0) && (strcmp(hostname, "xn01") != 0)) {
    on_error("Echo server must be run on ekaline lab xn01, xn02 or xn03 only");
  } 
  printf("Starting echo server on %s port %d\n",hostname,22222);
  if ( (sd = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
    on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const, sizeof(int)) < 0) on_error("setsockopt(SO_REUSEADDR) failed");

  struct linger so_linger = {
    true, // Linger ON
    0     // Abort pending traffic on close()
  };

  if (setsockopt(sd,SOL_SOCKET,SO_LINGER,&so_linger,sizeof(struct linger)) < 0) on_error("Cant set SO_LINGER");

  addr.sin_family = AF_INET;
  addr.sin_port = htons(22222);
  addr.sin_addr.s_addr = INADDR_ANY;
  if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) on_error("Bind");
  if ( listen(sd, 20) != 0 ) on_error("Listen");
  while (keep_work) {
    int client, addr_size = sizeof(addr);

    client = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
    printf("Connected from: %s:%d -- sock=%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),client);
    std::thread child(Child,client,ntohs(addr.sin_port));
    child.detach();
  }
  return 0;
}
