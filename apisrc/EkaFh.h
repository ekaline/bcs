#ifndef _EKA_FH_H_
#define _EKA_FH_H_

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
#include "Efh.h"
#include "EkaDev.h"

#define EFH_PRICE_SCALE 1

enum class EkaFhAddConf {CONF_SUCCESS=0, IGNORED=1, UNKNOWN_KEY=2, WRONG_VALUE=3, CONFLICTING_CONF=4} ;

class EkaFhGroup;

/* ##################################################################### */

class EkaFh {
 protected:
  EkaFh();

 public:
  virtual     ~EkaFh();
  virtual     EkaFhGroup* addGroup() = 0;

  int                     stop();

  int                     init(const EfhInitCtx* pEfhInitCtx, uint8_t numFh);
  int                     setId(EfhCtx* pEfhCtx, EkaSource exch, uint8_t numFh);

  int                     openGroups(EfhCtx*           pEfhCtx, 
				     const EfhInitCtx* pEfhInitCtx);

  virtual EkaOpResult     runGroups(EfhCtx*          pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint8_t          runGrId) = 0;

  virtual EkaOpResult     getDefinitions(EfhCtx*          pEfhCtx, 
					 const EfhRunCtx* pEfhRunCtx, 
					 EkaGroup*        group) = 0;
  
  void                send_igmp(bool join_leave, volatile bool igmp_thread_active);
  EkaFhAddConf        conf_parse(const char *key, const char *value);
  EkaOpResult         subscribeStaticSecurity(
					      uint8_t groupNum, 
					      uint64_t securityId, 
					      EfhSecurityType efhSecurityType,
					      EfhSecUserData efhSecUserData,
					      uint64_t opaqueAttrA,
					      uint64_t opaqueAttrB);

  EkaFhGroup*         nextGrToProcess(uint first, uint numGroups);
  //-----------------------------------------------------------------------------

 private:
  EkaOpResult         initGroups(EfhCtx*           pEfhCtx, 
				 const EfhRunCtx*  pEfhRunCtx, 
				 EkaFhRunGroup*    runGr);

  virtual uint8_t     getGrId(const uint8_t* pkt);
  
  virtual uint8_t*    getUdpPkt(EkaFhRunGroup* runGr, 
				uint*          msgInPkt, 
				uint64_t*      sequence,
				uint8_t*       gr_id) = 0;
  
  virtual bool        processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				    EkaFhGroup*      gr, 
				    const uint8_t*   pkt, 
				    uint             msgInPkt, 
				    uint64_t         sequence) = 0;

  //-----------------------------------------------------------------------------

 public:
  enum class            GapType { SNAPSHOT=0, RETRANSMIT=1 };

  EfhFeedVer            feed_ver              = EfhFeedVer::kInvalid;
  EkaSource             exch                  = EkaSource::kInvalid;

  uint                  qsize                 = 8 * 1024 * 1024;

  volatile uint8_t      groups                = 0;
  bool                  full_book             = false;
  bool                  subscribe_all         = false;
  bool                  print_parsed_messages = false;

  char                  auth_user[10]         = {};
  char                  auth_passwd[12]       = {};
  bool                  auth_set              = false;

  EkaCoreId             c                     = -1;
  uint8_t               id                    = -1;

  uint8_t               lastServed            = 0; // Q for RR processing
  bool                  any_group_getting_snapshot = false;

  EkaFhGroup*           b_gr[EKA_FH_GROUPS]   = {};
 private:
  EkaDev*               dev                   = NULL;

};

#endif
