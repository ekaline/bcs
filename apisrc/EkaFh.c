#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <errno.h>
#include <thread>
#include <assert.h>
#include <time.h>

#include "EkaFh.h"

 /* ##################################################################### */

static inline bool isPreTradeTime(int hour, int minute) {
  time_t rawtime;
  time (&rawtime);
  struct tm * ct = localtime (&rawtime);
  if (ct->tm_hour < hour || (ct->tm_hour == hour && ct->tm_min < minute)) return true;
  return false;
}

 /* ##################################################################### */

int EkaFh::setId(EfhCtx* pEfhCtx, EkaSource ec, uint8_t numFh) {
  assert (pEfhCtx != NULL);
  dev = pEfhCtx->dev;
  assert(dev != NULL);

  exch = ec;
  feed_ver = EFH_EXCH2FEED(exch);
  //  full_book = EFH_EXCH2FULL_BOOK(exch);
  id = numFh;
  if (exch == EkaSource::kInvalid) on_error ("exch (%d) == EkaSource::kInvalid",(int)exch);
  print_parsed_messages = dev->print_parsed_messages;

  return 0;
}

 /* ##################################################################### */
int EkaFh::init(const EfhInitCtx* pEfhInitCtx, uint8_t numFh) {
  c = pEfhInitCtx->coreId;

  assert (pEfhInitCtx->numOfGroups <= EKA_FH_GROUPS);
  groups = pEfhInitCtx->numOfGroups;

  for (uint i = 0; i < pEfhInitCtx->ekaProps->numProps; i++) {
    EkaProp& ekaProp{ pEfhInitCtx->ekaProps->props[i] };
    //    EKA_DEBUG("ekaProp.szKey = %s, ekaProp.szVal = %s",ekaProp.szKey,ekaProp.szVal);   fflush(stderr);
    conf_parse(ekaProp.szKey,ekaProp.szVal);
  }
  for (uint i=0; i < groups; i++) {
    if (auth_set && ! b_gr[i]->auth_set) {
      memcpy(b_gr[i]->auth_user,  auth_user,  sizeof(auth_user));
      memcpy(b_gr[i]->auth_passwd,auth_passwd,sizeof(auth_passwd));
      b_gr[i]->auth_set = true;
    }
    EKA_DEBUG("initializing FH %s:%u: MCAST: %s:%u, SNAPSHOT: %s:%u, RECOVERY: %s:%u, AUTH: %s:%s",
	    EKA_EXCH_DECODE(b_gr[i]->exch),
	    b_gr[i]->id,
	    EKA_IP2STR(b_gr[i]->mcast_ip),   be16toh(b_gr[i]->mcast_port),
	    EKA_IP2STR(b_gr[i]->snapshot_ip),be16toh(b_gr[i]->snapshot_port),
	    EKA_IP2STR(b_gr[i]->recovery_ip),be16toh(b_gr[i]->recovery_port),
	    b_gr[i]->auth_set ? std::string(b_gr[i]->auth_user,sizeof(b_gr[i]->auth_user)).c_str() : "NOT SET",
	    b_gr[i]->auth_set ? std::string(b_gr[i]->auth_passwd,sizeof(b_gr[i]->auth_passwd)).c_str() : "NOT SET"
	    );
  }
  any_group_getting_snapshot = false;

  EKA_DEBUG("%s is Initialized with feed_ver = %s, %u MC groups",EKA_EXCH_DECODE(exch), EKA_FEED_VER_DECODE(feed_ver),groups);

  return 0;
}

 /* ##################################################################### */

void EkaFh::send_igmp(bool join_leave, volatile bool igmp_thread_active) {
  //  EKA_DEBUG("Sending IGMP %s", join_leave ? "JOIN" : "LEAVE");
  for (auto i = 0; i < groups; i++) {
    if (b_gr[i] == NULL) continue;
    if (join_leave && ! igmp_thread_active) break; // dont send JOIN
    b_gr[i]->send_igmp(join_leave);
    usleep (10);
  }
  return;
}
 /* ##################################################################### */
int EkaFh::stop() {
  EKA_DEBUG("Stopping %s (fhId=%u)",EKA_EXCH_DECODE(exch),id);
  for (uint i=0; i < groups ; i++) {
    if (b_gr[i] != NULL) b_gr[i]->stop();
  }
  return 0;
}

 /* ##################################################################### */
