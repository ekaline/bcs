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
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhBook.h"

#include "EkaFhBoxGr.h"
#include "EkaFhBatsGr.h"

#include "EkaCore.h"
#include "eka_fh_q.h"
#include "EkaHwCaps.h"


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

  return 0;
}
/* ##################################################################### */

uint8_t EkaFh::getGrId(const uint8_t* pkt) {
  EkaIpHdr* ipHdr = (EkaIpHdr*)(pkt - 8 - 20);
  uint32_t ip = ipHdr->dest;
  for (uint8_t i = 0; i < groups; i++)
    if (b_gr[i]->mcast_ip == ip && b_gr[i]->mcast_port == EKA_UDPHDR_DST((pkt-8)))
      return i;
  
  EKA_WARN("ip 0x%08x %s not found",ip, EKA_IP2STR(ip));
  
  return 0xFF;
}
 /* ##################################################################### */
int EkaFh::init(const EfhInitCtx* pEfhInitCtx, uint8_t numFh) {
  c = pEfhInitCtx->coreId;

  if ((dev->ekaHwCaps->hwCaps.core.bitmap_md_cores & (1 << c)) == 0) {
    on_error("Core %u is not enabled in FPGA for Market Data RX",c);
  }

  assert (pEfhInitCtx->numOfGroups <= EKA_FH_GROUPS);
  groups = pEfhInitCtx->numOfGroups;

  print_parsed_messages = pEfhInitCtx->printParsedMessages;

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
    EKA_DEBUG("initializing FH coreId=%d %s:%u: MCAST: %s:%u, SNAPSHOT: %s:%u, RECOVERY: %s:%u, AUTH: %s:%s, connectRetryDelayTime=%d",
	      c,
	      EKA_EXCH_DECODE(b_gr[i]->exch),
	      b_gr[i]->id,
	      EKA_IP2STR(b_gr[i]->mcast_ip),   b_gr[i]->mcast_port,
	      EKA_IP2STR(b_gr[i]->snapshot_ip),be16toh(b_gr[i]->snapshot_port),
	      EKA_IP2STR(b_gr[i]->recovery_ip),be16toh(b_gr[i]->recovery_port),
	      b_gr[i]->auth_set ? std::string(b_gr[i]->auth_user,sizeof(b_gr[i]->auth_user)).c_str() : "NOT SET",
	      b_gr[i]->auth_set ? std::string(b_gr[i]->auth_passwd,sizeof(b_gr[i]->auth_passwd)).c_str() : "NOT SET",
	      b_gr[i]->connectRetryDelayTime
	      );
  }
  any_group_getting_snapshot = false;

  EKA_DEBUG("%s is Initialized: coreId = %u, feed_ver = %s, %u MC groups",
	    EKA_EXCH_DECODE(exch), c, EKA_FEED_VER_DECODE(feed_ver),groups);

  return 0;
}

 /* ##################################################################### */

/* void EkaFh::send_igmp(bool join_leave, volatile bool igmp_thread_active) { */
/*   //  EKA_DEBUG("Sending IGMP %s", join_leave ? "JOIN" : "LEAVE"); */
/*   for (auto i = 0; i < groups; i++) { */
/*     if (b_gr[i] == NULL) continue; */
/*     if (join_leave && ! igmp_thread_active) break; // dont send JOIN */
/*     b_gr[i]->send_igmp(join_leave); */
/*     usleep (10); */
/*   } */
/*   return; */
/* } */
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
    b_gr[i]->bookInit();

    EKA_LOG("%s:%u initialized as %s:%u at runGroups",
	    EKA_EXCH_DECODE(exch),b_gr[i]->id,EKA_EXCH_DECODE(b_gr[i]->exch),i);
  }
  return 0;
}

 /* ##################################################################### */

static inline uint8_t nextQ(uint8_t c,uint first, uint max) {
  if (max == 1 || c == max - 1) return first;
  return c + 1;
}

 /* ##################################################################### */

EkaFhGroup* EkaFh::nextGrToProcess(uint first, uint numGroups) {
  uint8_t curr = nextQ(lastServed,first, numGroups);
  for (uint8_t i = 0; i < numGroups; i ++) {
    if (b_gr[curr] == NULL) on_error ("%s: numGroups=%u, but b_gr[%u] == NULL",
				      EKA_EXCH_DECODE(exch),numGroups,curr);
    if (b_gr[curr]->q == NULL) on_error ("%s: b_gr[%u]->q == NULL",
					 EKA_EXCH_DECODE(exch),curr);

    if (b_gr[curr]->q->is_empty() || b_gr[curr]->state == EkaFhGroup::GrpState::GAP) {
      curr = nextQ(curr,first,(uint)numGroups);
      continue;
    }
    lastServed = curr;
    return b_gr[curr];
  }

  return NULL;
}

 /* ##################################################################### */

