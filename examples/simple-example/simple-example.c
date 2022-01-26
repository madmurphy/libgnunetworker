#include <stdio.h>
#include <gnunet/gnunet_worker_lib.h>


static void task_for_the_scheduler (void * const data) {

	printf("Hello world\n");

}


int main (const int argc, const char * const * const argv) {

	/*  Create a separate thread where GNUnet's scheduler is run  */
	GNUNET_WORKER_Handle * my_current_worker = GNUNET_WORKER_create(
		NULL,
		NULL,
		NULL
	);

	if (!my_current_worker) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	}

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		my_current_worker,
		&task_for_the_scheduler,
		NULL
	);

	/*  Make sure that threads have had enough time to start...  */
	sleep(1);

	/*  Shutdown the scheduler and wait until it returns  */
	GNUNET_WORKER_synch_destroy(my_current_worker);

}

