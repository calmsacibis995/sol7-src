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



/***** regTreeEntry         ********************************/

extern int get_regTreeEntry(int search_type, RegTreeEntry_t **regTreeEntry_data, IndexType *index)
{

	int res;

	*regTreeEntry_data = (RegTreeEntry_t*)calloc(1,sizeof(RegTreeEntry_t));
	if(regTreeEntry_data == NULL) return SNMP_ERR_GENERR;

	res = get_regTreeIndex(
	        search_type,
	        &((*regTreeEntry_data)->regTreeIndex),
	        index);
	if(res != SNMP_ERR_NOERROR) return res;

	res = get_regTreeAgentID(
	        search_type,
	        &((*regTreeEntry_data)->regTreeAgentID),
	        index);
	if(res != SNMP_ERR_NOERROR) return res;

	res = get_regTreeOID(
	        search_type,
	        &((*regTreeEntry_data)->regTreeOID),
	        index);
	if(res != SNMP_ERR_NOERROR) return res;

	res = get_regTreeStatus(
	        search_type,
	        &((*regTreeEntry_data)->regTreeStatus),
	        index);
	if(res != SNMP_ERR_NOERROR) return res;

	 return res;
}


void free_regTreeEntry(RegTreeEntry_t *regTreeEntry)
{
	free_regTreeOID(&(regTreeEntry->regTreeOID));
}

int get_regTreeIndex(int search_type, Integer *regTreeIndex, IndexType *index)
{
	/* In the case, the search_type is FIRST_ENTRY or NEXT_ENTRY */
	/* this function should modify the index argument to the */
	/* appropriate value */
	switch(search_type)
	{
		case FIRST_ENTRY:
			if(index->type == INTEGER){

				/* assume 1 is the first index */

				index->value[0] = 1;
				index->len = 1;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[0] = 1;
				index->value[1]= 1;
				index->len = 2;
			}
			break;

		case NEXT_ENTRY:
			if(index->type == INTEGER){
				index->value[0]++;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[index->len-1]++;
			}
			break;

		case EXACT_ENTRY:
			break;
	}

	/*assume that the mib variable has a value of 1 */

	*regTreeIndex = 1;
	return SNMP_ERR_NOERROR;
}

int get_regTreeAgentID(int search_type, Integer *regTreeAgentID, IndexType *index)
{
	/* In the case, the search_type is FIRST_ENTRY or NEXT_ENTRY */
	/* this function should modify the index argument to the */
	/* appropriate value */
	switch(search_type)
	{
		case FIRST_ENTRY:
			if(index->type == INTEGER){

				/* assume 1 is the first index */

				index->value[0] = 1;
				index->len = 1;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[0] = 1;
				index->value[1]= 1;
				index->len = 2;
			}
			break;

		case NEXT_ENTRY:
			if(index->type == INTEGER){
				index->value[0]++;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[index->len-1]++;
			}
			break;

		case EXACT_ENTRY:
			break;
	}

	/*assume that the mib variable has a value of 1 */

	*regTreeAgentID = 1;
	return SNMP_ERR_NOERROR;
}

int get_regTreeOID(int search_type, Oid *regTreeOID, IndexType *index)
{
	Subid *sub;
	Subid fake_sub[] = {1,3,6,1,4,1,4,42};
	int len;

	/* In the case, the search_type is FIRST_ENTRY or NEXT_ENTRY */
	/* this function should modify the index argument to the */
	/* appropriate value */
	switch(search_type)
	{
		case FIRST_ENTRY:
			if(index->type == INTEGER){

				/* assume 1 is the first index */

				index->value[0] = 1;
				index->len = 1;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[0] = 1;
				index->value[1]= 1;
				index->len = 2;
			}
			break;

		case NEXT_ENTRY:
			if(index->type == INTEGER){
				index->value[0]++;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[index->len-1]++;
			}
			break;

		case EXACT_ENTRY:
			break;
	}

	/* It is required to allocate memory to the pointers */
	/* inside the input argument */
	/* Here, we assume that "1.3.6.1.4.1.42" is the value */
	/* of the mib variable */
	/* please change it to the real one */

	/* 1.3.6.1.4.1.42 has 7 number separated by "." */

	len =7 ;
	sub = (Subid*)calloc(len,sizeof(Subid));
	if(sub==NULL) return SNMP_ERR_GENERR;
	memcpy(sub,fake_sub,len*sizeof(Subid));

	/* fill in the contents of the argument */

	regTreeOID->subids = sub;
	regTreeOID->len = len;
	return SNMP_ERR_NOERROR;
}

int set_regTreeOID(int pass, IndexType index, Oid *regTreeOID)
{
	switch(pass)
	{
		case FIRST_PASS:

			/* check the existence of the element which */
			/* corresponding to the given index and */
			/* check the validity fo the input value */
			/* if not valid or not exist, */

			return SNMP_ERR_GENERR;

		case SECOND_PASS:

			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable of the given */
			/* index */
			printf("The new value is %s\n",SSAOidString(regTreeOID));
			return SNMP_ERR_NOERROR;
	}
}


void free_regTreeOID(Oid *regTreeOID)
{
	 if(regTreeOID->subids!=NULL && regTreeOID->len !=0)
	{
		free(regTreeOID->subids);
		regTreeOID->len = 0;
	}
}

int get_regTreeStatus(int search_type, Integer *regTreeStatus, IndexType *index)
{
	/* In the case, the search_type is FIRST_ENTRY or NEXT_ENTRY */
	/* this function should modify the index argument to the */
	/* appropriate value */
	switch(search_type)
	{
		case FIRST_ENTRY:
			if(index->type == INTEGER){

				/* assume 1 is the first index */

				index->value[0] = 1;
				index->len = 1;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[0] = 1;
				index->value[1]= 1;
				index->len = 2;
			}
			break;

		case NEXT_ENTRY:
			if(index->type == INTEGER){
				index->value[0]++;
			}else{

				/* index type will be array of integer */
				/* assume that there are two index */

				index->value[index->len-1]++;
			}
			break;

		case EXACT_ENTRY:
			break;
	}

	/*assume that the mib variable has a value of 1 */

	*regTreeStatus = 1;
	return SNMP_ERR_NOERROR;
}

int set_regTreeStatus(int pass, IndexType index, Integer *regTreeStatus)
{
	switch(pass)
	{
		case FIRST_PASS:

			/* check the existence of the element which */
			/* corresponding to the given index and */
			/* check the validity fo the input value */
			/* if not valid or not exist, */

			return SNMP_ERR_GENERR;

		case SECOND_PASS:

			/* change the following coding, such that */
			/* the input value will be stored in the */
			/* corresponding mib variable of the given */
			/* index */
			printf("The new value is %d\n",regTreeStatus);
			return SNMP_ERR_NOERROR;
	}
}

