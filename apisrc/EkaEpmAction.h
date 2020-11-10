/* SHURIK */

#ifndef _EKA_EPM_ACTION_H_
#define _EKA_EPM_ACTION_H_

#include "EkaEpm.h"

class EkaEpmAction {
 public:
  EkaEpmAction(EkaDev*                 _dev,
	       char*                   _actionName,
	       EkaEpm::ActionType      _type,
	       uint                    _idx,
	       uint                    _localIdx,
	       uint                    _region,
	       uint8_t                 _coreId,
	       uint8_t                 _sessId,
	       uint                    _productIdx,
	       EpmActionBitmap         _actionBitParams,
	       uint64_t		       _heapAddr,
	       uint64_t		       _actionAddr,
	       uint64_t		       _dataTemplateAddr,
	       uint		       _templateId);

  /* ----------------------------------------------------- */
  int updateAttrs (uint8_t _coreId, uint8_t _sessId, const EpmAction *epmAction);
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
  int send(uint32_t _tcpCSum);
  /* ----------------------------------------------------- */
  int send();
  /* ----------------------------------------------------- */

  char actionName[30] = {};

  EkaDev*  dev;
  EkaEpm*  epm;
  EkaEpm::ActionType type = EkaEpm::ActionType::INVALID;
  uint          idx       = -1;
  uint     localIdx       = -1;
  uint       region       = -1;
  uint8_t    coreId       = -1;;
  uint8_t    sessId       = -1;
  uint   productIdx       = -1;
  uint64_t heapAddr       = -1;
  uint     heapOffs       = -1;
  uint64_t actionAddr     = -1;
  uint64_t templateAddr   = -1;
  uint     templateId     = -1;

  EpmAction epmActionLocalCopy = {};

  EpmActionBitmap actionBitParams = {};

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
};


#endif
