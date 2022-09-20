#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iterator>
#include <thread>

#include <linux/sockios.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/time.h>
#include <chrono>

#include <fcntl.h>

#include "EkaDev.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"
#include "Epm.h"

#include "eka_macros.h"

#include "EkaCtxs.h"
#include "EfhTestTypes.h"

#include "ekaNW.h"
#include "eka_hw_conf.h"


/* --------------------------------------------- */
volatile bool keep_work = true;
int numThreads = 1;
/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
  return;
}

/* --------------------------------------------- */
void printUsage(char* cmd) {
  printf("USAGE: %s "
	 "-t <Num of Threads to write SecCtx updates>"
	 "\n",cmd);
  return;
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":t:h")) != -1) {  
    switch(opt) {  
    case 't':  
      numThreads = atoi(optarg);
      printf("numThreads = %d\n",numThreads);  
      break;  
    case 'h':  
      printUsage(argv[0]);
      exit (1);
      break;  
    case '?':  
      printf("unknown option: %c\n", optopt); 
      break;  
    }  
  }  

  return 0;
}

void ctxThreadFunc(int threadId, EkaDev* dev) {
  TEST_LOG("Thread %d",threadId);

  int ctxWriteBank = 0;
  
  const int ArrSize = 7;
  int64_t cnt = 0;
  while (keep_work) {
    uint8_t a[ArrSize] = {};
    uint8_t p = 0;
    for (auto i = 0; i < ArrSize; i++) {
      a[i] = (uint8_t)(rand() & 0xFF);
      p |= __builtin_parity(a[i]) << i;
    }
    p |= __builtin_parity(p) << 7;

    uint8_t res[ArrSize+1] = {
			      a[0],
			      a[1],
			      a[2],
			      a[3],
			      a[4],
			      a[5],
			      a[6],
			      p
    };
    EfcSecCtxHandle handle = rand() % EkaDev::MAX_SEC_CTX;

    int writeChan = threadId;


    uint64_t ctxWrAddr = P4_CTX_CHANNEL_BASE + 
      writeChan * EKA_BANKS_PER_CTX_THREAD * EKA_WORDS_PER_CTX_BANK * 8 + 
      ctxWriteBank * EKA_WORDS_PER_CTX_BANK * 8;
  
    ctxWriteBank = (ctxWriteBank + 1) % EKA_BANKS_PER_CTX_THREAD;
 
    eka_write(dev,ctxWrAddr,*(uint64_t*)res);
  
    union large_table_desc done_val = {};
    done_val.ltd.src_bank           = ctxWriteBank;
    done_val.ltd.src_thread         = writeChan;
    done_val.ltd.target_idx         = handle;
  
    eka_write(dev, P4_CONFIRM_REG, done_val.lt_desc);

    ++cnt;
    if (cnt % 100000 == 0) {
      TEST_LOG("Thread %d: %jd",threadId,cnt);
      fflush(stdout);
      fflush(stderr);
    }
  }
}

/* ############################################# */
int main(int argc, char *argv[]) {
  
  signal(SIGINT, INThandler);
  getAttr(argc,argv);
  const int MaxThreads = (int)EkaDev::MAX_CTX_THREADS;
  if (numThreads > MaxThreads)
    on_error("numThreads %d > MAX_CTX_THREADS %u",
	     numThreads,MaxThreads);
  
  EkaDev* dev = NULL;
  const EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&dev, &ekaDevInitCtx);

  // FATAL DEBUG: ON
  eka_write(dev,0xf0f00,0xefa8beda);

  std::thread ctxThread[MaxThreads];
  for (int i = 0; i < numThreads; i++) {
    ctxThread[i] = std::thread(ctxThreadFunc,i,dev);
  }

  for (int i = 0; i < numThreads; i++) {
    ctxThread[i].join();
    TEST_LOG("Thread %d joined",i);
  }

  TEST_LOG("\n===========================\nEND OT TESTS\n===========================\n");
  printf("Closing device\n");

  ekaDevClose(dev);
  sleep(1);
  
  return 0;
}
