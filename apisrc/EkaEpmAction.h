#ifndef _EKA_EPM_ACTION_H_
#define _EKA_EPM_ACTION_H_

#include "EkaHwInternalStructs.h"
#include "EkaCore.h"
#include "EkaEpm.h"

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
	       uint		       _templateId) {

    dev             = _dev;
    strcpy(actionName,_actionName);
    epm             =  dev->epm;
    type            = _type;
    idx             = _idx;
    coreId          = _coreId;
    sessId          = _sessId;
    productIdx      = _productIdx;
    actionBitParams = _actionBitParams;
    heapAddr        = _heapAddr;
    actionAddr      = _actionAddr;
    templateAddr    = _dataTemplateAddr;
    templateId      = _templateId;

    nextIdx         = 0xffff; //last 

    hwAction.tcpCsSizeSource      = type == EkaEpm::ActionType::UserAction ? TcpCsSizeSource::FROM_ACTION : TcpCsSizeSource::FROM_DESCR;

    hwAction.data_db_ptr           = heapAddr;
    hwAction.template_db_ptr       = templateAddr;
    hwAction.tcpcs_template_db_ptr = templateId;
    hwAction.bit_params.israw      = actionBitParams.israw;
    hwAction.bit_params.report_en  = actionBitParams.report_en;
    hwAction.bit_params.feedbck_en = actionBitParams.feedbck_en;
    hwAction.target_prod_id        = productIdx;
    hwAction.target_core_id        = coreId;
    hwAction.target_session_id     = sessId;
    hwAction.next_action_index     = nextIdx; 
  }

  /* ----------------------------------------------------- */

  int update (uint8_t                 _coreId,
	      uint8_t                 _sessId,
	      epm_actionid_t          _nextAction,
	      uint64_t		      _heapAddr,
	      const uint64_t 	      _mask_post_strat,
	      const uint64_t 	      _mask_post_local,
	      const uint64_t  	      _user,
	      const epm_token_t	      _token,
	      uint16_t 		      _payloadSize,
	      uint32_t 		      _tcpCSum
	      ) {
    coreId          = _coreId;
    sessId          = _sessId;
    heapAddr        = _heapAddr;
    nextIdx         = _nextAction;

    hwAction.target_core_id        = coreId;
    hwAction.target_session_id     = sessId;
    hwAction.data_db_ptr           = heapAddr;
    hwAction.next_action_index     = nextIdx;
    hwAction.mask_post_strat       = _mask_post_strat;
    hwAction.mask_post_local       = _mask_post_local;
    hwAction.user                  = _user;
    hwAction.token                 = _token;
    hwAction.payloadSize           = _payloadSize;
    hwAction.tcpCSum               = _tcpCSum;

    return 0;
  }
  /* ----------------------------------------------------- */

  char actionName[30] = {};

  EkaDev*  dev;
  EkaEpm*  epm;
  EkaEpm::ActionType type = EkaEpm::ActionType::INVALID;
  uint          idx       = -1;
  uint8_t    coreId       = -1;;
  uint8_t    sessId       = -1;
  uint   productIdx       = -1;
  epm_actione_bitparams_t actionBitParams;
  uint64_t heapAddr                = -1;
  uint64_t actionAddr              = -1;
  uint64_t templateAddr            = -1;
  uint     templateId              = -1;

  epm_actionid_t    nextIdx        = 0xffff; //last 
  epm_action_t hwAction = {};


  void print() {
    EKA_LOG("%s: coreId = %u, sessId = %u, idx=%u, data_db_ptr=0x%jx, template_db_ptr=0x%jx, tcpcs_template_db_ptr=%u, coreId=%u, sessId=%u, heapAddr=0x%jx, payloadSize=%u ",
	    actionName,
	    coreId,
	    sessId,
	    idx,
	    hwAction.data_db_ptr,
	    hwAction.template_db_ptr,
	    hwAction.tcpcs_template_db_ptr,
	    hwAction.target_core_id,
	    hwAction.target_session_id,
	    heapAddr,
	    hwAction.payloadSize
	    );
  }

};


#endif
