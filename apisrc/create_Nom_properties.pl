#!/usr/bin/perl

# NOM_ITTO

print <<EOF;
#ifndef _EFH_NOM_ITTO__PROPS_H
#define _EFH_NOM_ITTO__PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 4;

$exch_name = "NOM_ITTO";


$mc_port            = 18000;
$mc_base            = "233.54.12.";
$mc_lsb_base        = 72;

$snapshot_ip        = "206.200.43.";
$snapshot_lsb_base  = 72;
$snapshot_port      = 18300;

$recovery_ip        = "206.200.43.";
$recovery_lsb_base  = 64;
$recovery_port      = 18100;

$username = "NGBAR2";
$passwd   = "HY4VXK";

print "EkaProp efhNomInitCtxEntries_A[] = {\n";
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
$mc_port            = 18000;
$mc_base            = "233.49.196.";
$mc_lsb_base        = 72;

$snapshot_ip        = "206.200.43.";
$snapshot_lsb_base  = 72;
$snapshot_port      = 18300;

$recovery_ip        = "206.200.43.";
$recovery_lsb_base  = 64;
$recovery_port      = 18100;

print "EkaProp efhNomInitCtxEntries_B[] = {\n";
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


print "const EkaGroup nomGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";
