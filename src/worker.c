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


#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <libintl.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_scheduler_lib.h>
#include <gnunet/gnunet_network_lib.h>
#include "gnunet_worker_lib.h"
#include "worker.h"



		/*\
		|*|
		|*|     LOCAL ENVIRONMENT
		|*|    ________________________________
		\*/



	/*  CONSTANTS AND VARIABLES  */


/**
	@brief      A "beep" to notify the worker (any ASCII character will do)
**/
static const unsigned char BEEP_CODE = '\a';


/**
	@brief      The handle of the current worker or `NULL`
**/
_Thread_local static GNUNET_WORKER_Handle * current_worker = NULL;


	/*  INLINED FUNCTIONS  */


/**

	@brief      Free the memory allocated for an uninitialized worker
	@param      worker_handle   A `GNUNET_WORKER_Handle` for the worker to free
	@return     Nothing

**/
static inline void GNUNET_WORKER_uninit_free (
	GNUNET_WORKER_Handle * const worker_handle
) {
	close(worker_handle->beep_fd[0]);
	close(worker_handle->beep_fd[1]);
	GNUNET_NETWORK_fdset_destroy(worker_handle->beep_fds);
	requirement_uninit(&worker_handle->scheduler_has_returned);
	requirement_uninit(&worker_handle->handle_is_disposable);
	pthread_mutex_destroy(&worker_handle->tasks_mutex);
	pthread_mutex_destroy(&worker_handle->state_mutex);
	free(worker_handle);
}


/**

	@brief      Free the memory allocated for an initialized worker and clean
	            the environment
	@param      worker_handle   A `GNUNET_WORKER_Handle` for the worker to free
	@return     Nothing

**/
static inline void GNUNET_WORKER_init_free (
	GNUNET_WORKER_Handle * const worker_handle
) {
	requirement_paint_green(&worker_handle->scheduler_has_returned);
	requirement_wait_for_green(&worker_handle->handle_is_disposable);
	/*  This mutex was locked by `kill_handler()`  */
	pthread_mutex_unlock(&worker_handle->state_mutex);
	current_worker = NULL;
	GNUNET_WORKER_uninit_free(worker_handle);
}



/**

	@brief      Launch `GNUNET_SCHEDULER_cancel()` on a pointer to a scheduled
	            task and set the pointer to `NULL`
	@param      schptr          A pointer to a pointer to a scheduled task
	@return     Nothing

**/
static inline void clear_schedule (
	struct GNUNET_SCHEDULER_Task ** const schptr
) {
	struct GNUNET_SCHEDULER_Task * const tmp = *schptr;
	*schptr = NULL;
	if (tmp) {
		GNUNET_SCHEDULER_cancel(tmp);
	}
}


	/*  FUNCTIONS  */


/**

	@brief      Terminate a worker (in most cases, handler added via
	            `GNUNET_SCHEDULER_add_shutdown()`)
	@param      v_worker_handle     The current `GNUNET_WORKER_Handle` passed
	                                as a pointer to `void`
	@return     Nothing

**/
static void kill_handler (
	void * const v_worker_handle
) {

	#define worker_handle ((GNUNET_WORKER_Handle *) v_worker_handle)

	/*  This mutex will be unlocked by `GNUNET_WORKER_init_free()`  */
	pthread_mutex_lock(&worker_handle->state_mutex);
	worker_handle->state = DYING_WORKER;
	worker_handle->shutdown_schedule = NULL;
	clear_schedule(&worker_handle->listener_schedule);

	GNUNET_WORKER_JobList * iter;

	pthread_mutex_lock(&worker_handle->tasks_mutex);

	if ((iter = worker_handle->wishlist)) {

		while (iter->next) {

			free((iter = iter->next)->prev);

		}

		free(iter);

		worker_handle->wishlist = NULL;

	}

	pthread_mutex_unlock(&worker_handle->tasks_mutex);

	if ((iter = worker_handle->schedules)) {


		/* \                                /\
		\ */     unschedule_task:          /* \
		 \/     ______________________     \ */


		GNUNET_SCHEDULER_cancel(iter->scheduled_as);

		if (iter->next) {

			free((iter = iter->next)->prev);
			goto unschedule_task;

		}

		free(iter);
		worker_handle->schedules = NULL;

	}

	if (worker_handle->on_terminate) {

		worker_handle->on_terminate(worker_handle->data);

	}

	worker_handle->state = DEAD_WORKER;

	#undef worker_handle

}


