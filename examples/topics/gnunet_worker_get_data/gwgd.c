#include <stdio.h>
#include <gnunet/gnunet_worker_lib.h>


static void task_for_the_scheduler (void * const v_my_string) {

	printf("%s\n", (char *) v_my_string);

}


int main (const int argc, const char * const * const argv) {

	GNUNET_WORKER_Handle * my_worker;

	/*  Create a separate thread where GNUnet's scheduler is run  */
	if (
		GNUNET_WORKER_create(
			&my_worker,
			NULL,
			NULL,
			"This is the data argument"
		)
	) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	};

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		my_worker,
		&task_for_the_scheduler,
		GNUNET_WORKER_get_data(my_worker)
	);

	/*  Make sure that threads have had enough time to start...  */
	sleep(1);

	/*  Shut down the scheduler and wait until it returns  */
	GNUNET_WORKER_synch_destroy(my_worker);

	return 0;

}

