#include <stdio.h>/*printf*/

#include "watchdog.h"

int main(int argc, char *argv[])
{
	/*ret is for the return status from MMI and StopWD*/
	int ret = 0;
	
	/*printf("My argv[1] - %s",argv[1]);*/
	ret = MakeMeImmortal(argc,argv, 3, 5);
/*Here need to be the critical code part*/
	sleep(10);

	ret = StopWD();
	return ret;
}