/**

	@brief      Terminate and free a worker (in most cases, handler added via
	            `GNUNET_SCHEDULER_add_shutdown()`)
	@param      v_worker_handle     The current `GNUNET_WORKER_Handle` passed
	                                as a pointer to `void`
	@return     Nothing

**/
static void exit_handler (
	void * const v_worker_handle
) {

	kill_handler(v_worker_handle);
	GNUNET_WORKER_init_free((GNUNET_WORKER_Handle *) v_worker_handle);

}


/**

	@brief      Perform a task and clean up afterwards
	@param      v_worker_job    The `GNUNET_WORKER_JobList` member to run
	                            passed as a pointer to `void`
	@return     Nothing

**/
static void call_and_unlist_handler (
	void * const v_worker_job
) {

	#define worker_job ((GNUNET_WORKER_JobList *) v_worker_job)

	if (worker_job->owner->schedules == worker_job) {

		/*  This is the first job in the list  */

		worker_job->owner->schedules = worker_job->next;

	} else if (worker_job->prev) {

		/*  This is **not** the first job in the list  */

		worker_job->prev->next = worker_job->next;

	}

	if (worker_job->next) {

		/*  This is **not** the last job in the list  */

		worker_job->next->prev = worker_job->prev;

	}

	worker_job->routine(worker_job->data);
	free(worker_job);

	#undef worker_job

}


/**

	@brief      A routine that is woken up by a pipe and schedules new tasks
	            requested by other threads
	@param      v_worker_handle     The current `GNUNET_WORKER_Handle` passed
	                                as a pointer to `void`
	@return     Nothing

**/
static void load_request_handler (
	void * const v_worker_handle
) {

	#define worker_handle ((GNUNET_WORKER_Handle *) v_worker_handle)

	pthread_mutex_lock(&worker_handle->tasks_mutex);

	unsigned char beep;
	GNUNET_WORKER_JobList * const last_wish = worker_handle->wishlist;
	const int what_to_do = worker_handle->future_plans;

	/*  Flush the pipe  */

	if (read(worker_handle->beep_fd[0], &beep, 1) != 1 || beep != BEEP_CODE) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("Unable to read the notification\n")
		);

	}

	worker_handle->wishlist = NULL;
	pthread_mutex_unlock(&worker_handle->tasks_mutex);

	switch (what_to_do) {

		case WORKER_MUST_SHUT_DOWN:

			worker_handle->listener_schedule = NULL;
			GNUNET_SCHEDULER_shutdown();
			return;

		case WORKER_MUST_BE_DISMISSED:

			worker_handle->listener_schedule = NULL;
			clear_schedule(&worker_handle->shutdown_schedule);
			exit_handler(worker_handle);
			return;

	}

	if (last_wish) {

		GNUNET_WORKER_JobList * first_wish, * iter = last_wish;

		/*  `worker_handle->wishlist` is processed in chronological order  */

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

		/*  `worker_handle->schedules` is not kept in chronological order  */

		if (worker_handle->schedules) {

			last_wish->next = worker_handle->schedules;
			worker_handle->schedules->prev = last_wish;

		}

		worker_handle->schedules = first_wish;

	}

	/*  To the next awakening...  */

	worker_handle->listener_schedule =
		worker_handle->state == ALIVE_WORKER ?
			GNUNET_SCHEDULER_add_select(
				WORKER_LISTENER_PRIORITY,
				GNUNET_TIME_UNIT_FOREVER_REL,
				worker_handle->beep_fds,
				NULL,
				&load_request_handler,
				v_worker_handle
			)
		:
			NULL;

	#undef worker_handle

}


