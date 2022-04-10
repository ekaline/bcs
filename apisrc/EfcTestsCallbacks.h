#ifndef _EFC_TESTS_CALLBACKS_H_
#define _EFC_TESTS_CALLBACKS_H_

static int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

static int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",
	  user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}

static int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

static int getTradeTimeCb (const EfhDateComponents *, uint32_t* iso8601Date,
		     time_t *epochTime, void* ctx) {
  return 0;
}

static void* onOrder(const EfhOrderMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0
  if (! testCtx->keep_work) return NULL;

  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  int secIdx = (int)secData;
#endif

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);

  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];

  if (pEfhCtx->printQStatistics && (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group,testCtx->verbose_statistics);
  }

  if (! testCtx->print_tob_updates) return NULL;

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

#endif
  return NULL;
}

void* onTrade(const EfhTradeMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0  
  if (! testCtx->keep_work) return NULL;

  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);
  
  //  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];  

#ifdef EKA_TEST_IGNORE_DEFINITIONS
  std::string currAvtSecName  = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  //  int64_t priceScaleFactor = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE;
#else
  int secIdx                  = (int)secData;
  std::string currAvtSecName  = gr->security.at(secIdx).avtSecName.c_str();	  
  std::string currClassSymbol = gr->security.at(secIdx).classSymbol;
  //  int64_t priceScaleFactor    = 100; //gr->security.at(secIdx).displayPriceScale;
#endif

  if (! testCtx->print_tob_updates) return NULL;

  fprintf(gr->MD,"Trade,");
  fprintf(gr->MD,"%s," ,eka_get_date().c_str());
  fprintf(gr->MD,"%s," ,eka_get_time().c_str());
  fprintf(gr->MD,"%ju,",msg->header.securityId);
  fprintf(gr->MD,"%s," ,currAvtSecName.c_str());
  fprintf(gr->MD,"%s," ,currClassSymbol.c_str());
  fprintf(gr->MD,"%ld,",msg->price);
  fprintf(gr->MD,"%u," ,msg->size);
  fprintf(gr->MD,"%d," ,(int)msg->tradeCond);
  fprintf(gr->MD,"%s," ,ts_ns2str(msg->header.timeStamp).c_str());
  fprintf(gr->MD,"%ju,",msg->header.timeStamp);
  fprintf(gr->MD,"\n");

#endif  
  return NULL;
}

