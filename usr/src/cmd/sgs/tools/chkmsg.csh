#!/bin/csh
#

# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# ident	"@(#)chkmsg.csh	1.5	97/10/21 SMI"
#
set MACH = `mach`
if ($1 == "-64") then
	if ($MACH == "sparc") then
		set MACH = sparcv9
	else
		echo "No 64-bit architecture available"
		exit
	endif
	shift
endif

set TOOLDIR = "${CODEMGR_WS}/usr/src/cmd/sgs/tools/"
set MSGFILE = $1

#
# remove the temporary files
#
rm -f CATA_MSG_INTL_LIST CATA_MSG_ORIG_LIST
rm -f MSG_INTL_LIST MSG_ORIG_LIST

#
#
#
nawk -f ${TOOLDIR}/catalog.awk mach=$MACH $1
sort CATA_MSG_INTL_LIST | uniq > _TMP
mv _TMP CATA_MSG_INTL_LIST

sort CATA_MSG_ORIG_LIST | uniq > _TMP
mv _TMP CATA_MSG_ORIG_LIST
shift

#
# Generate the lists for the source files
#
nawk -f  ${TOOLDIR}/getmessage.awk	$*

sort MSG_INTL_LIST | uniq > _TMP
mv _TMP MSG_INTL_LIST

sort MSG_ORIG_LIST | uniq > _TMP
mv _TMP MSG_ORIG_LIST

rm -f _TMP


#
# Start checking
#

#
# Check MESG_INTL message
#
comm -23 CATA_MSG_INTL_LIST MSG_INTL_LIST > _TMP
if (-z _TMP) then
	rm -f _TMP
else
	echo " "
	echo "These are the messages MACROS defined in ${MSGFILE}"
	echo "	in between _START_ and _END_."
	echo "However, these messages are not referenced by MSG_INTL()."
	cat _TMP | sed "s/^/	/"
	rm -f _TMP
endif

comm -13 CATA_MSG_INTL_LIST MSG_INTL_LIST > _TMP
if (-z _TMP) then
	rm -f _TMP
else
	echo " "
	echo "These are the messages MACROS referenced by MSG_INTL()"
	echo "However, these messages are not defined in ${MSGFILE}"
	echo "in between _START_ and _END_."
	cat _TMP | sed "s/^/	/"
	rm -f _TMP
endif

#
# Checke MESG_ORIG message
#
comm -23 CATA_MSG_ORIG_LIST MSG_ORIG_LIST > _TMP
if (-z _TMP) then
	rm -f _TMP
else
	echo " "
	echo -n "These are the messages MACROS defined in ${MSGFILE}"
	echo " after _END_."
	echo "However, these messages are not referenced by MSG_ORIG()."
	cat _TMP | sed "s/^/	/"
	rm -f _TMP
endif

comm -13 CATA_MSG_ORIG_LIST MSG_ORIG_LIST > _TMP
if (-z _TMP) then
	rm -f _TMP
else
	echo " "
	echo "These are the messages MACROS referenced by MSG_ORIG()."
	echo -n "However, these messages are not defined in ${MSGFILE}"
	echo " after _END_."
	cat _TMP | sed "s/^/	/"
	rm -f _TMP
endif

#
# remove the temporary files
#
rm -f CATA_MSG_INTL_LIST CATA_MSG_ORIG_LIST
rm -f MSG_INTL_LIST MSG_ORIG_LIST
