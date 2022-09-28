
#include "error.h"


static void application_end()
{
}


main(int agrc, char *argv[])
{
	error_init(argv[0], application_end);

	error("first message");
	error("tata\n");
	error("\n");
	error("titi\n");
	error("\n\n");
	error("toto\n\n\n");
	error("\n\n\n");

	error_open("/tmp/error_test.log");

	error("second message");
	error("tata\n");
	error("\n");
	error("titi\n");
	error("\n\n");
	error("toto\n\n\n");
	error("\n\n\n");

/*
	error_close_stderr();
*/

	error("third message");
	error("tata\n");
	error("\n");
	error("titi\n");
	error("\n\n");
	error("toto\n\n\n");
	error("\n\n\n");

	error_exit("end of this sample program");
}
