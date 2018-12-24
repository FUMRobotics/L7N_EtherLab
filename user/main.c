/* For CPU_ZERO and CPU_SET macros */
#define _GNU_SOURCE

/*****************************************************************************/

#include "ecrt.h"

/*****************************************************************************/

#include <string.h>
#include <stdio.h>
/* For setting the process's priority (setpriority) */
#include <sys/resource.h>
/* For pid_t and getpid() */
#include <unistd.h>
#include <sys/types.h>
/* For locking the program in RAM (mlockall) to prevent swapping */
#include <sys/mman.h>
/* clock_gettime, struct timespec, etc. */
#include <time.h>
/* Header for handling signals (definition of SIGINT) */
#include <signal.h>
/* For using real-time scheduling policy (FIFO) and sched_setaffinity */
#include <sched.h>
/* For using uint32_t format specifier, PRIu32 */
#include <inttypes.h>
/* For msgget and IPC_NOWAIT */
#include <sys/msg.h>
/* For definition of errno. */
#include <errno.h>
/*****************************************************************************/
/* Uncomment to execute the motion loop for a predetermined number of cycles (= NUMBER_OF_CYCLES). */
#define LOG

#ifdef LOG

/* Assuming an update rate of exactly 1 ms, number of cycles for 24h = 24*3600*1000. */
#define NUMBER_OF_CYCLES 86400000

#endif

/* Comment to disable configuring PDOs (i.e. in case the PDO configuration saved in EEPROM is our 
   desired configuration.)
*/
#define CONFIG_PDOS

/* Comment to disable distributed clocks. */
#define DC

/* Comment to disable interprocess communication with queues. */
#define IPC

/* Choose the syncronization method: The reference clock can be either master's, or the reference slave's (slave 0 by default) */
#ifdef DC

/* Slave0's clock is the reference: no drift. Algorithm from rtai_rtdm_dc example. Work in progress.*/
#define SYNC_MASTER_TO_REF
/* Master's clock (CPU) is the reference: lower overhead. */
//#define SYNC_REF_TO_MASTER

#endif

#ifdef DC

/* Comment to disable configuring slave's DC specification (shift time & cycle time) */
#define CONFIG_DC

/* Uncomment to enable performance measurement. */
/* Measure the difference in reference slave's clock timstamp each cycle, and print the result,
   which should be as close to cycleTime as possible. */
/* Note: Only works with DC enabled. */
#define MEASURE_PERF

#endif

/*****************************************************************************/

/* One motor revolution increments the encoder by 2^19 -1. */
#define ENCODER_RES 524287
/* The maximum stack size which is guranteed safe to access without faulting. */       
#define MAX_SAFE_STACK (8 * 1024) 

/* Calculate the time it took to complete the loop. */
//#define MEASURE_TIMING 

#define SET_CPU_AFFINITY

#define NSEC_PER_SEC (1000000000L)
#define FREQUENCY 1000
/* Period of motion loop, in nanoseconds */
#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)
/* Time to wait for a frame to be received. Determined by trial and error. */
#define RECV_TIME 20000

#ifdef DC

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

#endif

#ifdef CONFIG_DC

/* SYNC0 event happens halfway through the cycle */
#define SHIFT0 (PERIOD_NS/2)

#endif

/*****************************************************************************/

void print_config(void)
{

	printf("**********\n");

	#ifdef LOG
	printf("LOG. NUMBER_OF_CYCLES = %d\n", NUMBER_OF_CYCLES);
	#endif

	#ifdef CONFIG_PDOS
	printf("CONFIG_PDOS\n");
	#endif

	#ifdef DC


	printf("\nDC. ");

	#ifdef SYNC_MASTER_TO_REF
	printf("Mode: SYNC_MASTER_TO_REF\n");
	#endif

	#ifdef SYNC_REF_TO_MASTER
	printf("Mode: SYNC_REF_TO_MASTER\n");
	#endif

	#ifdef CONFIG_DC 
	printf("CONFIG_DC\n");
	#endif

	#ifdef MEASURE_PERF 
	printf("MEASURE_PERF\n");
	#endif


	#endif

	#ifdef IPC
	printf("\nIPC\n");
	#endif

	#ifdef SET_CPU_AFFINITY
	printf("SET_CPU_AFFINITY\n");
	#endif


	#ifdef MEASURE_TIMING
	printf("MEASURE_TIMING\n");
	#endif

	#ifdef FREQUENCY
	printf("FREQUENCY = %d\n", FREQUENCY);
	#endif

	printf("**********\n");

}

