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




float motor0_points[] = { 50.62154919, 50.62137813, 50.62018069, 50.61693061, 50.61060191, 50.60016933, 50.58460876, 50.56289793, 50.53401722, 50.49695055, 50.45068645, 50.39456035, 50.32927654, 50.25588084, 50.17541779, 50.0889296, 49.9974552, 49.90202952, 49.80368285, 49.70344041, 49.60232207, 49.59220011, 49.48995549, 49.38276785, 49.26705232, 49.14019993, 49.00048699, 48.8469848, 48.67946978, 48.49833402, 48.304496, 48.09931117, 47.88448225, 47.66196894, 47.43389661, 47.20246391, 46.96984898, 46.73811417, 46.50910928, 46.28437328, 46.06503471, 45.85171099, 45.64440689, 45.44241236, 45.24420011, 45.13359654, 44.93248172, 44.71402765, 44.47582818, 44.21786575, 43.9401211, 43.64257312, 43.32519887, 42.98797346, 42.63087004, 42.25385971, 41.85691148, 41.43999225, 41.00306671, 40.54609737, 40.06904453, 39.57186622, 39.05451826, 38.51695426, 37.95912564, 37.38098171, 36.78246973, 36.16353501, 35.52412106, 34.86416972, 34.18362138, 33.4824152, 32.76048937, 32.01778147, 31.25422876, 30.46976869, 29.66433934, 28.83787994, 27.99033155, 27.12163773, 26.2317453, 25.32060529, 24.38817383, 23.43441331, 22.4592936, 21.46279332, 20.44490142, 19.40561876, 18.34495997, 17.26295538, 16.15965327, 15.03512225, 13.88945387, 12.72276548, 11.53520339, 10.32694626, 9.09820877, 7.849245668, 6.580356063, 5.291888113, 3.984244043, 2.657885533, 1.313339474, -0.048795914, -1.427844583, -2.823042464, -4.229095378, -5.631967953, -7.028451409, -8.41774868, -9.799025517, -11.17141107, -12.53399873, -13.88584736, -15.22598274, -16.55339957, -17.86706369, -19.16591491, -20.44887015, -21.71482713, -22.96266857, -24.19126677, -25.3994888, -26.58620204, -27.75028025, -28.89060995, -30.00609718, -31.0956745, -32.15830818, -33.19300545, -34.19882172, -35.1748676, -36.12031351, -37.03156265, -37.90010336, -38.7251066, -39.50745995, -40.24814393, -40.94822176, -41.60882883, -42.23116201, -42.81646903, -43.36603806, -43.88118769, -44.36325745, -44.81359888, -45.23356732, -45.62451448, -45.98778166, -46.3246939, -46.63655474, -46.92464181, -47.19020312, -47.43445396, -47.65857448, -47.86370774, -48.05095837, -48.22139157, -48.37603258, -48.51586647, -48.64183823, -48.75485307, -48.855777, -48.94543751, -49.02462448, -49.09409106, -49.15455479, -49.20669865, -49.25117226, -49.28859299, -49.31954715, -49.34459122, -49.36425291, -49.37903241, -49.38940343, -49.39581428, -49.39868895, -49.39842811, -49.39541004, -49.38999156, -49.38250891, -49.3732786, -49.36259815, -49.35074684, -49.33798646, -49.32456187, -49.31070169, -49.29661884, -49.28251107, -49.26856145, -49.25493884, -49.24179826, -49.22928117, -49.21845523, -49.20495047, -49.18393457, -49.14958135, -49.09739152, -49.02408939, -48.927517, -48.80652439, -48.66085592, -48.49103354, -48.29823803, -48.08418975, -47.85103039, -47.60120716, -47.33736042, -47.06221579, -46.77848102, -46.48874797, -46.19539947, -45.90052078, -45.60581498, -45.31252185, -45.02133931, -44.73234678, -44.57071044, -44.28437146, -44.00183743, -43.72591421, -43.45937347, -43.20495582, -42.96537483, -42.74332161, -42.54147015, -42.36248319, -42.20901881, -42.08280677, -41.98187216, -41.90334273, -41.84437466, -41.80214572, -41.7738495, -41.75669055, -41.74788047, -41.74463486, -41.74417122};
float motor1_points[] = { -17.39188584, -17.39229947, -17.39519489, -17.40305439, -17.41836181, -17.44360437, -17.48127519, -17.53387663, -17.6039243, -17.693952, -17.80651745, -17.94337633, -18.10297919, -18.28294863, -18.4809034, -18.69445292, -18.9211923, -19.15869784, -19.40452312, -19.6561956, -19.9112139, -19.9368034, -20.19366797, -20.45359724, -20.71777492, -20.98658311, -21.25968895, -21.53613285, -21.81441878, -22.09260736, -22.36841204, -22.63929906, -22.90259116, -23.15557533, -23.39561426, -23.6202613, -23.827378, -24.01525361, -24.18272533, -24.32929813, -24.45526302, -24.56181276, -24.6511543, -24.72661788, -24.79276326, -24.82799497, -24.89158623, -24.96023364, -25.03457907, -25.11449548, -25.1998448, -25.29047779, -25.38623388, -25.48694101, -25.59241547, -25.70246164, -25.81687185, -25.93542608, -26.05789174, -26.18402336, -26.31356235, -26.44623661, -26.58176025, -26.7198332, -26.86014082, -27.0023535, -27.14612621, -27.29109805, -27.43689181, -27.58311337, -27.72935127, -27.87517611, -28.02014002, -28.16377602, -28.30559749, -28.44509752, -28.58174832, -28.71500058, -28.84428287, -28.96900107, -29.0885377, -29.20225146, -29.30947663, -29.40952265, -29.50167367, -29.58518819, -29.65929885, -29.72321225, -29.7761089, -29.81714336, -29.84544448, -29.86011583, -29.8602364, -29.84486139, -29.81302337, -29.76373361, -29.69598376, -29.60874773, -29.50098402, -29.37163819, -29.21964583, -29.04393573, -28.8434334, -28.61706487, -28.36376086, -28.08246177, -27.77313964, -27.43826826, -27.07844186, -26.69377026, -26.2843946, -25.85048673, -25.39224838, -24.9099101, -24.40373001, -23.87399239, -23.32100607, -22.74510268, -22.14663488, -21.52597433, -20.88350977, -20.21964494, -19.53479653, -18.82939212, -18.10386812, -17.35866783, -16.59423943, -15.81103416, -15.0095045, -14.19010249, -13.35327808, -12.49947769, -11.62914487, -10.74554052, -9.857777896, -8.968434228, -8.078510111, -7.188955962, -6.300673893, -5.414519674, -4.531304743, -3.651798249, -2.776729096, -1.906787969, -1.042629336, -0.184873397, 0.665892021, 1.509109625, 2.344250895, 3.17081439, 3.988324132, 4.796328069, 5.594396613, 6.382121258, 7.159113276, 7.92500249, 8.679436114, 9.422077677, 10.15260601, 10.87071428, 11.57610916, 12.26850996, 12.94764788, 13.61326534, 14.26511528, 14.90296061, 15.52657366, 16.13573563, 16.7302362, 17.30987308, 17.87445162, 18.42378452, 18.95769148, 19.47599895, 19.97853988, 20.46515354, 20.93568529, 21.38998644, 21.82791414, 22.24933121, 22.6541061, 23.04211278, 23.41323067, 23.76734464, 24.10434492, 24.42412713, 24.72659223, 25.01164655, 25.27920177, 25.52917495, 25.76148854, 25.9760704, 26.17285686, 26.33770407, 26.51929168, 26.7047845, 26.89688977, 27.09731203, 27.30686165, 27.52556392, 27.75276866, 27.98726025, 28.22736757, 28.47107366, 28.71612452, 28.9601368, 29.20070408, 29.43550142, 29.66238829, 29.87950978, 30.08539609, 30.27906063, 30.46009686, 30.62877406, 30.78613241, 30.93407758, 31.07547513, 31.15348228, 31.29178566, 31.42867875, 31.56277922, 31.69270542, 31.81707617, 31.93451064, 32.04362808, 32.14304773, 32.23138869, 32.30726978, 32.36977074, 32.41981586, 32.45879006, 32.48807778, 32.50906318, 32.52313018, 32.53166257, 32.53604406, 32.53765829, 32.53788889};


