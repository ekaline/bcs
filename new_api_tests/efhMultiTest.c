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
#include <vector>
#include <algorithm>


#include "EkaDev.h"
#include "eka_macros.h"
#include "Efh.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "EkaCtxs.h"

#include "EkaFhGroup.h"


#include "EkaFhBatsParser.h"

#include "EfhBatsProps.h"
#include "EfhBoxProps.h"
#include "EfhGemProps.h"
#include "EfhIseProps.h"
#include "EfhMiaxProps.h"
#include "EfhNomProps.h"
#include "EfhPhlxOrdProps.h"
#include "EfhPhlxTopoProps.h"
#include "EfhXdpProps.h"
#include "EfhCmeProps.h"

#include "EfhTestFuncs.h"

/* ------------------------------------------------------------ */

static int getAttr(int argc, char *argv[],
		   std::vector<TestRunGroup>& testRunGroups,
		   std::vector<std::string>&  underlyings,
		   bool *print_tob_updates,
		   bool *subscribe_all,
		   bool *print_parsed_messages) {
  int opt; 
  while((opt = getopt(argc, argv, ":u:g:htap")) != -1) {  
    switch(opt) {  
    case 't':  
      printf("Print TOB Updates (EFH)\n");  
      *print_tob_updates = true;
      break;  
    case 'a':  
      printf("subscribe all\n");
      *subscribe_all = true;
      break;  
    case 'u':  
      underlyings.push_back(std::string(optarg));
      printf("Underlying to subscribe: %s\n", optarg);  
      break;  
    case 'p':  
      *print_parsed_messages = true;
      printf("print_parsed_messages = true\n");  
      break;  
    case 'h':  
      print_usage(argv[0]);
      exit (1);
      break;  
    case 'g': {
      TestRunGroup newTestRunGroup = {};
      newTestRunGroup.optArgStr = std::string(optarg);
      testRunGroups.push_back(newTestRunGroup);
      break;  
    }
    case ':':  
      printf("option needs a value\n");  
      break;  
    case '?':  
      printf("unknown option: %c\n", optopt); 
      break;  
    } 

  }
  return 0;

}

int main(int argc, char *argv[]) {
  std::vector<TestRunGroup> testRunGroups;
  std::vector<EfhInitCtx>   efhInitCtx;
  std::vector<EfhRunCtx>    efhRunCtx;
  bool print_parsed_messages = false;

  getAttr(argc,argv,testRunGroups,underlyings,&print_tob_updates,&subscribe_all,&print_parsed_messages);

  if (testRunGroups.size() == 0) { 
    TEST_LOG("No test groups passed");
    print_usage(argv[0]);	
    return 1; 
  }

  createCtxts(testRunGroups,efhInitCtx,efhRunCtx);

/* ------------------------------------------------------- */
  signal(SIGINT, INThandler);

/* ------------------------------------------------------- */
  EkaDev* pEkaDev = NULL;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  //  std::vector<EfhCtx*>      pEfhCtx[16];
  EfhCtx*      pEfhCtx[16] = {};

/* ------------------------------------------------------- */

  for (size_t r = 0; r < testRunGroups.size(); r++) {
    try {
      efhInitCtx.at(r).printParsedMessages = print_parsed_messages;
      efhInit(&pEfhCtx[r],pEkaDev,&efhInitCtx.at(r));
      if (pEfhCtx[r] == NULL) on_error("pEfhCtx[r] == NULL");
      efhRunCtx.at(r).efhRunUserData = (EfhRunUserData)pEfhCtx[r];

      if (pEfhCtx[r]->fhId >= 16) on_error("pEfhCtx[r]->fhId = %u,pEfhCtx[r]=%p",
					  pEfhCtx[r]->fhId,pEfhCtx[r]);
      if (pEfhCtx[r]->fhId != (uint)r) 
	on_error("i=%jd, fhId = %u, pEfhCtx[r]=%p",r,pEfhCtx[r]->fhId,pEfhCtx[r]);

      pEfhCtx[r]->printQStatistics = true;

      /* ------------------------------------------------------- */
      for (uint8_t i = 0; i < efhRunCtx.at(r).numGroups; i++) {
	EkaSource exch = efhRunCtx.at(r).groups[i].source;
	EkaLSI    grId = efhRunCtx.at(r).groups[i].localId;
	auto gr = grCtx[(int)exch][grId];
	if (gr == NULL) on_error("gr == NULL");

	printf ("################ Group %u: %s:%u ################\n",
		i,EKA_EXCH_DECODE(exch),grId);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
	printf ("Skipping Definitions for EKA_TEST_IGNORE_DEFINITIONS\n");
#else
	efhGetDefs(pEfhCtx[r], &efhRunCtx.at(r), (EkaGroup*)&efhRunCtx.at(r).groups[i], NULL);
#endif
      }
      /* ------------------------------------------------------- */
      if (pEfhCtx[r]->fhId >= 16) on_error("pEfhCtx[r]->fhId = %u,pEfhCtx[r]=%p",
					  pEfhCtx[r]->fhId,pEfhCtx[r]);
      std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx[r], &efhRunCtx.at(r),(void**)NULL);
      efh_run_thread.detach();
      /* ------------------------------------------------------- */
    }
    catch (const std::out_of_range& oor) {
      on_error("Out of Range error");
    }
  }

  /* ------------------------------------------------------- */

  while (keep_work) usleep(0);

  ekaDevClose(pEkaDev);

  for (auto exch = 0; exch < MAX_EXCH; exch++) {
    for (auto grId = 0; grId < MAX_GROUPS; grId++) {
      auto gr = grCtx[exch][grId];
      if (gr == NULL) continue;
      fclose (gr->fullDict);
      fclose (gr->subscrDict);
      fclose(gr->MD);      
    }
  }
  printf ("Exitting normally...\n");

  return 0;
}


