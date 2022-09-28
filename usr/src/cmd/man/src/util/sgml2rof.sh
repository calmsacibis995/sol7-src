#!/bin/sh 
#
#pragma ident   "@(#)sgml2roff.sh 1.2 98/02/12 SMI"
#
#############################################################################
#
#	sgml2roff
#       adapted from docbook-to-man.sh
#
#############################################################################
# 
# Copyright (c) 1996 X Consortium
# Copyright (c) 1996 Dalrymple Consulting
# Copyright (c) 1997, by Sun Microsystems, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# X CONSORTIUM OR DALRYMPLE CONSULTING BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
# Except as contained in this notice, the names of the X Consortium and
# Dalrymple Consulting shall not be used in advertising or otherwise to
# promote the sale, use or other dealings in this Software without prior
# written authorization.
# 
#############################################################################
#
# Written 5/29/96 by Fred Dalrymple
#
#############################################################################

# ***** change the following paths if your installation of nsgmls and / or
# ***** DocBook isn't into the default places.

ROOT=/usr
SGMLS=$ROOT/lib/sgml
DTDS=$ROOT/share/lib/sgml/locale/C/dtds
TPTFILES=$ROOT/share/lib/sgml/locale/C/transpec

# ***** modify the following line (to "=false") if you're not using the
# ***** Elan Documentor's Workbench

doElanPSInclude=false



# Everything below this line should be pretty standard and not require
# modification.

#ulimit -c unlimited

PARSER=/usr/lib/sgml/nsgmls
INSTANT=/usr/lib/sgml/instant
INSTANT_OPT=-d

CATALOG=$DTDS/catalog
DECL=$DTDS/solbook.dcl
#PROLOG=$DOCBOOK/docbook.prolog

error=false


if [ $# -eq 1 ]
then	INSTANCE=$1
else	error=true
fi

if `$error`
#then	echo "usage:  docbook-to-man docbook-instance"
then	echo "usage:  sgml2roff <manpage>"
	exit 1
fi

# First things first. Is it a fragment?
#
if [ `cat $1 | grep -i '<refentry[ >]' | wc -l` -lt 1 ];
then
# Second things second. Is this a shadow man page?
   if  [ `head -10 $1 | grep "SHADOW_PAGE" | wc -l` -gt 0 ];
   then
	# Yes, it is. 
	# First, we need to invoke myself on master to make it
	MASTER=`cat $1 | nawk '/SYSTEM/ {gsub("\"","",$4);gsub(">","",$4);print $4}'`
	PATH_TO_MASTER="/usr/man/sman`echo $MASTER | nawk -F "." '{print $NF}'`"
	BMASTER=`basename $MASTER`
	rm -f  /var/tmp/$BMASTER
	(/usr/lib/sgml/sgml2roff $PATH_TO_MASTER/$BMASTER) > /var/tmp/$BMASTER
	chmod 777 /var/tmp/$BMASTER
	# Now we kick out stub pointing to it
        echo ".so /var/tmp/$BMASTER"
	echo '.\" reference page created by sgml2roff'
	exit 0
   else
	# This is just a plain fragment, quit 
	exit 0
   fi
fi


if `$doElanPSInclude`
then	cat > /tmp/dtm.$$.psinc <<\!
'\" te
...\" $Header: /usr/src/docbook-to-man/cmd/RCS/docbook-to-man.sh,v 1.3 1996/06/17 03:36:49 fld Exp $
...\"
...\"	transcript compatibility for postscript use.
...\"
...\"	synopsis:  .P! <file.ps>
...\"
.de P!
\\&.
.fl			\" force out current output buffer
\\!%PB
\\!/showpage{}def
...\" the following is from Ken Flowers -- it prevents dictionary overflows
\\!/tempdict 200 dict def tempdict begin
.fl			\" prolog
.sy cat \\$1\" bring in postscript file
...\" the following line matches the tempdict above
\\!end % tempdict %
\\!PE
\\!.
.sp \\$2u	\" move below the image
..
!
else	cat > /tmp/dtm.$$.psinc <<\!
'\" te
...\" $Header: /usr/src/docbook-to-man/cmd/RCS/docbook-to-man.sh,v 1.3 1996/06/17 03:36:49 fld Exp $
...\"
...\"	transcript compatibility for postscript use.
...\"
...\"	synopsis:  .P! <file.ps>
...\"
.de P!
.fl
\!!1 setgray
.fl
\\&.\"
.fl
\!!0 setgray
.fl			\" force out current output buffer
\!!save /psv exch def currentpoint translate 0 0 moveto
\!!/showpage{}def
.fl			\" prolog
.sy sed -e 's/^/!/' \\$1\" bring in postscript file
\!!psv restore
.
!
fi

cat >> /tmp/dtm.$$.psinc <<\!
.de pF
.ie     \\*(f1 .ds f1 \\n(.f
.el .ie \\*(f2 .ds f2 \\n(.f
.el .ie \\*(f3 .ds f3 \\n(.f
.el .ie \\*(f4 .ds f4 \\n(.f
.el .tm ? font overflow
.ft \\$1
..
.de fP
.ie     !\\*(f4 \{\
.	ft \\*(f4
.	ds f4\"
'	br \}
.el .ie !\\*(f3 \{\
.	ft \\*(f3
.	ds f3\"
'	br \}
.el .ie !\\*(f2 \{\
.	ft \\*(f2
.	ds f2\"
'	br \}
.el .ie !\\*(f1 \{\
.	ft \\*(f1
.	ds f1\"
'	br \}
.el .tm ? font underflow
..
.ds f1\"
.ds f2\"
.ds f3\"
.ds f4\"
!


#if [ ! -f $PROLOG ]
#then	cat > $PROLOG <<!
#<!DOCTYPE RefEntry PUBLIC "-//Davenport//DTD DocBook V2.4.1//EN" [
#<!ENTITY npzwc "">
#]>
#!
#fi
#DTD=/usr/share/lib/sgml/locale/C/dtd/solbook.dtd
#(cat /tmp/dtm.$$.psinc;
cat > /tmp/dtp.$$.preprocess <<!
'\" te
!
( cat /tmp/dtp.$$.preprocess;
 $PARSER -gl -m$CATALOG $DTD $INSTANCE 2>/dev/null|
	$INSTANT $INSTANT_OPT -c$TPTFILES/roff.cmap -s$TPTFILES/roff.sdata -t$TPTFILES/docbook-to-man.ts)

rm -f /tmp/dtm.$$.psinc    
