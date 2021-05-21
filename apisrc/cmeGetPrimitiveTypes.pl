#!/usr/bin/perl -w

 #       <type name="Asset" description="Asset" length="6" primitiveType="char" semanticType="String"/>

while (<>) {
#    print;
    $primitiveType = "";
    $name = "";
    $length = "";
    $description = "";
    $semanticType = "";
    $constValue = "";
    $constValue = $1 if s/\>(\S+)\<\/type\>//;
    foreach $f (split(/\s+/)) {
	next if $f eq "";
	next if $f eq "<type";
	$f =~  s/\"//g;
	$f =~  s#\/##g;
	$f =~  s#\>##g;

	($key,$value) = split (/\=/,$f);
#	printf "\t%s--%s\n",$key,$value;
	$primitiveType = $value if $key eq "primitiveType";
	$name          = $value . "_T;" if $key eq "name";
	$length        = $value if $key eq "length";
	$description   = $value if $key eq "description";
	$semanticType  = $value if $key eq "semanticType";
    }
    if ($primitiveType eq "char" && $length ne "" && $length ne "1") {
	$type2print = "char[$length]";
    } else {
	$type2print = $primitiveType;
    }
    printf "typedef %-10s\t%-30s  // length=\'%s\', semanticType=\'%s\', const=\'%s\',description=\'%s\'\n", $type2print,$name,$length,$semanticType,$constValue,$description;
}
