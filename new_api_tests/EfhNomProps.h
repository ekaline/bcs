#ifndef _EFH_NOM_ITTO__PROPS_H
#define _EFH_NOM_ITTO__PROPS_H

EkaProp efhNomInitCtxEntries_A[] = {
	{"efh.NOM_ITTO.group.0.staleDataNsThreshold"   ,1000000000},
	{"efh.NOM_ITTO.group.0.mcast.addr"   ,"233.54.12.72:18000"},
	{"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.0.snapshot.auth","NGBAR2:HY4VXK"},

	{"efh.NOM_ITTO.group.1.staleDataNsThreshold"   ,1000000000},
	{"efh.NOM_ITTO.group.1.mcast.addr"   ,"233.54.12.73:18001"},
	{"efh.NOM_ITTO.group.1.snapshot.addr","206.200.43.73:18301"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.1.recovery.addr","206.200.43.65:18101"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.1.snapshot.auth","NGBAR2:HY4VXK"},

	{"efh.NOM_ITTO.group.2.staleDataNsThreshold"   ,1000000000},
	{"efh.NOM_ITTO.group.2.mcast.addr"   ,"233.54.12.74:18002"},
	{"efh.NOM_ITTO.group.2.snapshot.addr","206.200.43.74:18302"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.2.recovery.addr","206.200.43.66:18102"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.2.snapshot.auth","NGBAR2:HY4VXK"},

	{"efh.NOM_ITTO.group.3.staleDataNsThreshold"   ,1000000000},
	{"efh.NOM_ITTO.group.3.mcast.addr"   ,"233.54.12.75:18003"},
	{"efh.NOM_ITTO.group.3.snapshot.addr","206.200.43.75:18303"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.3.recovery.addr","206.200.43.67:18103"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.3.snapshot.auth","NGBAR2:HY4VXK"},

};

EkaProp efhNomInitCtxEntries_B[] = {
	{"efh.NOM_ITTO.group.0.mcast.addr"   ,"233.49.196.72:18000"},
	{"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.0.snapshot.auth","NGBAR2:HY4VXK"},

	{"efh.NOM_ITTO.group.1.mcast.addr"   ,"233.49.196.73:18001"},
	{"efh.NOM_ITTO.group.1.snapshot.addr","206.200.43.73:18301"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.1.recovery.addr","206.200.43.65:18101"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.1.snapshot.auth","NGBAR2:HY4VXK"},

	{"efh.NOM_ITTO.group.2.mcast.addr"   ,"233.49.196.74:18002"},
	{"efh.NOM_ITTO.group.2.snapshot.addr","206.200.43.74:18302"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.2.recovery.addr","206.200.43.66:18102"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.2.snapshot.auth","NGBAR2:HY4VXK"},

	{"efh.NOM_ITTO.group.3.mcast.addr"   ,"233.49.196.75:18003"},
	{"efh.NOM_ITTO.group.3.snapshot.addr","206.200.43.75:18303"}, 	// TCP SOUPBIN
	{"efh.NOM_ITTO.group.3.recovery.addr","206.200.43.67:18103"}, 	// MOLD RECOVERY
	{"efh.NOM_ITTO.group.3.snapshot.auth","NGBAR2:HY4VXK"},

};

const EkaGroup nomGroups[] = {
	{EkaSource::kNOM_ITTO, (EkaLSI)0},
	{EkaSource::kNOM_ITTO, (EkaLSI)1},
	{EkaSource::kNOM_ITTO, (EkaLSI)2},
	{EkaSource::kNOM_ITTO, (EkaLSI)3},
};

#endif