void* onEfhGroupStateChange(const EfhGroupStateChangedMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  //  int file_idx = (uint8_t)(msg->group.localId);
  //  EfhCtx* pEfhCtx = (EfhCtx*) userData;
#if 0
  
#ifdef FH_LAB
  FILE* logFile = stdout;
#else  
  EkaSource exch = msg->group.source;
  EkaLSI    grId = msg->group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);
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
      if (++testCtx->fatalErrorCnt == testCtx->MaxFatalErrors) on_error("testCtx->MaxFatalErrors %d reached",testCtx->MaxFatalErrors);
      break;

    case EfhErrorDomain::kSocketError :
      printf ("=========================\n%s: SocketError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt == testCtx->MaxFatalErrors) on_error("testCtx->MaxFatalErrors %d reached",testCtx->MaxFatalErrors);
      break;

    case EfhErrorDomain::kCredentialError :
      printf ("=========================\n%s: CredentialError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt == testCtx->MaxFatalErrors) on_error("testCtx->MaxFatalErrors %d reached",testCtx->MaxFatalErrors);

      break;

    case EfhErrorDomain::kOSError :
      printf ("=========================\n%s: OSError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt == testCtx->MaxFatalErrors) on_error("testCtx->MaxFatalErrors %d reached",testCtx->MaxFatalErrors);

      break;

    case EfhErrorDomain::kDeviceError :
      printf ("=========================\n%s: DeviceError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt == testCtx->MaxFatalErrors) on_error("testCtx->MaxFatalErrors %d reached",testCtx->MaxFatalErrors);

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
  case EfhGroupState::kProgressing : {
    std::string gapType = std::string("Progressing");
    fprintf(logFile,"%s: %s : %s \n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
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
#endif  
  return NULL;
}

void onException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData) {
#if 0  
  if (! testCtx->keep_work) return;
  printf("%s: Doing nothing\n",__func__);
#endif  
  return;
}

/* void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size) { */
void onFireReport (const void* fire_report_buf, size_t size) {
#if 0  
  if (! testCtx->keep_work) return;
  printf ("%s: Doing nothing \n",__func__);
#endif  
  return;	 
}
extern FILE* mdFile;

void* onMd(const EfhMdHeader* msg, EfhRunUserData efhRunUserData) {
#if 0  
  if (! testCtx->keep_work) return NULL;
  EfhCtx* pEfhCtx = (EfhCtx*) efhRunUserData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

#ifdef FH_LAB
  FILE* logFile = (FILE*) efhRunUserData; //stdout;
#else  
  EkaSource exch = msg->group.source;
  EkaLSI    grId = msg->group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);
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
#endif  
  return NULL;	 
}

void* onQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0  
  if (! testCtx->keep_work) return NULL;
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);
  
  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];  

#ifdef EKA_TEST_IGNORE_DEFINITIONS
  std::string currAvtSecName  = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  int64_t priceScaleFactor = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE;
#else
  int secIdx                  = (int)secData;
  if (secIdx < 0 || secIdx >= (int)gr->security.size()) {
    /* on_error("Bad secIdx = %d, gr->security.size() = %d", */
    /* 	     secIdx, (int)gr->security.size()); */
      on_warning("Bad secIdx = %d, gr->security.size() = %d, msg->header.securityId = %ju",
		 secIdx, (int)gr->security.size(),msg->header.securityId);
      return NULL;
  }
  std::string currAvtSecName  = gr->security.at(secIdx).avtSecName.c_str();	  
  std::string currClassSymbol = gr->security.at(secIdx).classSymbol;
  int64_t priceScaleFactor    = gr->security.at(secIdx).displayPriceScale; // 100
#endif

  if (pEfhCtx->printQStatistics && (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group,testCtx->verbose_statistics);
  }

  if (! testCtx->print_tob_updates) return NULL;

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

#endif  
  return NULL;
}

static void* onComplexDefinition(const EfhComplexDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0  
  if (! testCtx->keep_work) return NULL;
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);
 
  fprintf(gr->MD,"ComplexDefinition,");
  fprintf(gr->MD,"%ju,",msg->header.securityId);
  fprintf(gr->MD,"\'%s\',",std::string(msg->commonDef.underlying,sizeof(msg->commonDef.underlying)).c_str());
  fprintf(gr->MD,"\'%s\',",std::string(msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol)).c_str());
  fprintf(gr->MD,"\'%s\',",std::string(msg->commonDef.exchSecurityName,sizeof(msg->commonDef.exchSecurityName)).c_str());
  for (auto i = 0; i < msg->numLegs; i++) {
    auto leg {&msg->legs[i]};
    fprintf(gr->MD,"{%ju(0x%jx),",leg->securityId,leg->securityId);
    fprintf(gr->MD,"%c,%c,",printEfhSecurityType(leg->type),printEfhOrderSide(leg->side));
    fprintf(gr->MD,"%d,%d,",leg->ratio,leg->optionDelta);
    fprintf(gr->MD,"%jd},",leg->price);
  }
  fprintf(gr->MD,"\n");

  TestSecurityCtx newSecurity = {
      .securityId        = msg->header.securityId,
      .avtSecName        = std::string(msg->commonDef.exchSecurityName,sizeof(msg->commonDef.exchSecurityName)),
      .underlying        = std::string(msg->commonDef.underlying,sizeof(msg->commonDef.underlying)),
      .classSymbol       = std::string(msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol)),
      .exch              = msg->commonDef.exchange,
      .displayPriceScale = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE,
      .isCompllex        = true
  };
    
    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;
    
    efhSubscribeStatic(pEfhCtx, (EkaGroup*)&msg->header.group, msg->header.securityId, EfhSecurityType::kOption,(EfhSecUserData) sec_idx,msg->commonDef.opaqueAttrA,msg->commonDef.opaqueAttrB);

