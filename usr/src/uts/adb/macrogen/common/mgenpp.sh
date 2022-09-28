#!/bin/sh
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)mgenpp.sh 1.3 98/02/02 SMI
#

#
#	Usage: mgenpp [C_style_defines] < dbg_script > preprocessed_dbg_script
#
#	This is macrogen's pre-processing script that runs cpp on the
#	input dbg script to pick the right includes and right adb script based
#	on the supplied #if's and #ifdef's. In sequence, this does the
#	following :
#		- protect all non-preprocessor code (comment out - CC style)
#		- protect all #includes (comment out)
#		- protect all #defines (comment out)
#		- protect all empty lines (comment out)
#		- run cpp through the rest (make it retain comments and
#					not produce line number information)
#		- strip off empty lines generated by cpp
#		- uncomment all cpp-protected stuff
#
#
/usr/bin/sed 's/^\([^#].*\)$/\/\/\1/
	s/^#include/\/\/#include/
	s/^#define/\/\/#define/
	s/^$/\/\//' 			| \
    /usr/ccs/lib/cpp $* -B -C -P	| \
    /usr/bin/grep -v '^$'			| \
    /usr/bin/sed    's/^\/\///'