EkaFh::~EkaFh() {
  EKA_DEBUG("destroying %s:%u",EKA_EXCH_DECODE(exch),id);
  for (uint i=0; i < groups ; i++) {
    if (b_gr[i] == NULL) continue;
    delete b_gr[i];
    b_gr[i] = NULL;
  }
}

/* ##################################################################### */

int EkaFh::openGroups(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  assert (pEfhInitCtx->numOfGroups <= EKA_FH_GROUPS);
  groups = pEfhInitCtx->numOfGroups;

  for (uint8_t i=0; i < groups ; i++) {
    b_gr[i] = addGroup();
    if (b_gr[i] == NULL) on_error("b_gr[i] == NULL");

    b_gr[i]->init(pEfhCtx,pEfhInitCtx,this,i,exch);
    b_gr[i]->bookInit(pEfhCtx,pEfhInitCtx);

    EKA_LOG("%s:%u initialized as %s:%u at runGroups",
	    EKA_EXCH_DECODE(exch),b_gr[i]->id,EKA_EXCH_DECODE(b_gr[i]->exch),i);

    if (print_parsed_messages) {
      std::string parsedMsgFileName = std::string(EKA_EXCH_DECODE(exch)) + std::to_string(i) + std::string("_PARSED_MESSAGES.txt");
      if((b_gr[i]->book->parser_log = fopen(parsedMsgFileName.c_str(),"w")) == NULL) on_error ("Error %s",parsedMsgFileName.c_str());
      EKA_LOG("%s:%u created file %s",EKA_EXCH_DECODE(exch),b_gr[i]->id,parsedMsgFileName.c_str());
    }

  }
  return 0;
}

 /* ##################################################################### */

static inline uint8_t nextQ(uint8_t c,uint first, uint max) {
  if (max == 1 || c == max - 1) return first;
  return c + 1;
}

 /* ##################################################################### */

FhGroup* EkaFh::nextGrToProcess(uint first, uint numGroups) {
  uint8_t curr = nextQ(lastServed,first, numGroups);
  for (uint8_t i = 0; i < numGroups; i ++) {
    if (b_gr[curr] == NULL) on_error ("%s: numGroups=%u, but b_gr[%u] == NULL",
				      EKA_EXCH_DECODE(exch),numGroups,curr);
    if (b_gr[curr]->q == NULL) on_error ("%s: b_gr[%u]->q == NULL",
					 EKA_EXCH_DECODE(exch),curr);

    if (b_gr[curr]->q->is_empty() || b_gr[curr]->state == FhGroup::GrpState::GAP) {
      curr = nextQ(curr,first,(uint)numGroups);
      continue;
    }
    lastServed = curr;
    return b_gr[curr];
  }

  return NULL;
}

 /* ##################################################################### */

EkaOpResult EkaFh::subscribeStaticSecurity(
					   uint8_t groupNum, 
					   uint64_t securityId, 
					   EfhSecurityType efhSecurityType,
					   EfhSecUserData efhSecUserData,
					   uint64_t opaqueAttrA,
					   uint64_t opaqueAttrB) {
  if (groupNum >= groups) on_error("groupNum (%u) >= groups (%u)",groupNum,groups);
  if (exch == EkaSource::kBOX_HSVF)
    b_gr[groupNum]->book->subscribe_security64 (securityId, 
						static_cast< uint8_t >( efhSecurityType ), 
						efhSecUserData,opaqueAttrA,opaqueAttrB);
  else
    b_gr[groupNum]->book->subscribe_security (securityId, 
					      static_cast< uint8_t >( efhSecurityType ), 
					      efhSecUserData,opaqueAttrA,opaqueAttrB);
  return EKA_OPRESULT__OK;
}

 /* ##################################################################### */