#endif    
  return NULL;
}
/* ------------------------------------------------------------ */

static void* onAuctionUpdate(const EfhAuctionUpdateMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0  
  if (! testCtx->keep_work) return NULL;
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr {testCtx->grCtx[(int)exch][grId]};
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);

  auto secCtx = gr->getSecurityCtx(msg->header.securityId);

  std::string currAvtSecName  = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  int64_t priceScaleFactor = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE;
  EfhExchange exchName = EfhExchange::kUnknown;
  
  if (secCtx != nullptr) {
      currAvtSecName   = secCtx->avtSecName;
      currClassSymbol  = secCtx->classSymbol;
      priceScaleFactor = secCtx->displayPriceScale;
      exchName         = secCtx->exch;
  }
   
  
  //  RfqTable5,date,time,QuoteID,Name,Security,Price,Size,Capacity,ExpirationTime,Side,Exchange,Type,ActionType,Customer,Imbalance
  //  RfqTable5,20210628,10:48:20.030.380,l6s01552,TSLA,TSLA__210702P00650000,5.5,67,C,,B,B,D,N,0333SF3,0
  fprintf(gr->MD,"RfqTable5,");
  fprintf(gr->MD,"%s,",    eka_get_date().c_str());
  fprintf(gr->MD,"%s,",    eka_get_time().c_str());
  fprintf(gr->MD,"%ju,",   msg->auctionId);
  fprintf(gr->MD,"%s,",    currClassSymbol.c_str());
  fprintf(gr->MD,"%s,",    currAvtSecName.c_str());
  fprintf(gr->MD,"%*.f,",  decPoints(msg->price,priceScaleFactor), ((float) msg->price / priceScaleFactor));
  fprintf(gr->MD,"%u,",    msg->quantity);
  fprintf(gr->MD,"%s,",    ts_ns2str(msg->endTimeNanos).c_str());

  fprintf(gr->MD,"%c,",    msg->side == EfhOrderSide::kBid ? 'B' : 'S');
  fprintf(gr->MD,"%c,",    (char)exchName);

  fprintf(gr->MD,"%c,",    (char)msg->auctionType);
  fprintf(gr->MD,"%c,",    (char)msg->capacity);
  fprintf(gr->MD,"%d,",    (int)msg->securityType);

  fprintf(gr->MD,"%s,",    std::string(msg->firmId,sizeof msg->firmId).c_str());

  fprintf(gr->MD,"%s,",    (ts_ns2str(msg->header.timeStamp)).c_str());
  fprintf(gr->MD,"\n");

#endif  
  return NULL;
}
/* ------------------------------------------------------------ */

static void* onFutureDefinition(const EfhFutureDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0  
  if (! testCtx->keep_work) return NULL;
  
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  auto exch {msg->header.group.source};
  auto grId {msg->header.group.localId};

  auto gr {testCtx->grCtx[(int)exch][grId]};
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);

  std::string underlyingName = std::string(msg->commonDef.underlying, sizeof(msg->commonDef.underlying));
  std::string classSymbol    = std::string(msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(), ' ', '\0');
  std::replace(classSymbol.begin(),    classSymbol.end(),    ' ', '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize   (strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = "Future";

  //  eka_create_avt_definition(avtSecName,msg);
  fprintf (gr->fullDict,"%s,%ju,%s,%s,%s\n",
  	   avtSecName,
  	   msg->header.securityId,
  	   underlyingName.c_str(),
  	   classSymbol.c_str(),
  	   EKA_PRINT_GRP(&msg->header.group)
  	   );


  if (testCtx->subscribe_all || (std::find(testCtx->underlyings.begin(), testCtx->underlyings.end(), underlyingName) != testCtx->underlyings.end())) {
    if (std::find(testCtx->securities.begin(),testCtx->securities.end(),msg->header.securityId) != testCtx->securities.end()) return NULL;
    testCtx->securities.push_back(msg->header.securityId);
	
    TestSecurityCtx newSecurity = {
  	.securityId        = msg->header.securityId,
  	.avtSecName        = std::string(avtSecName),
  	.underlying        = underlyingName,
  	.classSymbol       = classSymbol,
  	.exch              = msg->commonDef.exchange,
  	.displayPriceScale = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE,
	.isCompllex        = false
    };
    
    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;
    
    efhSubscribeStatic(pEfhCtx, (EkaGroup*)&msg->header.group, msg->header.securityId, EfhSecurityType::kOption,(EfhSecUserData) sec_idx,msg->commonDef.opaqueAttrA,msg->commonDef.opaqueAttrB);

    fprintf (gr->subscrDict,"%s,%ju%s\n",
  	     avtSecName,
  	     msg->header.securityId,
  	     EKA_PRINT_GRP(&msg->header.group)
  	     );
  }

#endif  
  return NULL;
}
/* ------------------------------------------------------------ */

