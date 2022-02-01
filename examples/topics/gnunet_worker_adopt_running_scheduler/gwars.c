#include <stdio.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_scheduler_lib.h>
#include <gnunet/gnunet_worker_lib.h>


static void foo (void * const data) {

	printf("Task for the scheduler added by the master thread\n");

}


static void bar (void * const data) {

	printf("Task for the scheduler added by the scheduler itself\n");

}


static void master_thread (
	GNUNET_WORKER_Handle * worker,
	void * const IGNOREME
) {

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		worker,
		&foo,
		NULL
	);

	sleep(1);

	GNUNET_WORKER_dismiss(worker);

	printf("Master has ended\n");

}


static void on_worker_end (void * const data) {

	printf("Worker has ended\n");

}


static void scheduler_main (void * const data) {

	GNUNET_WORKER_Handle * my_worker;

	if (
		GNUNET_WORKER_adopt_running_scheduler(
			&my_worker,
			master_thread,
			on_worker_end,
			NULL
		)
	) {

		printf("Sorry, something went wrong :-(\n");
		return;

	}

	GNUNET_SCHEDULER_add_now(bar, NULL);

	printf("Hello world\n");

}


int main (const int argc, const char * const * const argv) {

	GNUNET_SCHEDULER_run(scheduler_main, NULL);

	sleep(1);

	printf("Bye\n");

}