EkaOpResult EkaFh::subscribeStaticSecurity(uint8_t groupNum, 
					   uint64_t securityId, 
					   EfhSecurityType efhSecurityType,
					   EfhSecUserData efhSecUserData,
					   uint64_t opaqueAttrA,
					   uint64_t opaqueAttrB) {
  if (groupNum >= groups) on_error("groupNum (%u) >= groups (%u)",groupNum,groups);
  if (b_gr[groupNum] == NULL) on_error("b_gr[%u] == NULL",groupNum);

  b_gr[groupNum]->subscribeStaticSecurity(securityId, 
					  efhSecurityType,
					  efhSecUserData,
					  opaqueAttrA,
					  opaqueAttrB);

  b_gr[groupNum]->numSecurities++;
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
  // efh.NOM_ITTO.group.X.snapshot.connectRetryDelayTime, <numSec>
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"snapshot")==0) && (strcmp(k[5],"connectRetryDelayTime")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL",gr);

      b_gr[gr]->connectRetryDelayTime = atoi(v[0]);
      fflush(stderr);

      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.NOM_ITTO.group.X.snapshot.auth, user:passwd
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"snapshot")==0) && (strcmp(k[5],"auth")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL",gr);

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
  }  
  //---------------------------------------------------------------------
  // efh.C1_PITCH.group.X.recovery.grpAuth, user:passwd
  // k[0] k[1]   k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"recovery")==0) && (strcmp(k[5],"grpAuth")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      
      auto batsGr {reinterpret_cast<EkaFhBatsGr*>(b_gr[gr])};
      if (batsGr == NULL) on_error("b_gr[%u] == NULL",gr);

      /* memset(batsGr->grpUser,  '\0',sizeof(batsGr->grpUser)); */
      /* memset(batsGr->grpPasswd,'\0',sizeof(batsGr->grpPasswd)); */
      memset(batsGr->grpUser,  ' ',sizeof(batsGr->grpUser));
      memset(batsGr->grpPasswd,' ',sizeof(batsGr->grpPasswd));
      memcpy(batsGr->grpUser,  v[0],strlen(v[0]));
      memcpy(batsGr->grpPasswd,v[1],strlen(v[1]));

      /* EKA_DEBUG ("%s:%u: %s set to |%s|:|%s|", */
      /* 		 k[1],gr,k[5],batsGr->grpUser + '\0',batsGr->grpPasswd + '\0'); */

      batsGr->grpSet = true;
      fflush(stderr);

      return EkaFhAddConf::CONF_SUCCESS;
    }
  }  
  //---------------------------------------------------------------------
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

  //---------------------------------------------------------------------
  // efh.C1_PITCH.group.X.unit, "1"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"unit")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL",gr);

      ((EkaFhBatsGr*)b_gr[gr])->batsUnit = (uint8_t) strtoul(v[0],NULL,10);
      //      EKA_DEBUG ("batsUnit for %s:%u is set to %u",k[1],gr,((EkaFhBatsGr*)b_gr[gr])->batsUnit);
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
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL",gr);

      memcpy(&(((EkaFhBatsGr*)b_gr[gr])->sessionSubID),v[0],sizeof(((EkaFhBatsGr*)b_gr[gr])->sessionSubID));

      //      EKA_DEBUG ("%s:%u: sessionSubID set to %s",k[1],gr,v[0] +'\0');
      fflush(stderr);

      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.C1_PITCH.group.X.recovery.grpSessionSubID, "0285"
  // k[0] k[1]     k[2] k[3] k[4]   k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"recovery")==0) && (strcmp(k[5],"grpSessionSubID")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      auto batsGr {reinterpret_cast<EkaFhBatsGr*>(b_gr[gr])};
      if (batsGr == NULL) on_error("b_gr[%u] == NULL",gr);

      if (strlen(v[0]) != sizeof(batsGr->grpSessionSubID))
	on_error("parameter size mismatch: strlen(%s) %d != sizeof(batsGr->grpSessionSubID) %d",
		 v[0], (int)strlen(v[0]), (int)sizeof(batsGr->grpSessionSubID));
      memcpy(batsGr->grpSessionSubID,v[0],sizeof(batsGr->grpSessionSubID));

      //      EKA_DEBUG ("%s:%u: grpSessionSubID set to %s",k[1],gr,v[0] +'\0');
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
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL, groups = %u",gr,groups);
      inet_aton (v[0],(struct in_addr*) &b_gr[gr]->mcast_ip);
      b_gr[gr]->mcast_port =  (uint16_t)atoi(v[1]);
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
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL",gr);

      inet_aton (v[0],(struct in_addr*) &b_gr[gr]->snapshot_ip);
      b_gr[gr]->snapshot_port =  htons((uint16_t)atoi(v[1]));
      b_gr[gr]->snapshot_set = true;
      //      EKA_DEBUG ("%s %s for %s:%u is set to %s:%u",k[4],k[5],k[1],gr,v[0],(uint16_t)atoi(v[1]));
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  // efh.C1_PITCH.group.X.recovery.grpAddr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if ((strcmp(k[0],"efh")==0) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"recovery")==0) && (strcmp(k[5],"grpAddr")==0)) {
    if (EFH_GET_SRC(k[1]) == exch) {
      uint8_t gr = (uint8_t) atoi(k[3]);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      auto batsGr {reinterpret_cast<EkaFhBatsGr*>(b_gr[gr])};

      if (batsGr == NULL) on_error("b_gr[%u] == NULL",gr);
      batsGr->grpIp = inet_addr(v[0]);
      batsGr->grpPort = be16toh((uint16_t)atoi(v[1]));

      /* EKA_DEBUG ("%s:%u: %s is set to %s:%u", */
      /* 		 k[1],gr,k[5],EKA_IP2STR(batsGr->grpIp),be16toh(batsGr->grpPort)); */
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
	on_warning("%s -- %s : Ignoring group_id %d > groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("b_gr[%u] == NULL",gr);

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
      //      if (gr >= groups) on_error("%s -- %s : group_id %d >= groups (=%u)",key, value,gr,groups);
      if (gr >= groups) {
	on_warning("%s -- %s : Ignoring group_id %d >= groups (=%u)",key, value,gr,groups);
	return EkaFhAddConf::CONF_SUCCESS;
      }
      if (b_gr[gr] == NULL) on_error("Group %u does not exist",gr);
      EkaFhBoxGr* boxGr = dynamic_cast<EkaFhBoxGr*>(b_gr[gr]);
      if (boxGr == NULL) on_error("boxGr (b_gr[%u]) == NULL",gr);

      memset(&boxGr->line,' ',sizeof(boxGr->line));
      memcpy(&boxGr->line,v[0],std::min(sizeof(boxGr->line),sizeof(v[0])));
      //      EKA_DEBUG ("%s:%u line = %s", EKA_EXCH_SOURCE_DECODE(exch),gr,v[0]);
      fflush(stderr);
      return EkaFhAddConf::CONF_SUCCESS;
    }
  }
  //---------------------------------------------------------------------
  return EkaFhAddConf::UNKNOWN_KEY;
}
 /* ##################################################################### */

