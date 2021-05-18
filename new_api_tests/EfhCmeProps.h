#ifndef _EFH_CME_PROPS_H
#define _EFH_CME_PROPS_H

/*
OPTIONS
CmeDerivatives.CMEFFTOP2.processLogicFeedType = LISTED_OPTION
CmeDerivatives.CMEFFTOP2.incrementFeed.0.host.primary = 224.0.31.2
CmeDerivatives.CMEFFTOP2.incrementFeed.0.port.primary = 14311
CmeDerivatives.CMEFFTOP2.incrementFeed.0.host.secondary = 224.0.32.2
CmeDerivatives.CMEFFTOP2.incrementFeed.0.port.secondary = 15311
CmeDerivatives.CMEFFTOP2.definitionFeed.0.host.primary = 224.0.31.44
CmeDerivatives.CMEFFTOP2.definitionFeed.0.port.primary = 14311
CmeDerivatives.CMEFFTOP2.definitionFeed.0.host.secondary = 224.0.32.44
CmeDerivatives.CMEFFTOP2.definitionFeed.0.port.secondary = 15311
CmeDerivatives.CMEFFTOP2.snapshotFeed.0.host.primary = 224.0.31.23
CmeDerivatives.CMEFFTOP2.snapshotFeed.0.port.primary = 14311
CmeDerivatives.CMEFFTOP2.snapshotFeed.0.host.secondary = 224.0.32.23
CmeDerivatives.CMEFFTOP2.snapshotFeed.0.port.secondary = 15311
CmeDerivatives.CMEFFTOP2.tcpRrecovery.0.host.primary = 205.209.220.72
CmeDerivatives.CMEFFTOP2.tcpRrecovery.0.port.primary = 10000
CmeDerivatives.CMEFFTOP2.tcpRrecovery.0.host.secondary = 205.209.222.72
CmeDerivatives.CMEFFTOP2.tcpRrecovery.0.port.secondary = 10000

FUTURES
CmeDerivatives.CMEFFTOP3.processLogicFeedType = FUTURE
CmeDerivatives.CMEFFTOP3.incrementFeed.0.host.primary = 224.0.31.1
CmeDerivatives.CMEFFTOP3.incrementFeed.0.port.primary = 14310
CmeDerivatives.CMEFFTOP3.incrementFeed.0.host.secondary = 224.0.32.1
CmeDerivatives.CMEFFTOP3.incrementFeed.0.port.secondary = 15310
CmeDerivatives.CMEFFTOP3.definitionFeed.0.host.primary = 224.0.31.43
CmeDerivatives.CMEFFTOP3.definitionFeed.0.port.primary = 14310
CmeDerivatives.CMEFFTOP3.definitionFeed.0.host.secondary = 224.0.32.43
CmeDerivatives.CMEFFTOP3.definitionFeed.0.port.secondary = 15310
CmeDerivatives.CMEFFTOP3.snapshotFeed.0.host.primary = 224.0.31.22
CmeDerivatives.CMEFFTOP3.snapshotFeed.0.port.primary = 14310
CmeDerivatives.CMEFFTOP3.snapshotFeed.0.host.secondary =224.0.32.22
CmeDerivatives.CMEFFTOP3.snapshotFeed.0.port.secondary = 15310
CmeDerivatives.CMEFFTOP3.tcpRrecovery.0.host.primary = 205.209.220.72
CmeDerivatives.CMEFFTOP3.tcpRrecovery.0.port.primary = 10000
CmeDerivatives.CMEFFTOP3.tcpRrecovery.0.host.secondary = 205.209.222.72
CmeDerivatives.CMEFFTOP3.tcpRrecovery.0.port.secondary = 10000
*/

EkaProp efhCmeInitCtxEntries_A[] = {
  // Options
  {"efh.CME_SBE.group.0.mcast.addr"   ,"224.0.31.2:14311"},     // incrementFeed
  {"efh.CME_SBE.group.0.snapshot.addr","224.0.31.44:14311"}, 	// definitionFeed
  {"efh.CME_SBE.group.0.recovery.addr","224.0.31.23:14311"}, 	// snapshotFeed

  //Futures
  {"efh.CME_SBE.group.1.mcast.addr"   ,"224.0.31.1:14310"},     // incrementFeed
  {"efh.CME_SBE.group.1.snapshot.addr","224.0.31.43:14310"}, 	// definitionFeed 
  {"efh.CME_SBE.group.1.recovery.addr","224.0.31.22:14310"}, 	// snapshotFeed 
};

EkaProp efhCmeInitCtxEntries_B[] = {
  // Options
  {"efh.CME_SBE.group.0.mcast.addr"   ,"224.0.32.2:15311"},     // incrementFeed
  {"efh.CME_SBE.group.0.snapshot.addr","224.0.32.44:15311"}, 	// definitionFeed
  {"efh.CME_SBE.group.0.recovery.addr","224.0.32.23:15311"}, 	// snapshotFeed

  //Futures
  {"efh.CME_SBE.group.1.mcast.addr"   ,"224.0.32.1:15310"},     // incrementFeed
  {"efh.CME_SBE.group.1.snapshot.addr","224.0.32.43:15310"}, 	// definitionFeed 
  {"efh.CME_SBE.group.1.recovery.addr","224.0.32.22:15310"}, 	// snapshotFeed 
};

const EkaGroup cmeGroups[] = {
	{EkaSource::kCME_SBE, (EkaLSI)0},
	{EkaSource::kCME_SBE, (EkaLSI)1},
};

#endif
