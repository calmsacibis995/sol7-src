#include <sys/types.h>
#include <netinet/in.h>

#include "impl.h"
#include "asn1.h"
#include "error.h"
#include "snmp.h"
#include "trap.h"
#include "pdu.h"
#include "node.h"

#include "snmpdx_stub.h"


/***** GLOBAL VARIABLES *****/

char default_config_file[] = "/etc/snmp/conf/snmpdx.reg";
char default_sec_config_file[] = "/etc/snmp/conf/snmpdx.acl";
char default_error_file[] = "/var/snmp/snmpdxd.log";


/***********************************************************/

void agent_init()
{
}


/***********************************************************/

void agent_end()
{
}


/***********************************************************/

void agent_loop()
{
	int condition=FALSE;

	if(condition==TRUE){
	}
}


/***********************************************************/

void agent_select_info(fd_set *fdset, int *numfds)
{
}


/***********************************************************/

void agent_select_callback(fd_set *fdset)
{
}



