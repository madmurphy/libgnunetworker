/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| libgnunetworker/src/worker.c
|*|
|*| https://github.com/madmurphy/libgnunetworker
|*|
|*| Copyright (C) 2022 madmurphy <madmurphy333@gmail.com>
|*|
|*| **GNUnet Worker** is free software: you can redistribute it and/or modify
|*| it under the terms of the GNU Affero General Public License as published by
|*| the Free Software Foundation, either version 3 of the License, or (at your
|*| option) any later version.
|*|
|*| **GNUnet Worker** is distributed in the hope that it will be useful, but
|*| WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|*| or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
|*| License for more details.
|*|
|*| You should have received a copy of the GNU Affero General Public License
|*| along with this program. If not, see <http://www.gnu.org/licenses/>.
|*|
\*/


/**

	@file       worker.c
	@brief      GNUnet Worker implementation

**/


#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <libintl.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_scheduler_lib.h>
#include <gnunet/gnunet_network_lib.h>
#include <gnunet_worker_lib.h>
#include "requirement.h"
#include "worker.h"



		/*\
		|*|
		|*|     LOCAL ENVIRONMENT
		|*|    ________________________________
		\*/



	/*  CONSTANTS AND VARIABLES  */


/**

	@brief      A "beep" for notifying the worker (any ASCII character will do)

**/
static const unsigned char BEEP_CODE = '\a';


/**

	@brief      The handle of the worker this thread is serving as, or `NULL`

**/
_Thread_local static GNUNET_WORKER_Handle * currently_serving_as = NULL;



	/*  INLINED FUNCTIONS  */


/**

	@brief      Create a detached thread
	@param      start_routine   The routine to launch in a new detached thread
	                                                             [NON-NULLABLE]
	@param      data            The routine's argument               [NULLABLE]

**/
static inline int thread_create_detached (
	void * (* const start_routine) (void *),
	void * const data
) {
	pthread_t thr;
	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	const int retval = pthread_create(&thr, &tattr, start_routine, data);
	pthread_attr_destroy(&tattr);
	return retval;
}


/**

	@brief      Undo what `GNUNET_WORKER_allocate()` did
	@param      worker          The worker to free               [NON-NULLABLE]

**/
static inline void GNUNET_WORKER_free (
	GNUNET_WORKER_Handle * const worker
) {
	close(worker->beep_fd[0]);
	close(worker->beep_fd[1]);
	GNUNET_NETWORK_fdset_destroy(worker->beep_fds);
	requirement_uninit(&worker->scheduler_has_returned);
	requirement_uninit(&worker->worker_is_disposable);
	pthread_mutex_destroy(&worker->tasks_mutex);
	pthread_mutex_destroy(&worker->kill_mutex);
	free(worker);
}


/**

	@brief      Function that must always follow `kill_handler()`
	@param      worker          The worker to free               [NON-NULLABLE]

	This function assumes that `kill_handler()` was called immediately before.
	The reason why it has been split as a separate function is that
	`kill_handler()` always requires the scheduler to be running, while this
	function is often invoked after the scheduler has already returned.

**/
static inline void GNUNET_WORKER_after_kill (
	GNUNET_WORKER_Handle * const worker
) {
	requirement_paint_green(&worker->scheduler_has_returned);
	requirement_wait_for_green(&worker->worker_is_disposable);
	currently_serving_as = NULL;
	/*  This mutex was locked by `kill_handler()`  */
	pthread_mutex_unlock(&worker->kill_mutex);
	GNUNET_WORKER_free(worker);
}


/**

	@brief      Launch `GNUNET_SCHEDULER_cancel()` on a pointer to a scheduled
	            task and set the pointer to `NULL`
	@param      sch_ptr         A pointer to a pointer to a scheduled task
	                                                             [NON-NULLABLE]

	The @p sch_ptr paramenter cannot be `NULL`, however it can point to `NULL`.
	If that is the case this function is no-op.

**/
static inline void clear_schedule (
	struct GNUNET_SCHEDULER_Task ** const sch_ptr
) {
	struct GNUNET_SCHEDULER_Task * const tmp = *sch_ptr;
	if (tmp) {
		*sch_ptr = NULL;
		GNUNET_SCHEDULER_cancel(tmp);
	}
}



	/*  FUNCTIONS  */


