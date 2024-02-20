#ifndef __EKE_MOEX_NETWORK_CONFIG_H__
#define __EKE_MOEX_NETWORK_CONFIG_H__

static const TestTcpSess testDefaultTcpSess[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111},
    {2, "120.0.0.0", "10.0.0.12", 22222},
    {3, "130.0.0.0", "10.0.0.13", 22333}};

static const TestTcpSess tcp1_s[] = {
    {1, "110.0.0.0", "10.0.0.11", 22111}};
static const TestTcpSess tcp01_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111}};

static const TestTcpParams tcp1 = {tcp1_s,
                                   std::size(tcp1_s)};
static const TestTcpParams tcp01 = {tcp01_s,
                                    std::size(tcp01_s)};

static const TestTcpSess tcp3_s[] = {
    {3, "130.0.0.0", "10.0.0.13", 22333}};
static const TestTcpParams tcp3 = {tcp3_s,
                                   std::size(tcp3_s)};

static const McGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const UdpMcParams two_cores_1mc = {mc01,
                                          std::size(mc01)};

static const TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};

/* --------------------------------------------- */
static const McGroupParams mc0[] = {0, "224.0.10.100",
                                    30310};
static const UdpMcParams core0_1mc = {mc0, std::size(mc0)};
static const McGroupParams mc1[] = {1, "224.1.11.101",
                                    30311};
static const UdpMcParams core1_1mc = {mc1, std::size(mc1)};
// static const McGroupParams mc2[] = {2, "224.2.12.102",
//                                     30312}
static const McGroupParams mc2[] = {2, "239.195.1.16",
                                    16016};
static const UdpMcParams core2_1mc = {mc2, std::size(mc2)};
static const McGroupParams mc3[] = {3, "224.3.13.103",
                                    30313};
static const UdpMcParams core3_1mc = {mc3, std::size(mc3)};

#endif