EkaFhAddConf EkaFh::conf_parse(const char *key, const char *value) {
  char val_buf[200] = {};
  strcpy (val_buf,value);
  int i=0;
  char* v[10];
  v[i] = strtok(val_buf,":");
  while(v[i]!=NULL) v[++i] = strtok(NULL,":");

  // parsing KEY
  char key_buf[200] = {};
  strcpy (key_buf,key);
  i=0;
  char* k[10];
  k[i] = strtok(key_buf,".");
  while(k[i]!=NULL) k[++i] = strtok(NULL,".");
  //---------------------------------------------------------------------
 // efh.NOM_ITTO.group.X.snapshot.auth, user:passwd
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"snapshot")==0) && (strcmp(k[5],"auth")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      char string_filler = EFH_EXCH2FEED(exch) == EfhFeedVer::kXDP ? '\0' : ' ';
      memset(&(b_gr[gr]->auth_user),string_filler,sizeof(b_gr[gr]->auth_user));
      memset(&(b_gr[gr]->auth_passwd),string_filler,sizeof(b_gr[gr]->auth_passwd));
      memcpy(&(b_gr[gr]->auth_user),v[0],strlen(v[0]));
      memcpy(&(b_gr[gr]->auth_passwd),v[1],strlen(v[1]));

      //      EKA_DEBUG ("auth credentials for %s:%u are set to |%s|:|%s|",k[1],gr,b_gr[gr]->auth_user + '\0',b_gr[gr]->auth_passwd + '\0');
      b_gr[gr]->auth_set = true;
      fflush(stderr);

      return EkaFhAddConf::CONF_SUCCESS;
    }
  }  //---------------------------------------------------------------------
 // efh.NOM_ITTO.snapshot.auth, user:passwd
  // k[0] k[1]   k[2]     k[3] 
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"snapshot")==0) && (strcmp(k[3],"auth")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      char string_filler = EFH_EXCH2FEED(exch) == EfhFeedVer::kXDP ? '\0' : ' ';

      memset(&(auth_user),string_filler,sizeof(auth_user));
      memset(&(auth_passwd),string_filler,sizeof(auth_passwd));
      memcpy(&(auth_user),v[0],strlen(v[0]));
      memcpy(&(auth_passwd),v[1],strlen(v[1]));
      //      EKA_DEBUG ("auth credentials for %s are set to %s:%s",k[1],v[0] +'\0',v[1]+'\0');
      fflush(stderr);
      auth_set = true;
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.BATS_PITCH.group.X.unit, "1"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"unit")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      ((FhBatsGr*)b_gr[gr])->batsUnit = (uint8_t) strtoul(v[0],NULL,10);
      //      EKA_DEBUG ("batsUnit for %s:%u is set to %u",k[1],gr,((FhBatsGr*)b_gr[gr])->batsUnit);
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }  
  //---------------------------------------------------------------------
  // efh.BATS_PITCH.group.X.queue_capacity, "1048576"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"queue_capacity")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      //      uint8_t gr = (uint8_t) atoi(k[3]);
      qsize = strtoul(v[0],NULL,10);
      //      EKA_DEBUG ("%s qsize is set to %u",k[1],qsize);
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.BATS_PITCH.queue_capacity, "1048576"
  // k[0] k[1]       k[2] 
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"queue_capacity")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      qsize = strtoul(v[0],NULL,10);
      //      EKA_DEBUG ("%s qsize is set to %u",k[1],qsize);
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.BATS_PITCH.group.X.snapshot.sessionSubID, "0285"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"snapshot")==0) && (strcmp(k[5],"sessionSubID")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      memcpy(&(((FhBatsGr*)b_gr[gr])->sessionSubID),v[0],sizeof(((FhBatsGr*)b_gr[gr])->sessionSubID));

      EKA_DEBUG ("sessionSubID for %s:%u are set to %s",k[1],gr,v[0] +'\0');
      fflush(stderr);

      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.NOM_ITTO.group.X.mcast.addr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"mcast")==0) && (strcmp(k[5],"addr")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      //      if (gr >= groups) on_error("%s -- %s : group_id %d >= groups (=%u)",key, value,gr,groups);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d >= groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      inet_aton (v[0],(struct in_addr*) &b_gr[gr]->mcast_ip);
      b_gr[gr]->mcast_port =  htons((uint16_t)atoi(v[1]));
      b_gr[gr]->mcast_set  = true;
      //      EKA_DEBUG ("%s %s for %s:%u is set to %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));
      fflush(stderr);

      return EkaFhAddConf::CONF_SUCCESS;
    } 
  }
  //---------------------------------------------------------------------
  // efh.NOM_ITTO.group.X.snapshot.addr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"snapshot")==0) && (strcmp(k[5],"addr")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      //      if (gr >= groups) on_error("%s -- %s : group_id %d >= groups (=%u)",key, value,gr,groups);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d >= groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }

      inet_aton (v[0],(struct in_addr*) &b_gr[gr]->snapshot_ip);
      b_gr[gr]->snapshot_port =  htons((uint16_t)atoi(v[1]));
      b_gr[gr]->snapshot_set = true;
      //      EKA_DEBUG ("%s %s for %s:%u is set to %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.NOM_ITTO.group.X.recovery.addr, x.x.x.x:xxxx
  // k[0] k[1]    k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"recovery")==0) && (strcmp(k[5],"addr")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      //      if (gr >= groups) on_error("%s -- %s : group_id %d >= groups (=%u)",key, value,gr,groups);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d >= groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      inet_aton (v[0],(struct in_addr*) &b_gr[gr]->recovery_ip);
      b_gr[gr]->recovery_port = htons((uint16_t)atoi(v[1]));
      b_gr[gr]->recovery_set  = true;
      //      EKA_DEBUG ("%s %s for %s:%u is set to %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.BOX_HSVF.group.X.mcast.line   ,"11"
  // k[0] k[1]    k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"mcast")==0) && (strcmp(k[5],"line")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (b_gr[gr] == NULL) on_error("Group %u does not exist",gr);
      //      if (gr >= groups) on_error("%s -- %s : group_id %d >= groups (=%u)",key, value,gr,groups);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d >= groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }

      memset(&b_gr[gr]->line,' ',sizeof(b_gr[gr]->line));
      memcpy(&b_gr[gr]->line,v[0],std::min(sizeof(b_gr[gr]->line),sizeof(v[0])));
      //      EKA_DEBUG ("%s:%u line = %s", EKA_EXCH_SOURCE_DECODE(exch),gr,v[0]);
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  return EkaFhAddConf::UNKNOWN_KEY;
}
 /* ##################################################################### */

