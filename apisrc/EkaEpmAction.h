#ifndef _EKA_EPM_ACTION_H_
#define _EKA_EPM_ACTION_H_

#include "EkaHwInternalStructs.h"
#include "EkaCore.h"
#include "EkaEpm.h"
#include "EpmStrategy.h"

class EkaEpmAction {
 public:
  EkaEpmAction(EkaDev*                 _dev,
	       char*                   _actionName,
	       EkaEpm::ActionType      _type,
	       uint                    _idx,
	       uint8_t                 _coreId,
	       uint8_t                 _sessId,
	       uint                    _productIdx,
	       epm_actione_bitparams_t _actionBitParams,
	       uint64_t		       _heapAddr,
	       uint64_t		       _actionAddr,
	       uint64_t		       _dataTemplateAddr,
	       uint		       _templateId);

  /* ----------------------------------------------------- */

  int updateAttrs (uint8_t            _coreId,
		   uint8_t            _sessId,
		   uint               _payloadOffs,
		   epm_actionid_t     _nextAction,
		   const uint64_t     _mask_post_strat,
		   const uint64_t     _mask_post_local,
		   const uint64_t     _user,
		   const epm_token_t  _token);

  /* ----------------------------------------------------- */
  void print();
  /* ----------------------------------------------------- */
  int setNwHdrs(uint8_t* macDa, 
		uint8_t* macSa, 
		uint32_t srcIp, 
		uint32_t dstIp, 
		uint16_t srcPort, 
		uint16_t dstPort);
  /* ----------------------------------------------------- */
  int setFullPkt(uint thrId, void* buf, uint len);
  /* ----------------------------------------------------- */
  int setPktPayload(uint thrId, void* buf, uint len);
  /* ----------------------------------------------------- */
  int send();
  /* ----------------------------------------------------- */

  char actionName[30] = {};

  EkaDev*  dev;
  EkaEpm*  epm;
  EkaEpm::ActionType type = EkaEpm::ActionType::INVALID;
  uint          idx       = -1;
  uint8_t    coreId       = -1;;
  uint8_t    sessId       = -1;
  uint   productIdx       = -1;
  uint64_t heapAddr       = -1;
  uint     heapOffs       = -1;
  uint64_t actionAddr     = -1;
  uint64_t templateAddr   = -1;
  uint     templateId     = -1;

  epm_actione_bitparams_t actionBitParams = {};

  epm_actionid_t nextIdx  = EPM_LAST_ACTION;
  epm_action_t hwAction   = {};

  epm_strategyid_t strategyId      = -1;
  uint             payloadLen      = 0;
  uint             pktSize         = 0;
  uint32_t         tcpCSum         = 0;

  EkaEthHdr*       ethHdr          = NULL;
  EkaIpHdr*        ipHdr           = NULL;
  EkaTcpHdr*       tcpHdr          = NULL;
  uint8_t*         payload         = NULL;

  uint8_t          region          = 0;
};


#endif
