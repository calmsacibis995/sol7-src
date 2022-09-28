-- Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
--pragma ident  "@(#)mib.asn.solmgr	2.16 96/07/23 Sun Microsystems"

                RFC1066-MIB { iso org(3) dod(6) internet(1) mgmt(2) 1 }

--***************************************************************************
--*     Copyright (c) 1988, 1989  Epilogue Technology Corporation
--*     All rights reserved.
--*
--*     This is unpublished proprietary source code of Epilogue Technology
--*     Corporation.
--*
--*     The copyright notice above does not evidence any actual or intended
--*     publication of such source code.
--***************************************************************************

-- $Header:   D:/snmpv2/agent/sun/mib.asv   2.2   20 Jun 1990 18:01:00  $

		FORCE-INCLUDE <stdio.h>
		FORCE-INCLUDE <sys/types.h>
		FORCE-INCLUDE <sys/socket.h>
		FORCE-INCLUDE <netinet/in.h>
		FORCE-INCLUDE <netinet/in_systm.h>
		FORCE-INCLUDE <netinet/ip.h>
		FORCE-INCLUDE <netinet/ip_icmp.h>
		FORCE-INCLUDE <net/if.h>
		FORCE-INCLUDE <netinet/if_ether.h>
		FORCE-INCLUDE <asn1.h>
		FORCE-INCLUDE <mib.h>
		FORCE-INCLUDE "general.h"
		FORCE-INCLUDE "snmp.h"
		FORCE-INCLUDE "snmpvars.h"

		-- MIB file for Sun OS 4.x
		-- set up MIB-wide defaults
		-- all functions can use %n for object name and
		-- %t for object type, %p for parent name,
		-- %d for name of default-bearing node and %% for %
		DEFAULT test-function	it_exists
		DEFAULT	set-function	set_%n
		DEFAULT	get-function	get_%n
		DEFAULT	next-function	std_next
		DEFAULT cookie		(char *)NULL

-- There are four view definitions:
-- 0x01 is a read-only view of the system group
-- 0x02 is a read-write view of the system group
-- 0x04 is a read-only view of the entire MIB
-- 0x08 is a read-write view of the entire MIB
-- For simplicity the defaults are set to cover the bulk of the MIB,
--   i.e. everything outside the system group.
		DEFAULT view-mask	0x0C
		DEFAULT write-mask	0x08

                DEFINITIONS ::= BEGIN

                IMPORTS
                        mgmt, OBJECT-TYPE, NetworkAddress, IpAddress,
                        Counter, Gauge, TimeTicks
                            FROM RFC1065-SMI;

                  mib        OBJECT IDENTIFIER ::= { mgmt 1 }

		  private    OBJECT IDENTIFIER ::= { internet 4 }
 	          enterprises   OBJECT IDENTIFIER ::= { private 1 }

                  system     OBJECT IDENTIFIER ::= { mib 1 }
                  interfaces OBJECT IDENTIFIER ::= { mib 2 }
                  at         OBJECT IDENTIFIER ::= { mib 3 }
                  ip         OBJECT IDENTIFIER ::= { mib 4 }
                  icmp       OBJECT IDENTIFIER ::= { mib 5 }
                  tcp        OBJECT IDENTIFIER ::= { mib 6 }
                  udp        OBJECT IDENTIFIER ::= { mib 7 }
