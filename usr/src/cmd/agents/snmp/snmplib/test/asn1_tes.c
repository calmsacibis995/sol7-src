#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "asn1.h"
#include "error.h"
#include "pdu.h"


#define BUFFER_SZ	10

static u_char static_buffer[] = { 0x41, 0x03, 0xF5, 0x1E, 0x5C };

static void second_test(long value, u_char asn_type);
static void third_test();


main()
{
	u_char *p;
	int len;
	long integer = 0;
	u_char type = 0;


	/* 1st test */

	len = sizeof(static_buffer);
	integer = 0;
	type = 0;
	p = asn_parse_int(static_buffer, &len, &type, &integer, sizeof(long), error_label);
	if(p == NULL)
	{
		fprintf(stderr, "asn_parse_int() failed: %s\n", error_label);
		exit(1);
	}
	printf("type:    0x%x\n", type);
	printf("integer: %ld\n", integer);
	printf("\n");


	/* 2nd test */

	second_test(0xFF, COUNTER);
	second_test(-0xFF, COUNTER);

	second_test(0xFFFF, COUNTER);
	second_test(-0xFFFF, COUNTER);

	second_test(0xFFFFFF, COUNTER);
	second_test(-0xFFFFFF, COUNTER);

	second_test(16523569, COUNTER);
	second_test(-1363058786, COUNTER);


	/* 3rd test */

	third_test();


	exit(0);

}


static void second_test(long value, u_char asn_type)
{
	u_char *p;
	int len;
	long integer = 0;
	u_char type = 0;
	u_char buffer[BUFFER_SZ];
	int i;


	printf("VALUE: %ld - TYPE: 0x%x\n\n", value, asn_type);

	integer = value;
	type = asn_type;
	memset(buffer, 0, sizeof(buffer));
	len = BUFFER_SZ;
	p = asn_build_int(buffer, &len, type, &integer, sizeof(long), error_label);
	if(p == NULL)
	{
		fprintf(stderr, "asn_build_int() failed: %s\n", error_label);
		exit(1);
	}
	printf("len:     %d\n", len);
	printf("buffer: ");
	for(i = 0; i < BUFFER_SZ; i++)
	{
		printf(" %02x", buffer[i]);
	}
	printf("\n");

	integer = 0;
	type = 0;
	len = BUFFER_SZ;
	p = asn_parse_int(buffer, &len, &type, &integer, sizeof(long), error_label);
	if(p == NULL)
	{
		fprintf(stderr, "asn_parse_int() failed: %s\n", error_label);
		exit(1);
	}
	printf("type:    0x%x\n", type);
	printf("integer: %ld\n", integer);
	printf("\n");
}


static void third_test()
{
	SNMP_pdu *pdu;

	pdu = snmp_pdu_new(error_label);
}
