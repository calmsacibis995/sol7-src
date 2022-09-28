divert(-1)
# Copyright (c) 1993, 1997
#	Sun Microsystems, Inc.  All rights reserved.
divert(0)
VERSIONID(`@(#)sun_reverse_alias_files.m4	1.2 (Sun) 01/27/98')
divert(-1)

define(`_RALIAS_', 1)
 
PUSHDIVERT(6)
#define reverse alias lookup map
#*IMPORTANT* make sure the is no trailing spaces in your aliases file
Kralias text -a<@$m> -z: -k1 -v0 /etc/mail/aliases
POPDIVERT
