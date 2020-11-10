#!/usr/bin/perl

# PHLX_ORD

print <<EOF;
#ifndef _EFH_PHLX_ORD__PROPS_H
#define _EFH_PHLX_ORD__PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 4;

$exch_name = "PHLX_ORD";


$mc_port            = 18064;
$mc_base            = "233.47.179.";
$mc_lsb_base        = 120;

$snapshot_ip        = "206.200.151.";
$snapshot_lsb_base  = 34;
$snapshot_port      = 18164;

$recovery_ip        = $snapshot_ip;
$recovery_lsb_base  = $snapshot_lsb_base;
$recovery_port      = $snapshot_port;

$username = "STRK01";
$passwd   = "6EUG2W";

print "EkaProp efhPhlxOrdInitCtxEntries_A[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $snapshot_lsb  = $snapshot_lsb_base + $i;
    $recovery_lsb  = $recovery_lsb_base + $i;

    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$snapshot_ip$snapshot_lsb:$snapshot_port\"\}, \t// TCP SOUPBIN\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip$recovery_lsb:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $recovery_port ++;
    $snapshot_port ++;
}
print "};\n\n";
#####################################################
$mc_port            = 18064;
$mc_base            = "233.47.179.";
$mc_lsb_base        = 184;

$snapshot_ip        = "206.200.151.";
$snapshot_lsb_base  = 98;
$snapshot_port      = 18164;

$recovery_ip        = $snapshot_ip;
$recovery_lsb_base  = $snapshot_lsb_base;
$recovery_port      = $snapshot_port;

print "EkaProp efhPhlxOrdInitCtxEntries_B[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $snapshot_lsb  = $snapshot_lsb_base + $i;
    $recovery_lsb  = $recovery_lsb_base + $i;

    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$snapshot_ip$snapshot_lsb:$snapshot_port\"\}, \t// TCP SOUPBIN\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip$recovery_lsb:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $recovery_port ++;
    $snapshot_port ++;
}
print "};\n\n";
#####################################################


print "const EkaGroup phlxOrdGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";