void* onOptionDefinition(const EfhOptionDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
#if 0  
  if (! testCtx->keep_work) return NULL;
  
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  auto exch {msg->header.group.source};
  auto grId {msg->header.group.localId};

  auto gr {testCtx->grCtx[(int)exch][grId]};
  if (gr == NULL) on_error("Uninitialized testCtx->grCtx[%d][%d]",(int)exch,grId);

  std::string underlyingName = std::string(msg->commonDef.underlying, sizeof(msg->commonDef.underlying));
  std::string classSymbol    = std::string(msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(), ' ', '\0');
  std::replace(classSymbol.begin(),    classSymbol.end(),    ' ', '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize   (strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = {};

  eka_create_avt_definition(avtSecName,msg);
  fprintf (gr->fullDict,"%s,%ju,%s,%s,%s,%s,%ju,%ju\n",
	   avtSecName,
	   msg->header.securityId,
	   exch == EkaSource::kC1_PITCH ? EKA_PRINT_BATS_SYMBOL((char*)&msg->commonDef.opaqueAttrA) : " ",
	   underlyingName.c_str(),
	   classSymbol.c_str(),
	   EKA_PRINT_GRP(&msg->header.group),
	   msg->commonDef.opaqueAttrA,
	   msg->commonDef.opaqueAttrB
	   );


  if (testCtx->subscribe_all || (std::find(testCtx->underlyings.begin(), testCtx->underlyings.end(), underlyingName) != testCtx->underlyings.end())) {
    if (std::find(testCtx->securities.begin(),testCtx->securities.end(),msg->header.securityId) != testCtx->securities.end()) return NULL;
    testCtx->securities.push_back(msg->header.securityId);
	
    TestSecurityCtx newSecurity = {
	.securityId        = msg->header.securityId,
	.avtSecName        = std::string(avtSecName),
	.underlying        = underlyingName,
	.classSymbol       = classSymbol,
	.exch              = msg->commonDef.exchange,
	.displayPriceScale = exch == EkaSource::kCME_SBE ? CME_DEFAULT_DISPLAY_PRICE_SCALE : DEFAULT_DISPLAY_PRICE_SCALE,
	.isCompllex        = false
    };
    
    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;
    
    efhSubscribeStatic(pEfhCtx, (EkaGroup*)&msg->header.group, msg->header.securityId, EfhSecurityType::kOption,(EfhSecUserData) sec_idx, msg->commonDef.opaqueAttrA,msg->commonDef.opaqueAttrB);

    fprintf (gr->subscrDict,"%s,%ju,%s,%s\n",
	     avtSecName,
	     msg->header.securityId,
	     exch == EkaSource::kC1_PITCH ? EKA_PRINT_BATS_SYMBOL((char*)&msg->commonDef.opaqueAttrA) : " ",
	     EKA_PRINT_GRP(&msg->header.group)
	     );
  }

#endif  
  return NULL;
}
#endif
