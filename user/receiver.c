
#include <stdio.h>
/* For IPC_CREAT */
#include <sys/msg.h>
/* For int32_t */
#include <stdint.h>
/* Header for handling signals (definition of SIGINT) */
#include <signal.h>
/* For pid_t and getpid() */
#include <unistd.h>
#include <sys/types.h>
/* For using real-time scheduling policy (FIFO) and sched_setaffinity */
#include <sched.h>
/* For locking the program in RAM (mlockall) to prevent swapping */
#include <sys/mman.h>

/* #define LOG depends on #define MEASURE_PERF in main.c, so the latter should be defined in main.c if LOG is defined here. */
#define LOG

#ifdef LOG

/* Assuming an update rate of exactly 1 ms, number of cycles for 24h = 24*3600*1000. */
#define NUMBER_OF_CYCLES 86400000
/* Flush the data every ... cycle */
#define FLUSH_CYCLE 60000

#endif

/* Queue ID */
int qID;

#ifdef LOG
FILE *fp;
#endif

void print_config(void)
{

#ifdef LOG
printf("\nLOG is defined. Number of cycles = %d\n", NUMBER_OF_CYCLES);
printf("FLUSH_CYCLE = %d\n\n", FLUSH_CYCLE);

#endif

}





void signal_handler(int sig)
{
	printf("Removing the queue...\n");
	if (!(msgctl(qID, IPC_RMID, NULL)))
		printf("The queue was successfully removed.\n");
	#ifdef LOG
	/* close the file */
        if (!fclose(fp))
		printf("The log file was successfully closed.\n");
	#endif
	pid_t pid = getpid();
	kill(pid, SIGKILL);
}

int main(int argc, char **argv)
{

	signal(SIGINT, signal_handler);
	
	print_config();
	
	/* SCHED_FIFO tasks are allowed to run until they have completed their work or voluntarily yield. */
	/* Note that even the lowest priority realtime thread will be scheduled ahead of any thread with a non-realtime policy; 
	   if only one realtime thread exists, the SCHED_FIFO priority value does not matter.
	*/  
	struct sched_param param = {};
	param.sched_priority = 20;
	printf("Using priority %i.\n", param.sched_priority);
	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) 
	{
		printf("sched_setscheduler failed\n");
	}
	
	/* Lock the program into RAM to prevent page faults and swapping */
	/* MCL_CURRENT: Lock in all current pages.
	   MCL_FUTURE:  Lock in pages for heap and stack and shared memory.
	*/
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
	{
		printf("mlockall failed\n");
		return -1;
	}


	#ifdef LOG
	/* open the file */
        fp = fopen("log.txt", "w");
	
	if (fp == NULL)
	{
		printf("Failed to open file log.txt\n");
		return -1;
	}
	
	/* Cycle number. */
	int i = 0;
	#endif
	
	/* key is specified by the process which creates the queue (receiver). */
	key_t qKey = 1234;
	
	/* IPC_CREAT: Create a new queue.*/
	int qFlag = IPC_CREAT;
	
        printf("Creating a queue with key = %d\n", qKey);

	if ((qID = msgget(qKey, qFlag)) < 0) 
	{
		printf("Queue creation failed. Terminating the process...\n");
		return -1;
	}
	else 
		printf("Queue creation successful.\n");
	
	
	typedef struct myMsgType 
	{
		/* Mandatory */
		long       mtype;
		/* Data */
		long       updatePeriod;
		int32_t    actPos[2];
		int32_t    targetPos[2];
       	} myMsg;
	
	/* Received message. */
	myMsg recvdMsg;
	
	size_t msgSize;
	
	/* size of data = size of structure - size of mtype */
	msgSize = sizeof(struct myMsgType) - sizeof(long);
	
	/* Pick messages with type = 1. (msg.mtype = 1 in the producer) */
	int msgType = 1;
	
	/* No flag for receiving the message. */
	int msgFlag = 0;
	
	#ifdef LOG
	while (i != NUMBER_OF_CYCLES - 1)
	#else
	while (1)
	#endif
	{
	
		/* Removes a message from the queue specified by qID and 
		   places it in the buffer pointed to by recvdMsg. 
		*/
		if (msgrcv(qID, &recvdMsg, msgSize, msgType, msgFlag) < 0) 
		{
			printf("Error picking a message from the queue. Terminating the process...\n");
			return -1;
		}
		
		#ifdef LOG
	        /* Write data to the log. */
		fprintf(fp, "%ld\n", recvdMsg.updatePeriod);
		#else
		/* Print the message's data. */
		printf("Motor 0 actual position: %d\n", recvdMsg.actPos[0]);
		printf("Motor 1 actual position: %d\n", recvdMsg.actPos[1]);
		printf("Update period:           %ld\n", recvdMsg.updatePeriod);
		#endif
		
		#ifdef LOG
		i = i + 1;
		#endif

		/* Flush the buffer every 1 minute. */
		if (i % FLUSH_CYCLE == 0)
		{
			if (fflush(fp))
				printf("Flushing output stream buffer failed! %d\n", i);
			
		}
		
	}
	
	#ifdef LOG
	/* close the file */
        if (!fclose(fp))
		printf("The log file was successfully closed.\n");
	#endif
	
	return 0;	
}
