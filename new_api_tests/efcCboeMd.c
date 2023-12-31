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
#include <fstream>

#include <linux/sockios.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/time.h>
#include <chrono>

#include "EkaDev.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"
#include "Epm.h"

#include "eka_macros.h"

#include "EkaCtxs.h"
#include "EkaEfcDataStructs.h"

#include <fcntl.h>
#include "ekaNW.h"
#include "EkaFhBatsParser.h"
#include "EfhTestFuncs.h"

extern TestCtx* testCtx;

/* --------------------------------------------- */
std::string ts_ns2str(uint64_t ts);
static EkaOpResult printReport( EfcCtx* pEfcCtx, const EfcReportHdr* p, bool mdOnly);
int printSecCtx(EkaDev* dev, const SecCtx* msg);
int printBoeFire(EkaDev* dev,const BoeNewOrderMsg* msg);
int printMdReport(EkaDev* dev, const EfcMdReport* msg);
int printControllerStateReport(EkaDev* dev, const EfcControllerState* msg);

/* --------------------------------------------- */
static const int  MaxSecurities         = 2000000;
static       bool printFireReport       = false;
static       bool printUnsubscribedOnly = false;
static       bool initializeEfh         = false;
static       bool getExchDefinitions    = false;
static       bool runFhGroup            = false;
static       bool fatalDebug            = false;
static       bool reportOnly            = false;
static       bool armController         = false;
  
static       uint8_t  alwaysFire        = 0;
static       uint8_t  fireUnsubscribed  = 0;
  
std::vector<uint64_t>    efcSecurities;
uint64_t* securityList  = NULL;
int       subscribedNum = 0;
FILE*     efcSecuritiesFile;
/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  testCtx->keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
  return;
}

/* --------------------------------------------- */

int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}
/* --------------------------------------------- */

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, size_t userLength, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %.*s is acquired for %s:%hhu\n",(int)userLength,user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}
/* --------------------------------------------- */

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

/* ------------------------------------------------------------ */