/**

	@brief      The scheduler's main task that initializes the worker for
	            `GNUNET_WORKER_create()`
	@param      v_worker_handle     The current `GNUNET_WORKER_Handle` passed
	                                as a pointer to `void`
	@return     Nothing

**/
static void worker_main_routine (
	void * const v_worker_handle
) {

	#define worker_handle ((GNUNET_WORKER_Handle *) v_worker_handle)

	worker_handle->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
		&kill_handler,
		v_worker_handle
	);

	if (
		!worker_handle->on_start ||
		worker_handle->on_start(worker_handle->data)
	) {

		worker_handle->listener_schedule = GNUNET_SCHEDULER_add_select(
			WORKER_LISTENER_PRIORITY,
			GNUNET_TIME_UNIT_FOREVER_REL,
			worker_handle->beep_fds,
			NULL,
			&load_request_handler,
			v_worker_handle
		);

	}

	#undef worker_handle

}


/**

	@brief      The function that is launched in a separate thread and starts
	            the scheduler for `GNUNET_WORKER_create()`, or the function
	            that does the same in the current thread for
	            `GNUNET_WORKER_start_serving()`
	@param      v_worker_handle     The current `GNUNET_WORKER_Handle` passed
	                                as a pointer to `void`
	@return     Nothing

**/
static void * worker_thread (
	void * v_worker_handle
) {

	#define worker_handle ((GNUNET_WORKER_Handle *) v_worker_handle)

	current_worker = worker_handle;
	GNUNET_SCHEDULER_run(&worker_main_routine, v_worker_handle);

	if (!current_worker) {

		/*  The user has launched `GNUNET_WORKER_dismiss()`  */

		return NULL;

	}

	if (worker_handle->state != DEAD_WORKER) {

		/*  If we ended up here `GNUNET_SCHEDULER_add_shutdown()` has a bug  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_("The scheduler has returned unexpectedly\n")
		);

	}

	GNUNET_WORKER_init_free(worker_handle);
	return NULL;

	#undef worker_handle

}


/**

	@brief      The function that is run in a new thread and invokes the
	            worker's master for `GNUNET_WORKER_start_serving()`
	@param      v_worker_handle     The current `GNUNET_WORKER_Handle` passed
	                                as a pointer to `void`
	@return     Nothing

**/
static void * master_thread (
	void * v_worker_handle
) {

	#define worker_handle ((GNUNET_WORKER_Handle *) v_worker_handle)

	current_worker = NULL;
	worker_handle->master(worker_handle, worker_handle->data);
	return NULL;

	#undef worker_handle

}


/**

	@brief      Allocate memory for a new worker
	@param      master_routine      Same as in `GNUNET_WORKER_start_serving()`
	                                                                 [NULLABLE]
	@param      on_worker_start     Same as in `GNUNET_WORKER_create()` and
	                                `GNUNET_WORKER_start_serving()`  [NULLABLE]
	@param      on_worker_end       Same as in `GNUNET_WORKER_create()` and
	                                `GNUNET_WORKER_start_serving()`  [NULLABLE]
	@param      worker_data         Same as in `GNUNET_WORKER_create()` and
	                                `GNUNET_WORKER_start_serving()`  [NULLABLE]
	@param      owned_thread        We are the ones who started the scheduler's
	                                thread
	@return     A newly allocated (but not running) `GNUNET_WORKER_Handle`


**/
static GNUNET_WORKER_Handle * worker_allocate_scope (
	const GNUNET_WorkerHandlerRoutine master_routine,
	const GNUNET_ConfirmRoutine on_worker_start,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data,
	const bool owned_thread
) {

	GNUNET_WORKER_Handle * new_handle = malloc(sizeof(GNUNET_WORKER_Handle));

	if (!new_handle) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_("Error allocating memory for the worker\n")
		);

		return NULL;

	}

	memcpy(
		new_handle,
		&((GNUNET_WORKER_Handle) {
			.on_start = on_worker_start,
			.on_terminate = on_worker_end,
			.master = master_routine,
			.data = worker_data,
			.wishlist = NULL,
			.listener_schedule = NULL,
			.schedules = NULL,
			.state = ALIVE_WORKER,
			.beep_fds = GNUNET_NETWORK_fdset_create(),
			.scheduler_thread_is_owned = owned_thread
		}),
		sizeof(GNUNET_WORKER_Handle)
	);

    if (pipe(new_handle->beep_fd) < 0) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_(
				"Unable to open a file descriptor for communicating with the "
				"worker\n"
			)
		);

		GNUNET_NETWORK_fdset_destroy(new_handle->beep_fds);
		free(new_handle);
		return NULL;

	}

	pthread_mutex_init(&new_handle->tasks_mutex, NULL);
	pthread_mutex_init(&new_handle->state_mutex, NULL);
	requirement_init(&new_handle->scheduler_has_returned, REQ_INIT_RED);
	requirement_init(&new_handle->handle_is_disposable, REQ_INIT_GREEN);

	GNUNET_NETWORK_fdset_set_native(
		new_handle->beep_fds,
		new_handle->beep_fd[0]
	);

	return new_handle;

}



		/*\
		|*|
		|*|     GLOBAL ENVIRONMENT
		|*|    ________________________________
		\*/



	/*  See the public header for the documentation about this section  */