#define NUMBER_OF_motorpuls 524288


int32_t degree_to_motor_position(float point) {
	return (int32_t)(point * NUMBER_OF_motorpuls / 360);
}
float motor_to_degree_position(int32_t point) {
	return (float)((float)point * (float)360 /  (float)NUMBER_OF_motorpuls);
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
	
	
	
	if (ecrt_slave_config_pdos(drive0, EC_END, slave_0_syncs))
	{
		printf("Failed to configure slave 0 PDOs\n");
		return -1;
	}
	
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

	
	/* Return value of msgsnd. */
	int retVal;
	
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
	int test =100;
	int counter =   0 ;
	/* Wake up 1 msec after the start of the previous loop. */
	sleepTime = cycleTime;
	/* Update wakeupTime = current time */
	clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
	int position_number = 0;
	int rev_position_number = sizeof(motor0_points) / sizeof(motor0_points[0]) - 1 ;
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
		/* Fetches received frames from the newtork device and processes the datagrams. */
		ecrt_master_receive(master);
		/* Evaluates the working counters of the received datagrams and outputs statistics,
		   if necessary.
		   This function is NOT essential to the receive/process/send procedure and can be 
		   commented out 
		*/
		ecrt_domain_process(domain1);
		
		
		#if defined(MEASURE_PERF) && defined(SYNC_REF_TO_MASTER)
		ecrt_master_reference_clock_time(master, &t_cur);	
		#endif
		
		/********************************************************************************/
		
		/* Read PDOs from the datagram */
		actPos0 = EC_READ_S32(domain1_pd + offset_actPos0);
		actPos1 = EC_READ_S32(domain1_pd + offset_actPos1);
		

		/*
			calculate the position 
		*/

		
		
		//targetPos0 = degree_to_motor_position(motor0_points[position_number]);
		//targetPos0 =motor1_points[position_number];	
		//position_number++;
		//printf("actpos %ld \n" , actPos0);
		/* Process the received data 524288*/
		if(counter <= 50){
			counter = counter + 1 ;
			targetPos0 = 0 ;
		}else {
		
			if(position_number < sizeof(motor0_points) / sizeof(motor0_points[0])){
				//position_number = position_number %  (sizeof(motor0_points) / sizeof(motor0_points[0]));
				//printf("pos value : %d \t actpos0 %ld\t dpos: %ld\n" , position_number , actPos0 , degree_to_motor_position(motor0_points[position_number]));
				targetPos0 = degree_to_motor_position(motor0_points[position_number]);
				position_number++;
				if(position_number == sizeof(motor0_points) / sizeof(motor0_points[0] )){
					rev_position_number =  sizeof(motor0_points) / sizeof(motor0_points[0]) - 1 ; 
				}
			}else{
				//position_number = position_number %  (sizeof(motor0_points) / sizeof(motor0_points[0]));
				//printf("rev value : %d \t actpos0 %ld\t dpos: %ld\n" , rev_position_number , actPos0 , degree_to_motor_position(motor0_points[rev_position_number]));
				targetPos0 = degree_to_motor_position(motor0_points[rev_position_number]);
				rev_position_number -- ;
				if(rev_position_number < 0 ){
					position_number = 0 ;
				}
				
			}
			
			
		}
		//printf("pos val: %f\n" ,motor_to_degree_position(targetPos0));
		//targetPos0= 0 ;
		//test = 0;
		targetPos1 = (actPos1  + 0 ) ; // / NUMBER_OF_motorpuls )  * NUMBER_OF_motorpuls - degree_to_motor_position(motor1_points[position_number]);
		
		//position_number++;
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

		/* wakeupTime = wakeupTime + sleepTime */
		timespec_add(&wakeupTime, &wakeupTime, &sleepTime);
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
		i = i + 1;
		#endif
	
	}
	
	ecrt_release_master(master);

	return 0;
}
