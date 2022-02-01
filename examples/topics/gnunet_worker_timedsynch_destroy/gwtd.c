#include <stdio.h>
#include <gnunet/gnunet_worker_lib.h>


static void goodbye (void * const data) {

	printf("The worker says goodbye.\n");

}


static void task_for_the_scheduler (void * const data) {

	printf("Hello world\n");
	sleep(2);

}


void now_plus_milliseconds (
	struct timespec * const dest_time,
	const int milliseconds
) {

	clock_gettime(CLOCK_REALTIME, dest_time);
	dest_time->tv_sec += milliseconds / 1000;
	dest_time->tv_nsec += ((milliseconds % 1000) * 1000000);

	if (dest_time->tv_nsec >= 1000000000) {

		dest_time->tv_sec++;
		dest_time->tv_nsec -= 1000000000;

	} else if (dest_time->tv_nsec < 0) {

		dest_time->tv_sec--;
		dest_time->tv_nsec += 1000000000;

	}

}


int main (const int argc, const char * const * const argv) {

	GNUNET_WORKER_Handle * my_worker;

	/*  Create a separate thread where GNUnet's scheduler is run  */
	if (GNUNET_WORKER_create(&my_worker, NULL, &goodbye, NULL)) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	};

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		my_worker,
		&task_for_the_scheduler,
		NULL
	);

	/*  Make sure that threads have had enough time to start...  */
	sleep(1);

	/*  Shutdown the scheduler and wait until it returns (max 1 sec)  */
	struct timespec my_time;
	now_plus_milliseconds(&my_time, 1000);

	if (
		GNUNET_WORKER_timedsynch_destroy(
			my_worker,
			&my_time
		) == GNUNET_WORKER_ERR_EXPIRED
	) {

		printf("Time has expired\n");

	}


	printf("The main thread has returned.\n");

	sleep(1);

	return 0;

}