int GNUNET_WORKER_asynch_destroy (
	GNUNET_WORKER_Handle * const worker_handle
) {

	if (
		worker_handle->state != ALIVE_WORKER || (
			current_worker != worker_handle &&
			pthread_mutex_trylock(&worker_handle->state_mutex)
		)
	) {

		GNUNET_log(GNUNET_ERROR_TYPE_ERROR, _("Double free detected\n"));
		return GNUNET_WORKER_ERR_DOUBLE_FREE;

	}

	worker_handle->state = DYING_WORKER;

	if (current_worker == worker_handle) {

		/*  The user has called this function from the scheduler's thread  */

		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_ERR_OK;

	}

	if (worker_handle->scheduler_thread_is_owned) {

		pthread_detach(worker_handle->own_scheduler_thread);

	}

	requirement_paint_red(&worker_handle->handle_is_disposable);
	pthread_mutex_lock(&worker_handle->tasks_mutex);
	worker_handle->future_plans = WORKER_MUST_SHUT_DOWN;

	int retval;

	if (
		!worker_handle->wishlist &&
		write(worker_handle->beep_fd[1], &BEEP_CODE, 1) != 1
	) {

		/*  This will probably never happen, pipes don't break...  */

		worker_handle->state = ZOMBIE_WORKER;
		retval = GNUNET_WORKER_ERR_NOTIFICATION;

	} else {

		retval = GNUNET_WORKER_ERR_OK;

	}

	pthread_mutex_unlock(&worker_handle->tasks_mutex);
	pthread_mutex_unlock(&worker_handle->state_mutex);
	requirement_paint_green(&worker_handle->handle_is_disposable);
	return retval;

}


int GNUNET_WORKER_dismiss (
	GNUNET_WORKER_Handle * const worker_handle
) {

	if (
		worker_handle->state != ALIVE_WORKER || (
			current_worker != worker_handle &&
			pthread_mutex_trylock(&worker_handle->state_mutex)
		)
	) {

		GNUNET_log(GNUNET_ERROR_TYPE_ERROR, _("Double free detected\n"));
		return GNUNET_WORKER_ERR_DOUBLE_FREE;

	}

	if (current_worker == worker_handle) {

		/*  The user has called this function from the scheduler's thread  */

		clear_schedule(&worker_handle->shutdown_schedule);
		exit_handler(worker_handle);
		return GNUNET_WORKER_ERR_OK;

	}

	if (worker_handle->scheduler_thread_is_owned) {

		pthread_detach(worker_handle->own_scheduler_thread);

	}

	requirement_paint_red(&worker_handle->handle_is_disposable);
	pthread_mutex_lock(&worker_handle->tasks_mutex);
	worker_handle->future_plans = WORKER_MUST_BE_DISMISSED;

	int retval;

	if (
		!worker_handle->wishlist &&
		write(worker_handle->beep_fd[1], &BEEP_CODE, 1) != 1
	) {

		/*  This will probably never happen, pipes don't break...  */

		worker_handle->state = ZOMBIE_WORKER;
		retval = GNUNET_WORKER_ERR_NOTIFICATION;

	} else {

		retval = GNUNET_WORKER_ERR_OK;

	}

	pthread_mutex_unlock(&worker_handle->tasks_mutex);
	pthread_mutex_unlock(&worker_handle->state_mutex);
	requirement_paint_green(&worker_handle->handle_is_disposable);
	return retval;

}


