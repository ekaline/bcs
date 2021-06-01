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

#include "EfhTestFuncs.h"

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s: Ctrl-C detected:  exitting...\n",__func__);
  fflush(stdout);
  return;
}

std::string eka_get_date () {
  const char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str,"%d-%s-%02d",1900+tm.tm_year,months[tm.tm_mon],tm.tm_mday);
  return std::string(t_str);
}

std::string eka_get_time () {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str,"%02d:%02d:%02d",tm.tm_hour,tm.tm_min,tm.tm_sec);
  return std::string(t_str);
}

int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

void* onOrder(const EfhOrderMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  if (! keep_work) return NULL;

  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  int secIdx = (int)secData;
#endif

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);

  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];

  if (pEfhCtx->printQStatistics && (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  if (! print_tob_updates) return NULL;

  fprintf(gr->MD,"%s,%s,%ju,%s,%s,%.*f,%u,%c,%s,%ju\n",
	  eka_get_date().c_str(),
	  eka_get_time().c_str(),
	  msg->header.securityId,
#ifdef EKA_TEST_IGNORE_DEFINITIONS
	  "UNDERLYING_ID",
	  "AVT_SEC_NAME",
#else
	  gr->security.at(secIdx).classSymbol.c_str(),
	  gr->security.at(secIdx).avtSecName.c_str(),	  
#endif
	  //	  EKA_DEC_POINTS_10000(msg->bookSide.price), ((float) msg->bookSide.price / 10000),
	  decPoints(msg->bookSide.price,10000), ((float) msg->bookSide.price / 10000),
	  msg->bookSide.size,

	  EKA_TS_DECODE(msg->tradeStatus),

	  (ts_ns2str(msg->header.timeStamp)).c_str(),
	  msg->header.timeStamp
	  );

  return NULL;
}

void* onTrade(const EfhTradeMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  if (! keep_work) return NULL;
  return NULL;
}

void* onEfhGroupStateChange(const EfhGroupStateChangedMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  //  int file_idx = (uint8_t)(msg->group.localId);
  //  EfhCtx* pEfhCtx = (EfhCtx*) userData;

#ifdef FH_LAB
  FILE* logFile = stdout;
#else  
  EkaSource exch = msg->group.source;
  EkaLSI    grId = msg->group.localId;

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);
  if (gr->MD == NULL) on_error("gr->MD == NULL");

  FILE* logFile = gr->MD;
#endif

  fprintf(logFile,"%s: %s : FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());

  switch (msg->groupState) {
    /* ----------------------------- */
  case EfhGroupState::kError :
    switch (msg->errorDomain) {
    case EfhErrorDomain::kUpdateTimeout :
      printf ("=========================\n%s: MdTimeOut\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      break;
      
    case EfhErrorDomain::kExchangeError :
      printf ("=========================\n%s: ExchangeError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);
      break;

    case EfhErrorDomain::kSocketError :
      printf ("=========================\n%s: SocketError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);
      break;

    case EfhErrorDomain::kCredentialError :
      printf ("=========================\n%s: CredentialError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);

      break;

    case EfhErrorDomain::kOSError :
      printf ("=========================\n%s: OSError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);

      break;

    case EfhErrorDomain::kDeviceError :
      printf ("=========================\n%s: DeviceError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);

      break;

    default:
      printf ("=========================\n%s: errorDomain = \'%c\'\n=========================\n",
	      EKA_PRINT_GRP(&msg->group),(char)msg->errorDomain);
    }

    break;
    /* ----------------------------- */

  case EfhGroupState::kInitializing :
  case EfhGroupState::kClosed : 
  case EfhGroupState::kGap : {
    std::string gapType = std::string("Unknown");
    switch (msg->systemState) {
    case EfhSystemState::kInitial :
      gapType = std::string("InitialGap");
      break;
    case EfhSystemState::kTrading :
      gapType = std::string("NormalGap");
      break;
    case EfhSystemState::kClosed :
      gapType = std::string("System Closed");
      break;
    default:
      gapType = std::string("Unknown Gap");
    }
    fprintf(logFile,"%s: %s : %s FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
    printf ("=========================\n%s: %s: %s %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),gapType.c_str(),msg->code);
  }
    break;
    /* ----------------------------- */
  case EfhGroupState::kNormal : {
    std::string gapType = std::string("Unknown");
    switch (msg->systemState) {
    case EfhSystemState::kInitial :
      gapType = std::string("InitialFeedUp");
      break;
    case EfhSystemState::kTrading :
      gapType = std::string("NormalFeedUp");
      break;
    default:
      gapType = std::string("Unknown Gap");
    }
    fprintf(logFile,"%s: %s : %s \n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
    printf ("=========================\n%s: %s: %s %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),gapType.c_str(),msg->code);
  }
    break;
    /* ----------------------------- */
  case EfhGroupState::kWarning : {
    fprintf(logFile,"%s: %s : BACK-IN-TIME \n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());
    printf ("=========================\n%s: %s: BACK-IN-TIME %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),msg->code);
  }
    break;
    /* ----------------------------- */
  default:
    on_error("Unexpected EfhGroupState \'%c\'",(char)msg->groupState);
  }
  return NULL;
}

void onException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData) {
  if (! keep_work) return;
  printf("%s: Doing nothing\n",__func__);
  return;
}

void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size) {
  if (! keep_work) return;
  printf ("%s: Doing nothing \n",__func__);
  return;	 
}
extern FILE* mdFile;

void* onMd(const EfhMdHeader* msg, EfhRunUserData efhRunUserData) {
  if (! keep_work) return NULL;
  EfhCtx* pEfhCtx = (EfhCtx*) efhRunUserData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

#ifdef FH_LAB
  FILE* logFile = (FILE*) efhRunUserData; //stdout;
#else  
  EkaSource exch = msg->group.source;
  EkaLSI    grId = msg->group.localId;

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);
  if (gr->MD == NULL) on_error("gr->MD == NULL");

  FILE* logFile = gr->MD;
#endif

  if (logFile == NULL) on_error("logFile == NULL");
  fprintf(logFile,
	  "%ju,"                      /* sequence       */
	  "%s,"                       /* ts string      */
	  "0x%jx,",                   /* ts hex         */
	  msg->sequenceNumber,
	  ts_ns2str(msg->timeStamp).c_str(),
	  msg->timeStamp);

  switch (msg->mdMsgType) {
  case EfhMdType::NewOrder : {
    auto m {reinterpret_cast<const MdNewOrder*>(msg)};
    const char* c= (const char*)&m->hdr.securityId;
    fprintf(logFile,
	    "%s (0x%x),"                /* msg type       */
	    "\'%c%c%c%c%c%c%c%c\',"     /* secId string   */
	    "0x%016jx,"                 /* secId hex      */
	    "0x%jx,"                    /* orderId        */
	    "\'%c\',"                   /* side           */
	    "P:%ju (0x%016jx),"         /* price          */
	    "S:%u (0x%08x)\n",          /* size           */
	    DecodeMdType(m->hdr.mdMsgType),	m->hdr.mdRawMsgType,
	    c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],
	    //	    std::string(*(char*)&m->hdr.securityId,sizeof(m->hdr.securityId)).c_str(),
	    m->hdr.securityId,
	    m->orderId,
	    m->side == EfhOrderSide::kBid ? 'B' : 'A',
	    m->price,m->price,
	    m->size,m->size
	    );
    //    fflush(logFile);
  }
    break;

  case EfhMdType::NewPlevel : {
    auto m {reinterpret_cast<const MdNewPlevel*>(msg)};
    fprintf(logFile,"%s (0x%x),0x%016jx (%ju),%ju,%s,%ju,%c,%u,%ju,%u\n",
	    DecodeMdType(m->hdr.mdMsgType),
	    m->hdr.mdRawMsgType,
	    m->hdr.securityId,
	    m->hdr.securityId,
	    m->hdr.sequenceNumber,
	    ts_ns2str(m->hdr.timeStamp).c_str(),
	    m->hdr.timeStamp,
	    m->side == EfhOrderSide::kBid ? 'B' : 'A',
	    m->pLvl,
	    m->price,
	    m->size
	    );
  }
    break;
  case EfhMdType::ChangePlevel : {
    auto m {reinterpret_cast<const MdChangePlevel*>(msg)};
    fprintf(logFile,"%s (0x%x),0x%016jx (%ju),%ju,%s,%ju,%c,%u,%ju,%u\n",
	    DecodeMdType(m->hdr.mdMsgType),
	    m->hdr.mdRawMsgType,
	    m->hdr.securityId,
	    m->hdr.securityId,
	    m->hdr.sequenceNumber,
	    ts_ns2str(m->hdr.timeStamp).c_str(),
	    m->hdr.timeStamp,
	    m->side == EfhOrderSide::kBid ? 'B' : 'A',
	    m->pLvl,
	    m->price,
	    m->size
	    );
  }
    break;
  case EfhMdType::DeletePlevel : {
    auto m {reinterpret_cast<const MdDeletePlevel*>(msg)};
    fprintf(logFile,"%s (0x%x),0x%016jx (%ju),%ju,%s,%ju,%c,%u\n",
	    DecodeMdType(m->hdr.mdMsgType),
	    m->hdr.mdRawMsgType,
	    m->hdr.securityId,
	    m->hdr.securityId,
	    m->hdr.sequenceNumber,
	    ts_ns2str(m->hdr.timeStamp).c_str(),
	    m->hdr.timeStamp,
	    m->side == EfhOrderSide::kBid ? 'B' : 'A',
	    m->pLvl
	    );
  }
    break;     
  default:
    return NULL;
  }
  return NULL;	 
}

void* onQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  if (! keep_work) return NULL;
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);
  
  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];  

#ifdef EKA_TEST_IGNORE_DEFINITIONS
  std::string currAvtSecName  = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  int64_t priceScaleFactor = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE;
#else
  int secIdx                  = (int)secData;
  std::string currAvtSecName  = gr->security.at(secIdx).avtSecName.c_str();	  
  std::string currClassSymbol = gr->security.at(secIdx).classSymbol;
  int64_t priceScaleFactor    = 100; //gr->security.at(secIdx).displayPriceScale;
#endif

  if (pEfhCtx->printQStatistics && (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  if (! print_tob_updates) return NULL;

  fprintf(gr->MD,"%s,%s,%ju,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%d,%d,%s,%ju\n",
	  eka_get_date().c_str(),
	  eka_get_time().c_str(),
	  msg->header.securityId,
	  currAvtSecName.c_str(),
	  currClassSymbol.c_str(),
	  '1',
	  msg->bidSide.size,
	  decPoints(msg->bidSide.price,priceScaleFactor), ((float) msg->bidSide.price / priceScaleFactor),
	  msg->bidSide.customerSize,
	  msg->askSide.size,
	  decPoints(msg->askSide.price,priceScaleFactor), ((float) msg->askSide.price / priceScaleFactor),
	  msg->askSide.customerSize,
	  EKA_TS_DECODE(msg->tradeStatus),
	  EKA_TS_DECODE(msg->tradeStatus),
	  0,0, // Size Breakdown
	  (ts_ns2str(msg->header.timeStamp)).c_str(),
	  msg->header.timeStamp
	  );

  return NULL;
}

void eka_create_avt_definition (char* dst, const EfhOptionDefinitionMsg* msg) {
  if (msg->header.group.source  == EkaSource::kCME_SBE && msg->securityType == EfhSecurityType::kOption) {
    std::string classSymbol    = std::string(msg->classSymbol,sizeof(msg->classSymbol));
    sprintf(dst,"%s_%c%04jd",
	    classSymbol.c_str(),
	    msg->optionType == EfhOptionType::kCall ? 'C' : 'P',
	    msg->strikePrice);
  } else {
  
    uint8_t y,m,d;

    d = msg->expiryDate % 100;
    m = ((msg->expiryDate - d) / 100) % 100;
    y = msg->expiryDate / 10000 - 2000;

    memcpy(dst,msg->underlying,6);
    for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_';
    char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P';
    sprintf(dst+6,"%02u%02u%02u%c%08jd",y,m,d,call_put,msg->strikePrice);
  }
  return;
}

/* ------------------------------------------------------------ */

void* onOptionDefinition(const EfhOptionDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  if (! keep_work) return NULL;
  
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);

  std::string underlyingName = std::string(msg->underlying, sizeof(msg->underlying));
  std::string classSymbol    = std::string(msg->classSymbol,sizeof(msg->classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(), ' ', '\0');
  std::replace(classSymbol.begin(),    classSymbol.end(),    ' ', '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize   (strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = {};

  eka_create_avt_definition(avtSecName,msg);
  fprintf (gr->fullDict,"%s,%ju,%s,%s,%s,%s,%ju,%ju\n",
	   avtSecName,
	   msg->header.securityId,
	   exch == EkaSource::kC1_PITCH ? EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA) : " ",
	   underlyingName.c_str(),
	   classSymbol.c_str(),
	   EKA_PRINT_GRP(&msg->header.group),
	   msg->opaqueAttrA,
	   msg->opaqueAttrB
	   );


  if (subscribe_all || (std::find(underlyings.begin(), underlyings.end(), underlyingName) != underlyings.end())) {
    if (std::find(securities.begin(),securities.end(),msg->header.securityId) != securities.end()) return NULL;
    securities.push_back(msg->header.securityId);
	
    TestSecurityCtx newSecurity = {
      .avtSecName        = std::string(avtSecName),
      .underlying        = underlyingName,
      .classSymbol       = classSymbol,
      .displayPriceScale = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE
    };
    
    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;
    
    efhSubscribeStatic(pEfhCtx, (EkaGroup*)&msg->header.group, msg->header.securityId, EfhSecurityType::kOption,(EfhSecUserData) sec_idx,0,0);

    fprintf (gr->subscrDict,"%s,%ju,%s,%s\n",
	     avtSecName,
	     msg->header.securityId,
	     exch == EkaSource::kC1_PITCH ? EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA) : " ",
	     EKA_PRINT_GRP(&msg->header.group)
	     );
  }

  return NULL;
}

EkaSource feedname2source(std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return EkaSource::kC1_PITCH;
  if (feedName == std::string("CB")) return EkaSource::kC1_PITCH;
  if (feedName == std::string("CC")) return EkaSource::kC1_PITCH;
  if (feedName == std::string("CD")) return EkaSource::kC1_PITCH;
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return EkaSource::kMIAX_TOM;
  if (feedName == std::string("MB")) return EkaSource::kMIAX_TOM;

  if (feedName == std::string("PA")) return EkaSource::kPEARL_TOM;
  if (feedName == std::string("PB")) return EkaSource::kPEARL_TOM;
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return EkaSource::kARCA_XDP;
  if (feedName == std::string("RB")) return EkaSource::kARCA_XDP;

  if (feedName == std::string("XA")) return EkaSource::kAMEX_XDP;
  if (feedName == std::string("XB")) return EkaSource::kAMEX_XDP;
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return EkaSource::kBOX_HSVF;
  if (feedName == std::string("BB")) return EkaSource::kBOX_HSVF;
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return EkaSource::kNOM_ITTO;
  if (feedName == std::string("NB")) return EkaSource::kNOM_ITTO;
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return EkaSource::kGEM_TQF;
  if (feedName == std::string("GB")) return EkaSource::kGEM_TQF;
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return EkaSource::kISE_TQF;
  if (feedName == std::string("IB")) return EkaSource::kISE_TQF;
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return EkaSource::kPHLX_TOPO;
  if (feedName == std::string("TB")) return EkaSource::kPHLX_TOPO;
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return EkaSource::kPHLX_ORD;
  if (feedName == std::string("OB")) return EkaSource::kPHLX_ORD;
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return EkaSource::kCME_SBE;
  if (feedName == std::string("EB")) return EkaSource::kCME_SBE;
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static EkaProp* feedname2prop (std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return efhBatsC1InitCtxEntries_A;
  if (feedName == std::string("CB")) return efhBatsC1InitCtxEntries_B;
  if (feedName == std::string("CC")) return efhBatsC1InitCtxEntries_C;
  if (feedName == std::string("CD")) return efhBatsC1InitCtxEntries_D;
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return efhMiaxInitCtxEntries_A;
  if (feedName == std::string("MB")) return efhMiaxInitCtxEntries_B;

  if (feedName == std::string("PA")) return efhPearlInitCtxEntries_A;
  if (feedName == std::string("PB")) return efhPearlInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return efhArcaInitCtxEntries_A;
  if (feedName == std::string("RB")) return efhArcaInitCtxEntries_B;

  if (feedName == std::string("XA")) return efhAmexInitCtxEntries_A;
  if (feedName == std::string("XB")) return efhAmexInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return efhBoxInitCtxEntries_A;
  if (feedName == std::string("BB")) return efhBoxInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return efhNomInitCtxEntries_A;
  if (feedName == std::string("NB")) return efhNomInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return efhGemInitCtxEntries_A;
  if (feedName == std::string("GB")) return efhGemInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return efhIseInitCtxEntries_A;
  if (feedName == std::string("IB")) return efhIseInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return efhPhlxTopoInitCtxEntries_A;
  if (feedName == std::string("TB")) return efhPhlxTopoInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return efhPhlxOrdInitCtxEntries_A;
  if (feedName == std::string("OB")) return efhPhlxOrdInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return efhCmeInitCtxEntries_A;
  if (feedName == std::string("EB")) return efhCmeInitCtxEntries_B;
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static size_t feedname2numProps (std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return std::size(efhBatsC1InitCtxEntries_A);
  if (feedName == std::string("CB")) return std::size(efhBatsC1InitCtxEntries_B);
  if (feedName == std::string("CC")) return std::size(efhBatsC1InitCtxEntries_C);
  if (feedName == std::string("CD")) return std::size(efhBatsC1InitCtxEntries_D);
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return std::size(efhMiaxInitCtxEntries_A);
  if (feedName == std::string("MB")) return std::size(efhMiaxInitCtxEntries_B);

  if (feedName == std::string("PA")) return std::size(efhPearlInitCtxEntries_A);
  if (feedName == std::string("PB")) return std::size(efhPearlInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return std::size(efhArcaInitCtxEntries_A);
  if (feedName == std::string("RB")) return std::size(efhArcaInitCtxEntries_B);

  if (feedName == std::string("XA")) return std::size(efhAmexInitCtxEntries_A);
  if (feedName == std::string("XB")) return std::size(efhAmexInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return std::size(efhBoxInitCtxEntries_A);
  if (feedName == std::string("BB")) return std::size(efhBoxInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return std::size(efhNomInitCtxEntries_A);
  if (feedName == std::string("NB")) return std::size(efhNomInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return std::size(efhGemInitCtxEntries_A);
  if (feedName == std::string("GB")) return std::size(efhGemInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return std::size(efhIseInitCtxEntries_A);
  if (feedName == std::string("IB")) return std::size(efhIseInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return std::size(efhPhlxTopoInitCtxEntries_A);
  if (feedName == std::string("TB")) return std::size(efhPhlxTopoInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return std::size(efhPhlxOrdInitCtxEntries_A);
  if (feedName == std::string("OB")) return std::size(efhPhlxOrdInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return std::size(efhCmeInitCtxEntries_A);
  if (feedName == std::string("EB")) return std::size(efhCmeInitCtxEntries_B);
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static size_t feedname2numGroups(std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return std::size(batsC1Groups);
  if (feedName == std::string("CB")) return std::size(batsC1Groups);
  if (feedName == std::string("CC")) return std::size(batsC1Groups);
  if (feedName == std::string("CD")) return std::size(batsC1Groups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return std::size(miaxGroups);
  if (feedName == std::string("MB")) return std::size(miaxGroups);

  if (feedName == std::string("PA")) return std::size(pearlGroups);
  if (feedName == std::string("PB")) return std::size(pearlGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return std::size(arcaGroups);
  if (feedName == std::string("RB")) return std::size(arcaGroups);

  if (feedName == std::string("XA")) return std::size(amexGroups);
  if (feedName == std::string("XB")) return std::size(amexGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return std::size(boxGroups);
  if (feedName == std::string("BB")) return std::size(boxGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return std::size(nomGroups);
  if (feedName == std::string("NB")) return std::size(nomGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return std::size(gemGroups);
  if (feedName == std::string("GB")) return std::size(gemGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return std::size(iseGroups);
  if (feedName == std::string("IB")) return std::size(iseGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return std::size(phlxTopoGroups);
  if (feedName == std::string("TB")) return std::size(phlxTopoGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return std::size(phlxOrdGroups);
  if (feedName == std::string("OB")) return std::size(phlxOrdGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return std::size(cmeGroups);
  if (feedName == std::string("EB")) return std::size(cmeGroups);
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

int createCtxts(std::vector<TestRunGroup>& testRunGroups,
		std::vector<EfhInitCtx>&   efhInitCtx,
		std::vector<EfhRunCtx>&    efhRunCtx) {
  const std::regex rg_regex("([0-9])\\:([A-Z][A-Z])\\:([0-9]+)\\.\\.([0-9]+)");

  for (auto &runGr : testRunGroups) {
    std::smatch base_match;
    if (! std::regex_match(runGr.optArgStr, base_match, rg_regex))
      on_error("%s is not valid format run Gr",runGr.optArgStr.c_str());

    auto feedName  = base_match[2].str();
    auto exch      = static_cast<EkaSource>(feedname2source(feedName));
    auto coreId    = static_cast<EkaCoreId>(std::stoi(base_match[1].str()));
    auto firstGrId = static_cast<EkaLSI>   (std::stoi(base_match[3].str()));
    auto lastGrId  = static_cast<EkaLSI>   (std::stoi(base_match[4].str()));
    /* ------------------------------------------------------- */

    EfhInitCtx newEfhInitCtx = {
      .ekaProps       = new EkaProps(),
      .numOfGroups    = feedname2numGroups(feedName),
      .coreId         = coreId,
      .recvSoftwareMd = true,
    };
    newEfhInitCtx.ekaProps->numProps = feedname2numProps(feedName);
    newEfhInitCtx.ekaProps->props    = feedname2prop(feedName);

    efhInitCtx.push_back(newEfhInitCtx);

    /* ------------------------------------------------------- */
    int i = 0;

    size_t numRunGroups = lastGrId - firstGrId + 1;
    auto pGroups = new EkaGroup[numRunGroups];
    if (pGroups == NULL) on_error("pGroups == NULL");

    for (EkaLSI grId = firstGrId; grId <= lastGrId; grId++) {
      pGroups[i].source  = exch;
      pGroups[i].localId = grId;
      if (grCtx[(int)exch][grId] != NULL)
	on_error("grCtx[%d][%d] already exists",(int)exch,grId);
      grCtx[(int)exch][grId] = new McGrpCtx(exch,grId);
      if (grCtx[(int)exch][grId] == NULL)
	on_error("failed creating grCtx[%d][%d]",(int)exch,grId);
      i++;
    }

    EfhRunCtx newEfhRunCtx = {
      .groups                      = pGroups,
      .numGroups                   = numRunGroups,
      .efhRunUserData              = 0,
      .onEfhOptionDefinitionMsgCb  = onOptionDefinition,
      .onEfhTradeMsgCb             = onTrade,
      .onEfhQuoteMsgCb             = onQuote,
      .onEfhOrderMsgCb             = onOrder,
      .onEfhGroupStateChangedMsgCb = onEfhGroupStateChange,
      .onEkaExceptionReportCb      = onException,
      .onEfhMdCb                   = onMd
    };
    efhRunCtx.push_back(newEfhRunCtx);

    /* ------------------------------------------------------- */

    TEST_LOG("RunGroup: %s, coreId=%d, %d..%d, numProps=%jd",
	     EKA_EXCH_DECODE(exch),
	     coreId,
	     firstGrId,
	     lastGrId,
	     newEfhInitCtx.ekaProps->numProps
	     );
    for (size_t j = 0; j < newEfhInitCtx.ekaProps->numProps; j++) {
      if (newEfhInitCtx.ekaProps == NULL) on_error("currEfhInitCtx.ekaProps == NULL");
      //      TEST_LOG("%s -- %s",newEfhInitCtx.ekaProps->props[j].szKey,newEfhInitCtx.ekaProps->props[j].szVal);
    }
  }
  return 0;
}
