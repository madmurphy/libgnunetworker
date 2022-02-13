#include <stdio.h>
#include <gnunet/gnunet_worker_lib.h>


static void task_for_both_threads (void * const data) {

	GNUNET_WORKER_Handle
		current_worker = GNUNET_WORKER_get_current_handle();

	if (current_worker) {

		printf("This is the worker thread\n");

	} else {

		printf("This is not the worker thread\n");

	}

}


int main (const int argc, const char * const * const argv) {

	GNUNET_WORKER_Handle my_worker;

	/*  Create a separate thread where GNUnet's scheduler is run  */
	if (GNUNET_WORKER_create(&my_worker, NULL, NULL, NULL)) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	};

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		my_worker,
		&task_for_both_threads,
		NULL
	);

	task_for_both_threads(NULL);

	/*  Make sure that threads have had enough time to start...  */
	sleep(1);

	/*  Shut down the scheduler and wait until it returns  */
	GNUNET_WORKER_synch_destroy(my_worker);

	return 0;

}