int GNUNET_WORKER_synch_destroy (
	GNUNET_WORKER_Handle * const worker_handle
) {

	if (
		worker_handle->state != ALIVE_WORKER || (
			current_worker != worker_handle &&
			pthread_mutex_trylock(&worker_handle->state_mutex)
		)
	) {

		GNUNET_log(GNUNET_ERROR_TYPE_ERROR, _("Double free detected\n"));
		return GNUNET_WORKER_ERR_DOUBLE_FREE;

	}

	worker_handle->state = DYING_WORKER;

	if (current_worker == worker_handle) {

		/*  The user has called this function from the scheduler's thread  */

		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_ERR_OK;

	}

	requirement_paint_red(&worker_handle->handle_is_disposable);
	pthread_mutex_lock(&worker_handle->tasks_mutex);
	worker_handle->future_plans = WORKER_MUST_SHUT_DOWN;

	int tempval =
		!worker_handle->wishlist &&
		write(worker_handle->beep_fd[1], &BEEP_CODE, 1) != 1;

	pthread_mutex_unlock(&worker_handle->tasks_mutex);

	if (tempval) {

		/*  This will probably never happen, pipes don't break...  */

		worker_handle->state = ZOMBIE_WORKER;
		requirement_paint_green(&worker_handle->handle_is_disposable);
		pthread_mutex_unlock(&worker_handle->state_mutex);
		return GNUNET_WORKER_ERR_NOTIFICATION;

	}

	pthread_mutex_unlock(&worker_handle->state_mutex);

	if (worker_handle->scheduler_thread_is_owned) {

		/*  We started the scheduler's thread: it must be joined  */

		pthread_t thread_ref_copy = worker_handle->own_scheduler_thread;
		requirement_paint_green(&worker_handle->handle_is_disposable);
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
						"`pthread_join()` has returned `%d`, possibly due to "
						"a bug in the GNUnet Worker module\n"
					),
					tempval
				);

				return GNUNET_WORKER_ERR_INTERNAL_BUG;

		}

		/*  This should never happen with a decent C library...  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("`pthread_join()` has returned `%d` (unknown code)\n"),
			tempval
		);

		return GNUNET_WORKER_ERR_UNKNOWN;

	}

	/*  We did not start the scheduler's thread: it must live  */

	tempval = requirement_wait_for_green(
		&worker_handle->scheduler_has_returned
	);

	requirement_paint_green(&worker_handle->handle_is_disposable);

	switch (tempval) {

		case __EOK__: return GNUNET_WORKER_ERR_OK;

		case EINVAL: case EPERM:

			GNUNET_log(
				GNUNET_ERROR_TYPE_WARNING,
				_(
					"`pthread_cond_wait()` has returned `%d`, possibly due to "
					"a bug in the GNUnet Worker module\n"
				),
				tempval
			);

			return GNUNET_WORKER_ERR_INTERNAL_BUG;

	}

	/*  This should never happen with a decent C library...  */

	GNUNET_log(
		GNUNET_ERROR_TYPE_WARNING,
		_("`pthread_cond_wait()` has returned `%d` (unknown code)\n"),
		tempval
	);

	return GNUNET_WORKER_ERR_UNKNOWN;

}