/*****************************************************************************/
/* Note: Anything relying on definition of SYNC_MASTER_TO_REF is essentially copy-pasted from /rtdm_rtai_dc/main.c */

#ifdef SYNC_MASTER_TO_REF

/* First used in system_time_ns() */
static int64_t  system_time_base = 0LL;
/* First used in sync_distributed_clocks() */
static uint64_t dc_time_ns = 0;
static int32_t  prev_dc_diff_ns = 0;
/* First used in update_master_clock() */
static int32_t  dc_diff_ns = 0;
static unsigned int cycle_ns = PERIOD_NS;
static uint8_t  dc_started = 0;
static int64_t  dc_diff_total_ns = 0LL;
static int64_t  dc_delta_total_ns = 0LL;
static int      dc_filter_idx = 0;
static int64_t  dc_adjust_ns;
#define DC_FILTER_CNT          1024
/** Return the sign of a number
 *
 * ie -1 for -ve value, 0 for 0, +1 for +ve value
 *
 * \retval the sign of the value
 */
#define sign(val) \
    ({ typeof (val) _val = (val); \
    ((_val > 0) - (_val < 0)); })

static uint64_t dc_start_time_ns = 0LL;

#endif

ec_master_t* master;

/*****************************************************************************/

#ifdef SYNC_MASTER_TO_REF

/** Get the time in ns for the current cpu, adjusted by system_time_base.
 *
 * \attention Rather than calling rt_get_time_ns() directly, all application
 * time calls should use this method instead.
 *
 * \ret The time in ns.
 */
uint64_t system_time_ns(void)
{
	struct timespec time;
	int64_t time_ns;
	clock_gettime(CLOCK_MONOTONIC, &time);
	time_ns = TIMESPEC2NS(time);

	if (system_time_base > time_ns) 
	{
		printf("%s() error: system_time_base greater than"
		       " system time (system_time_base: %ld, time: %lu\n",
			__func__, system_time_base, time_ns);
		return time_ns;
	}
	else 
	{
		return time_ns - system_time_base;
	}
}


/** Synchronise the distributed clocks
 */

/* If SYNC_MASTER_TO_REF and MEASURE_PERF are both defined. */
#ifdef MEASURE_PERF
void sync_distributed_clocks(uint32_t* t_cur)
/* If only SYNC_MASTER_TO_REF is defined. */
#else
void sync_distributed_clocks(void)	
#endif
{

	uint32_t ref_time = 0;
	uint64_t prev_app_time = dc_time_ns;

	dc_time_ns = system_time_ns();

	// set master time in nano-seconds
	ecrt_master_application_time(master, dc_time_ns);

	// get reference clock time to synchronize master cycle
	ecrt_master_reference_clock_time(master, &ref_time);
	#ifdef MEASURE_PERF
	*t_cur = ref_time;
	#endif
	dc_diff_ns = (uint32_t) prev_app_time - ref_time;

	// call to sync slaves to ref slave
	ecrt_master_sync_slave_clocks(master);
}


/** Update the master time based on ref slaves time diff
 *
 * called after the ethercat frame is sent to avoid time jitter in
 * sync_distributed_clocks()
 */
void update_master_clock(void)
{

	// calc drift (via un-normalised time diff)
	int32_t delta = dc_diff_ns - prev_dc_diff_ns;
	//printf("%d\n", (int) delta);
	prev_dc_diff_ns = dc_diff_ns;

	// normalise the time diff
	dc_diff_ns = ((dc_diff_ns + (cycle_ns / 2)) % cycle_ns) - (cycle_ns / 2);
        
	// only update if primary master
	if (dc_started) 
	{

		// add to totals
		dc_diff_total_ns += dc_diff_ns;
		dc_delta_total_ns += delta;
		dc_filter_idx++;

		if (dc_filter_idx >= DC_FILTER_CNT) 
		{
			// add rounded delta average
			dc_adjust_ns += ((dc_delta_total_ns + (DC_FILTER_CNT / 2)) / DC_FILTER_CNT);
                
			// and add adjustment for general diff (to pull in drift)
			dc_adjust_ns += sign(dc_diff_total_ns / DC_FILTER_CNT);

			// limit crazy numbers (0.1% of std cycle time)
			if (dc_adjust_ns < -1000) 
			{
				dc_adjust_ns = -1000;
			}
			if (dc_adjust_ns > 1000) 
			{
				dc_adjust_ns =  1000;
			}
		
			// reset
			dc_diff_total_ns = 0LL;
			dc_delta_total_ns = 0LL;
			dc_filter_idx = 0;
		}

		// add cycles adjustment to time base (including a spot adjustment)
		system_time_base += dc_adjust_ns + sign(dc_diff_ns);
	}
	else 
	{
		dc_started = (dc_diff_ns != 0);

		if (dc_started) 
		{
			// output first diff
			printf("First master diff: %d.\n", dc_diff_ns);

			// record the time of this initial cycle
			dc_start_time_ns = dc_time_ns;
		}
	}
}

