#ifndef _EKA_EPM_REGION_H_
#define _EKA_EPM_REGION_H_

#include "EkaEpm.h"
#include "EkaDev.h"

class EkaEpmRegion {
public:
	struct RegionConfig {
		const char* name;
		size_t nActions;
		size_t actionHeapBudget;
	};

	enum Regions : int {
		Efc = 0,
		TcpTxFullPkt = 5,
		TcpTxEmptyAck = 6,
		EfcMc = 7,
		Total = 32
	};
	
  static const size_t HeapPerRegularAction = EkaDev::MAX_ETH_FRAME_SIZE;
  static const size_t HeapPerSmallAction = 64; // IGMP, TCP ACK
  static const size_t HeapPerAverageTcpAction =
		(HeapPerRegularAction + HeapPerSmallAction) / 2; // = 800
  static const size_t NumEfcActions = 2048;
	static const size_t NumNewsActions = 256;
	static const size_t NumIgmpActions = 2 * 64;
	static const size_t NumEfcIgmpActions = 256; // jsust a num
	
	static const size_t NumTcpActions = EKA_MAX_CORES *
		EKA_MAX_TCP_SESSIONS_PER_CORE *	2; // 4 * 32 * 2 = 256
	
	constexpr static RegionConfig region[Regions::Total] = {
		[Regions::Efc]   = {"EFC",  NumEfcActions,
												HeapPerRegularAction},
		[1]  = {"News",  NumNewsActions, HeapPerRegularAction},
		[2]  = {"News",  NumNewsActions, HeapPerRegularAction},
		[3]  = {"News",  NumNewsActions, HeapPerRegularAction},
		[4]  = {"News",  NumNewsActions, HeapPerRegularAction},
		[Regions::TcpTxFullPkt] = {"TcpTxFullPkt", NumTcpActions,
															 HeapPerRegularAction},
		[Regions::TcpTxEmptyAck] = {"TcpTxEmptyAck", NumTcpActions,
															 HeapPerSmallAction},
		[Regions::EfcMc] = {"EfcMc", NumEfcIgmpActions,
												HeapPerSmallAction},
		[8]  = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[9]  = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[10] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[11] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[12] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[13] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[14] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[15] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[16] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[17] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[18] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[19] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[20] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[21] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[22] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[23] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[24] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[25] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[26] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[27] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[28] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[29] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[30] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction},
		[31] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction}
	};

	constexpr static void printConfig(void* buf, size_t bufLen) {
		auto c = (char*)buf;
		for (auto i = 0; i < Regions::Total; i++) {
			c += sprintf(c,"%2d %-14s: %4jd Actions: %4d..%4jd, "
									 "%7jdB Heap: %7d..%7jd\n",
									 i,region[i].name,
									 region[i].nActions,
									 getBaseActionIdx(i),
									 getBaseActionIdx(i) + region[i].nActions - 1,
									 region[i].nActions * region[i].actionHeapBudget,
									 getBaseHeapOffs(i),
									 getBaseHeapOffs(i)  + region[i].nActions *
									 region[i].actionHeapBudget - 1
									 );
			if (c - (char*)buf > (int) bufLen)
				on_error("string len %jd > %d",
								 c - (char*)buf, (int)bufLen);
									 
		}
	}
	
	constexpr static void sanityCheckRegionId(int regionId) {
		if (regionId < 0 || regionId >= Regions::Total)
			on_error("Bad regionId %d",regionId);
	}

	constexpr static void sanityCheckActionId(int regionId,
																						int actionId) {
		if (actionId < 0 ||
				actionId >= getBaseActionIdx(regionId) + getMaxActions(regionId))
			on_error("Bad actionId %d at region %d: "
							 "getBaseActionIdx + getMaxActions = %d",
							 actionId,regionId,
							 getBaseActionIdx(regionId) + getMaxActions(regionId));
	}
	
	constexpr static int getBaseActionIdx(int regionId) {
		sanityCheckRegionId(regionId);
		int baseIdx = 0;
		for (auto i = 0; i < regionId; i++) {
			baseIdx += region[i].nActions;
		}
		return baseIdx;
	}

	constexpr static int getMaxActions(int regionId) {
		sanityCheckRegionId(regionId);
		return region[regionId].nActions;
	}
	
	constexpr static int getBaseHeapOffs(int regionId) {
		sanityCheckRegionId(regionId);
		int baseHeapOffs = 0;
		for (auto i = 0; i < regionId; i++) {
			baseHeapOffs += region[i].nActions * region[i].actionHeapBudget;
		}
		return baseHeapOffs;
	}

		
	constexpr static uint getHeapSize(int regionId) {
		sanityCheckRegionId(regionId);

		return region[regionId].nActions * region[regionId].actionHeapBudget;
	}
	
	constexpr static int getActionHeapOffs(int regionId, int actionId) {
		sanityCheckRegionId(regionId);
		sanityCheckActionId(regionId,actionId);
		
		return getBaseHeapOffs(regionId) +
			actionId * region[regionId].actionHeapBudget;
	}
	
	constexpr static int getEfhIgmpRegion(int udpChId) {
		auto Reserved = Regions::EfcMc;
		if (udpChId >= Regions::Total - Reserved)
			on_error("udpChId %d exceeds MaxUdpChannelRegions %d",
							 udpChId,Regions::Total - Reserved);

		return udpChId + Reserved;
	}

	// ######################################################
  EkaEpmRegion(EkaDev* _dev, int _id) {
    dev            = _dev;
    id             = _id;

    baseActionIdx  = getBaseActionIdx(id);
    localActionIdx = 0;

    baseHeapOffs   = getBaseHeapOffs(id);
		//    heapOffs       = baseHeapOffs;

    // writing region's baseActionIdx to FPGA
    eka_write(dev,0x82000 + 8 * id, baseActionIdx);

    EKA_LOG("Created EpmRegion %s %u: "
						"baseActionIdx=%u, baseHeapOffs=%x",
						region[id].name,id,
						baseActionIdx,baseHeapOffs);
  }


  EkaDev*   dev            = NULL;
  int       id             = -1;
  
  epm_actionid_t baseActionIdx  = -1;
  epm_actionid_t localActionIdx = 0;

  uint      baseHeapOffs   = 0;
	//  uint      heapOffs       = 0;

private:
		
};

#endif
