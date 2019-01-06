#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FORKSRV_FD 198	

using namespace std;
static long savedDi;
register long rdi asm("di");		// the warning is fine - we need the warning because of a bug in dyninst

/* AFL bitmap tracing vars. */
static char * trace_bits;
static long shm_id;
#define SHM_ENV_VAR "__AFL_SHM_ID"

/* This is for the dummy tracer - i.e., it exits when hitting <main>. */
void mainExit() {
	exit(0);
}

/* Forkserver variant with tracer file descriptors + hashmap initialization + SHM. */
void forkServer() {
	int temp_data;
	pid_t fork_pid;

	/* Set up the SHM bitmap. */
	char *shm_env_var = getenv(SHM_ENV_VAR);
	if (!shm_env_var) {
		perror("Error getenv() SHM\n");
		return;
	}
	shm_id = atoi(shm_env_var);
	trace_bits = (char *) shmat(shm_id, NULL, 0);
	if (trace_bits == (char *) - 1) {
		perror("Error shmat()");
		return;
	}

	/* Tell the parent that we're alive. If the parent doesn't want
		 to talk, assume that we're not running in forkserver mode. */
	if (write(FORKSRV_FD + 1, &temp_data, 4) != 4) {
		perror("ERROR: fork server not running");
		return;
	}
	
	/* All right, let's await orders... */
	while (1) {

		/* Parent - Verify status message length. */
		int stMsgLen = read(FORKSRV_FD, &temp_data, 4);
		if (stMsgLen != 4) {
			/* we use a status message length 2 to terminate the fork server. */
			if(stMsgLen == 2)
				exit(EXIT_SUCCESS);
			perror("Error reading fork server");
			exit(EXIT_FAILURE);
		}

    	/* Parent - Fork off worker process that actually runs the benchmark. */
		fork_pid = fork();
		if (fork_pid < 0) {
      		perror("Fork server fork() failed");
			exit(EXIT_FAILURE);
    	}
		
		/* Child worker - Close descriptors and return (runs the benchmark). */
		if (fork_pid == 0) {
			close(FORKSRV_FD);
			close(FORKSRV_FD + 1);
			return; 
		} 
		
		/* Parent - Inform controller that we started a new run. */
		if (write(FORKSRV_FD + 1, &fork_pid, 4) != 4) {
    		perror("Fork server write(pid) failed");
			exit(EXIT_FAILURE);
  		}
  		/* Parent - Sleep until child/worker finishes. */
		if (waitpid(fork_pid, &temp_data, 2) < 0) {
    		perror("Fork server waitpid() failed"); 
			exit(EXIT_FAILURE);
  		}
		
		/* Parent - Inform controller that run finished. */
		if (write(FORKSRV_FD + 1, &temp_data, 4) != 4) {
    		perror("Fork server write(temp_data) failed");
			exit(EXIT_FAILURE);
  		}
  		/* Jump back to beginning of this loop and repeat. */
	}
}

/* Basic block callback with trace to file. */ 
void traceFile(char * curBlkAddr, char *tracePath, unsigned int curBlkID)
{	
	static FILE *traceFile = fopen(tracePath, "w");
	fprintf(traceFile, "%s\n", curBlkAddr);  
}

/* Basic block callback with trace to file + hashmap + SHM. */
void traceSHM(unsigned int curBlkID)
{	
	if (trace_bits && trace_bits[curBlkID] == 0)
		trace_bits[curBlkID]++;
}

void saveRdi() {
	savedDi = rdi;
	/*
	asm("pop %rax"); // take care of rip
	asm("push %rdi");
	asm("push %rax");
	*/
}

void restoreRdi() {
	rdi = savedDi;
	/*
	asm("pop %rax"); // take care of rip
	asm("pop %rdi");
	asm("push %rax");
	*/
}
