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



int get_relayProcessIDFile(String *relayProcessIDFile)
{
	u_char *str;
	int len;

	/* It is required to allocate memory to the pointers */
	/* inside the input argument */
	/* Here, we assume that "hello" is the value of the mib variable */
	/* please change it to the real one */

	len = strlen("hello");
	str = (u_char*)calloc(len,sizeof(char));
	if(str==NULL){
		return SNMP_ERR_GENERR;
	}
	memcpy(str,"hello",len);

	/*fill in the contents of the argument */

	relayProcessIDFile->chars = str;
	relayProcessIDFile->len = len;
	return SNMP_ERR_NOERROR;
}

int set_relayProcessIDFile(int pass, String *relayProcessIDFile)
{
	char buf[100];

	switch(pass)
	{
		case FIRST_PASS:

			/* check the validity of the input argument */
			/* if not valid, return SNMP_GEN_ERROR */

			return SNMP_ERR_NOERROR;

		case SECOND_PASS:
			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable */

			memcpy(buf,relayProcessIDFile->chars,relayProcessIDFile->len);
			buf[relayProcessIDFile->len+1] = '\0';
			printf("The new value is %s\n",buf);
			return SNMP_ERR_NOERROR;
	}
}


void free_relayProcessIDFile(String *relayProcessIDFile)
{
	 if(relayProcessIDFile->chars!=NULL && relayProcessIDFile->len !=0)
	{
		free(relayProcessIDFile->chars);
		relayProcessIDFile->len = 0;
	}
}

int get_relayResourceFile(String *relayResourceFile)
{
	u_char *str;
	int len;

	/* It is required to allocate memory to the pointers */
	/* inside the input argument */
	/* Here, we assume that "hello" is the value of the mib variable */
	/* please change it to the real one */

	len = strlen("hello");
	str = (u_char*)calloc(len,sizeof(char));
	if(str==NULL){
		return SNMP_ERR_GENERR;
	}
	memcpy(str,"hello",len);

	/*fill in the contents of the argument */

	relayResourceFile->chars = str;
	relayResourceFile->len = len;
	return SNMP_ERR_NOERROR;
}

int set_relayResourceFile(int pass, String *relayResourceFile)
{
	char buf[100];

	switch(pass)
	{
		case FIRST_PASS:

			/* check the validity of the input argument */
			/* if not valid, return SNMP_GEN_ERROR */

			return SNMP_ERR_NOERROR;

		case SECOND_PASS:
			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable */

			memcpy(buf,relayResourceFile->chars,relayResourceFile->len);
			buf[relayResourceFile->len+1] = '\0';
			printf("The new value is %s\n",buf);
			return SNMP_ERR_NOERROR;
	}
}


void free_relayResourceFile(String *relayResourceFile)
{
	 if(relayResourceFile->chars!=NULL && relayResourceFile->len !=0)
	{
		free(relayResourceFile->chars);
		relayResourceFile->len = 0;
	}
}

int get_relayPersonalFileDir(String *relayPersonalFileDir)
{
	u_char *str;
	int len;

	/* It is required to allocate memory to the pointers */
	/* inside the input argument */
	/* Here, we assume that "hello" is the value of the mib variable */
	/* please change it to the real one */

	len = strlen("hello");
	str = (u_char*)calloc(len,sizeof(char));
	if(str==NULL){
		return SNMP_ERR_GENERR;
	}
	memcpy(str,"hello",len);

	/*fill in the contents of the argument */

	relayPersonalFileDir->chars = str;
	relayPersonalFileDir->len = len;
	return SNMP_ERR_NOERROR;
}

int set_relayPersonalFileDir(int pass, String *relayPersonalFileDir)
{
	char buf[100];

	switch(pass)
	{
		case FIRST_PASS:

			/* check the validity of the input argument */
			/* if not valid, return SNMP_GEN_ERROR */

			return SNMP_ERR_NOERROR;

		case SECOND_PASS:
			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable */

			memcpy(buf,relayPersonalFileDir->chars,relayPersonalFileDir->len);
			buf[relayPersonalFileDir->len+1] = '\0';
			printf("The new value is %s\n",buf);
			return SNMP_ERR_NOERROR;
	}
}


void free_relayPersonalFileDir(String *relayPersonalFileDir)
{
	 if(relayPersonalFileDir->chars!=NULL && relayPersonalFileDir->len !=0)
	{
		free(relayPersonalFileDir->chars);
		relayPersonalFileDir->len = 0;
	}
}

int get_relayTrapPort(Integer *relayTrapPort)
{
	/* assume that the mib variable has a value of 1 */

	*relayTrapPort = 1;
	return SNMP_ERR_NOERROR;
}

int get_relayCheckPoint(String *relayCheckPoint)
{
	u_char *str;
	int len;

	/* It is required to allocate memory to the pointers */
	/* inside the input argument */
	/* Here, we assume that "hello" is the value of the mib variable */
	/* please change it to the real one */

	len = strlen("hello");
	str = (u_char*)calloc(len,sizeof(char));
	if(str==NULL){
		return SNMP_ERR_GENERR;
	}
	memcpy(str,"hello",len);

	/*fill in the contents of the argument */

	relayCheckPoint->chars = str;
	relayCheckPoint->len = len;
	return SNMP_ERR_NOERROR;
}

int set_relayCheckPoint(int pass, String *relayCheckPoint)
{
	char buf[100];

	switch(pass)
	{
		case FIRST_PASS:

			/* check the validity of the input argument */
			/* if not valid, return SNMP_GEN_ERROR */

			return SNMP_ERR_NOERROR;

		case SECOND_PASS:
			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable */

			memcpy(buf,relayCheckPoint->chars,relayCheckPoint->len);
			buf[relayCheckPoint->len+1] = '\0';
			printf("The new value is %s\n",buf);
			return SNMP_ERR_NOERROR;
	}
}


void free_relayCheckPoint(String *relayCheckPoint)
{
	 if(relayCheckPoint->chars!=NULL && relayCheckPoint->len !=0)
	{
		free(relayCheckPoint->chars);
		relayCheckPoint->len = 0;
	}
}

int get_relayPollInterval(Integer *relayPollInterval)
{
	/* assume that the mib variable has a value of 1 */

	*relayPollInterval = 1;
	return SNMP_ERR_NOERROR;
}

int get_relayMaxAgentTimeOut(Integer *relayMaxAgentTimeOut)
{
	/* assume that the mib variable has a value of 1 */

	*relayMaxAgentTimeOut = 1;
	return SNMP_ERR_NOERROR;
}

int get_agentTableIndex(Integer *agentTableIndex)
{
	/* assume that the mib variable has a value of 1 */

	*agentTableIndex = 1;
	return SNMP_ERR_NOERROR;
}

int set_agentTableIndex(int pass, Integer *agentTableIndex)
{
	switch(pass)
	{
		case FIRST_PASS:

			/* check the validity of the input argument */
			/* if not valid, return SNMP_GEN_ERROR */

			return SNMP_ERR_NOERROR;

		case SECOND_PASS:
			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable */

			printf("The new value is %d\n",agentTableIndex);
			return SNMP_ERR_NOERROR;
	}
}


int get_regTblTableIndex(Integer *regTblTableIndex)
{
	/* assume that the mib variable has a value of 1 */

	*regTblTableIndex = 1;
	return SNMP_ERR_NOERROR;
}

int get_regTreeTableIndex(Integer *regTreeTableIndex)
{
	/* assume that the mib variable has a value of 1 */

	*regTreeTableIndex = 1;
	return SNMP_ERR_NOERROR;
}
