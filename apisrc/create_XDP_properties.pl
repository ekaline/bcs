#!/usr/bin/perl

# ARCA Side B
print <<EOF;
#ifndef _EFH_XDP_PROPS_H
#define _EFH_XDP_PROPS_H

EOF

print "EkaProp efhArcaInitCtxEntries_A[] = {\n";

$port = 11032;
$unit = 1;
$rt_mc_base  = "224.0.41.";
$rt_mc_lsb = 32;

$exch_name = "ARCA_XDP";

$user   = "bcoxdp10";
$passwd = "bc0xdp10";

for ($i=0; $i<26;$i++) {
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"159.125.66.225:51700\"\}, // Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"${user}:${passwd}\"\}, \t// Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"0.0.0.0:0\"\}, \t\t// Not used \n\n";

    $port++;
    $rt_mc_lsb++;
}

print "};\n\n";

#####################################################

print "EkaProp efhArcaInitCtxEntries_B[] = {\n";

$port = 12160;
$unit = 1;
$rt_mc_base  = "224.0.41.";
$rt_mc_lsb = 160;

$exch_name = "ARCA_XDP";

$user   = "bcoxdp10";
$passwd = "bc0xdp10";

for ($i=0; $i<26;$i++) {
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"159.125.66.225:51700\"\}, // Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"${user}:${passwd}\"\}, \t// Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"0.0.0.0:0\"\}, \t\t// Not used \n\n";

    $port++;
    $rt_mc_lsb++;
}

print "};\n\n";

#####################################################

print "EkaProp efhAmexInitCtxEntries_A[] = {\n";

$port = 11032;
$unit = 1;
$rt_mc_base  = "224.0.58.";
$rt_mc_lsb = 32;

$exch_name = "AMEX_XDP";

$user   = "bcoxdp10";
$passwd = "bc0xdp10";

for ($i=0; $i<26;$i++) {
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"159.125.66.227:51700\"\}, // Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"${user}:${passwd}\"\}, \t// Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"0.0.0.0:0\"\}, \t\t// Not used \n\n";

    $port++;
    $rt_mc_lsb++;
}

print "};\n\n";

#####################################################

print "EkaProp efhAmexInitCtxEntries_B[] = {\n";

$port = 12160;
$unit = 1;
$rt_mc_base  = "224.0.58.";
$rt_mc_lsb = 160;

$exch_name = "AMEX_XDP";

$user   = "bcoxdp10";
$passwd = "bc0xdp10";

for ($i=0; $i<26;$i++) {
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"159.125.66.227:51700\"\}, // Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"${user}:${passwd}\"\}, \t// Definitions \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"0.0.0.0:0\"\}, \t\t// Not used \n\n";

    $port++;
    $rt_mc_lsb++;
}

print "};\n\n";

#####################################################

print "const EkaGroup arcaGroups[] = {\n";

for ($i = 0; $i < 26; $i ++) {
    print "\t{EkaSource::kARCA_XDP, (EkaLSI)$i},\n";
}
print "};\n\n";

print "const EkaGroup amexGroups[] = {\n";

for ($i = 0; $i < 26; $i ++) {
    print "\t{EkaSource::kAMEX_XDP, (EkaLSI)$i},\n";
}
print "};\n\n";

#####################################################

print "#endif\n";