/**

	@brief      Terminate a worker (in most cases, handler added via
	            `GNUNET_SCHEDULER_add_shutdown()`)
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void kill_handler (
	void * const v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	GNUNET_WORKER_JobList * iter;

	clear_schedule(&worker->listener_schedule);
	worker->shutdown_schedule = NULL;

	if (worker->on_terminate) {

		atomic_store(&worker->state, WORKER_SAYS_BYE);
		worker->on_terminate(worker->data);

	}

	/*  This mutex will be unlocked by `GNUNET_WORKER_after_kill()`  */
	pthread_mutex_lock(&worker->kill_mutex);
	atomic_store(&worker->state, WORKER_IS_DYING);
	pthread_mutex_lock(&worker->tasks_mutex);

	if ((iter = worker->wishlist)) {

		while (iter->next) {

			free((iter = iter->next)->prev);

		}

		free(iter);
		worker->wishlist = NULL;

	}

	pthread_mutex_unlock(&worker->tasks_mutex);

	if ((iter = worker->schedules)) {


		/* \                                /\
		\ */     unschedule_task:          /* \
		 \/     ______________________     \ */


		GNUNET_SCHEDULER_cancel(iter->scheduled_as);

		if (iter->next) {

			free((iter = iter->next)->prev);
			goto unschedule_task;

		}

		free(iter);
		worker->schedules = NULL;

	}

	atomic_store(&worker->state, WORKER_IS_DEAD);

	#undef worker

}


/**

	@brief      Terminate and free a worker (in most cases, handler added via
	            `GNUNET_SCHEDULER_add_shutdown()`)
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void exit_handler (
	void * const v_worker
) {

	kill_handler(v_worker);
	GNUNET_WORKER_after_kill((GNUNET_WORKER_Handle *) v_worker);

}


/**

	@brief      Perform a task and clean up afterwards
	@param      v_wjob          The member of `GNUNET_WORKER_Handle::schedules`
	                            to run, passed as `void *`       [NON-NULLABLE]

**/
static void call_and_unlist_handler (
	void * const v_wjob
) {

	#define wjob ((GNUNET_WORKER_JobList *) v_wjob)

	if (wjob->assigned_to->schedules == wjob) {

		/*  This is the first job in the list  */

		wjob->assigned_to->schedules = wjob->next;

	} else if (wjob->prev) {

		/*  This is **not** the first job in the list  */

		wjob->prev->next = wjob->next;

	}

	if (wjob->next) {

		/*  This is **not** the last job in the list  */

		wjob->next->prev = wjob->prev;

	}

	wjob->routine(wjob->data);
	free(wjob);

	#undef wjob

}


/**

	@brief      A routine that is woken up by a pipe and schedules new tasks
	            requested by other threads
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void load_request_handler (
	void * const v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	pthread_mutex_lock(&worker->tasks_mutex);

	unsigned char beep;
	GNUNET_WORKER_JobList * const last_wish = worker->wishlist;
	const int what_to_do = worker->future_plans;

	/*  Flush the pipe  */

	if (read(worker->beep_fd[0], &beep, 1) != 1 || beep != BEEP_CODE) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("Unable to read the notification sent to the worker thread\n")
		);

	}

	worker->wishlist = NULL;
	pthread_mutex_unlock(&worker->tasks_mutex);

	switch (what_to_do) {

		case WORKER_MUST_SHUT_DOWN:

			worker->listener_schedule = NULL;
			GNUNET_SCHEDULER_shutdown();
			return;

		case WORKER_MUST_BE_DISMISSED:

			worker->listener_schedule = NULL;
			clear_schedule(&worker->shutdown_schedule);
			exit_handler(worker);
			return;

	}

	if (last_wish) {

		GNUNET_WORKER_JobList * first_wish, * iter = last_wish;

		/*  `worker->wishlist` is processed in chronological order  */

		do {

			first_wish = iter;
			iter = first_wish->next;
			first_wish->next = first_wish->prev;
			first_wish->prev = iter;

		} while (iter);

		iter = first_wish;

		do {

			iter->scheduled_as = GNUNET_SCHEDULER_add_with_priority(
				iter->priority,
				call_and_unlist_handler,
				iter
			);

		} while ((iter = iter->next));

		/*  `worker->schedules` is not kept in chronological order  */

		if (worker->schedules) {

			last_wish->next = worker->schedules;
			worker->schedules->prev = last_wish;

		}

		worker->schedules = first_wish;

	}

	/*  To the next awakening...  */

	worker->listener_schedule =
		atomic_load(&worker->state) == WORKER_IS_ALIVE ?
			GNUNET_SCHEDULER_add_select(
				WORKER_LISTENER_PRIORITY,
				GNUNET_TIME_UNIT_FOREVER_REL,
				worker->beep_fds,
				NULL,
				&load_request_handler,
				v_worker
			)
		:
			NULL;

	#undef worker

}


