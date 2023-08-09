#ifndef _EFH_BX_DPTH__PROPS_H
#define _EFH_BX_DPTH__PROPS_H

EkaProp efhBxInitCtxEntries_A[] = {
	{"efh.BX_DPTH.group.0.products","vanilla_book"},
	{"efh.BX_DPTH.group.0.mcast.addr"   ,"233.54.12.52:18000"},
	{"efh.BX_DPTH.group.0.snapshot.addr","206.200.131.164:18300"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.0.recovery.addr","206.200.131.160:18100"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.0.snapshot.auth","BLZ001:KRDIOASF"},

	{"efh.BX_DPTH.group.1.products","vanilla_book"},
	{"efh.BX_DPTH.group.1.mcast.addr"   ,"233.54.12.53:18001"},
	{"efh.BX_DPTH.group.1.snapshot.addr","206.200.131.165:18301"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.1.recovery.addr","206.200.131.161:18101"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.1.snapshot.auth","BLZ001:KRDIOASF"},

	{"efh.BX_DPTH.group.2.products","vanilla_book"},
	{"efh.BX_DPTH.group.2.mcast.addr"   ,"233.54.12.54:18002"},
	{"efh.BX_DPTH.group.2.snapshot.addr","206.200.131.166:18302"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.2.recovery.addr","206.200.131.162:18102"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.2.snapshot.auth","BLZ001:KRDIOASF"},

	{"efh.BX_DPTH.group.3.products","vanilla_book"},
	{"efh.BX_DPTH.group.3.mcast.addr"   ,"233.54.12.55:18003"},
	{"efh.BX_DPTH.group.3.snapshot.addr","206.200.131.167:18303"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.3.recovery.addr","206.200.131.163:18103"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.3.snapshot.auth","BLZ001:KRDIOASF"},

};

EkaProp efhBxInitCtxEntries_B[] = {
	{"efh.BX_DPTH.group.0.products","vanilla_book"},
	{"efh.BX_DPTH.group.0.mcast.addr"   ,"233.49.196.72:18000"},
	{"efh.BX_DPTH.group.0.snapshot.addr","206.200.131.196:18300"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.0.recovery.addr","206.200.43.64:18100"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.0.snapshot.auth","BLZ001:KRDIOASF"},

	{"efh.BX_DPTH.group.1.products","vanilla_book"},
	{"efh.BX_DPTH.group.1.mcast.addr"   ,"233.49.196.73:18001"},
	{"efh.BX_DPTH.group.1.snapshot.addr","206.200.131.197:18301"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.1.recovery.addr","206.200.43.65:18101"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.1.snapshot.auth","BLZ001:KRDIOASF"},

	{"efh.BX_DPTH.group.2.products","vanilla_book"},
	{"efh.BX_DPTH.group.2.mcast.addr"   ,"233.49.196.74:18002"},
	{"efh.BX_DPTH.group.2.snapshot.addr","206.200.131.198:18302"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.2.recovery.addr","206.200.43.66:18102"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.2.snapshot.auth","BLZ001:KRDIOASF"},

	{"efh.BX_DPTH.group.3.products","vanilla_book"},
	{"efh.BX_DPTH.group.3.mcast.addr"   ,"233.49.196.75:18003"},
	{"efh.BX_DPTH.group.3.snapshot.addr","206.200.131.199:18303"}, 	// TCP GLIMPSE
	{"efh.BX_DPTH.group.3.recovery.addr","206.200.43.67:18103"}, 	// MOLD RECOVERY
	{"efh.BX_DPTH.group.3.snapshot.auth","BLZ001:KRDIOASF"},

};

const EkaGroup bxGroups[] = {
	{EkaSource::kBX_DPTH, (EkaLSI)0},
	{EkaSource::kBX_DPTH, (EkaLSI)1},
	{EkaSource::kBX_DPTH, (EkaLSI)2},
	{EkaSource::kBX_DPTH, (EkaLSI)3},
};

#endif
