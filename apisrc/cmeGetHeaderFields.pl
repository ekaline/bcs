#!/usr/bin/perl -w

while (<>) {
#        <field name="MinTradeVol" id="562" type="uInt32" description="The minimum trading volume for a security" offset="83" semanticType="Qty"/>

    if (/\<field name\=\"(\S+)\"\s+id\=\"\d+\"\s+type\=\"(\S+)\"/) {
	$type = $2 . "_T";
	$name = $1;
	printf "\t%-30s\t%s;\n",$type,$name;
    } else {
	print "PARSER WARNING: $_";
    }
}