int GNUNET_WORKER_timedsynch_destroy (
	GNUNET_WORKER_Handle * const worker_handle,
	const struct timespec * const absolute_time
) {

	if (
		worker_handle->state != ALIVE_WORKER || (
			current_worker != worker_handle &&
			pthread_mutex_trylock(&worker_handle->state_mutex)
		)
	) {

		GNUNET_log(GNUNET_ERROR_TYPE_ERROR, _("Double free detected\n"));
		return GNUNET_WORKER_ERR_DOUBLE_FREE;

	}

	worker_handle->state = DYING_WORKER;

	if (current_worker == worker_handle) {

		/*  The user has called this function from the scheduler's thread  */

		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_ERR_OK;

	}

	requirement_paint_red(&worker_handle->handle_is_disposable);
	pthread_mutex_lock(&worker_handle->tasks_mutex);
	worker_handle->future_plans = WORKER_MUST_SHUT_DOWN;

	int tempval =
		!worker_handle->wishlist &&
		write(worker_handle->beep_fd[1], &BEEP_CODE, 1) != 1;

	pthread_mutex_unlock(&worker_handle->tasks_mutex);

	if (tempval) {

		/*  This will probably never happen, pipes don't break...  */

		worker_handle->state = ZOMBIE_WORKER;
		requirement_paint_green(&worker_handle->handle_is_disposable);
		pthread_mutex_unlock(&worker_handle->state_mutex);
		return GNUNET_WORKER_ERR_NOTIFICATION;

	}

	pthread_mutex_unlock(&worker_handle->state_mutex);

	if (worker_handle->scheduler_thread_is_owned) {

		/*  We started the scheduler's thread: it must be joined  */

		pthread_t thread_ref_copy = worker_handle->own_scheduler_thread;
		requirement_paint_green(&worker_handle->handle_is_disposable);
		tempval = pthread_timedjoin_np(thread_ref_copy, NULL, absolute_time);

		if (tempval) {

			pthread_detach(thread_ref_copy);

		}

		switch (tempval) {

			case __EOK__: return GNUNET_WORKER_ERR_OK;

			case EBUSY: case EDEADLK: case EPERM: case ESRCH:

				GNUNET_log(
					GNUNET_ERROR_TYPE_WARNING,
					_(
						"`pthread_timedjoin_np()` has returned `%d`, possibly "
						"due to a bug in the GNUnet Worker module\n"
					),
					tempval
				);

				return GNUNET_WORKER_ERR_INTERNAL_BUG;

			case ETIMEDOUT: return GNUNET_WORKER_ERR_EXPIRED;
			case EINVAL: return GNUNET_WORKER_ERR_INVALID_TIME;

		}

		/*  This should never happen with a decent C library...  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("`pthread_join()` has returned `%d` (unknown code)\n"),
			tempval
		);

		return GNUNET_WORKER_ERR_UNKNOWN;

	}

	/*  We did not start the scheduler's thread: it must live  */

	tempval = requirement_timedwait_for_green(
		&worker_handle->scheduler_has_returned,
		absolute_time
	);

	requirement_paint_green(&worker_handle->handle_is_disposable);

	switch (tempval) {

		case __EOK__: return GNUNET_WORKER_ERR_OK;
		case ETIMEDOUT: return GNUNET_WORKER_ERR_EXPIRED;
		case EINVAL: return GNUNET_WORKER_ERR_INVALID_TIME;

		case EPERM:

			GNUNET_log(
				GNUNET_ERROR_TYPE_WARNING,
				_(
					"`pthread_cond_timedwait()` has returned `%d`, possibly "
					"due to a bug in the GNUnet Worker module\n"
				),
				tempval
			);

			return GNUNET_WORKER_ERR_INTERNAL_BUG;

	}

	/*  This should never happen with a decent C library...  */

	GNUNET_log(
		GNUNET_ERROR_TYPE_WARNING,
		_("`pthread_cond_timedwait()` has returned `%d` (unknown code)\n"),
		tempval
	);

	return GNUNET_WORKER_ERR_UNKNOWN;

}


int GNUNET_WORKER_push_load_with_priority (
	GNUNET_WORKER_Handle * const worker_handle,
	enum GNUNET_SCHEDULER_Priority const job_priority,
	const GNUNET_CallbackRoutine job_routine,
	void * const job_data
) {

	int retval = GNUNET_WORKER_ERR_OK;

	requirement_paint_red(&worker_handle->handle_is_disposable);
	pthread_mutex_lock(&worker_handle->tasks_mutex);

	if (worker_handle->state != ALIVE_WORKER) {

		retval = GNUNET_WORKER_ERR_INVALID_HANDLE;
		goto unlock_and_exit;

	}

	if (current_worker == worker_handle) {

		/*  The user has called this function from the scheduler's thread  */

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
		.owner = worker_handle,
		.prev = NULL,
		.next = worker_handle->wishlist
	};

	if (worker_handle->wishlist) {

		worker_handle->wishlist->prev = new_job;
		worker_handle->wishlist = new_job;

	} else {

		worker_handle->wishlist = new_job;

		if (write(worker_handle->beep_fd[1], &BEEP_CODE, 1) != 1) {

			/*  Without a "beep" the list stays empty  */

			worker_handle->wishlist = NULL;
			free(new_job);
			retval = GNUNET_WORKER_ERR_NOTIFICATION;
			goto unlock_and_exit;

		}

	}


	/* \                                /\
	\ */     unlock_and_exit:          /* \
	 \/     ______________________     \ */


	pthread_mutex_unlock(&worker_handle->tasks_mutex);
	requirement_paint_green(&worker_handle->handle_is_disposable);
	return retval;

}


GNUNET_WORKER_Handle * GNUNET_WORKER_create (
	const GNUNET_ConfirmRoutine on_worker_start,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data
) {

	GNUNET_WORKER_Handle * worker_handle = worker_allocate_scope(
		NULL,
		on_worker_start,
		on_worker_end,
		worker_data,
		true
	);

	if (!worker_handle) {

		return NULL;

	}

	if (
		pthread_create(
			&worker_handle->own_scheduler_thread,
			NULL,
			worker_thread,
			worker_handle
		)
	) {

		GNUNET_WORKER_uninit_free(worker_handle);

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_("Unable to start a new thread\n")
		);

		return NULL;

	}

	return worker_handle;

}


