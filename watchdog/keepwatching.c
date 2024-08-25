
#include <stddef.h>    /* size_t */
#include <unistd.h>    /* pid_t */
#include <assert.h>    /* assert */
#include <stdio.h>     /* fprintf, stderr */
#include <stdlib.h>    /* exit */
#include <semaphore.h> /*sem_t, sem_open, sem_wait, sem_post*/
#include <signal.h>    /* signal, SIGUSR1, SIGUSR2 */
#include <sys/wait.h>  /*fork exec*/
#include <string.h>    /*strcpy*/
#include <sys/select.h>/*sigset_t*/ 


#include "scheduler.h"
#include "keep_watching.h"

#define WD_NAME "/wd_sem"

enum erorr
{
	SUCCESS = 0,
	SCEDULER_FAILED,
	FAILED_BLOCK_SIG,
	WD_EXIT_FAILURE,
    NAME_LEN = 30
};

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
} WD_data_ty;

WD_data_ty wd_data;
char **wd_t_arg = NULL;

void SigUsr1(int signo);
void SigUsr2(int signo);
int SchedAddSig(void* wd_data);
int SchedCheckCounter(void* param);
void Revive();
int CleanUp(sched_ty *scheduler);
void InitParams(char *to_launch_, char *my_name_, pid_t pid_, size_t interval_, size_t max_failures_);
void ParamsToArgs();
void KeepWatching(char *to_launch_, char *my_name_, pid_t pid_, size_t interval_, size_t max_failures_);

    
void KeepWatching(char *to_launch_, char *my_name_, pid_t pid_, size_t interval_, size_t max_failures_)
{
	sigset_t mask;
    struct sigaction sa;

    /* unmask to get sigusr1/2 */
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    printf("pid in :%d\n",pid_);
    if (pthread_sigmask(SIG_UNBLOCK, &mask, NULL) != 0)
    {
        fprintf(stderr,"Failed to unblock signals");
        exit(FAILED_BLOCK_SIG);
    }

    sa.sa_handler = SigUsr1;

    sa.sa_flags = 0;

    sa.sa_mask = mask;

    sigaction(SIGUSR1, &sa, NULL);
	
    /*update fields*/
    InitParams( to_launch_, my_name_,pid_,  interval_, max_failures_);

	/*sched create*/
	wd_data.wd_sched = SchedCreate();
	/*return if failed*/
	if(NULL == wd_data.wd_sched)
	{
		fprintf(stderr, "Sched allocation failed\n");
		exit(SCEDULER_FAILED);
	}

	/*scheduler add tasks*/
    SchedAddTask(wd_data.wd_sched, SchedAddSig, NULL, 1);
    SchedAddTask(wd_data.wd_sched, SchedCheckCounter, NULL, max_failures_);

    /* sched run */
    SchedRun(wd_data.wd_sched);
}

int SchedAddSig(void* params)
{
    (void)params;
   printf("im :%d send to: %d \n",getpid(),wd_data.pid); 
    /* send signal */
    kill(wd_data.pid, SIGUSR1);

    /* Increase counter-atomic */
    __sync_fetch_and_add(&wd_data.counter, 1);

    /* Return 1 to keep the task alive */
    return 1;
}

int SchedCheckCounter(void *param)
{
	(void)param;
    printf("Im counting\n");
	/* check counter > max && stop_flag */
    if ((size_t)wd_data.counter >= wd_data.max_counter)
    {    
       
        printf(" counter :%d , max_counter: %lu\n",wd_data.counter, wd_data.max_counter);

        /* Reset the counter to 0 */
        __sync_lock_test_and_set(&wd_data.counter, 0);

        Revive();
    }

    if (wd_data.stop_wd_flag)
    {
        CleanUp(wd_data.wd_sched);
    }

    /* return 1 to keep the task alive */
    return 1;
}

void SigUsr1(int signo)
{
    printf("i am %d resiving from %d\n", getpid(),wd_data.pid);
	/*count = 0- atomic*/

	__sync_lock_test_and_set(&wd_data.counter, 0);
    
}

void SigUsr2(int signo)
{
	/*set flag to stop watchdog*/
    __sync_lock_test_and_set(&wd_data.stop_wd_flag, 1);
}

void Revive()
{
    pid_t child_pid;
    int status;

    
    /* Kill the existing process */
    kill(wd_data.pid, SIGKILL);

    /* Check if the process is killed */
    waitpid(wd_data.pid, &status, 0);

    /* Fork a new process */
    if(0 == strcmp(wd_data.p_to_launch,"run_watchdog"))
    {
        ParamsToArgs();

        child_pid = fork();

        if (child_pid < 0)
            {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            }
        if (child_pid == 0)
        {
            execv(wd_t_arg[0], wd_t_arg);
        }
        else
        {
           wd_data.pid = child_pid; 
        }
        
    }
    else
    {
        child_pid = fork();
        printf("So the client is death?\n");
        if (child_pid < 0)
            {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            }
        if (child_pid == 0)
        {   
            execvp(wd_data.argv[0],wd_data.argv);
        }
        else
        {
           wd_data.pid = child_pid; 
        }
    }
}

int CleanUp(sched_ty *scheduler)
{
    /*sched stop*/
    if (SchedStop(scheduler) == 1)
    {
        exit(WD_EXIT_FAILURE);
    }
    /*destroy sched*/
    SchedDestroy(scheduler);

    /*clean sem*/
    sem_unlink(WD_NAME);

    /*kill wd_ENV*/
    unsetenv("WD_PID");

    kill(getpid(),SIGKILL);

    return 0;
}

void InitParams(char *to_launch_, char *my_name_, pid_t pid_, size_t interval_, size_t max_failures_)
{
    strcpy(wd_data.program_name, my_name_);
    strcpy(wd_data.p_to_launch, to_launch_);
    wd_data.pid = pid_;
    wd_data.interval = interval_;
    wd_data.max_counter = max_failures_;
    wd_data.stop_wd_flag = 0;
    wd_data.counter = 0;
}

void ParamsToArgs()
{
    int i = 0;

    wd_t_arg = (char**)malloc(wd_data.argc*sizeof(char*));

    for (i = 0; i < wd_data.argc; ++i)
    {
        wd_t_arg[i] = (char*)malloc(sizeof(NAME_LEN));
        if (NULL == wd_t_arg[i])
        {
            free(wd_t_arg);
            wd_t_arg = NULL;
            perror("Malloc failed");
            exit(1);
        }
    }

    strcpy(wd_t_arg[0], wd_data.p_to_launch); 
    strcpy(wd_t_arg[1], wd_data.program_name);  
    sprintf(wd_t_arg[2], "%d", getpid());
    sprintf(wd_t_arg[3], "%lu", wd_data.interval);
    sprintf(wd_t_arg[4], "%lu", wd_data.max_counter);

    for(i = 5; i < wd_data.argc-1 ; ++i)
    {
       wd_t_arg[i] = wd_data.argv[i-5];
    }
    wd_t_arg[i] = NULL;
}