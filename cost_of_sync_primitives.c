/*
 * test_sync_primitives - test synchronization primitives
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define TEN_TIMES(x)	\
	x;		\
	x;		\
	x;		\
	x;		\
	x;		\
	x;		\
	x;		\
	x;		\
	x;		\
	x;

/* a clock */

/*
 * Reference [1] for assembly usage, [2] for architecture predefinition
 *
 * [1] https://www.mcs.anl.gov/~kazutomo/rdtsc.html
 * [2] https://sourceforge.net/p/predef/wiki/Architectures/
 */
#if defined(__i386__)
#define ACLK_HW_CLOCK
static __inline__ unsigned long long aclk_clock(void)
{
	unsigned long long int x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;
}

#elif defined(__x86_64__)
#define ACLK_HW_CLOCK
static __inline__ unsigned long long aclk_clock(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#elif defined(__powerpc__)
#define ACLK_HW_CLOCK
static __inline__ unsigned long long aclk_clock(void)
{
	unsigned long long int result=0;
	unsigned long int upper, lower,tmp;
	__asm__ volatile(
			"0:                  \n"
			"\tmftbu   %0           \n"
			"\tmftb    %1           \n"
			"\tmftbu   %2           \n"
			"\tcmpw    %2,%0        \n"
			"\tbne     0b         \n"
			: "=r"(upper),"=r"(lower),"=r"(tmp)
			);
	result = upper;
	result = result<<32;
	result = result|lower;

	return(result);
}

#else
static inline unsigned long long aclk_clock(void)
{
	return (unsigned long long)clock();
}

#endif

/*
 * aclk_freq - return clock frequency
 *
 * This function get cpu frequency in two ways.  If aclk does not support
 * hardware rdtsc instruction for the architecture this code is running, it
 * fallbacks to CLOCKS_PER_SEC.  Otherwise, it calculates the frequency by
 * measuring hardware rdtsc before and after sleep of 0.1 second.
 */
static inline unsigned long long aclk_freq(void)
{
/*
 * clock() cannot used with sleep. Refer to [1] for more information
 * [1] http://cboard.cprogramming.com/linux-programming/91589-using-clock-sleep.html
 */
#ifndef ACLK_HW_CLOCK
	return CLOCKS_PER_SEC;
#endif
	unsigned long long start;

	start = aclk_clock();
	usleep(1000 * 100);
	return (aclk_clock() - start) * 10;
}


static unsigned long long cpu_freq;

/* Only ates_measure_latency_(start|end) should access. */
static unsigned long long latency_start;

/**
 * Note that this function is not thread safe!
 */
void ates_measure_latency_start(void)
{
	if (cpu_freq == 0) {
		fprintf(stderr, "test should run with run_tets\n");
		exit(1);
	}

	latency_start = aclk_clock();
}

/**
 * Note that this function is not thread safe!
 */
double ates_measure_latency_end(void)
{
	return (double)(aclk_clock() - latency_start) / cpu_freq;
}

/**
 * Calculates operations per second
 */
double ates_calc_ops(double latency, unsigned nr_ops)
{
	return nr_ops / latency;
}

enum op {
	INC,	/* ++ */
	FAA,	/* Fetch And Add */
	CAS,	/* Compare And Swap */
	FCAS,	/* failure-like CAS */
	CCAS,	/* Comapre and CAS */
	MBA,	/* Memory Barrier */
	LOCK,	/* Mutex Lock */
	CASLOCK	/* CAS based spinlock */
} op;

unsigned OP_PERIOD = 2000;	/* milli-seconds */
enum op *PERF_TARGETS = (enum op[]) {
	INC, FAA, CAS, FCAS, CCAS, MBA, LOCK, CASLOCK};
size_t PERF_TARGETS_SZ = 8;
int *PERF_THRS = (int[]) {1,4,8,12,16,20,24,28,32};
size_t PERF_THRS_SZ = 9;

struct op_glob_arg {
	enum op op;
	pthread_mutex_t *lock;
	unsigned long long *spinlock;
	unsigned start;
	unsigned end;
	unsigned long long *addr;
};

struct op_local_arg {
	struct op_glob_arg *glob;
	unsigned long long count;
	double lat;
};

char *opnames[] = {"inc", "faa", "cas", "fcas", "ccas", "mba", "lock", "casl"};

#define CAS_LOCK(x)						\
	while (__sync_val_compare_and_swap(x, 0, 1) != 0) {}

#define CAS_UNLOCK(x)	\
	(*x = 0)

#define DO_UNTIL_END(x)						\
	while (1) {						\
		TEN_TIMES(x);					\
		if (cnt++ % 1000 == 0 && ACCESS_ONCE(o->end))	\
			break;					\
	}

void *do_op(void *arg)
{
	struct op_local_arg *l;
	struct op_glob_arg *o;
	unsigned long long *val;
	unsigned long long cnt;

	l = (struct op_local_arg *)arg;
	o = l->glob;
	val = o->addr;
	cnt = 0;
	while (ACCESS_ONCE(o->start) == 0);

	ates_measure_latency_start();
	if (o->op == INC) {
		DO_UNTIL_END((*val)++);
	} else if (o->op == FAA) {
		DO_UNTIL_END(__sync_fetch_and_add(val, 1));
	} else if (o->op == CAS) {
		unsigned long long oldval;

		DO_UNTIL_END(oldval = *val;
				oldval = __sync_val_compare_and_swap(
						val, oldval, oldval + 1));
	} else if (o->op == FCAS) {
		unsigned long long oldval;

		oldval = *val;
		DO_UNTIL_END(oldval = __sync_val_compare_and_swap(
					val, oldval, oldval + 1));
	} else if (o->op == CCAS) {
		unsigned long long oldval;

		DO_UNTIL_END(oldval = ACCESS_ONCE(*val);
				if (oldval == *val)
					__sync_val_compare_and_swap(
						val, oldval, oldval + 1));
	} else if (o->op == MBA) {
		DO_UNTIL_END(__sync_synchronize(); ACCESS_ONCE((*val))++);
	} else if (o->op == LOCK) {
		pthread_mutex_t *lock;
		lock = o->lock;

		DO_UNTIL_END(pthread_mutex_lock(lock);
				(*val)++;
				pthread_mutex_unlock(lock));
	} else if (o->op == CASLOCK) {
		unsigned long long *spinlock;
		spinlock = (unsigned long long *)o->spinlock;

		DO_UNTIL_END(CAS_LOCK(spinlock);
				(*val)++;
				CAS_UNLOCK(spinlock) );
	}
	l->lat = ates_measure_latency_end();
	/* cnt is increased for 10 ops due to loop unrolling */
	l->count = cnt * 10;

	return NULL;
}

void test_op(enum op op, int nr_thrs)
{
	struct op_glob_arg o;
	struct op_local_arg *las;
	pthread_t *ps;
	pthread_mutex_t lock;
	unsigned long long spinlock;
	unsigned long long total_count;
	int i;
       
	o.op = op;
	o.lock = &lock;
	pthread_mutex_init(o.lock, NULL);
	spinlock = 0;
	o.spinlock = &spinlock;
	o.addr = (unsigned long long *)malloc(sizeof(unsigned long long));
	*(o.addr) = 0;
	o.start = 0;
	o.end = 0;

	ps = (pthread_t *)malloc(sizeof(pthread_t) * nr_thrs);
	las = (struct op_local_arg *)malloc(
			sizeof(struct op_local_arg) * nr_thrs);
	for (i = 0; i < nr_thrs; i++) {
		las[i].count = 0;
		las[i].glob = &o;
		pthread_create(&ps[i], NULL, do_op, (void *)&las[i]);
	}

	o.start = 1;
	usleep(OP_PERIOD * 1000);
	o.end = 1;

	total_count = 0;
	for (i = 0; i < nr_thrs; i++) {
		pthread_join(ps[i], NULL);
		total_count += las[i].count;
	}
	printf("%4d %15.2lf ",
			nr_thrs, ates_calc_ops(las[0].lat, total_count));

	printf("%15.2lf\n", ates_calc_ops(las[0].lat, *(o.addr)));
	fflush(stdout);

	pthread_mutex_destroy(o.lock);
	free(ps);
	free(las);
	free(o.addr);
}

int test_performance(void)
{
	int i, j;

	printf("# %4s %15s %15s\n",
			"thrs", "issues", "succ");
	for (i = 0; i < PERF_TARGETS_SZ; i++) {
		printf("%s\n", opnames[i]);
		for (j = 0; j < PERF_THRS_SZ; j++) {
			test_op(PERF_TARGETS[i], PERF_THRS[j]);
		}
		if (i < PERF_TARGETS_SZ - 1)
			printf("\n\n");
	}
	return 0;
}

int main(int argc, char *argv[])
{
	cpu_freq = aclk_freq();
	return test_performance();
}
