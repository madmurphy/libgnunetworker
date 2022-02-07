#include <stdio.h>
#include <gnunet/gnunet_worker_lib.h>
#include <gnunet/gnunet_scheduler_lib.h>


static void task_for_the_scheduler_3 (void * const data) {

	printf("This task too... The scheduler will return now.\n");

}


static void task_for_the_scheduler_2 (void * const data) {

	printf(
		"This task is running in the scheduler's thread, but without "
		"a worker\n"
	);

	GNUNET_SCHEDULER_add_delayed(
		GNUNET_TIME_relative_multiply(
			GNUNET_TIME_UNIT_SECONDS,
			2
		),
		&task_for_the_scheduler_3,
		NULL
	);

}


static void task_for_the_scheduler_1 (void * const data) {

	printf("Hello world\n");

	GNUNET_SCHEDULER_add_delayed(
		GNUNET_TIME_relative_multiply(
			GNUNET_TIME_UNIT_SECONDS,
			2
		),
		&task_for_the_scheduler_2,
		NULL
	);

}


int main (const int argc, const char * const * const argv) {

	GNUNET_WORKER_Handle * my_worker;

	/*  Create a separate thread where GNUnet's scheduler is run  */
	if (GNUNET_WORKER_create(&my_worker, NULL, NULL, NULL)) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	};

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		my_worker,
		&task_for_the_scheduler_1,
		NULL
	);

	/*  Make sure that threads have had enough time to start...  */
	sleep(1);

	/*  Shut down the scheduler and wait until it returns  */
	GNUNET_WORKER_dismiss(my_worker);
	printf("Worker has been dismissed\n");

	sleep(5);

	printf("The main thread has returned\n");

	return 0;

}