EkaOpResult EkaFh::initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhRunGr* runGr) {
  for (uint8_t i = 0; i < runGr->numGr; i++) {
    if (! runGr->isMyGr(pEfhRunCtx->groups[i].localId)) 
      on_error("pEfhRunCtx->groups[%d].localId = %u doesnt belong to %s",
	       i,pEfhRunCtx->groups[i].localId,runGr->list2print);
    if (pEfhRunCtx->groups[i].source != exch) 
      on_error("pEfhRunCtx->groups[i].source != exch");
    FhGroup* gr = b_gr[pEfhRunCtx->groups[i].localId];
    if (gr == NULL) on_error ("b_gr[%u] == NULL",pEfhRunCtx->groups[i].localId);
    gr->createQ(pEfhCtx,qsize);
    gr->expected_sequence = 1;

    runGr->udpCh->igmp_mc_join (dev->core[c]->srcIp, gr->mcast_ip,gr->mcast_port,0);
    EKA_DEBUG("%s:%u: joined %s:%u for %u securities",
	      EKA_EXCH_DECODE(exch),gr->id,
	      EKA_IP2STR(gr->mcast_ip),be16toh(gr->mcast_port),
	      gr->book->total_securities);
  }
  return EKA_OPRESULT__OK;
}

 /* ##################################################################### */
EkaOpResult EkaFhNasdaq::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  overture();

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;
    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;

    uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (pkt == NULL) continue;
    FhNasdaqGr* gr = (FhNasdaqGr*)b_gr[gr_id];
    if (gr == NULL) on_error("b_gr[%u] = NULL",gr_id);

    //-----------------------------------------------------------------------------
    switch (gr->state) {
    case FhGroup::GrpState::INIT : { 
      gr->gapClosed = false;
      gr->state = FhGroup::GrpState::SNAPSHOT_GAP;
      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx,gr, 1, 0);
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",
		EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = FhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx, expected_sequence, sequence + msgInPkt);

      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::SNAPSHOT_GAP : {
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence,gr_id);

      if (gr->gapClosed) {
	gr->state = FhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed - expected_sequence=%ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->expected_sequence);
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::RETRANSMIT_GAP : {
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence,gr_id);
      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, switching to fetch from Q",
		EKA_EXCH_DECODE(exch),gr->id);
	gr->state = FhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
      }
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
	       EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  return EKA_OPRESULT__OK;
}





 /* ##################################################################### */