void* onMdTestDefinition(const EfhOptionDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  if (! testCtx->keep_work) return NULL;
  
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  std::string underlyingName = std::string(msg->commonDef.underlying, sizeof(msg->commonDef.underlying));
  std::string classSymbol    = std::string(msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(), ' ', '\0');
  std::replace(classSymbol.begin(),    classSymbol.end(),    ' ', '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize   (strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = {};

  eka_create_avt_definition(avtSecName,msg);
  fprintf (efcSecuritiesFile,"%s,%ju,%s,%s,%s,%s,%ju,%ju\n",
	   avtSecName,
	   msg->header.securityId,
	   EKA_PRINT_BATS_SYMBOL((char*)&msg->commonDef.opaqueAttrA),
	   underlyingName.c_str(),
	   classSymbol.c_str(),
	   EKA_PRINT_GRP(&msg->header.group),
	   msg->commonDef.opaqueAttrA,
	   msg->commonDef.opaqueAttrB
	   );


  efcSecurities.push_back(msg->header.securityId);

  return NULL;
}

/* ------------------------------------------------------------ */
#if 0
static EkaOpResult printReport( EfcCtx* pEfcCtx, const EfcReportHdr* p, bool mdOnly) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  const uint8_t* b = (const uint8_t*)p;
  //--------------------------------------------------------------------------
  if (((EkaContainerGlobalHdr*)b)->type == EkaEventType::kExceptionReport) {
    EKA_LOG("EXCEPTION_REPORT");
    return EKA_OPRESULT__OK;
  }
  //--------------------------------------------------------------------------
  if (((EkaContainerGlobalHdr*)b)->type != EkaEventType::kFireReport) 
    on_error("UNKNOWN Event report: 0x%02x",
	     static_cast< uint32_t >( ((EkaContainerGlobalHdr*)b)->type ) );

  uint total_reports = ((EkaContainerGlobalHdr*)b)->num_of_reports;
  b += sizeof(EkaContainerGlobalHdr);
  //--------------------------------------------------------------------------
  if (((EfcReportHdr*)b)->type != EfcReportType::kControllerState) 
    on_error("EfcControllerState report expected, 0x%02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type));
  b += sizeof(EfcReportHdr);
  {
    auto msg{ reinterpret_cast< const EfcControllerState* >( b ) };

    if (printUnsubscribedOnly) {
      if ((msg->fire_reason & EFC_FIRE_REASON_SUBSCRIBED) != 0)
	return EKA_OPRESULT__OK;
    }

    if (! mdOnly) printControllerStateReport(dev,msg);
    b += sizeof(EfcControllerState);
  }
  total_reports--;
  //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kMdReport) 
    on_error("MdReport report expected, %02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  b += sizeof(EfcReportHdr);
  {
    auto msg{ reinterpret_cast< const EfcMdReport* >( b ) };

    printMdReport(dev,msg);

    b += sizeof(*msg);
  }
  total_reports--;

  //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kSecurityCtx) 
    on_error("SecurityCtx report expected, %02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  b += sizeof(EfcReportHdr);
  {
    auto msg{ reinterpret_cast< const SecCtx* >( b ) };

    if (! mdOnly) printSecCtx(dev, msg);

    b += sizeof(*msg);
  }
  total_reports--;

  //--------------------------------------------------------------------------
  if (((EfcReportHdr*)b)->type != EfcReportType::kFirePkt) 
    on_error("FirePkt report expected, %02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  EKA_LOG("\treport_type = %u SecurityCtx, idx=%u, size=%ju",
  	  static_cast< uint32_t >( ((EfcReportHdr*)b)->type ),
  	  ((EfcReportHdr*)b)->idx,
  	  ((EfcReportHdr*)b)->size);
  b += sizeof(EfcReportHdr);
  {
    auto msg {reinterpret_cast<const BoeNewOrderMsg*>(b +
						      sizeof(EkaEthHdr) +
						      sizeof(EkaIpHdr) +
						      sizeof(EkaTcpHdr))};
    if (! mdOnly) printBoeFire(dev,msg);
    b += ((EfcReportHdr*)b)->size;
  }
  total_reports--;
    
  //--------------------------------------------------------------------------

  return EKA_OPRESULT__OK;
}
#endif
/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s \n"
	 "\t\t\t\t-s <Securities List file path>\n"
	 "\t\t\t\t-f -- Initialize EFH\n"
	 "\t\t\t\t-x -- Get Exchange Definitions\n"
	 "\t\t\t\t-m -- Run EFH Group (for raw MD callbacks)\n"
	 "\t\t\t\t-r -- Report Only\n"
	 "\t\t\t\t-a -- Arm EFC\n"
	 "\t\t\t\t-w -- Always Fire\n"
	 "\t\t\t\t-u -- Fire on Unsubscribed\n"
	 "\t\t\t\t-a -- Arm EFC\n"
	 "\t\t\t\t-p -- Print Fire Report\n"
	 "\t\t\t\t-o -- Print Fire Report only for Unsubscribed\n"
	 "\t\t\t\t-d -- FATAL DEBUG ON\n"
	 ,cmd);
  return;
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::vector<std::string>&  underlyings,
		   char* secIdFileName
		   ) {
  int opt; 
  while((opt = getopt(argc, argv, ":s:xfmdrapuwoh")) != -1) {  
    switch(opt) {
    case 's':  
      strcpy(secIdFileName,optarg);
      printf("secIdFileName = %s\n", secIdFileName);  
      break;  			 
    case 'f':  
      printf("initializeEfh = true\n");
      initializeEfh = true;
      break;
    case 'x':  
      printf("getExchDefinitions = true\n");
      getExchDefinitions = true;
      break;
    case 'm':  
      printf("runFhGroup = true\n");
      runFhGroup = true;
      break;
    case 'd':  
      printf("fatalDebug = ON\n");
      fatalDebug = true;
      break;
    case 'r':  
      printf("reportOnly = ON\n");
      reportOnly = true;
      break;      
    case 'a':  
      printf("armController = ON\n");
      armController = true;
      break;
    case 'w':  
      printf("alwaysFire = ON\n");
      alwaysFire = 1;
      break;
    case 'u':  
      printf("fireUnsubscribed = ON\n");
      fireUnsubscribed = 1;
      break;            
    case 'p':  
      printf("printFireReport = ON\n");
      printFireReport = true;
      break;
    case 'o':  
      printf("printUnsubscribedOnly = true\n");
      printUnsubscribedOnly = true;
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

/* --------------------------------------------- */
static std::string action2string(EpmTriggerAction action) {
  switch (action) {
  case Unknown:         return std::string("Unknown");
  case Sent:            return std::string("Sent");
  case InvalidToken:    return std::string("InvalidToken");
  case InvalidStrategy: return std::string("InvalidStrategy");
  case InvalidAction:   return std::string("InvalidAction");
  case DisabledAction:  return std::string("DisabledAction");
  case SendError:       return std::string("SendError");
  default:              on_error("Unexpected action %d",action);
  }
    
};

/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  // ==============================================
  char     secIdFileName[1024] = {};

  getAttr(argc,argv,testCtx->underlyings,secIdFileName);

  const char* efcSecuritiesFileName = "CBOE_EFC_SECURITIES.txt";
  if((efcSecuritiesFile = fopen(efcSecuritiesFileName,"w")) == NULL)
      on_error ("Error %s",efcSecuritiesFileName);
  
  // ==============================================
  // EkaDev general setup
  EkaDev*     dev = NULL;
  EkaCoreId   coreId = 0;
  EkaOpResult rc;
  const EkaDevInitCtx ekaDevInitCtx = {
    .logCallback  = NULL,
    .logContext   = NULL,
    .credAcquire  = credAcquire,
    .credRelease  = credRelease,
    .credContext  = NULL,
    .createThread = createThread,
    .createThreadContext = NULL
  };
  ekaDevInit(&dev, &ekaDevInitCtx);

  // ==============================================
  // Setup EFC MC groups

  EpmTriggerParams triggerParam[] = {
      /* {0,"233.54.12.72",18000}, */
    {0,"224.0.74.0",30301},
    /* {0,"224.0.74.1",30302}, */
    /* {0,"224.0.74.2",30303}, */
    /* {0,"224.0.74.3",30304}, */
  };

  EfcCtx efcCtx = {};
  EfcCtx* pEfcCtx = &efcCtx;

  EfcInitCtx initCtx = {
      .feedVer = EfhFeedVer::kCBOE
  };
  
  rc = efcInit(&pEfcCtx,dev,&initCtx);
  if (rc != EKA_OPRESULT__OK) on_error("efcInit returned %d",(int)rc);

  // ==============================================
  // Configuring EFC as EPM Strategy

  const EpmStrategyParams efcEpmStrategyParams = {
    .numActions    = 256,          // just a number
    .triggerParams = triggerParam,         
    .numTriggers   = std::size(triggerParam),
    .reportCb      = NULL,         // set via EfcRunCtx
    .cbCtx         = NULL
  };
  rc = epmInitStrategies(dev, &efcEpmStrategyParams, 1);
  if (rc != EKA_OPRESULT__OK) on_error("epmInitStrategies failed: rc = %d",rc);

  // ==============================================
  // Global EFC config
  EfcStratGlobCtx efcStratGlobCtx = {
    .enable_strategy = 0,
    .report_only                       = (uint8_t)reportOnly,
    .debug_always_fire_on_unsubscribed = fireUnsubscribed,
    .debug_always_fire                 = alwaysFire,
    .max_size                          = 1000,
    .watchdog_timeout_sec              = 100000,
  };
  efcInitStrategy(pEfcCtx, &efcStratGlobCtx);


  // ==============================================
  // Launching EFH for Definitions and onMd callbacks
  if (initializeEfh) {
      EfhCtx* pEfhCtx = NULL;
      EkaProp efhBatsC1InitCtxEntries_CC_0[] = {
	  {"efh.C1_PITCH.group.0.unit","1"},
	  {"efh.C1_PITCH.group.0.mcast.addr","224.0.74.0:30301"},
	  {"efh.C1_PITCH.group.0.snapshot.addr","170.137.114.100:18901"},
	  {"efh.C1_PITCH.group.0.snapshot.auth","GTSS:sb2gtss"},
	  {"efh.C1_PITCH.group.0.snapshot.sessionSubID","0432"},
	  {"efh.C1_PITCH.group.0.recovery.addr","224.0.74.37:30301"},
	  {"efh.C1_PITCH.group.0.recovery.grpAddr","170.137.114.102:17006"},
	  {"efh.C1_PITCH.group.0.recovery.grpAuth","GTSS:eb3gtss"},
	  {"efh.C1_PITCH.group.0.recovery.grpSessionSubID","0587"},
      };
      EkaSource exch = EkaSource::kC1_PITCH;
      EkaLSI    grId = 0;
      const EkaGroup batsC1Groups[] = {
	  {exch, grId},
      };
      
      testCtx->grCtx[(int)exch][grId] = new McGrpCtx(exch,grId);
      if (testCtx->grCtx[(int)exch][grId] == NULL)
	  on_error("failed creating testCtx->grCtx[%d][%d]",(int)exch,grId);
      
      EkaProps props = {
	  .numProps = std::size(efhBatsC1InitCtxEntries_CC_0),
	  .props    = efhBatsC1InitCtxEntries_CC_0
      };
  
      EfhInitCtx efhInitCtx = {
	  .ekaProps            = &props,
	  .numOfGroups         = 1,
	  .coreId              = coreId,
	  .printParsedMessages = true,
	  .recvSoftwareMd      = true,
	  .subscribe_all       = false,
	  .noTob               = true
      };

      efhInit(&pEfhCtx,dev,&efhInitCtx);

      const EfhRunCtx efhRunCtx = {
	  .groups                      = batsC1Groups,
	  .numGroups                   = std::size(batsC1Groups),
	  .efhRunUserData              = (EfhRunUserData) pEfhCtx,
	  .onEfhOptionDefinitionMsgCb  = onMdTestDefinition,
	  .onEfhTradeMsgCb             = NULL, //onTrade,
	  .onEfhQuoteMsgCb             = NULL, //onQuote,
	  .onEfhOrderMsgCb             = NULL, //onOrder,
	  .onEfhGroupStateChangedMsgCb = onEfhGroupStateChange,
	  .onEkaExceptionReportCb      = NULL, //onException,
	  .onEfhMdCb                   = onMd
      };

      if (getExchDefinitions)
	efhGetDefs(pEfhCtx, &efhRunCtx, (EkaGroup*)&efhRunCtx.groups[0], NULL);

      if (runFhGroup) {
	std::thread efhRunThread = std::thread(efhRunGroups,pEfhCtx,&efhRunCtx,(void**)NULL);
	efhRunThread.detach();

	auto fh = pEfhCtx->dev->fh[pEfhCtx->fhId];
	if (fh == NULL) on_error("fh == NULL");
	TEST_LOG("Waiting for EFH active");
	while (! fh->active) sleep(0);
      }
  }



  // ==============================================
  // Subscribing on securities

  securityList = new uint64_t[MaxSecurities];
  if (securityList == NULL) on_error("securityList == NULL");
  
  if (strlen(secIdFileName) != 0) {
      // security list is passed from arfv[]
      std::ifstream file(secIdFileName);
      if (file.is_open()) {
	  std::string line;
	  while (std::getline(file, line)) {
	      securityList[subscribedNum++] = std::stoul(line,0,0);
	  }
	  file.close();
      } else {
	  on_error("Cannot open %s",secIdFileName);
      }

  } else {
      // security list received from efhGetDefs
      for (auto &sec : efcSecurities) {
	  securityList[subscribedNum++] = sec;
      }
  }

  efcEnableFiringOnSec(pEfcCtx, securityList, subscribedNum);
  TEST_LOG("Subscribing on %d securities",subscribedNum);
  
  // ==============================================
  // setting security contexts
  for (auto i = 0; i < subscribedNum; i++) {
    auto handle = getSecCtxHandle(pEfcCtx, securityList[i]);
    if (handle < 0) {
      EKA_WARN("Security[%d] 0x%016jx was not fit into FPGA hash: handle = %jd",
	       i,securityList[i],handle);
      continue;
    }
    SecCtx secCtx = {
	.bidMinPrice       = static_cast<decltype(secCtx.bidMinPrice)>(1),
	.askMaxPrice       = static_cast<decltype(secCtx.askMaxPrice)>(0xFFFF),  
	.bidSize              = 1,
	.askSize              = 1,
	.lowerBytesOfSecId = (uint8_t)(securityList[i] & 0xFF)
    };
    /* EKA_TEST("Setting StaticSecCtx[%d] secId=0x%016jx, handle=%jd", */
    /* 	     i,securityList[i],handle); */

    rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK) on_error ("failed to efcSetStaticSecCtx");
  }
  // ==============================================
  // Establishing Dummy EXC connection for ReportOnly fires 
  int excSock = excSocket(dev,coreId,0,0,0);
  if (excSock < 0) on_error("failed to open sock");
  // ==============================================
  // Manually prepared FPGA firing template

  const BoeNewOrderMsg fireMsg = {
      .StartOfMessage    = 0xBABA,
      .MessageLength     = sizeof(BoeNewOrderMsg) - 2,
      .MessageType       = 0x38,
      .MatchingUnit      = 0,
      .SequenceNumber    = 0,

      .ClOrdID           = {'E','K','A','t','e','s','t',
			    'D','U','M','M','Y','F','I','R','E',
			    'A','B','C','D'},
	  
      .Side              = '_',
      .OrderQty          = 0,

      .NumberOfBitfields = 4, 
      .NewOrderBitfield1 = 1 | 2 | 4 | 16 | 32,
      .NewOrderBitfield2 = 1 | 64,             
      .NewOrderBitfield3 = 1,                  
      .NewOrderBitfield4 = 0,

      .ClearingFirm      = {'F','A','K','E'},
      .ClearingAccount   = {'T','E','S','T'},
      .Price             = 0,
      .OrdType           = '2',     
      .TimeInForce       = '3',     
      .Symbol            = {},
      .Capacity          = 'C',
      .Account           = {'1','2','3','4','5','6','7','8','9','0',
			    'A','B','C','D','E','F'},
      .OpenClose         = 'O'
  };
  // ==============================================
  //  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  //  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint heapOffset    = 0;
  
  static const uint64_t AlwaysFire      = 0xadcd;
  static const uint64_t DefaultToken    = 0x1122334455667788;

  const EpmAction fire0 = {
    .type          = EpmActionType::BoeFire,        
    .token         = DefaultToken,                  
    .hConn         = 0,  /* DUMMY!!!! */            
    .offset        = heapOffset + nwHdrOffset,      
    .length        = (uint32_t)sizeof(fireMsg),     
    .actionFlags   = AF_Valid,                      
    .nextAction    = EPM_LAST_ACTION,               
    .enable        = AlwaysFire,                    
    .postLocalMask = AlwaysFire,                    
    .postStratMask = AlwaysFire,                    
    .user          = 0x1234567890abcdef             
  };
  rc = epmPayloadHeapCopy(dev, 
			  EFC_STRATEGY,
			  fire0.offset,
			  fire0.length,
			  &fireMsg);
  if (rc != EKA_OPRESULT__OK) 
    on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
	     fire0.offset,fire0.length,(int)rc);
  rc = epmSetAction(dev, 
		    EFC_STRATEGY, 
		    0,               // epm_actionid_t must correspond to the MC gr ID
		    &fire0);

  if (rc != EKA_OPRESULT__OK) on_error("epmSetAction returned %d",(int)rc);

  
  // ==============================================
  EfcRunCtx efcRunCtx = {};
  efcRunCtx.onEfcFireReportCb = efcPrintFireReport;
  
  if (fatalDebug) {
    TEST_LOG(RED "\n=====================\n"
	     "FATAL DEBUG: ON"
	     "\n=====================\n" RESET);
    eka_write(dev,0xf0f00,0xefa0beda);
  }
  // ==============================================
  if (armController) efcEnableController(pEfcCtx, 0);
  // ==============================================
  efcRun(pEfcCtx, &efcRunCtx);
  // ==============================================


  TEST_LOG("\n===========================\nWORKING\n===========================\n");

  while (testCtx->keep_work) { sleep(0); }

  sleep(1);
  fflush(stdout);fflush(stderr);
  if (efcSecuritiesFile != NULL) fclose(efcSecuritiesFile);

  /* ============================================== */

  printf("Closing device\n");

  ekaDevClose(dev);
  sleep(1);
  
  return 0;
}