/**

	@brief      The scheduler's main task that initializes the worker
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void worker_main_routine (
	void * const v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	worker->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
		&kill_handler,
		v_worker
	);

	if (!worker->on_start || worker->on_start(worker->data)) {

		worker->listener_schedule = GNUNET_SCHEDULER_add_select(
			WORKER_LISTENER_PRIORITY,
			GNUNET_TIME_UNIT_FOREVER_REL,
			worker->beep_fds,
			NULL,
			&load_request_handler,
			v_worker
		);

	}

	#undef worker

}


/**

	@brief      The routine that is run in a new thread and invokes the
	            worker's master for `GNUNET_WORKER_start_serving()`
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]
	@return     Nothing

**/
static void * master_thread (
	void * v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	currently_serving_as = NULL;
	worker->master(worker, worker->data);
	return NULL;

	#undef worker

}


/**

	@brief      The routine that launches the GNUnet scheduler
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]
	@return     Nothing

**/
static void * worker_thread (
	void * v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	currently_serving_as = worker;
	GNUNET_SCHEDULER_run(&worker_main_routine, v_worker);

	if (!currently_serving_as) {

		/*  The user has launched `GNUNET_WORKER_dismiss()`  */

		return NULL;

	}

	if (atomic_load(&worker->state) != WORKER_IS_DEAD) {

		/*  If we ended up here `GNUNET_SCHEDULER_add_shutdown()` has a bug  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_("The scheduler has returned unexpectedly\n")
		);

		exit(ECANCELED);

	}

	GNUNET_WORKER_after_kill(worker);
	return NULL;

	#undef worker

}


/**

	@brief      Allocate the memory necessary for a new worker
	@param      save_handle         A placeholder for storing a handle for the
                                    new worker created           [NON-NULLABLE]
	@param      master_routine      A master function to call in a new thread
	                                                                 [NULLABLE]
	@param      on_worker_start     The first routine invoked by the worker
	                                                                 [NULLABLE]
	@param      on_worker_end       The last routine invoked by the worker
	                                                                 [NULLABLE]
	@param      worker_data         The custom data owned by the scheduler
                                                                     [NULLABLE]
	@param      owned_thread        Are we the ones that created the
	                                scheduler's thread?
	@return     A newly allocated (but not running) worker

**/
int GNUNET_WORKER_allocate (
	GNUNET_WORKER_Handle ** save_handle,
	const GNUNET_WORKER_MasterRoutine master_routine,
	const GNUNET_ConfirmRoutine on_worker_start,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data,
	const bool owned_thread
) {

	GNUNET_WORKER_Handle * new_worker = malloc(sizeof(GNUNET_WORKER_Handle));

	if (!new_worker) {

		return GNUNET_WORKER_ERR_NO_MEMORY;

	}

    if (pipe(new_worker->beep_fd) < 0) {

		free(new_worker);
		return GNUNET_WORKER_ERR_SIGNAL;

	}

	requirement_init(&new_worker->scheduler_has_returned, REQ_INIT_RED);
	requirement_init(&new_worker->worker_is_disposable, REQ_INIT_GREEN);
	pthread_mutex_init(&new_worker->tasks_mutex, NULL);
	pthread_mutex_init(&new_worker->kill_mutex, NULL);
	new_worker->wishlist = NULL;
	new_worker->schedules = NULL;
	new_worker->listener_schedule = NULL;
	new_worker->shutdown_schedule = NULL;
	*((GNUNET_WORKER_MasterRoutine *) &new_worker->master) = master_routine;
	*((GNUNET_ConfirmRoutine *) &new_worker->on_start) = on_worker_start;
	*((GNUNET_CallbackRoutine *) &new_worker->on_terminate) = on_worker_end;
	*((void **) &new_worker->data) = worker_data;

	*((struct GNUNET_NETWORK_FDSet **) &new_worker->beep_fds) =
		GNUNET_NETWORK_fdset_create();

	new_worker->state = WORKER_IS_ALIVE;
	new_worker->future_plans = WORKER_MUST_CONTINUE;
	*((bool *) &new_worker->thread_is_owned) = owned_thread;

	/*  Fields left undefined: `::own_thread`  */

	GNUNET_NETWORK_fdset_set_native(
		new_worker->beep_fds,
		new_worker->beep_fd[0]
	);

	*save_handle = new_worker;
	return GNUNET_WORKER_ERR_OK;

}



		/*\
		|*|
		|*|     GLOBAL ENVIRONMENT
		|*|    ________________________________
		\*/



	/*  Please see the public header for the complete documentation  */


/**

	@brief      Terminate a worker and free its memory, without waiting for the
	            scheduler to return (asynchronous)
	@param      worker          The worker to destroy            [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE` and `GNUNET_WORKER_ERR_SIGNAL`

*/
int GNUNET_WORKER_asynch_destroy (
	GNUNET_WORKER_Handle * const worker
) {

	switch (atomic_load(&worker->state)) {

		case WORKER_SAYS_BYE:

			/*  It was still safe to call this function...  */
			return GNUNET_WORKER_ERR_OK;

		case WORKER_IS_ALIVE:

			if (
				currently_serving_as != worker &&
				pthread_mutex_trylock(&worker->kill_mutex)
			) {

		default:

				GNUNET_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				return GNUNET_WORKER_ERR_DOUBLE_FREE;

			}

	}

	atomic_store(
		&worker->state,
		worker->on_terminate ?
			WORKER_SAYS_BYE
		:
			WORKER_IS_DYING
	);

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		if (worker->thread_is_owned) {

			pthread_detach(pthread_self());

		}

		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_ERR_OK;

	}

	if (worker->thread_is_owned) {

		pthread_detach(worker->own_thread);

	}

	requirement_paint_red(&worker->worker_is_disposable);
	pthread_mutex_lock(&worker->tasks_mutex);
	worker->future_plans = WORKER_MUST_SHUT_DOWN;

	int retval;

	if (
		!worker->wishlist &&
		write(worker->beep_fd[1], &BEEP_CODE, 1) != 1
	) {

		/*  This will probably never happen, pipes don't break...  */

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		retval = GNUNET_WORKER_ERR_SIGNAL;

	} else {

		retval = GNUNET_WORKER_ERR_OK;

	}

	pthread_mutex_unlock(&worker->tasks_mutex);
	pthread_mutex_unlock(&worker->kill_mutex);
	requirement_paint_green(&worker->worker_is_disposable);
	return retval;

}


/**

	@brief      Uninstall and destroy a worker without shutting down its
	            scheduler
	@param      worker          The worker to dismiss            [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE` and `GNUNET_WORKER_ERR_SIGNAL`

*/
int GNUNET_WORKER_dismiss (
	GNUNET_WORKER_Handle * const worker
) {

	switch (atomic_load(&worker->state)) {

		case WORKER_SAYS_BYE:

			/*  It was still safe to call this function...  */
			return GNUNET_WORKER_ERR_OK;

		case WORKER_IS_ALIVE:

			if (
				currently_serving_as != worker &&
				pthread_mutex_trylock(&worker->kill_mutex)
			) {

		default:

				GNUNET_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				return GNUNET_WORKER_ERR_DOUBLE_FREE;

			}

	}

	atomic_store(
		&worker->state,
		worker->on_terminate ?
			WORKER_SAYS_BYE
		:
			WORKER_IS_DYING
	);

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		if (worker->thread_is_owned) {

			pthread_detach(pthread_self());

		}

		clear_schedule(&worker->shutdown_schedule);
		exit_handler(worker);
		return GNUNET_WORKER_ERR_OK;

	}

	if (worker->thread_is_owned) {

		pthread_detach(worker->own_thread);

	}

	requirement_paint_red(&worker->worker_is_disposable);
	pthread_mutex_lock(&worker->tasks_mutex);
	worker->future_plans = WORKER_MUST_BE_DISMISSED;

	int retval;

	if (!worker->wishlist && write(worker->beep_fd[1], &BEEP_CODE, 1) != 1) {

		/*  This will probably never happen, pipes don't break...  */

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		retval = GNUNET_WORKER_ERR_SIGNAL;

	} else {

		retval = GNUNET_WORKER_ERR_OK;

	}

	pthread_mutex_unlock(&worker->tasks_mutex);
	pthread_mutex_unlock(&worker->kill_mutex);
	requirement_paint_green(&worker->worker_is_disposable);
	return retval;

}


