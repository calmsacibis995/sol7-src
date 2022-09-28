#pragma ident	"@(#)mkversion.pl	1.2	97/04/24 SMI"
#! /usr/bin/perl

$version = <>;
chop $version;
print <<END;
#define SP_VERSION SP_T("$version")
END
