#ifndef _EFH_GEM_TQF__PROPS_H
#define _EFH_GEM_TQF__PROPS_H


  EkaProp efhGemInitCtxEntries_A[] = {
    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.148:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.148:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.148:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.148:18003"},

    {"efh.GEM_TQF.group.0.snapshot.auth","GGTBC4:BR5ODP"},
    {"efh.GEM_TQF.group.1.snapshot.auth","GGTBC5:0WN9GH"},
    {"efh.GEM_TQF.group.2.snapshot.auth","GGTBC6:03BHXL"},
    {"efh.GEM_TQF.group.3.snapshot.auth","GGTBC7:C21TH1"},

    {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.120:18300"},
    {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.121:18301"},
    {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.122:18302"},
    {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.123:18303"},

    {"efh.GEM_TQF.group.0.recovery.addr","206.200.230.128:18100"},
    {"efh.GEM_TQF.group.1.recovery.addr","206.200.230.129:18101"},
    {"efh.GEM_TQF.group.2.recovery.addr","206.200.230.130:18102"},
    {"efh.GEM_TQF.group.3.recovery.addr","206.200.230.131:18103"},
  };
  EkaProp efhGemInitCtxEntries_B[] = {
    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.164:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.164:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.164:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.164:18003"},

    {"efh.GEM_TQF.group.0.snapshot.auth","GGTBC4:BR5ODP"},
    {"efh.GEM_TQF.group.1.snapshot.auth","GGTBC5:0WN9GH"},
    {"efh.GEM_TQF.group.2.snapshot.auth","GGTBC6:03BHXL"},
    {"efh.GEM_TQF.group.3.snapshot.auth","GGTBC7:C21TH1"},

    {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.248:18300"},
    {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.249:18301"},
    {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.250:18302"},
    {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.251:18303"},

    {"efh.GEM_TQF.group.0.recovery.addr","206.200.230.128:18100"},
    {"efh.GEM_TQF.group.1.recovery.addr","206.200.230.129:18101"},
    {"efh.GEM_TQF.group.2.recovery.addr","206.200.230.130:18102"},
    {"efh.GEM_TQF.group.3.recovery.addr","206.200.230.131:18103"},
  };

const  EkaGroup gemGroups[] = {
    {EkaSource::kGEM_TQF, (EkaLSI)0},
    {EkaSource::kGEM_TQF, (EkaLSI)1},
    {EkaSource::kGEM_TQF, (EkaLSI)2},
    {EkaSource::kGEM_TQF, (EkaLSI)3}
  };

#endif

