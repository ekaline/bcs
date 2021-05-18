#ifndef _EFH_ISE_TQF__PROPS_H
#define _EFH_ISE_TQF__PROPS_H

EkaProp efhIseInitCtxEntries_A[] = {
    // side B
    {"efh.ISE_TQF.group.0.mcast.addr","233.54.12.152:18000"},
    {"efh.ISE_TQF.group.1.mcast.addr","233.54.12.152:18001"},
    {"efh.ISE_TQF.group.2.mcast.addr","233.54.12.152:18002"},
    {"efh.ISE_TQF.group.3.mcast.addr","233.54.12.152:18003"},

    {"efh.ISE_TQF.group.0.snapshot.auth","IGTBC9:NI8HKX"},
    {"efh.ISE_TQF.group.1.snapshot.auth","IGTB1B:AZK9CI"},
    {"efh.ISE_TQF.group.2.snapshot.auth","IGTB1D:6V1SWS"},
    {"efh.ISE_TQF.group.3.snapshot.auth","IGTB1F:4A6ZXQ"},

    {"efh.ISE_TQF.group.0.snapshot.addr","206.200.230.104:18300"},
    {"efh.ISE_TQF.group.1.snapshot.addr","206.200.230.105:18301"},
    {"efh.ISE_TQF.group.2.snapshot.addr","206.200.230.106:18302"},
    {"efh.ISE_TQF.group.3.snapshot.addr","206.200.230.107:18303"},

    {"efh.ISE_TQF.group.0.recovery.addr","206.200.230.104:18300"},
    {"efh.ISE_TQF.group.1.recovery.addr","206.200.230.105:18301"},
    {"efh.ISE_TQF.group.2.recovery.addr","206.200.230.106:18302"},
    {"efh.ISE_TQF.group.3.recovery.addr","206.200.230.107:18303"},
};  

EkaProp efhIseInitCtxEntries_B[] = {
    // side B
    {"efh.ISE_TQF.group.0.mcast.addr","233.54.12.168:18000"},
    {"efh.ISE_TQF.group.1.mcast.addr","233.54.12.168:18001"},
    {"efh.ISE_TQF.group.2.mcast.addr","233.54.12.168:18002"},
    {"efh.ISE_TQF.group.3.mcast.addr","233.54.12.168:18003"},

    {"efh.ISE_TQF.group.0.snapshot.auth","IGTBC9:NI8HKX"},
    {"efh.ISE_TQF.group.1.snapshot.auth","IGTB1B:AZK9CI"},
    {"efh.ISE_TQF.group.2.snapshot.auth","IGTB1D:6V1SWS"},
    {"efh.ISE_TQF.group.3.snapshot.auth","IGTB1F:4A6ZXQ"},

    {"efh.ISE_TQF.group.0.snapshot.addr","206.200.230.104:18300"},
    {"efh.ISE_TQF.group.1.snapshot.addr","206.200.230.105:18301"},
    {"efh.ISE_TQF.group.2.snapshot.addr","206.200.230.106:18302"},
    {"efh.ISE_TQF.group.3.snapshot.addr","206.200.230.107:18303"},

    {"efh.ISE_TQF.group.0.recovery.addr","206.200.230.160:18100"},
    {"efh.ISE_TQF.group.1.recovery.addr","206.200.230.161:18101"},
    {"efh.ISE_TQF.group.2.recovery.addr","206.200.230.162:18102"},
    {"efh.ISE_TQF.group.3.recovery.addr","206.200.230.163:18103"},
};


const  EkaGroup iseGroups[] = {
    {EkaSource::kISE_TQF, (EkaLSI)0},
    {EkaSource::kISE_TQF, (EkaLSI)1},
    {EkaSource::kISE_TQF, (EkaLSI)2},
    {EkaSource::kISE_TQF, (EkaLSI)3}
  };

#endif

