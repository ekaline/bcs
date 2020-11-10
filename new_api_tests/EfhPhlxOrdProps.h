#ifndef _EFH_PHLX_ORD__PROPS_H
#define _EFH_PHLX_ORD__PROPS_H

EkaProp efhPhlxOrdInitCtxEntries_A[] = {
	{"efh.PHLX_ORD.group.0.mcast.addr"   ,"233.47.179.120:18064"},
	{"efh.PHLX_ORD.group.0.snapshot.addr","206.200.151.34:18164"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.0.recovery.addr","206.200.151.34:18164"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.0.snapshot.auth","STRK01:6EUG2W"},

	{"efh.PHLX_ORD.group.1.mcast.addr"   ,"233.47.179.121:18065"},
	{"efh.PHLX_ORD.group.1.snapshot.addr","206.200.151.35:18165"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.1.recovery.addr","206.200.151.35:18165"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.1.snapshot.auth","STRK01:6EUG2W"},

	{"efh.PHLX_ORD.group.2.mcast.addr"   ,"233.47.179.122:18066"},
	{"efh.PHLX_ORD.group.2.snapshot.addr","206.200.151.36:18166"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.2.recovery.addr","206.200.151.36:18166"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.2.snapshot.auth","STRK01:6EUG2W"},

	{"efh.PHLX_ORD.group.3.mcast.addr"   ,"233.47.179.123:18067"},
	{"efh.PHLX_ORD.group.3.snapshot.addr","206.200.151.37:18167"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.3.recovery.addr","206.200.151.37:18167"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.3.snapshot.auth","STRK01:6EUG2W"},

};

EkaProp efhPhlxOrdInitCtxEntries_B[] = {
	{"efh.PHLX_ORD.group.0.mcast.addr"   ,"233.47.179.184:18064"},
	{"efh.PHLX_ORD.group.0.snapshot.addr","206.200.151.98:18164"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.0.recovery.addr","206.200.151.98:18164"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.0.snapshot.auth","STRK01:6EUG2W"},

	{"efh.PHLX_ORD.group.1.mcast.addr"   ,"233.47.179.185:18065"},
	{"efh.PHLX_ORD.group.1.snapshot.addr","206.200.151.99:18165"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.1.recovery.addr","206.200.151.99:18165"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.1.snapshot.auth","STRK01:6EUG2W"},

	{"efh.PHLX_ORD.group.2.mcast.addr"   ,"233.47.179.186:18066"},
	{"efh.PHLX_ORD.group.2.snapshot.addr","206.200.151.100:18166"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.2.recovery.addr","206.200.151.100:18166"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.2.snapshot.auth","STRK01:6EUG2W"},

	{"efh.PHLX_ORD.group.3.mcast.addr"   ,"233.47.179.187:18067"},
	{"efh.PHLX_ORD.group.3.snapshot.addr","206.200.151.101:18167"}, 	// TCP SOUPBIN
	{"efh.PHLX_ORD.group.3.recovery.addr","206.200.151.101:18167"}, 	// MOLD RECOVERY
	{"efh.PHLX_ORD.group.3.snapshot.auth","STRK01:6EUG2W"},

};

const EkaGroup phlxOrdGroups[] = {
	{EkaSource::kPHLX_ORD, (EkaLSI)0},
	{EkaSource::kPHLX_ORD, (EkaLSI)1},
	{EkaSource::kPHLX_ORD, (EkaLSI)2},
	{EkaSource::kPHLX_ORD, (EkaLSI)3},
};

#endif
