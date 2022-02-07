/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <gnunet/gnunet_worker_lib.h>


pthread_barrier_t worker_ready_barrier;
pthread_barrier_t task_performed_barrier;


static void task_for_the_scheduler (void * const data) {

	printf("Hello world\n");
	pthread_barrier_wait(&task_performed_barrier);

}


static bool confirm_startup (void * const data) {

	/*  In the unlikely event that `pthread_barrier_wait()` fails we kill the
		worker...  */

	return pthread_barrier_wait(
		&worker_ready_barrier
	) == PTHREAD_BARRIER_SERIAL_THREAD;

}


int main (const int argc, const char * const * const argv) {

	GNUNET_WORKER_Handle * my_worker;

	/*  Create a separate thread where GNUnet's scheduler is run  */
	if (GNUNET_WORKER_create(&my_worker, &confirm_startup, NULL, NULL)) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	};

    pthread_barrier_init(&worker_ready_barrier, NULL, 2);
    pthread_barrier_init(&task_performed_barrier, NULL, 2);

	/*  Make sure that the worker has had enough time to start...  */
	pthread_barrier_wait(&worker_ready_barrier);

	/*  Run a function in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		my_worker,
		&task_for_the_scheduler,
		NULL
	);

	/*  Make sure that the task has had time to be completed...  */
	pthread_barrier_wait(&task_performed_barrier);

	/*  Shut down the scheduler and wait until it returns  */
	GNUNET_WORKER_synch_destroy(my_worker);

	/*  Cleanup  */
	pthread_barrier_destroy(&task_performed_barrier);
	pthread_barrier_destroy(&worker_ready_barrier);

	return 0;

}