-- no egp, thank you! we're a host not a router...
--                egp        OBJECT IDENTIFIER ::= { mib 8 }
	          snmp       OBJECT IDENTIFIER ::= { mib 11 }

                  -- object types

                  -- the System group

                  sysDescr OBJECT-TYPE
                          SYNTAX  DisplayString (SIZE (0..255))
                          ACCESS  read-only
                          STATUS  mandatory
			  DEFAULT get-function	get_string
			  DEFAULT cookie snmp_sysDescr
			  DEFAULT view-mask	0x0F
			  DEFAULT write-mask	0x0A
                          ::= { system 1 }

               sysObjectID OBJECT-TYPE
                       SYNTAX  OBJECT IDENTIFIER
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function get_object_identifier
		       DEFAULT cookie (char *)&snmp_sysObjectID
		       DEFAULT view-mask	0x0F
		       DEFAULT write-mask	0x0A
                       ::= { system 2 }

                  sysUpTime OBJECT-TYPE
                          SYNTAX  TimeTicks
                          ACCESS  read-only
                          STATUS  mandatory
			  DEFAULT view-mask	0x0F
			  DEFAULT write-mask	0x0A
                          ::= { system 3 }

               sysContact OBJECT-TYPE
                       SYNTAX  DisplayString (SIZE (0..255))
                       ACCESS  read-write
                       STATUS  mandatory
		       DEFAULT get-function get_string
		       DEFAULT set-function set_sysContact
		       DEFAULT cookie (char *)snmp_sysContact
		       DEFAULT view-mask	0x0F
		       DEFAULT write-mask	0x0A
                       ::= { system 4 }

               sysName OBJECT-TYPE
                       SYNTAX  DisplayString (SIZE (0..255))
                       -- ACCESS  read-write
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT set-function null_set_proc
		       DEFAULT get-function get_sysName
		       DEFAULT view-mask	0x0F
		       DEFAULT write-mask	0x0A
                       ::= { system 5 }

               sysLocation OBJECT-TYPE
                       SYNTAX  DisplayString (SIZE (0..255))
                       -- ACCESS  read-only
                       ACCESS  read-write
                       STATUS  mandatory
		       DEFAULT get-function get_string
		       DEFAULT set-function set_sysLocation
		       DEFAULT cookie (char *)snmp_sysLocation
		       DEFAULT view-mask	0x0F
		       DEFAULT write-mask	0x0A
                       ::= { system 6 }

               sysServices OBJECT-TYPE
                       SYNTAX  INTEGER (0..127)
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function get_cookie
		       -- Services are:
		       --	internet (e.g., IP gateways), but
		       --		only if ipforwarding is set.
		       --	end-to-end
		       --	applications
		       DEFAULT cookie (char *)0x48
		       DEFAULT view-mask	0x0F
		        DEFAULT write-mask	0x0A
                       ::= { system 7 }

              -- the SNMP group

               snmpInPkts OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInPkts
                       ::=  { snmp 1 }

               snmpOutPkts OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutPkts
                       ::=  { snmp 2 }

               snmpInBadVersions OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInBadVersions
                       ::=  { snmp 3 }

               snmpInBadCommunityNames OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInBadCommunityNames
                       ::=  { snmp 4 }

               snmpInBadCommunityUses OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInBadCommunityUses
                       ::=  { snmp 5 }

               snmpInASNParseErrs OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInASNParseErrs
                       ::=  { snmp 6 }


               snmpInTooBigs OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInTooBigs
                       ::=  { snmp 8 }

               snmpInNoSuchNames OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInNoSuchNames
                       ::=  { snmp 9 }

               snmpInBadValues OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInBadValues
                       ::=  { snmp 10 }

               snmpInReadOnlys OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInReadOnlys
                       ::=  { snmp 11 }

               snmpInGenErrs OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInGenErrs
                       ::=  { snmp 12 }

               snmpInTotalReqVars OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInTotalReqVars
                       ::=  { snmp 13 }

               snmpInTotalSetVars OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInTotalSetVars
                       ::=  { snmp 14 }

               snmpInGetRequests OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInGetRequests
                       ::=  { snmp 15 }

               snmpInGetNexts OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInGetNexts
                       ::=  { snmp 16 }

               snmpInSetRequests OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInSetRequests
                       ::=  { snmp 17 }

               snmpInGetResponses OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInGetResponses
                       ::=  { snmp 18 }

               snmpInTraps OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpInTraps
                       ::=  { snmp 19 }

               snmpOutTooBigs OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutTooBigs
                       ::=  { snmp 20 }

               snmpOutNoSuchNames OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutNoSuchNames
                       ::=  { snmp 21 }

               snmpOutBadValues OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutBadValues
                       ::=  { snmp 22 }

               snmpOutGenErrs OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutGenErrs
                       ::=  { snmp 24 }

               snmpOutGetRequests OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutGetRequests
                       ::=  { snmp 25 }

               snmpOutGetNexts OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutGetNexts
                       ::=  { snmp 26 }

               snmpOutSetRequests OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutSetRequests
                       ::=  { snmp 27 }

               snmpOutGetResponses OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutGetResponses
                       ::=  { snmp 28 }

               snmpOutTraps OBJECT-TYPE
                       SYNTAX  Counter
                       ACCESS  read-only
                       STATUS  mandatory
		       DEFAULT get-function	get_ulong
		       DEFAULT cookie	(char *)&snmp_stats.snmpOutTraps
                       ::=  { snmp 29 }

	       snmpEnableAuthenTraps OBJECT-TYPE
                       SYNTAX  INTEGER {
                                   enabled(1),
                                   disabled(2)
                               }
                       ACCESS  read-write
                       STATUS  mandatory
		       DEFAULT get-function	get_snmpEnableAuthTraps
		       DEFAULT set-function	set_snmpEnableAuthTraps
		       DEFAULT cookie		(char *)0
                       ::=  { snmp 30 }


--  **********************************************************************
--  SUN EXTENSIONS
--  **********************************************************************
		sun		OBJECT IDENTIFIER ::= { enterprises 42 }
		products	OBJECT IDENTIFIER ::= { sun 2 }
		sunMib		OBJECT IDENTIFIER ::= { sun 3 }

		sunSystem	OBJECT IDENTIFIER ::= { sunMib 1 }
		sunInterfaces	OBJECT IDENTIFIER ::= { sunMib 2 }
		sunAt		OBJECT IDENTIFIER ::= { sunMib 3 }
		sunIp		OBJECT IDENTIFIER ::= { sunMib 4 }
		sunIcmp		OBJECT IDENTIFIER ::= { sunMib 5 }
		sunTcp		OBJECT IDENTIFIER ::= { sunMib 6 }
		sunUdp		OBJECT IDENTIFIER ::= { sunMib 7 }
		sunSnmp		OBJECT IDENTIFIER ::= { sunMib 11 }


--  **********************************************************************
--  SUN SYSTEM GROUP
--  **********************************************************************

		agentDescr	OBJECT-TYPE
				SYNTAX	DisplayString (SIZE (0..255))
				ACCESS	read-only
				STATUS	mandatory
				DEFAULT	get-function	get_string
				DEFAULT	cookie __AGNT_DSCR__
				DEFAULT	view-mask	0x0F
				DEFAULT	write-mask	0x0A
			::= { sunSystem 1 }
				
		hostID	OBJECT-TYPE
				SYNTAX  OCTET STRING (SIZE (4))
				ACCESS  read-only
				STATUS	mandatory
				DEFAULT	view-mask	0x0F
				DEFAULT	write-mask	0x0A
			::= { sunSystem 2 }
				
		motd	OBJECT-TYPE
				SYNTAX	DisplayString (SIZE (0..255))
				ACCESS  read-only
				STATUS	mandatory
				DEFAULT	view-mask	0x0F
				DEFAULT	write-mask	0x0A
			::= { sunSystem 3 }
				
                unixTime OBJECT-TYPE
	                        SYNTAX  Counter
        	                ACCESS  read-only
                	        STATUS  mandatory
				DEFAULT	view-mask	0x0F
				DEFAULT	write-mask	0x0A
                       ::= { sunSystem 4 }

               END
