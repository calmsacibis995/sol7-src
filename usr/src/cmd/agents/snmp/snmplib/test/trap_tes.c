#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "error.h"
#include "trace.h"
#include "snmp.h"
#include "trap.h"


main()
{
	trace_flags = 0xFFF;

	if(trap_destinator_add("panda", error_label))
	{
		fprintf(stderr, "trap_destinator_add() failed: %s\n",
			error_label);
		exit(1);
	}

	if(trap_send_to_all_destinators(NULL, SNMP_TRAP_WARMSTART, 0, NULL, error_label))
	{
		fprintf(stderr, "trap_send_to_alldestinators() failed: %s\n",
			error_label);
		exit(1);
	}

	exit(0);
}
