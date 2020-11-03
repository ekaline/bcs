
#ifndef _EKA_FH_H
#define _EKA_FH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string>
#include <regex>

#include "eka_hw_conf.h"
#include "eka_fh_book.h"
#include "Efh.h"
#include "eka_fh_group.h"
#include "EkaDev.h"


enum class EkaFhAddConf {CONF_SUCCESS=0, IGNORED=1, UNKNOWN_KEY=2, WRONG_VALUE=3, CONFLICTING_CONF=4} ;
/* ##################################################################### */

class EkaFh {
 public:
  //  class EkaFh();
  //  class ~EkaFh() {};
  EkaFh();
  virtual ~EkaFh();

  int stop();

  int init(const EfhInitCtx* pEfhInitCtx, uint8_t numFh);
  int setId(EfhCtx* pEfhCtx, EkaSource exch, uint8_t numFh);

  int openGroups(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);
  EkaOpResult initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhRunGr* runGr);

  uint8_t getGrId(const uint8_t* pkt);

  inline bool anyInGap();

  enum class GapType { SNAPSHOT=0, RETRANSMIT=1 };

  virtual EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {return EKA_OPRESULT__OK;};

  virtual EkaOpResult getDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {return EKA_OPRESULT__OK;};
  

  void send_igmp(bool join_leave, volatile bool igmp_thread_active);
  EkaFhAddConf conf_parse(const char *key, const char *value);
  EkaOpResult subscribeStaticSecurity(uint8_t groupNum, uint64_t securityId, EfhSecurityType efhSecurityType,EfhSecUserData efhSecUserData,uint64_t opaqueAttrA,uint64_t opaqueAttrB);
  FhGroup* nextGrToProcess(uint first, uint numGroups);
  //-----------------------------------------------------------------------------

  EfhFeedVer            feed_ver;
  EkaSource             exch;

  uint                  qsize;

  volatile uint8_t      groups;
  bool                  full_book;
  bool                  subscribe_all;
  bool                  print_parsed_messages;

  /* char                  auth_user[6];     // Glimpse */
  /* char                  auth_passwd[10];  // Glimpse */
  char                  auth_user[10];
  char                  auth_passwd[12];
  bool                  auth_set;

  EkaCoreId             c;
  uint8_t               id;

  uint8_t               lastServed; // Q for RR processing
  bool                  any_group_getting_snapshot;

  EkaDev*               dev;

  FhGroup*              b_gr[EKA_FH_GROUPS];
 private:
};

//###############################################################

class FhMiax : public EkaFh { // MiaxTom & PearlTom
 public:
  static const uint QSIZE = 1024 * 1024;
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  virtual ~FhMiax() {};
  uint8_t* getUdpPkt(FhRunGr* runGr, uint* msgInPkt, int16_t* pktLen, uint64_t* sequence,uint8_t* gr_id);
  bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhMiaxGr* gr, const uint8_t* pkt, int16_t pktLen);
  void pushUdpPkt2Q(FhMiaxGr* gr, const uint8_t* pkt, int16_t pktLen,int8_t gr_id);
 private:

};
//###############################################################

class FhNasdaq : public EkaFh { // base class for Nom, Gem, Ise, Phlx
  static const uint QSIZE = 1024 * 1024;
 public:
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );

  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  virtual ~FhNasdaq() {};
  virtual uint8_t* getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id);
  bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence);
  void pushUdpPkt2Q(FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence,int8_t gr_id);
 private:

};
//###############################################################

class FhNom : public FhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
 public:
  //  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  virtual ~FhNom() {};
 private:

};

//###############################################################

class FhGem : public FhNasdaq{ // Gem & Ise
  static const uint QSIZE = 1 * 1024 * 1024;
 public:
  //  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  virtual ~FhGem() {};
 private:
};
//###############################################################

class FhPhlx : public FhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
  //  static const uint QSIZE = 1024 * 1024;
public:
  //  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  virtual ~FhPhlx() {};
private:

};
//###############################################################

class FhPhlxOrd : public FhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
  //  static const uint QSIZE = 1024 * 1024;
public:
  //  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  uint8_t* getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id);
  virtual ~FhPhlxOrd() {};
private:

};
//###############################################################

class FhBats : public EkaFh {
   static const uint QSIZE = 1024 * 1024;
public:
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  uint8_t     getGrId(const uint8_t* pkt);
  bool        processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhBatsGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence);
  void        pushUdpPkt2Q(FhBatsGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequenceu,int8_t gr_id);
  uint8_t*    getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id);

  virtual ~FhBats() {};
private:

};
//###############################################################

class FhXdp : public EkaFh {
   static const uint QSIZE = 1 * 1024 * 1024;

public:
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  EkaOpResult initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhRunGr* runGr);
  uint8_t*    getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint* pktSize, uint64_t* sequence, uint8_t* gr_id, uint16_t* streamId, uint8_t* pktType);
  bool        processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhXdpGr* gr,  uint pktSize, uint streamIdx, const uint8_t* pktPtr, uint msgInPkt, uint64_t seq);

  virtual ~FhXdp() {};
private:

};
//###############################################################

class FhBox : public EkaFh { // MiaxTom & PearlTom
 public:
  static const uint QSIZE = 1024 * 1024;
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  EkaOpResult initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhRunGr* runGr);

  uint8_t* getUdpPkt(FhRunGr* runGr, uint16_t* pktLen, uint64_t* sequence, uint8_t* gr_id);

  bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhBoxGr* gr, const uint8_t* pkt, int16_t pktLen);
  void pushUdpPkt2Q(FhBoxGr* gr, const uint8_t* pkt, int16_t pktLen,int8_t gr_id);

  virtual ~FhBox() {};
 private:

};

//###############################################################

class EkaFhThreadAttr {
 public:
  EkaFhThreadAttr(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint runGrId,FhGroup* gr, uint64_t startSeq, uint64_t endSeq, EkaFhMode   op);

  EfhCtx*     pEfhCtx;
  EfhRunCtx*  pEfhRunCtx;
  uint        runGrId;
  FhGroup*    gr;
  uint64_t    startSeq;
  uint64_t    endSeq;
  EkaFhMode   op;
};

#endif
