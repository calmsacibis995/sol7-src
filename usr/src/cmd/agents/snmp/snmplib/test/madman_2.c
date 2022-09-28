#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "error.h"
#include "trace.h"
#include "snmp.h"
#include "madman_trap.h"



#define BUF_SZ		1000


static Subid agents_subids[] = { 1, 3, 6, 1, 4, 1, 42, 2, 8, 1 };
static Oid sysObjectID_value = { agents_subids, 10 };


/******************************************************************/

void test1()
{
	char buffer[BUF_SZ + 1];
	int i, j;


	for(i = 500; i < BUF_SZ; i++)
	{
		for(j = 0; j < i; j++)
		{
			buffer[j] = 'a';
		}
		buffer[i] = '\0';

		send_trap_appl_alarm(99, "toto application name",
			i, SEVERITY_LOW, buffer);
	}
}


/******************************************************************/

void test2()
{
	int i;


	for(i = 0; i < 10000; i++)
	{
		fprintf(stderr, "%d\n", i);
		send_trap_appl_alarm(1, "Solstice X.400 MTA:",
			i, SEVERITY_LOW, "Just a test message");
	}
}


/******************************************************************/

void test3()
{
	send_trap_appl_alarm(1, "Solstice X.400 MTA:",
		1, SEVERITY_LOW, "Just a test message: LOW");
	sleep(5);

	send_trap_appl_alarm(1, "Solstice X.400 MTA:",
		2, SEVERITY_MEDIUM, "Just a test message: MEDIUM");
	sleep(5);

	send_trap_appl_alarm(1, "Solstice X.400 MTA:",
		3, SEVERITY_HIGH, "Just a test message: HIGH");
}


/******************************************************************/

main()
{
/*
	trace_flags = 0xFFF;
*/

	if(trap_init(&sysObjectID_value, error_label))
	{
		fprintf(stderr, "trap_init() failed: %s\n", error_label);
		exit(1);
	}

	if(trap_destinator_add("panda", error_label))
	{
		fprintf(stderr, "trap_destinator_add() failed: %s\n",
			error_label);
		exit(1);
	}

	test3();

	exit(0);
}