#endif

/*****************************************************************************/

void ODwrite(ec_master_t* master, uint16_t slavePos, uint16_t index, uint8_t subIndex, uint8_t objectValue)
{
	/* Blocks until a reponse is received */
	uint8_t retVal = ecrt_master_sdo_download(master, slavePos, index, subIndex, &objectValue, sizeof(objectValue), NULL);
	/* retVal != 0: Failure */
	if (retVal)
		printf("OD write unsuccessful\n");
}

void initDrive(ec_master_t* master, uint16_t slavePos)
{
	/* Reset alarm */
	ODwrite(master, slavePos, 0x6040, 0x00, 128);
	/* Servo on and operational */
	ODwrite(master, slavePos, 0x6040, 0x00, 0xF);
	/* Mode of operation, CSP */
	ODwrite(master, slavePos, 0x6060, 0x00, 0x8);
}

/*****************************************************************************/

/* Add two timespec structures (time1 and time2), store the the result in result. */
/* result = time1 + time2 */
inline void timespec_add(struct timespec* result, struct timespec* time1, struct timespec* time2)
{

	if ((time1->tv_nsec + time2->tv_nsec) >= NSEC_PER_SEC) 
	{
		result->tv_sec  = time1->tv_sec + time2->tv_sec + 1;
		result->tv_nsec = time1->tv_nsec + time2->tv_nsec - NSEC_PER_SEC;
	} 
	else 
	{
		result->tv_sec  = time1->tv_sec + time2->tv_sec;
		result->tv_nsec = time1->tv_nsec + time2->tv_nsec;
	}

}

#ifdef MEASURE_TIMING
/* Substract two timespec structures (time1 and time2), store the the result in result. */
/* result = time1 - time2 */
inline void timespec_sub(struct timespec* result, struct timespec* time1, struct timespec* time2)
{

	if ((time1->tv_nsec - time2->tv_nsec) < 0) 
	{
		result->tv_sec  = time1->tv_sec - time2->tv_sec - 1;
		result->tv_nsec = NSEC_PER_SEC - (time1->tv_nsec - time2->tv_nsec);
	} 
	else 
	{
		result->tv_sec  = time1->tv_sec - time2->tv_sec;
		result->tv_nsec = time1->tv_nsec - time2->tv_nsec;
	}

}
#endif

/*****************************************************************************/

/* We have to pass "master" to ecrt_release_master in signal_handler, but it is not possible
   to define one with more than one argument. Therefore, master should be a global variable. 
*/
void signal_handler(int sig)
{
	printf("\nReleasing master...\n");
	ecrt_release_master(master);
	pid_t pid = getpid();
	kill(pid, SIGKILL);
}

/*****************************************************************************/

/* We make sure 8kB (maximum stack size) is allocated and locked by mlockall(MCL_CURRENT | MCL_FUTURE). */
void stack_prefault(void)
{
    unsigned char dummy[MAX_SAFE_STACK];
    memset(dummy, 0, MAX_SAFE_STACK);
}

/*****************************************************************************/