/**

	@brief      Terminate a worker and free its memory (synchronous)
	@param      worker          The worker to destroy            [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
	            `GNUNET_WORKER_ERR_DOUBLE_FREE`, `GNUNET_WORKER_ERR_UNKNOWN`
	            `GNUNET_WORKER_ERR_NOT_ALONE`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_INTERNAL_BUG`

*/
int GNUNET_WORKER_synch_destroy (
	GNUNET_WORKER_Handle * const worker
) {

	switch (atomic_load(&worker->state)) {

		case WORKER_SAYS_BYE:

			/*  It was still safe to call this function...  */
			return GNUNET_WORKER_ERR_NOT_ALONE;

		case WORKER_IS_ALIVE:

			if (
				currently_serving_as != worker &&
				pthread_mutex_trylock(&worker->kill_mutex)
			) {

		default:

				GNUNET_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				return GNUNET_WORKER_ERR_DOUBLE_FREE;

			}

	}

	atomic_store(
		&worker->state,
		worker->on_terminate ?
			WORKER_SAYS_BYE
		:
			WORKER_IS_DYING
	);

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		if (worker->thread_is_owned) {

			pthread_detach(pthread_self());

		}

		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_ERR_OK;

	}

	requirement_paint_red(&worker->worker_is_disposable);
	pthread_mutex_lock(&worker->tasks_mutex);
	worker->future_plans = WORKER_MUST_SHUT_DOWN;

	int tempval =
		!worker->wishlist &&
		write(worker->beep_fd[1], &BEEP_CODE, 1) != 1;

	pthread_mutex_unlock(&worker->tasks_mutex);

	if (tempval) {

		/*  This will probably never happen, pipes don't break...  */

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		requirement_paint_green(&worker->worker_is_disposable);
		pthread_mutex_unlock(&worker->kill_mutex);
		return GNUNET_WORKER_ERR_SIGNAL;

	}

	pthread_mutex_unlock(&worker->kill_mutex);

	if (worker->thread_is_owned) {

		/*  We started the scheduler's thread: it must be joined  */

		pthread_t thread_ref_copy = worker->own_thread;
		requirement_paint_green(&worker->worker_is_disposable);
		tempval = pthread_join(thread_ref_copy, NULL);

		if (tempval) {

			pthread_detach(thread_ref_copy);

		}

		switch (tempval) {

			case __EOK__: return GNUNET_WORKER_ERR_OK;

			case EDEADLK: case EINVAL: case EPERM: case ESRCH:

				GNUNET_log(
					GNUNET_ERROR_TYPE_WARNING,
					_(
						"`%s` has returned `%d`, possibly due to a bug in the "
						"GNUnet Worker module\n"
					),
					"pthread_join()",
					tempval
				);

				return GNUNET_WORKER_ERR_INTERNAL_BUG;

		}

		/*  This should never happen with a decent C library...  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("`%s` has returned `%d` (unknown code)\n"),
			"pthread_join()",
			tempval
		);

		return GNUNET_WORKER_ERR_UNKNOWN;

	}

	/*  We did not start the scheduler's thread: it must live  */

	tempval = requirement_wait_for_green(&worker->scheduler_has_returned);
	requirement_paint_green(&worker->worker_is_disposable);

	switch (tempval) {

		case __EOK__: return GNUNET_WORKER_ERR_OK;

		case EINVAL: case EPERM:

			GNUNET_log(
				GNUNET_ERROR_TYPE_WARNING,
				_(
					"`%s` has returned `%d`, possibly due to a bug in the "
					"GNUnet Worker module\n"
				),
				"pthread_cond_wait()",
				tempval
			);

			return GNUNET_WORKER_ERR_INTERNAL_BUG;

	}

	/*  This should never happen with a decent C library...  */

	GNUNET_log(
		GNUNET_ERROR_TYPE_WARNING,
		_("`%s` has returned `%d` (unknown code)\n"),
		"pthread_cond_wait()",
		tempval
	);

	return GNUNET_WORKER_ERR_UNKNOWN;

}


