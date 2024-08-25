/*******************************************************************************
Project: Watchdog
Name: Netanel shmuel
Reviewer: Shahar 
Date: 14.1.2024
Version: 1.0
*******************************************************************************/

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <stddef.h> /* size_t */


/*
	Description: Run a watchdog to keep track of your process for not crashing,
				 if your process crashes then the watchdog will re-execute the your process,
				 making sure it keeps running.

	Input:  argv - contain also the name of your executable file
			interval - send signals every interval seconds
			max_failures_num_ - watchdog will re-execute your program after max_failures_num_ passed with no received signal

	Return: 0 - success, otherwise - specific fail status

	Notes:	1. you can use this function only on multi-threaded machine
			2. you can't call this function several times in a row
	 		3. in theis function we create process for execute run_watchdog.out - make sure that you doesn't
	       	   change his name or his path.
	        4. after calling this function you can't use SIGUSR1 and SIGUSR2
	        5. if you didn't call StopWD after you call this function, it will lead to undefined behavior

*/
int MakeMeImmortal(int argc, char *argv[], size_t interval_, size_t max_failures_num_);


/*
	Description: Stop the watchdog from watching your program, and clean up all resources

	Return: 0 - success, otherwise - specific fail status

	Notes:	1. you can use this function only on multi-threaded machine
			2. you can't call this function several times in a row
			3. after you call this function you can use SIGUSR1 and SIGUSR2
*/
int StopWD(void);



#endif /*__WATCHDOG_H__*/