int GNUNET_WORKER_start_serving (
	const GNUNET_WorkerHandlerRoutine master_routine,
	const GNUNET_ConfirmRoutine on_worker_start,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data,
	GNUNET_WORKER_Handle ** save_handle
) {

	if (current_worker) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_(
				"`GNUNET_WORKER_start_serving()` has been invoked from a "
				"thread that is already serving as a worker\n"
			)
		);

		return GNUNET_WORKER_ERR_ALREADY_SERVING;

	}

	GNUNET_WORKER_Handle * worker_handle = worker_allocate_scope(
		master_routine,
		on_worker_start,
		on_worker_end,
		worker_data,
		false
	);

	if (
		save_handle ?
			!(*save_handle = worker_handle)
		:
			!worker_handle
	) {

		return GNUNET_WORKER_ERR_NO_MEMORY;

	}

	pthread_t detached_thread;
	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

	if (
		master_routine && pthread_create(
			&detached_thread,
			&tattr,
			master_thread,
			worker_handle
		)
	) {

		GNUNET_WORKER_uninit_free(worker_handle);

		if (save_handle) {

			*save_handle = NULL;

		}

		pthread_attr_destroy(&tattr);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	pthread_attr_destroy(&tattr);
	worker_thread(worker_handle);
	return GNUNET_WORKER_ERR_OK;

}


GNUNET_WORKER_Handle * GNUNET_WORKER_adopt_running_scheduler (
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data
) {

	if (current_worker) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_ERROR,
			_(
				"The current scheduler is already serving as a worker; the "
				"worker's data will not be changed\n"
			)
		);

		return current_worker;

	}

	current_worker = worker_allocate_scope(
		NULL,
		NULL,
		on_worker_end,
		worker_data,
		false
	);

	current_worker->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
		&exit_handler,
		current_worker
	);

	current_worker->listener_schedule = GNUNET_SCHEDULER_add_select(
		WORKER_LISTENER_PRIORITY,
		GNUNET_TIME_UNIT_FOREVER_REL,
		current_worker->beep_fds,
		NULL,
		&load_request_handler,
		current_worker
	);

	return current_worker;

}


GNUNET_WORKER_Handle * GNUNET_WORKER_get_current_handle (void) {

	return current_worker;

}


void * GNUNET_WORKER_get_data (
	const GNUNET_WORKER_Handle * const worker_handle
) {

	return worker_handle->data;

}


bool GNUNET_WORKER_ping (
	const GNUNET_WORKER_Handle * const worker_handle
) {

	return write(worker_handle->beep_fd[1], &BEEP_CODE, 1) == 1;

}


/*  EOF  */

