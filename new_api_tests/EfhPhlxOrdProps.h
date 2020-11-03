#ifndef _EFH_PHLX_ORD__PROPS_H
#define _EFH_PHLX_ORD__PROPS_H

EkaProp efhPhlxOrdInitCtxEntries_A[] = {
	{"efh.PHLX_ORD.group.0.mcast.addr"   ,"233.47.179.141:19101"},
	{"efh.PHLX_ORD.group.0.snapshot.addr","159.79.3.138:19201"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.0.recovery.addr","159.79.3.139:19201"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.0.snapshot.auth","XXXX:YYYY"},

	{"efh.PHLX_ORD.group.1.mcast.addr"   ,"233.47.179.142:19102"},
	{"efh.PHLX_ORD.group.1.snapshot.addr","159.79.3.140:19202"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.1.recovery.addr","159.79.3.141:19202"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.1.snapshot.auth","XXXX:YYYY"},

	{"efh.PHLX_ORD.group.2.mcast.addr"   ,"233.47.179.143:19103"},
	{"efh.PHLX_ORD.group.2.snapshot.addr","159.79.3.142:19203"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.2.recovery.addr","159.79.3.143:19203"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.2.snapshot.auth","XXXX:YYYY"},

	{"efh.PHLX_ORD.group.3.mcast.addr"   ,"233.47.179.140:19104"},
	{"efh.PHLX_ORD.group.3.snapshot.addr","159.79.3.146:19204"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.3.recovery.addr","159.79.3.147:19204"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.3.snapshot.auth","XXXX:YYYY"},

};

EkaProp efhPhlxOrdInitCtxEntries_B[] = {
	{"efh.PHLX_ORD.group.0.mcast.addr"   ,"233.47.179.157:19101"},
	{"efh.PHLX_ORD.group.0.snapshot.addr","159.79.3.202:19201"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.0.recovery.addr","159.79.3.203:19201"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.0.snapshot.auth","XXXX:YYYY"},

	{"efh.PHLX_ORD.group.1.mcast.addr"   ,"233.47.179.158:19102"},
	{"efh.PHLX_ORD.group.1.snapshot.addr","159.79.3.204:19202"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.1.recovery.addr","159.79.3.205:19202"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.1.snapshot.auth","XXXX:YYYY"},

	{"efh.PHLX_ORD.group.2.mcast.addr"   ,"233.47.179.159:19103"},
	{"efh.PHLX_ORD.group.2.snapshot.addr","159.79.3.206:19203"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.2.recovery.addr","159.79.3.207:19203"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.2.snapshot.auth","XXXX:YYYY"},

	{"efh.PHLX_ORD.group.3.mcast.addr"   ,"233.47.179.156:19104"},
	{"efh.PHLX_ORD.group.3.snapshot.addr","159.79.3.210:19204"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.3.recovery.addr","159.79.3.211:19204"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.3.snapshot.auth","XXXX:YYYY"},

};

const EkaGroup phlxOrdGroups[] = {
	{EkaSource::kPHLX_ORD, (EkaLSI)0},
	{EkaSource::kPHLX_ORD, (EkaLSI)1},
	{EkaSource::kPHLX_ORD, (EkaLSI)2},
	{EkaSource::kPHLX_ORD, (EkaLSI)3},
};

#endif