int main(int argc, char **argv)
{
	
	print_config();
	
	#ifdef SET_CPU_AFFINITY
	cpu_set_t set;
	/* Clear set, so that it contains no CPUs. */
	CPU_ZERO(&set);
	/* Add CPU (core) 1 to the CPU set. */
	CPU_SET(1, &set);
	#endif
	
	/* 0 for the first argument means set the affinity of the current process. */
	/* Returns 0 on success. */
	if (sched_setaffinity(0, sizeof(set), &set))
	{
		printf("Setting CPU affinity failed!\n");
		return -1;
	}
	
	/* SCHED_FIFO tasks are allowed to run until they have completed their work or voluntarily yield. */
	/* Note that even the lowest priority realtime thread will be scheduled ahead of any thread with a non-realtime policy; 
	   if only one realtime thread exists, the SCHED_FIFO priority value does not matter.
	*/  
	struct sched_param param = {};
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
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
	
	/* Allocate the entire stack, locked by mlockall(MCL_FUTURE). */
	stack_prefault();
	
	/* Register the signal handler function. */
	signal(SIGINT, signal_handler);
	
	/***************************************************/
	#ifdef IPC
	
	int qID;
	/* key is specified by the process which creates the queue (receiver). */
	key_t qKey = 1234;
	
	/* When qFlag is zero, msgget obtains the identifier of a previously created message queue. */
	int qFlag = 0;
	
	/* msgget returns the System V message queue identifier associated with the value of the key argument. */
	if ((qID = msgget(qKey, qFlag))) 
	{
		printf("Failed to access the queue with key = %d : %s\n", qKey, strerror(errno));
		return -1;
	}
	
	
	typedef struct myMsgType 
	{
		/* Mandatory, must be a positive number. */
		long       mtype;
		/* Data */
		#ifdef MEASURE_PERF
		long       updatePeriod;
		#endif
		int32_t    actPos[2];
		int32_t    targetPos[2];
       	} myMsg;
	
	myMsg msg;
	
	size_t msgSize;
	/* size of data = size of structure - size of mtype */
	msgSize = sizeof(struct myMsgType) - sizeof(long);
	    
	/* mtype must be a positive number. The receiver picks messages with a specific mtype.*/
	msg.mtype = 1;
	
	#endif
	/***************************************************/
	
	/* Reserve the first master (0) (/etc/init.d/ethercat start) for this program */
	master = ecrt_request_master(0);
	if (!master)
		printf("Requesting master failed\n");
	
	initDrive(master, 0);
	initDrive(master, 1);
	
	uint16_t alias = 0;
	uint16_t position0 = 0;
	uint16_t position1 = 1;
	uint32_t vendor_id = 0x00007595;
	uint32_t product_code = 0x00000000;
	
	/* Creates and returns a slave configuration object, ec_slave_config_t*, for the given alias and position. */
	/* Returns NULL (0) in case of error and pointer to the configuration struct otherwise */
	ec_slave_config_t* drive0 = ecrt_master_slave_config(master, alias, position0, vendor_id, product_code);
	ec_slave_config_t* drive1 = ecrt_master_slave_config(master, alias, position1, vendor_id, product_code);
	
	
	/* If the drive0 = NULL or drive1 = NULL */
	if (!drive0 || !drive1)
	{
		printf("Failed to get slave configuration\n");
		return -1;
	}
	
	
	/***************************************************/
	#ifdef CONFIG_PDOS
	
	/* Slave 0's structures, obtained from $ethercat cstruct -p 0 */ 
	ec_pdo_entry_info_t slave_0_pdo_entries[] = 
	{
	{0x6040, 0x00, 16}, /* Controlword */
	{0x607a, 0x00, 32}, /* Target Position */
	{0x6041, 0x00, 16}, /* Statusword */
	{0x6064, 0x00, 32}, /* Position Actual Value */
	};
	
	ec_pdo_info_t slave_0_pdos[] =
	{
	{0x1601, 2, slave_0_pdo_entries + 0}, /* 2nd Receive PDO Mapping */
	{0x1a01, 2, slave_0_pdo_entries + 2}, /* 2nd Transmit PDO Mapping */
	};
	
	ec_sync_info_t slave_0_syncs[] =
	{
	{0, EC_DIR_OUTPUT, 0, NULL            , EC_WD_DISABLE},
	{1, EC_DIR_INPUT , 0, NULL            , EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 1, slave_0_pdos + 0, EC_WD_DISABLE},
	{3, EC_DIR_INPUT , 1, slave_0_pdos + 1, EC_WD_DISABLE},
	{0xFF}
	};
	
		
	if (ecrt_slave_config_pdos(drive0, EC_END, slave_0_syncs))
	{
		printf("Failed to configure slave 0 PDOs\n");
		return -1;
	}
	
	/* Slave 1's structures, obtained from $ethercat cstruct -p 1 */ 
	ec_pdo_entry_info_t slave_1_pdo_entries[] = 
	{
	{0x6040, 0x00, 16}, /* Controlword */
	{0x607a, 0x00, 32}, /* Target Position */
	{0x6041, 0x00, 16}, /* Statusword */
	{0x6064, 0x00, 32}, /* Position Actual Value */
	};
	
	ec_pdo_info_t slave_1_pdos[] =
	{
	{0x1601, 2, slave_1_pdo_entries + 0}, /* 2nd Receive PDO Mapping */
	{0x1a01, 2, slave_1_pdo_entries + 2}, /* 2nd Transmit PDO Mapping */
	};
	
	ec_sync_info_t slave_1_syncs[] =
	{
	{0, EC_DIR_OUTPUT, 0, NULL            , EC_WD_DISABLE},
	{1, EC_DIR_INPUT , 0, NULL            , EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 1, slave_1_pdos + 0, EC_WD_DISABLE},
	{3, EC_DIR_INPUT , 1, slave_1_pdos + 1, EC_WD_DISABLE},
	{0xFF}
	};
	
	
	if (ecrt_slave_config_pdos(drive1, EC_END, slave_1_syncs))
	{
		printf("Failed to configure slave 1 PDOs\n");
		return -1;
	}
	
	#endif
	/***************************************************/

	unsigned int offset_controlWord0, offset_targetPos0, offset_statusWord0, offset_actPos0;
	unsigned int offset_controlWord1, offset_targetPos1, offset_statusWord1, offset_actPos1;
	
	ec_pdo_entry_reg_t domain1_regs[] =
	{
	{0, 0, 0x00007595, 0x00000000, 0x6040, 0x00, &offset_controlWord0},
	{0, 0, 0x00007595, 0x00000000, 0x607a, 0x00, &offset_targetPos0  },
	{0, 0, 0x00007595, 0x00000000, 0x6041, 0x00, &offset_statusWord0 },
	{0, 0, 0x00007595, 0x00000000, 0x6064, 0x00, &offset_actPos0     },
	
	{0, 1, 0x00007595, 0x00000000, 0x6040, 0x00, &offset_controlWord1},
	{0, 1, 0x00007595, 0x00000000, 0x607a, 0x00, &offset_targetPos1  },
	{0, 1, 0x00007595, 0x00000000, 0x6041, 0x00, &offset_statusWord1 },
	{0, 1, 0x00007595, 0x00000000, 0x6064, 0x00, &offset_actPos1     },
	{}
	};
	
	/* Creates a new process data domain. */
	/* For process data exchange, at least one process data domain is needed. */
	ec_domain_t* domain1 = ecrt_master_create_domain(master);
	
	/* Registers PDOs for a domain. */
	/* Returns 0 on success. */
	if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs))
	{
		printf("PDO entry registration failed\n");
		return -1;
	}
	
	#ifdef CONFIG_DC
	/* Do not enable Sync1 */
	ecrt_slave_config_dc(drive0, 0x0300, PERIOD_NS, SHIFT0, 0, 0);
	ecrt_slave_config_dc(drive1, 0x0300, PERIOD_NS, SHIFT0, 0, 0);
	#endif
	
	#ifdef SYNC_REF_TO_MASTER
	/* Initialize master application time. */
	struct timespec masterInitTime;
	clock_gettime(CLOCK_MONOTONIC, &masterInitTime);
	ecrt_master_application_time(master, TIMESPEC2NS(masterInitTime));
	#endif
	
	#ifdef SYNC_MASTER_TO_REF
	/* Initialize master application time. */
	dc_start_time_ns = system_time_ns();
	dc_time_ns = dc_start_time_ns;
	ecrt_master_application_time(master, dc_start_time_ns);
	
	/* If this method is not called for a certain master, then the first slave with DC functionality
	   will provide the reference clock.
	*/
	/* But we call this function anyway, just for emphasis. */
	if (ecrt_master_select_reference_clock(master, drive0))
	{
		printf("Selecting slave 0 as reference clock failed!\n");
		return -1;
	}
	#endif
	
	
	/* Up to this point, we have only requested the master. See log messages */
	printf("Activating master...\n");
	/* Important points from ecrt.h 
	   - This function tells the master that the configuration phase is finished and
	     the real-time operation will begin. 
	   - It tells the master state machine that the bus configuration is now to be applied.
	   - By calling the ecrt master activate() method, all slaves are configured according to
             the prior method calls and are brought into OP state.
	   - After this function has been called, the real-time application is in charge of cylically
	     calling ecrt_master_send() and ecrt_master_receive(). Before calling this function, the 
	     master thread is responsible for that. 
	   - This method allocated memory and should not be called in real-time context.
	*/
	     
	if (ecrt_master_activate(master))
		return -1;

	
	uint8_t* domain1_pd;
	/* Returns a pointer to (I think) the first byte of PDO data of the domain */
	if (!(domain1_pd = ecrt_domain_data(domain1)))
		return -1;
	
	ec_slave_config_state_t slaveState0;
	ec_slave_config_state_t slaveState1;
	struct timespec wakeupTime;
	
	#ifdef SYNC_REF_TO_MASTER
	struct timespec	time;
	#endif
	
	#ifdef MEASURE_PERF
	/* The slave time received in the current and the previous cycle */
	uint32_t t_cur, t_prev;
	#endif
	
	struct timespec cycleTime = {0, PERIOD_NS};
	struct timespec recvTime = {0, RECV_TIME};
	
	clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
	
	/* The slaves (drives) enter OP mode after exchanging a few frames. */
	/* We exchange frames with no RPDOs (targetPos) untill all slaves have 
	   reached OP state, and then we break out of the loop.
	*/
	while (1)
	{
		
		timespec_add(&wakeupTime, &wakeupTime, &cycleTime);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
		
		ecrt_master_receive(master);
		ecrt_domain_process(domain1);
		
		ecrt_slave_config_state(drive0, &slaveState0);
		ecrt_slave_config_state(drive1, &slaveState1);
		
		if (slaveState0.operational && slaveState1.operational)
		{
			printf("All slaves have reached OP state\n");
			break;
		}
	
		ecrt_domain_queue(domain1);
		
		#ifdef SYNC_REF_TO_MASTER
		/* Syncing reference slave to master:
                   1- The master's (PC) clock is the reference.
		   2- Sync the reference slave's clock to the master's.
		   3- Sync the other slave clocks to the reference slave's.
		*/
		
		clock_gettime(CLOCK_MONOTONIC, &time);
		ecrt_master_application_time(master, TIMESPEC2NS(time));
		/* Queues the DC reference clock drift compensation datagram for sending.
		   The reference clock will by synchronized to the **application (PC)** time provided
		   by the last call off ecrt_master_application_time().
		*/
		ecrt_master_sync_reference_clock(master);
		/* Queues the DC clock drift compensation datagram for sending.
		   All slave clocks will be synchronized to the reference slave clock.
		*/
		ecrt_master_sync_slave_clocks(master);
		#endif
		
		#ifdef SYNC_MASTER_TO_REF
		
		// sync distributed clock just before master_send to set
     	        // most accurate master clock time
		#ifdef MEASURE_PERF
                sync_distributed_clocks(&t_cur);
		#else
		sync_distributed_clocks();
		#endif
		
		#endif
		
		ecrt_master_send(master);
		
		#ifdef SYNC_MASTER_TO_REF
		// update the master clock
     		// Note: called after ecrt_master_send() to reduce time
                // jitter in the sync_distributed_clocks() call
                update_master_clock();
		#endif
	
	}
	
	int32_t actPos0, targetPos0;
	int32_t actPos1, targetPos1;
	
	/* Sleep is how long we should sleep each loop to keep the cycle's frequency as close to cycleTime as possible. */ 
	struct timespec sleepTime;
	#ifdef MEASURE_TIMING 
	struct timespec execTime, endTime;
	#endif 	
	
	#ifdef LOG
	/* Cycle number. */
	int i = 0;
	#endif
	
	/* Wake up 1 msec after the start of the previous loop. */
	sleepTime = cycleTime;
	/* Update wakeupTime = current time */
	clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
	
	#ifdef LOG
	while (i != NUMBER_OF_CYCLES)
	#else
	while (1)
	#endif
	{
		#ifdef MEASURE_TIMING
		clock_gettime(CLOCK_MONOTONIC, &endTime);
		/* wakeupTime is also start time of the loop. */
		/* execTime = endTime - wakeupTime */
		timespec_sub(&execTime, &endTime, &wakeupTime);
		printf("Execution time: %lu ns\n", execTime.tv_nsec);
		#endif
		
		/* Sleep until the frame is received by the network device. */
		/* Note that the flag 0 means sleep interval is *relative* to the current value of the clock. */
		clock_nanosleep(CLOCK_MONOTONIC, 0, &recvTime, NULL);
		
		/* Fetches received frames from the network device and processes the datagrams. */
		ecrt_master_receive(master);
		/* Evaluates the working counters of the received datagrams and outputs statistics,
		   if necessary.
		   This function is NOT essential to the receive/process/send procedure and can be 
		   commented out 
		*/
		ecrt_domain_process(domain1);
		
		/* If we want to both measure update rate and the synchrnonization method is chosen as 
		   "syncing reference clock to the master", we read the reference clock here. 
		   If the synchronization mechanism is "syncing master to reference", later in this code we read the reference clock 
		   as part of the algoritm. See sync_distributed_clocks(&t_cur);
		*/
		#if defined(MEASURE_PERF) && defined(SYNC_REF_TO_MASTER)
		ecrt_master_reference_clock_time(master, &t_cur);	
		#endif
		
		/********************************************************************************/
		
		/* Read PDOs from the datagram */
		actPos0 = EC_READ_S32(domain1_pd + offset_actPos0);
		actPos1 = EC_READ_S32(domain1_pd + offset_actPos1);
		
		/* Process the received data */
		targetPos0 = actPos0 + 5000;
		targetPos1 = actPos1 - 5000;
		
		/* Write PDOs to the datagram */
		EC_WRITE_U8  (domain1_pd + offset_controlWord0, 0xF );
		EC_WRITE_S32 (domain1_pd + offset_targetPos0  , targetPos0);
		
		EC_WRITE_U8  (domain1_pd + offset_controlWord1, 0xF );
		EC_WRITE_S32 (domain1_pd + offset_targetPos1  , targetPos1);
		
		/********************************************************************************/
		
		/* Queues all domain datagrams in the master's datagram queue. 
		   Call this function to mark the domain's datagrams for exchanging at the
		   next call of ecrt_master_send() 
		*/
		ecrt_domain_queue(domain1);
		
		#ifdef SYNC_REF_TO_MASTER
		/* Distributed clocks */
		clock_gettime(CLOCK_MONOTONIC, &time);
		ecrt_master_application_time(master, TIMESPEC2NS(time));
		ecrt_master_sync_reference_clock(master);
		ecrt_master_sync_slave_clocks(master);
		#endif
		
		#ifdef SYNC_MASTER_TO_REF
		
		// sync distributed clock just before master_send to set
     	        // most accurate master clock time
		#ifdef MEASURE_PERF
                sync_distributed_clocks(&t_cur);
		#else
		sync_distributed_clocks();
		#endif
		
		#endif
		
		/* wakeupTime = wakeupTime + cycleTime */
		timespec_add(&wakeupTime, &wakeupTime, &cycleTime);
		/* Sleep to adjust the update frequency */
		/* Note: TIMER_ABSTIME flag is key in ensuring the execution with the desired frequency.
		   We don't have to conider the loop's execution time (as long as it doesn't get too close to 1 ms), 
		   as the sleep ends cycleTime (=1 msecs) *after the start of the previous loop*.
		*/
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
		
		/* Sends all datagrams in the queue.
		   This method takes all datagrams that have been queued for transmission,
		   puts them into frames, and passes them to the Ethernet device for sending. 
		*/
		ecrt_master_send(master);
		
		#ifdef SYNC_MASTER_TO_REF
		// update the master clock
     		// Note: called after ecrt_master_send() to reduce time
                // jitter in the sync_distributed_clocks() call
                update_master_clock();
		#endif
		
		#ifdef IPC
		msg.actPos[0] = actPos0;
		msg.actPos[1] = actPos1;
	
		msg.targetPos[0] = targetPos0;
		msg.targetPos[1] = targetPos1;
		
		#ifdef MEASURE_PERF
		msg.updatePeriod = t_cur - t_prev;
		t_prev = t_cur;
		#endif
		
		/*  msgsnd appends a copy of the message pointed to by msg to the message queue 
		    whose identifier is specified by msqid.
		*/
		if (msgsnd(qID, &msg, msgSize, IPC_NOWAIT)) 
		{
			printf("Error sending message to the queue: %s\n", strerror(errno));
			printf("Terminating the process...\n");
			return -1;
		}
		#endif
		
		#ifdef LOG
		/* i is the number of cycles which we log their data. */
		i = i + 1;
		#endif
	
	}
	
	ecrt_release_master(master);
	
	return 0;
}
