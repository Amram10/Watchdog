#define _POSIX_C_SOURCE 200112L

#include <stddef.h>     /* size_t */
#include <assert.h>     /* assert */
#include <signal.h>     /* signal handling */
#include <semaphore.h>  /* semaphore operations */
#include <unistd.h>     /* pid_t */
#include <fcntl.h>      /* O_CREAT O_EXCL */

#include "keepwatching.c"


#define SIZE 100

const char *variable_name = "WD_PID";

int main(int argc, char *argv[])
{
    sem_t *wd_sem;
    char WD_PID[10];
    struct sigaction sa;
    int i = 0;

    /*sem init*/
    wd_sem = sem_open(variable_name, O_CREAT | O_EXCL, 0644, 0);
    if (wd_sem == SEM_FAILED)
    {
        /* If the semaphore already exists, open it */
        wd_sem = sem_open(variable_name, 0);
        if (wd_sem == SEM_FAILED)
        {
            /*return if failed*/
            fprintf(stderr,"sem_open");
            exit(1);
        }
    }

    /*allocate saved args for client*/
    wd_data.argv = (char**)malloc((argc - 5)*sizeof(char*));

    /*saved args for client*/
    for(i = 0; i < argc - 5; ++i)
    {
        wd_data.argv[i] = (char*)malloc(sizeof(char) * SIZE);
        wd_data.argv[i] = argv[i+5];
    }

    /* update WD_PID */
    sprintf(WD_PID, "%d", getpid());
    
    /*set enviorment var*/
    if (0 != setenv(variable_name, WD_PID, 1)) 
    {
        /*if failed return failed*/
        exit(1);
    }


    if (-1 == sem_post(wd_sem))
    {
        /*if failed return failed*/
        exit(1);
    }
    sem_close(wd_sem);

    /*handler for sigusr1*/
    sa.sa_handler = SigUsr2;

    /*set flags*/
    wd_data.stop_wd_flag = 0;

    /*set sigaction*/
    sigaction(SIGUSR2, &sa, NULL);

    printf( "in run watchdog my name is: %s\n",argv[0] );
    /* Call the function to keep watching */
    KeepWatching(argv[1],argv[0],(pid_t)(atoi(argv[2])),(size_t)(atoi(argv[3])),(size_t)(atoi(argv[4])));

    return 0;
}