/**

	@brief      Terminate a worker and free its memory (synchronous if it
	            happens within a certain time)
	@param      worker          The worker to destroy            [NON-NULLABLE]
	@param      absolute_time   The absolute time to wait until  [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
	            `GNUNET_WORKER_ERR_DOUBLE_FREE`, `GNUNET_WORKER_ERR_EXPIRED`,
	            `GNUNET_WORKER_ERR_INVALID_TIME`, `GNUNET_WORKER_ERR_UNKNOWN`
	            `GNUNET_WORKER_ERR_NOT_ALONE`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_INTERNAL_BUG`

*/
int GNUNET_WORKER_timedsynch_destroy (
	GNUNET_WORKER_Handle * const worker,
	const struct timespec * const absolute_time
) {

	switch (atomic_load(&worker->state)) {

		case WORKER_SAYS_BYE:

			/*  It was still safe to call this function...  */
			return GNUNET_WORKER_ERR_NOT_ALONE;

		case WORKER_IS_ALIVE:

			if (
				currently_serving_as != worker &&
				pthread_mutex_trylock(&worker->kill_mutex)
			) {

		default:

				GNUNET_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				return GNUNET_WORKER_ERR_DOUBLE_FREE;

			}

	}

	atomic_store(
		&worker->state,
		worker->on_terminate ?
			WORKER_SAYS_BYE
		:
			WORKER_IS_DYING
	);

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		if (worker->thread_is_owned) {

			pthread_detach(pthread_self());

		}

		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_ERR_OK;

	}

	requirement_paint_red(&worker->worker_is_disposable);
	pthread_mutex_lock(&worker->tasks_mutex);
	worker->future_plans = WORKER_MUST_SHUT_DOWN;

	int tempval =
		!worker->wishlist &&
		write(worker->beep_fd[1], &BEEP_CODE, 1) != 1;

	pthread_mutex_unlock(&worker->tasks_mutex);

	if (tempval) {

		/*  This will probably never happen, pipes don't break...  */

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		requirement_paint_green(&worker->worker_is_disposable);
		pthread_mutex_unlock(&worker->kill_mutex);
		return GNUNET_WORKER_ERR_SIGNAL;

	}

	pthread_mutex_unlock(&worker->kill_mutex);

	if (worker->thread_is_owned) {

		/*  We started the scheduler's thread: it must be joined  */

		pthread_t thread_ref_copy = worker->own_thread;
		requirement_paint_green(&worker->worker_is_disposable);
		tempval = pthread_timedjoin_np(thread_ref_copy, NULL, absolute_time);

		if (tempval) {

			pthread_detach(thread_ref_copy);

		}

		switch (tempval) {

			case __EOK__: return GNUNET_WORKER_ERR_OK;

			case EDEADLK: case EPERM: case ESRCH:

				GNUNET_log(
					GNUNET_ERROR_TYPE_WARNING,
					_(
						"`%s` has returned `%d`, possibly due to a bug in the "
						"GNUnet Worker module\n"
					),
					"pthread_timedjoin_np()",
					tempval
				);

				return GNUNET_WORKER_ERR_INTERNAL_BUG;

			case ETIMEDOUT: return GNUNET_WORKER_ERR_EXPIRED;

			case EINVAL: return GNUNET_WORKER_ERR_INVALID_TIME;

		}

		/*  This should never happen with a decent C library...  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("`%s` has returned `%d` (unknown code)\n"),
			"pthread_timedjoin_np()",
			tempval
		);

		return GNUNET_WORKER_ERR_UNKNOWN;

	}

	/*  We did not start the scheduler's thread: it must live  */

	tempval = requirement_timedwait_for_green(
		&worker->scheduler_has_returned,
		absolute_time
	);

	requirement_paint_green(&worker->worker_is_disposable);

	switch (tempval) {

		case __EOK__: return GNUNET_WORKER_ERR_OK;

		case ETIMEDOUT: return GNUNET_WORKER_ERR_EXPIRED;

		case EINVAL: return GNUNET_WORKER_ERR_INVALID_TIME;

		case EPERM:

			GNUNET_log(
				GNUNET_ERROR_TYPE_WARNING,
				_(
					"`%s` has returned `%d`, possibly due to a bug in the "
					"GNUnet Worker module\n"
				),
				"pthread_cond_timedwait()",
				tempval
			);

			return GNUNET_WORKER_ERR_INTERNAL_BUG;

	}

	/*  This should never happen with a decent C library...  */

	GNUNET_log(
		GNUNET_ERROR_TYPE_WARNING,
		_("`%s` has returned `%d` (unknown code)\n"),
		"pthread_cond_timedwait()",
		tempval
	);

	return GNUNET_WORKER_ERR_UNKNOWN;

}