EkaOpResult EkaFh::initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhRunGroup* runGr) {
  for (uint8_t i = 0; i < runGr->numGr; i++) {
    if (! runGr->isMyGr(pEfhRunCtx->groups[i].localId)) 
      on_error("pEfhRunCtx->groups[%d].localId = %u doesnt belong to %s",
	       i,pEfhRunCtx->groups[i].localId,runGr->list2print);
    if (pEfhRunCtx->groups[i].source != exch) 
      on_error("id=%u,fhId = %u, pEfhRunCtx->groups[i].source %s != exch %s",
	       id,pEfhCtx->fhId,
	       EKA_EXCH_SOURCE_DECODE(pEfhRunCtx->groups[i].source),
	       EKA_EXCH_SOURCE_DECODE(exch));

    EkaFhGroup* gr = b_gr[pEfhRunCtx->groups[i].localId];
    if (gr == NULL) on_error ("b_gr[%u] == NULL",pEfhRunCtx->groups[i].localId);

    //    b_gr[i]->bookInit();
    gr->createQ(pEfhCtx,qsize);
    gr->expected_sequence = 1;

    runGr->igmpMcJoin(gr->mcast_ip,gr->mcast_port,0,&gr->pktCnt);
    EKA_DEBUG("%s:%u: joined %s:%u for %u securities",
	      EKA_EXCH_DECODE(exch),gr->id,
	      EKA_IP2STR(gr->mcast_ip),gr->mcast_port,
	      gr->getNumSecurities());
  }
  return EKA_OPRESULT__OK;
}


 /* ##################################################################### */
