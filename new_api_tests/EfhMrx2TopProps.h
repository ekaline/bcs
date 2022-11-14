#ifndef _EFH_MRX2TOP_DPTH__PROPS_H
#define _EFH_MRX2TOP_DPTH__PROPS_H

EkaProp efhMrx2TopInitCtxEntries_A[] = {
	{"efh.MRX2_TOP.group.0.products","vanilla_book"},
	{"efh.MRX2_TOP.group.0.mcast.addr"   ,"233.104.73.0:18000"},
	{"efh.MRX2_TOP.group.0.recovery.addr","206.200.182.64:18100"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.0.snapshot.addr","206.200.182.72:18300"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.0.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.1.products","vanilla_book"},
	{"efh.MRX2_TOP.group.1.mcast.addr"   ,"233.104.73.1:18001"},
	{"efh.MRX2_TOP.group.1.recovery.addr","206.200.182.66:18101"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.1.snapshot.addr","206.200.182.73:18301"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.1.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.2.products","vanilla_book"},
	{"efh.MRX2_TOP.group.2.mcast.addr"   ,"233.104.73.2:18002"},
	{"efh.MRX2_TOP.group.2.recovery.addr","206.200.182.68:18102"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.2.snapshot.addr","206.200.182.74:18302"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.2.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.3.products","vanilla_book"},
	{"efh.MRX2_TOP.group.3.mcast.addr"   ,"233.104.73.3:18003"},
	{"efh.MRX2_TOP.group.3.recovery.addr","206.200.182.70:18103"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.3.snapshot.addr","206.200.182.75:18303"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.3.snapshot.auth","MGGT04:UWYYFCYI"},

};

EkaProp efhMrx2TopInitCtxEntries_B[] = {
	{"efh.MRX2_TOP.group.0.products","vanilla_book"},
	{"efh.MRX2_TOP.group.0.mcast.addr"   ,"233.104.56.0:18000"},
	{"efh.MRX2_TOP.group.0.recovery.addr","206.200.182.192:18100"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.0.snapshot.addr","206.200.182.200:18300"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.0.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.1.products","vanilla_book"},
	{"efh.MRX2_TOP.group.1.mcast.addr"   ,"233.104.56.1:18001"},
	{"efh.MRX2_TOP.group.1.recovery.addr","206.200.182.194:18101"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.1.snapshot.addr","206.200.182.201:18301"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.1.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.2.products","vanilla_book"},
	{"efh.MRX2_TOP.group.2.mcast.addr"   ,"233.104.56.2:18002"},
	{"efh.MRX2_TOP.group.2.recovery.addr","206.200.182.196:18102"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.2.snapshot.addr","206.200.182.202:18302"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.2.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.3.products","vanilla_book"},
	{"efh.MRX2_TOP.group.3.mcast.addr"   ,"233.104.56.3:18003"},
	{"efh.MRX2_TOP.group.3.recovery.addr","206.200.182.198:18103"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.3.snapshot.addr","206.200.182.203:18303"}, 	// TCP GLIMPSE
	{"efh.MRX2_TOP.group.3.snapshot.auth","MGGT04:UWYYFCYI"},

};

const EkaGroup mrx2TopGroups[] = {
	{EkaSource::kMRX2_TOP, (EkaLSI)0},
	{EkaSource::kMRX2_TOP, (EkaLSI)1},
	{EkaSource::kMRX2_TOP, (EkaLSI)2},
	{EkaSource::kMRX2_TOP, (EkaLSI)3},
};

#endif
