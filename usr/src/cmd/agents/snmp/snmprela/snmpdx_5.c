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


struct CallbackItem genCallItem[10];
int genNumCallItem=0;
int genTrapTableMap[10];
int genNumTrapElem=0;
struct TrapHndlCxt genTrapBucket[10];