/**

	@brief      Schedule a new function for the worker, with a priority
	@param      worker          The worker for which the task must be scheduled
	                                                             [NON-NULLABLE]
	@param      job_priority    The priority of the task
	@param      job_routine     The task to schedule             [NON-NULLABLE]
	@param      job_data        Custom data to pass to the task      [NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_INVALID_HANDLE`

*/
int GNUNET_WORKER_push_load_with_priority (
	GNUNET_WORKER_Handle * const worker,
	enum GNUNET_SCHEDULER_Priority const job_priority,
	const GNUNET_CallbackRoutine job_routine,
	void * const job_data
) {

	switch (atomic_load(&worker->state)) {

		case WORKER_IS_ALIVE:

			requirement_paint_red(&worker->worker_is_disposable);
			pthread_mutex_lock(&worker->tasks_mutex);
			break;

		case WORKER_SAYS_BYE:

			/*

			We return `GNUNET_WORKER_ERR_OK` here. It will appear as if the job
			was scheduled and then immediately cancelled by the shutdown,
			although none of it really took place...

			*/

			return GNUNET_WORKER_ERR_OK;

		default:

			GNUNET_log(
				GNUNET_ERROR_TYPE_ERROR,
				_("Detected attempt to push load into a destroyed worker\n")
			);

			return GNUNET_WORKER_ERR_INVALID_HANDLE;

	}

	int retval = GNUNET_WORKER_ERR_OK;

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		GNUNET_SCHEDULER_add_with_priority(
			job_priority,
			job_routine,
			job_data
		);

		goto unlock_and_exit;

	}

	GNUNET_WORKER_JobList * new_job = malloc(sizeof(GNUNET_WORKER_JobList));

	if (!new_job) {

		retval = GNUNET_WORKER_ERR_NO_MEMORY;
		goto unlock_and_exit;

	}

	*new_job = (GNUNET_WORKER_JobList) {
		.routine = job_routine,
		.scheduled_as = NULL,
		.data = job_data,
		.priority = job_priority,
		.assigned_to = worker,
		.prev = NULL,
		.next = worker->wishlist
	};

	if (worker->wishlist) {

		worker->wishlist->prev = new_job;
		worker->wishlist = new_job;

	} else {

		worker->wishlist = new_job;

		if (write(worker->beep_fd[1], &BEEP_CODE, 1) != 1) {

			/*  Without a "beep" the list stays empty  */

			worker->wishlist = NULL;
			free(new_job);
			retval = GNUNET_WORKER_ERR_SIGNAL;
			goto unlock_and_exit;

		}

	}


	/* \                                /\
	\ */     unlock_and_exit:          /* \
	 \/     ______________________     \ */


	pthread_mutex_unlock(&worker->tasks_mutex);
	requirement_paint_green(&worker->worker_is_disposable);
	return retval;

}


