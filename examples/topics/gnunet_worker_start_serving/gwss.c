#include <stdio.h>
#include <gnunet/gnunet_worker_lib.h>


static void task_for_the_scheduler (void * const data) {

	printf("Hello world\n");

}


static void master_main (
	GNUNET_WORKER_Handle const worker,
	void * const data
) {

	printf("Master\n");

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		worker,
		&task_for_the_scheduler,
		NULL
	);

	/*  Make sure that threads have had enough time to start...  */
	sleep(1);

	/*  Shut down the scheduler and wait until it returns  */
	GNUNET_WORKER_synch_destroy(worker);

}


int main (const int argc, const char * const * const argv) {

	GNUNET_WORKER_Handle my_worker;

	/*  Run the GNUnet's scheduler in the current thread  */
	if (
		GNUNET_WORKER_start_serving(
			&my_worker,
			&master_main,
			NULL,
			NULL,
			NULL
		)
	) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	};

	return 0;

}

