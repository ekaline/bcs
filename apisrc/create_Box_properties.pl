#!/usr/bin/perl

# BOX_HSVF

print <<EOF;
#ifndef _EFH_BOX_PROPS_H
#define _EFH_BOX_PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 8;

$mc_port = 21401;
$mc_base  = "224.0.124.";
$mc_lsb = 1;
$mc_line = 11;

$recovery_ip = "198.235.27.47";

$recovery_port = 21410;

$exch_name = "BOX_HSVF";

print "EkaProp efhBoxInitCtxEntries_A[] = {\n";
for ($i=0; $i<$groups;$i++) {
    $username = "  ";
    $passwd   = "  ";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.line\"   ,\"$mc_line\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$recovery_ip:$recovery_port\"\}, \t// RECOVERY \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\}, \t\t\t// RECOVERY \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip:$recovery_port\"\}, \t// RECOVERY \n\n";

    $mc_lsb++;
    $mc_port += 1000;
    $mc_line += 10;
    $recovery_port += 1000;
}
print "};\n\n";
#####################################################
$mc_port = 21404;
$mc_lsb = 49;
$mc_line = 11;
$recovery_ip = "198.235.27.55";
$recovery_port = 21410;

print "EkaProp efhBoxInitCtxEntries_B[] = {\n";
for ($i=0; $i<$groups;$i++) {
    $username = "                ";
    $passwd   = "                ";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.line\"   ,\"$mc_line\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$recovery_ip:$recovery_port\"\}, \t// RECOVERY \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\}, \t\t\t// RECOVERY \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip:$recovery_port\"\}, \t// RECOVERY \n\n";

    $mc_lsb++;
    $mc_port += 1000;
    $mc_line += 10;
    $recovery_port += 1000;
}
print "};\n\n";
#####################################################


print "const EkaGroup boxGroups[] = {\n";

for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::kBOX_HSVF, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";