/**

	@brief      Start the GNUnet scheduler in a separate thread
	@param      save_handle         A placeholder for storing a handle for the
	                                new worker created               [NULLABLE]
	@param      on_worker_start     The first routine invoked by the worker,
	                                with @p worker_data passed as argument; the
	                                scheduler will be immediately interrupted
	                                if this function returns `false` [NULLABLE]
	@param      on_worker_end       The last routine invoked by the worker,
	                                with @p worker_data passed as argument
	                                                                 [NULLABLE]
	@param      worker_data         Custom user data retrievable at any moment
	                                                                 [NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_THREAD_CREATE`

*/
int GNUNET_WORKER_create (
    GNUNET_WORKER_Handle ** save_handle,
    const GNUNET_ConfirmRoutine on_worker_start,
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data
) {

	GNUNET_WORKER_Handle * worker;

	const int tempval = GNUNET_WORKER_allocate(
		&worker,
		NULL,
		on_worker_start,
		on_worker_end,
		worker_data,
		true
	);

	if (tempval) {

		return tempval;

	}

	if (
		pthread_create(
			(pthread_t *) &worker->own_thread,
			NULL,
			worker_thread,
			worker
		)
	) {

		GNUNET_WORKER_free(worker);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	if (save_handle) {

		*save_handle = worker;

	}

	return GNUNET_WORKER_ERR_OK;

}


/**

	@brief      Launch the GNUnet scheduler in the current thread and turn it
	            into a worker
	@param      save_handle         A placeholder for storing a handle for the
	                                new worker created               [NULLABLE]
	@param      master_routine      A master function to call in a new detached
	                                thread                           [NULLABLE]
	@param      on_worker_start     The first routine invoked by the worker,
	                                with @p worker_data passed as argument; the
	                                scheduler will be immediately interrupted
	                                if this function returns `false` [NULLABLE]
	@param      on_worker_end       The last routine invoked by the worker,
	                                with @p worker_data passed as argument
	                                                                 [NULLABLE]
	@param      worker_data         Custom user data retrievable at any moment
	                                                                 [NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
	            `GNUNET_WORKER_ERR_ALREADY_SERVING`,
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_THREAD_CREATE`

*/
int GNUNET_WORKER_start_serving (
	GNUNET_WORKER_Handle ** save_handle,
	const GNUNET_WORKER_MasterRoutine master_routine,
	const GNUNET_ConfirmRoutine on_worker_start,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data
) {

	if (currently_serving_as) {

		return GNUNET_WORKER_ERR_ALREADY_SERVING;

	}

	GNUNET_WORKER_Handle * worker;

	const int tempval = GNUNET_WORKER_allocate(
		&worker,
		master_routine,
		on_worker_start,
		on_worker_end,
		worker_data,
		false
	);

	if (tempval) {

		return tempval;

	}

	if (master_routine && thread_create_detached(master_thread, worker)) {

		GNUNET_WORKER_free(worker);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	if (save_handle) {

		*save_handle = worker;

	}

	worker_thread(worker);
	return GNUNET_WORKER_ERR_OK;

}


/**

	@brief      Install a load listener for an already running scheduler and
	            turn the latter into a worker
	@param      save_handle         A placeholder for storing a handle for the
	                                new worker created               [NULLABLE]
	@param      master_routine      A master function to call in a new detached
	                                thread                           [NULLABLE]
	@param      on_worker_end       The last routine invoked by the worker,
	                                with @p worker_data passed as argument
	                                                                 [NULLABLE]
	@param      worker_data         Custom user data retrievable at any moment
	                                                                 [NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
	            `GNUNET_WORKER_ERR_ALREADY_SERVING`,
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_THREAD_CREATE`

*/
int GNUNET_WORKER_adopt_running_scheduler (
    GNUNET_WORKER_Handle ** save_handle,
	const GNUNET_WORKER_MasterRoutine master_routine,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data
) {

	if (currently_serving_as) {

		return GNUNET_WORKER_ERR_ALREADY_SERVING;

	}

	const int tempval = GNUNET_WORKER_allocate(
		&currently_serving_as,
		master_routine,
		NULL,
		on_worker_end,
		worker_data,
		false
	);

	if (tempval) {

		return tempval;

	}

	if (
		master_routine &&
		thread_create_detached(master_thread, currently_serving_as)
	) {

		GNUNET_WORKER_free(currently_serving_as);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	if (save_handle) {

		*save_handle = currently_serving_as;

	}

	currently_serving_as->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
		&exit_handler,
		currently_serving_as
	);

	currently_serving_as->listener_schedule = GNUNET_SCHEDULER_add_select(
		WORKER_LISTENER_PRIORITY,
		GNUNET_TIME_UNIT_FOREVER_REL,
		currently_serving_as->beep_fds,
		NULL,
		&load_request_handler,
		currently_serving_as
	);

	return GNUNET_WORKER_ERR_OK;

}


/**

	@brief      Get the handle of the current worker
	@return     The worker installed in the current thread or `NULL` if this is
	            not a worker thread

**/
GNUNET_WORKER_Handle * GNUNET_WORKER_get_current_handle (void) {

	return currently_serving_as;

}


/**

	@brief      Retrieve the custom data initially passed to the worker
	@param      worker          The worker to query for the data [NON-NULLABLE]
	@return     A pointer to the data initially passed to the worker

**/
void * GNUNET_WORKER_get_data (
	const GNUNET_WORKER_Handle * const worker
) {

	return worker->data;

}


/**

	@brief      Ping the worker and try to wake up its listener function
	@param      worker          The worker to ping               [NON-NULLABLE]
	@return     A boolean: `true` if the ping was successful, `false` otherwise

**/
bool GNUNET_WORKER_ping (
	const GNUNET_WORKER_Handle * const worker
) {

	return write(worker->beep_fd[1], &BEEP_CODE, 1) == 1;

}


/*  EOF  */

