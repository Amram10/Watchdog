#define _POSIX_C_SOURCE 200112L

#include <stddef.h>     /* size_t */
#include <assert.h>     /* assert */
#include <semaphore.h>  /* sem_t, sem_open, sem_wait, sem_post */
#include <signal.h>     /* sigaction, sigemptyset, sigaddset, sigprocmask */
#include <unistd.h>     /* fork, execv, getpid, kill */
#include <sys/wait.h>   /* waitpid */
#include <time.h>       /* clock_gettime, timespec */
#include <stdio.h>      /* fprintf */
#include <stdlib.h>     /* exit setenv*/
#include <fcntl.h>      /* O_CREAT O_EXCL */
#include <pthread.h>    /* pthread_create pthread_join */
#include <string.h>     /* strcpy */


#include "watchdog.h"
#include "scheduler.h"
#include "keep_watching.h"

#define PARAMSTY_FIELDS_NUM 4
#define THREAD_ARGS 5
#define NAME_LEN 30

typedef struct
{
    char program_name[NAME_LEN]; 
    char p_to_launch[NAME_LEN];
    char **argv;
    int stop_wd_flag;
    int argc;
    pid_t pid;
    size_t interval;
    size_t max_counter;
    sig_atomic_t counter;
    sched_ty *wd_sched;
} WD_client_data_ty;

sem_t *wd_sem;
pid_t pid;
char *variable_name = "WD_PID";
char **args = NULL;
char *threads_args[THREAD_ARGS];
pthread_t watchdog_thread;
size_t num_args = 0;
WD_client_data_ty wd_data;

void *KeepWatchingWrapper(void *args_);
void FreeAllocations(char *args[], size_t len);
int AllocateArgs(size_t len, char **args_);
static int UpdateArgs(char *argv[], int argc, size_t interval_, size_t max_signals_until_crash_);



int MakeMeImmortal(int argc, char *argv[], size_t interval_, size_t max_failures_num_)
{
    sigset_t mask;
    struct timespec time;
    char *env_value = NULL;
    size_t i = 0;
    char char_pid[10];
   
    /*update args*/
    if(1 == UpdateArgs(argv,argc,interval_,max_failures_num_))
    {
        return 1;
    }

	/*do mask thread to don't get the sigusr*/
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        perror("sigprocmask");
        exit(1);
    }


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
	
    /* check WD_PID */
    env_value = getenv(variable_name);
    printf("env value: %s\n",env_value );
    if (env_value == NULL)
    {
        /* create watchdog process by fork */
        pid = fork();

        if (pid < 0)
        {
            /*sem close*/
            sem_close(wd_sem);
            perror("fork");
            exit(1);
        }
        /* child process */
        else if (pid == 0)
        {
            /*execv argoments*/
            execv(args[0], args);
        }
        else
        {
            /*casting pid into string*/
            sprintf(char_pid,"%d",pid);
            /*set environment variable*/
        }
        /*var_time to keep current time + a few secs*/
        clock_gettime(CLOCK_REALTIME, &time);
        time.tv_sec += 4;
        /* sem_time_wait(sem_nem,var_time) */
        sem_timedwait(wd_sem, &time);
        
        printf("Watchdog!!\n");
    }
    else
    {
        printf("Relaunch Client:\n");
        pid = atoi(env_value);
    }

    /*allocate args*/
    if(1 == AllocateArgs(THREAD_ARGS,threads_args))
    {
        /*return malloc error*/
        return 1;
    }
    /*update threads_args*/
    strcpy(threads_args[0],"run_watchdog");
    strcpy(threads_args[1],argv[0]);
    sprintf(threads_args[2], "%d",pid);
    sprintf(threads_args[3], "%lu",interval_);
    sprintf(threads_args[4], "%lu",max_failures_num_);
    printf("here\n");

    /* Create the watchdog thread */
    if (pthread_create(&watchdog_thread, NULL, KeepWatchingWrapper, (void *)threads_args)!= 0)
    {
        perror("pthread_create");
        printf("thread Failed\n");
        exit(1);
    }

	/*return status*/
    return 0;

}

int StopWD(void)
{
    int status= 0;
    size_t i = 0;
	/*sent to watchdog SIGUSR2*/

    if (kill(pid, SIGUSR2) == -1)
    {
        perror("Failed to send SIGUSR2");
        exit(1);
    }
    /*need to check*/
    /* Wait for the watchdog thread to finish */
    waitpid(pid, &status,0);

    SchedStop(wd_data.wd_sched);

    if (pthread_join(watchdog_thread, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    /*free args*/
    FreeAllocations(args,num_args);
    free(args);
    FreeAllocations(threads_args,THREAD_ARGS);

    SchedDestroy(wd_data.wd_sched);

	/*return status*/
	return 0;
}

void *KeepWatchingWrapper(void *args_)
{
    char **args = args_;
    printf("in run keepwraper: argv [0]: %s\n",args[0]);
    printf("in run keepwraper: argv [1]: %s\n",args[1]);
    printf("in run keepwraper: argv [2]: %s\n",args[2]);
    printf("in run keepwraper: argv [3]: %s\n",args[3]);
    printf("in run keepwraper: argv [4]: %s\n",args[4]);
    KeepWatching(args[0],args[1],(pid_t)(atoi(args[2])),(size_t)(atoi(args[3])),(size_t)(atoi(args[4])));

    return NULL;
}

static int UpdateArgs(char *argv[], int argc, size_t interval_, size_t max_signals_until_crash_)
{
    size_t i = 0;
    size_t num_args = 0;

    num_args = 6 + argc;

    args = (char **)malloc(num_args * sizeof(char *));

    /*allocate args*/
    if(1 == AllocateArgs(num_args,args))
    {
        return 1;
    }

    /*update args*/
    strcpy(args[0],"run_watchdog");
    strcpy(args[1],argv[0]);
    sprintf(args[2], "%d", getpid());
    sprintf(args[3], "%lu", interval_);
    sprintf(args[4], "%lu", max_signals_until_crash_);

    /*copy argv*/
    for(i=5;i<num_args-1;++i)
    {
        args[i] = argv[i-5];
    }
    args[i] = NULL;

    /*allocation for saving args for client*/
    wd_data.argv = (char **)malloc((num_args - 6) * sizeof(char *));

    /*init args*/
    for(i = 0; i < num_args - 6; ++i)
    {
        wd_data.argv[i] = (char*)malloc(sizeof(char) * 100);
        wd_data.argv[i] = argv[i];
    }

    /*saved number of args*/
    wd_data.argc = num_args;

    return 0;
}

int AllocateArgs(size_t len, char **args_)
{
    size_t i = 0;
    size_t j = 0;

    /*allocate args_*/
    for(i = 0; i < len; ++i)
    {
        /*malloc args_[i]*/
        args_[i] = (char*)malloc(sizeof(char)*100);

        /*check if failed*/
        if(NULL == args_[i])
        {
            /*free all alocated already*/
            FreeAllocations(args_,i);

            return 1;
        }
    }

    return 0;
}

void FreeAllocations(char *args[], size_t len)
{
    size_t i = 0;

    /* Free allocated memory*/
    for(i = 0; i < len; ++i)
    {
        free(args[i]);
        args[i] = NULL;
    }
}