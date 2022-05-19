#!/usr/bin/perl

# BX_DPTH

print <<EOF;
#ifndef _EFH_BX_DPTH__PROPS_H
#define _EFH_BX_DPTH__PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 4;

$exch_name = "BX_DPTH";


$mc_port            = 18000;
$mc_base            = "233.54.12.";
$mc_lsb_base        = 52;

$snapshot_ip        = "206.200.131.";
$snapshot_lsb_base  = 164;
$snapshot_port      = 18300;

$recovery_ip        = "206.200.131.";
$recovery_lsb_base  = 160;
$recovery_port      = 18100;

$username = "BLZ001";
$passwd   = "KRDIOASF";

print "EkaProp efhBxInitCtxEntries_A[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $snapshot_lsb  = $snapshot_lsb_base + $i;
    $recovery_lsb  = $recovery_lsb_base + $i;

    print "\t\{\"efh.$exch_name.group.$i.products\",\"vanilla_book\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$snapshot_ip$snapshot_lsb:$snapshot_port\"\}, \t// TCP GLIMPSE\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip$recovery_lsb:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $recovery_port ++;
    $snapshot_port ++;
}
print "};\n\n";
#####################################################
$mc_port            = 18000;
$mc_base            = "233.49.196.";
$mc_lsb_base        = 72;

$snapshot_ip        = "206.200.131.";
$snapshot_lsb_base  = 196;
$snapshot_port      = 18300;

$recovery_ip        = "206.200.43.";
$recovery_lsb_base  = 64;
$recovery_port      = 18100;

print "EkaProp efhBxInitCtxEntries_B[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $snapshot_lsb  = $snapshot_lsb_base + $i;
    $recovery_lsb  = $recovery_lsb_base + $i;

    print "\t\{\"efh.$exch_name.group.$i.products\",\"vanilla_book\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$snapshot_ip$snapshot_lsb:$snapshot_port\"\}, \t// TCP GLIMPSE\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip$recovery_lsb:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $recovery_port ++;
    $snapshot_port ++;
}
print "};\n\n";
#####################################################


print "const EkaGroup bxGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";
