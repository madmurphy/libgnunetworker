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
#include <stdatomic.h>
#include <time.h>
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


/*

Some rules of thumb:

* Keep the mutexes locked as short as possible
* Use `malloc()` for allocating memory in functions that already return error
  codes, and return `GNUNET_WORKER_ERR_NO_MEMORY` when `malloc()` fails;
  otherwise (if functions do not return error codes) use `GNUNET_new()`, which
  never returns `NULL` and aborts if no more memory is available.
* Use log messages only for errors caused by the user or for unexpected or
  potentially fatal events that should never happen; do not use log messages at
  all for ordinary failures
* When you do print log messages, use `GNUNET_log()` for external or unexpected
  events; use `GNUNET_WORKER_log()` instead for avoidable errors that are due
  to misuse of this library

*/



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
	const __thread_ftype__ start_routine,
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


/**

	@brief      Free a pointed `GNUNET_WORKER_JobList` and set the pointer to
	            `NULL`
	@param      jobl_ptr        The pointer to set to `NULL`     [NON-NULLABLE]

	The @p jobl_ptr paramenter cannot be `NULL`, however it can point to
	`NULL`. If that is the case this function is no-op.

**/
static inline void job_list_clear_unlocked (
	GNUNET_WORKER_JobList ** const jobl_ptr
) {
	GNUNET_WORKER_JobList * iter;
	if ((iter = *jobl_ptr)) {
		*jobl_ptr = NULL;
		while (iter->next) {
			free((iter = iter->next)->prev);
		}
		free(iter);
	}
}


/**

	@brief      Lock a mutex, free a pointed `GNUNET_WORKER_JobList`, set the
	            pointer to `NULL` and unlock the mutex
	@param      jobl_ptr        The pointer to set to `NULL`     [NON-NULLABLE]
	@param      mutex_ptr       The mutex to lock and unlock     [NON-NULLABLE]

	The @p jobl_ptr paramenter cannot be `NULL`, however it can point to
	`NULL`. If that is the case this function is no-op.

**/
static inline void job_list_clear_locked (
	GNUNET_WORKER_JobList ** const jobl_ptr,
	pthread_mutex_t * const mutex_ptr
) {
	pthread_mutex_lock(mutex_ptr);
	job_list_clear_unlocked(jobl_ptr);
	pthread_mutex_unlock(mutex_ptr);
}


/**

	@brief      Undo what `GNUNET_WORKER_allocate()` did
	@param      worker          The worker to free               [NON-NULLABLE]

	@note   The two linked lists `GNUNET_WORKER_Handle::schedules` and
	        `GNUNET_WORKER_Handle::wishlist` must be freed separately before
	        calling this function.

**/
static inline void GNUNET_WORKER_unallocate (
	GNUNET_WORKER_Handle * const worker
) {
	close(worker->beep_fd[0]);
	close(worker->beep_fd[1]);
	GNUNET_NETWORK_fdset_destroy(worker->beep_fds);
	requirement_uninit(&worker->scheduler_has_returned);
	requirement_uninit(&worker->worker_is_disposable);
	pthread_mutex_destroy(&worker->wishes_mutex);
	pthread_mutex_destroy(&worker->kill_mutex);
	free(worker);
}


/**

	@brief      Clear the environment and undo what `GNUNET_WORKER_allocate()`
	            did
	@param      worker          The worker to dispose            [NON-NULLABLE]

	Please lock the `worker->kill_mutex` mutex before calling this function and
	never unlock it.

	The worker thread must not paint `worker->worker_is_disposable` red before
	invoking this function, or it will hang forever.

	@note   The two linked lists `GNUNET_WORKER_Handle::schedules` and
	        `GNUNET_WORKER_Handle::wishlist` must be freed separately before
	        calling this function.

**/
static inline void GNUNET_WORKER_dispose (
	GNUNET_WORKER_Handle * const worker
) {
	requirement_paint_green(&worker->scheduler_has_returned);
	requirement_wait_for_green(&worker->worker_is_disposable);
	currently_serving_as = NULL;
	pthread_mutex_unlock(&worker->kill_mutex);
	GNUNET_WORKER_unallocate(worker);
}


/**

	@brief      Clear the environment and undo what `GNUNET_WORKER_allocate()`
	            did, but only if the worker was installed into a pre-existing
	            scheduler via `GNUNET_WORKER_adopt_running_scheduler()`.
	@param      worker          The worker to dispose            [NON-NULLABLE]

	Please lock the `worker->kill_mutex` mutex before calling this function and
	never unlock it.

	If the worker is not a guest worker `GNUNET_WORKER_dispose()` will be
	invoked later by `scheduler_launcher()`.

	The worker thread must not paint `worker->worker_is_disposable` red before
	invoking this function, or it might hang forever.

	@note   The two linked lists `GNUNET_WORKER_Handle::schedules` and
	        `GNUNET_WORKER_Handle::wishlist` must be freed separately before
	        calling this function.

**/
static inline void GNUNET_WORKER_dispose_if_guest (
	GNUNET_WORKER_Handle * const worker
) {
	if (worker->flags & WORKER_FLAG_IS_GUEST) {
		GNUNET_WORKER_dispose(worker);
	}
}


/**

	@brief      Terminate a worker
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static inline void GNUNET_WORKER_terminate (
	GNUNET_WORKER_Handle * const worker
) {
	if (worker->on_terminate) {
		worker->on_terminate(worker->data);
	}
	atomic_store(&worker->state, WORKER_IS_DEAD);
}



	/*  FUNCTIONS  */


/**

	@brief      Cancel all the tasks in a pointed `GNUNET_WORKER_JobList`, free
	            the memory and set the pointer to `NULL`
	@param      jobl_ptr        The pointer to set to `NULL`     [NON-NULLABLE]

	The @p jobl_ptr paramenter cannot be `NULL`, however it can point to
	`NULL`. If that is the case this function is no-op.

**/
static void job_list_unschedule_and_clear (
	GNUNET_WORKER_JobList ** const jobl_ptr
) {

	GNUNET_WORKER_JobList * iter;

	if ((iter = *jobl_ptr)) {

		*jobl_ptr = NULL;


		/* \                                 /\
		\ */     unschedule_task:           /* \
		 \/     _______________________     \ */


		GNUNET_SCHEDULER_cancel(iter->scheduled_as);

		if (iter->next) {

			free((iter = iter->next)->prev);
			goto unschedule_task;

		}

		free(iter);

	}

}


/**

	@brief      Handler added via `GNUNET_SCHEDULER_add_shutdown()` when the
	            shutdown is triggered by some of our functions
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void attended_shutdown_handler (
	void * const v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	/*  `worker->kill_mutex` will be unlocked by `GNUNET_WORKER_dispose()`
		either now or later...  */

	pthread_mutex_lock(&worker->kill_mutex);
	worker->shutdown_schedule = NULL;
	GNUNET_WORKER_terminate(worker);
	GNUNET_WORKER_dispose_if_guest(worker);
	#undef worker

}


/**

	@brief      Handler added via `GNUNET_SCHEDULER_add_shutdown()` when the
	            shutdown is triggered without invoking our functions
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void unattended_shutdown_handler (
	void * const v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	pthread_mutex_lock(&worker->kill_mutex);

	if (
		atomic_load(&worker->state) == WORKER_IS_ALIVE &&
		(worker->flags & WORKER_FLAG_OWN_THREAD)
	) {

		pthread_detach(worker->worker_thread);

	}

	atomic_store(
		&worker->state,
		worker->on_terminate ?
			WORKER_SAYS_BYE
		:
			WORKER_IS_DYING
	);

	clear_schedule(&worker->listener_schedule);
	job_list_unschedule_and_clear(&worker->schedules);
	job_list_clear_locked(&worker->wishlist, &worker->wishes_mutex);
	worker->shutdown_schedule = NULL;
	GNUNET_WORKER_terminate(worker);
	GNUNET_WORKER_dispose_if_guest(worker);

	/*  `worker->kill_mutex` will be unlocked by `GNUNET_WORKER_dispose()`
		either now or later...  */

	#undef worker

}


/**

	@brief      Perform a task and clean up afterwards
	@param      v_job           The member of `GNUNET_WORKER_Handle::schedules`
	                            to run, passed as `void *`       [NON-NULLABLE]

**/
static void call_and_unlist_handler (
	void * const v_job
) {

	#define job ((GNUNET_WORKER_JobList *) v_job)

	if (job->assigned_to->schedules == job) {

		/*  This is the first job in the list  */

		job->assigned_to->schedules = job->next;

	} else if (job->prev) {

		/*  This is **not** the first job in the list  */

		job->prev->next = job->next;

	}

	if (job->next) {

		/*  This is **not** the last job in the list  */

		job->next->prev = job->prev;

	}

	job->routine(job->data);
	free(job);

	#undef job

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

	pthread_mutex_lock(&worker->wishes_mutex);

	GNUNET_WORKER_JobList * const last_wish = worker->wishlist;
	const int what_to_do = worker->future_plans;
	unsigned char beep = BEEP_CODE;

	/*  Flush the pipe  */

	if (
		(
			read(worker->beep_fd[0], &beep, 1) != 1 &&
			worker->listener_schedule
		) || beep != BEEP_CODE
	) {

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_("Unable to read the notification sent to the worker thread\n")
		);

	}

	if (what_to_do != WORKER_MUST_CONTINUE) {

		/*  Worker must die (possibly shutting down the scheduler)  */

		pthread_mutex_lock(&worker->kill_mutex);
		worker->listener_schedule = NULL;
		job_list_clear_unlocked(&worker->wishlist);
		pthread_mutex_unlock(&worker->wishes_mutex);
		clear_schedule(&worker->shutdown_schedule);
		job_list_unschedule_and_clear(&worker->schedules);
		GNUNET_WORKER_terminate(worker);

		/*  `worker->kill_mutex` will be unlocked by `GNUNET_WORKER_dispose()`
			either now or later...  */

		if (what_to_do == WORKER_MUST_SHUT_DOWN) {

			GNUNET_WORKER_dispose_if_guest(worker);
			GNUNET_SCHEDULER_shutdown();
			return;

		} else {

			/*  `what_to_do` equals `WORKER_MUST_BE_DISMISSED`  */

			GNUNET_WORKER_dispose(worker);

		}

		return;

	}

	/*  Worker must live  */

	worker->wishlist = NULL;
	pthread_mutex_unlock(&worker->wishes_mutex);

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
				&call_and_unlist_handler,
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

	@brief      The scheduler's main task that initializes a worker
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]

**/
static void worker_main_routine (
	void * const v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	worker->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
		&unattended_shutdown_handler,
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
	            worker's master for `GNUNET_WORKER_start_serving()` or
	            `GNUNET_WORKER_adopt_running_scheduler()`
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]
	@return     Nothing

**/
static void * master_launcher (
	void * v_worker
) {

	#define worker ((GNUNET_WORKER_Handle *) v_worker)

	currently_serving_as = NULL;
	worker->master(worker, worker->data);
	return NULL;

	#undef worker

}


/**

	@brief      The routine that launches the GNUnet scheduler (often invoked
	            via `pthread_create()`)
	@param      v_worker        The current `GNUNET_WORKER_Handle` passed as
	                            `void *`                         [NON-NULLABLE]
	@return     Nothing

**/
static void * scheduler_launcher (
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
			_(
				"The worker thread's event loop has been unexpectedly cut off "
				"- the scheduler is down\n"
			)
		);

		exit(EINTR);

	}

	GNUNET_WORKER_dispose(worker);
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
	GNUNET_WORKER_Handle ** const save_handle,
	const GNUNET_WORKER_MasterRoutine master_routine,
	const GNUNET_ConfirmRoutine on_worker_start,
	const GNUNET_CallbackRoutine on_worker_end,
	void * const worker_data,
	const unsigned int worker_flags
) {

	GNUNET_WORKER_Handle
		* const new_worker = malloc(sizeof(GNUNET_WORKER_Handle));

	if (!new_worker) {

		return GNUNET_WORKER_ERR_NO_MEMORY;

	}

	if (pipe2(*((int (*)[2]) &new_worker->beep_fd), O_NONBLOCK) < 0) {

		free(new_worker);
		return GNUNET_WORKER_ERR_SIGNAL;

	}

	requirement_init(&new_worker->scheduler_has_returned, REQ_INIT_RED);
	requirement_init(&new_worker->worker_is_disposable, REQ_INIT_GREEN);
	pthread_mutex_init(&new_worker->wishes_mutex, NULL);
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
	*((unsigned int *) &new_worker->flags) = worker_flags;

	/*  Fields left undefined: `::worker_thread`  */

	GNUNET_NETWORK_fdset_set_native(
		new_worker->beep_fds,
		new_worker->beep_fd[0]
	);

	*save_handle = new_worker;
	return GNUNET_WORKER_SUCCESS;

}



		/*\
		|*|
		|*|     GLOBAL ENVIRONMENT
		|*|    ________________________________
		\*/



	/*  Please see the public header for the complete documentation  */


/**

	@brief      Terminate a worker and free its memory, without waiting for the
	            scheduler to return -- this will be completed in parallel
	            (asynchronous)
	@param      worker          The worker to destroy            [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_DOUBLE_FREE` and `GNUNET_WORKER_ERR_SIGNAL`

*/
int GNUNET_WORKER_asynch_destroy (
	GNUNET_WORKER_Handle * const worker
) {

	int retval = GNUNET_WORKER_SUCCESS;

	requirement_paint_red(&worker->worker_is_disposable);

	switch (atomic_load(&worker->state)) {

		case WORKER_IS_ZOMBIE:

			if (currently_serving_as == worker) {

				/*  The zombie will be unzombified...  */

				goto asynch_self_destroy;

			}

			if (write(worker->beep_fd[1], &BEEP_CODE, 1) == 1) {

				/*  The zombie will be unzombified...  */

				goto paint_green_and_exit;

			}

			retval = GNUNET_WORKER_ERR_SIGNAL;
			goto paint_green_and_exit;

		case WORKER_IS_ALIVE:

			if (pthread_mutex_trylock(&worker->kill_mutex)) {

				if (worker->on_terminate) {

		case WORKER_SAYS_BYE:

					/*  It was still safe to call this function...  */

					goto paint_green_and_exit;

				}

		default:

				GNUNET_WORKER_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				retval = GNUNET_WORKER_ERR_DOUBLE_FREE;
				goto paint_green_and_exit;

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

		if (worker->flags & WORKER_FLAG_OWN_THREAD) {

			pthread_detach(worker->worker_thread);

		}


		/* \                                 /\
		\ */     asynch_self_destroy:       /* \
		 \/     _______________________     \ */


		clear_schedule(&worker->listener_schedule);
		job_list_unschedule_and_clear(&worker->schedules);
		job_list_clear_locked(&worker->wishlist, &worker->wishes_mutex);
		GNUNET_SCHEDULER_cancel(worker->shutdown_schedule);

		worker->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
			&attended_shutdown_handler,
			worker
		);

		pthread_mutex_unlock(&worker->kill_mutex);
		requirement_paint_green(&worker->worker_is_disposable);
		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_SUCCESS;

	}

	/*  The user has **not** called this function from the worker thread  */

	if (worker->flags & WORKER_FLAG_OWN_THREAD) {

		pthread_detach(worker->worker_thread);

	}

	pthread_mutex_lock(&worker->wishes_mutex);
	worker->future_plans = WORKER_MUST_SHUT_DOWN;

	if (
		!worker->wishlist &&
		write(worker->beep_fd[1], &BEEP_CODE, 1) != 1
	) {

		/*  Pipe is down...  */

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		retval = GNUNET_WORKER_ERR_SIGNAL;

	} else {

		retval = GNUNET_WORKER_SUCCESS;

	}

	pthread_mutex_unlock(&worker->wishes_mutex);
	pthread_mutex_unlock(&worker->kill_mutex);


	/* \                                 /\
	\ */     paint_green_and_exit:      /* \
	 \/     _______________________     \ */


	requirement_paint_green(&worker->worker_is_disposable);
	return retval;

}


/**

	@brief      Uninstall and destroy a worker without shutting down its
	            scheduler
	@param      worker          The worker to dismiss            [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_DOUBLE_FREE` and `GNUNET_WORKER_ERR_SIGNAL`

*/
int GNUNET_WORKER_dismiss (
	GNUNET_WORKER_Handle * const worker
) {

	int retval = GNUNET_WORKER_SUCCESS;

	requirement_paint_red(&worker->worker_is_disposable);

	switch (atomic_load(&worker->state)) {

		case WORKER_IS_ZOMBIE:

			if (currently_serving_as == worker) {

				/*  The zombie will be unzombified...  */

				goto self_dismiss;

			}

			if (write(worker->beep_fd[1], &BEEP_CODE, 1) == 1) {

				/*  The zombie will be unzombified...  */

				goto paint_green_and_exit;

			}

			retval = GNUNET_WORKER_ERR_SIGNAL;
			goto paint_green_and_exit;

		case WORKER_IS_ALIVE:

			if (pthread_mutex_trylock(&worker->kill_mutex)) {

				if (worker->on_terminate) {

		case WORKER_SAYS_BYE:

					/*  It was still safe to call this function...  */

					goto paint_green_and_exit;

				}

		default:

				GNUNET_WORKER_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				retval = GNUNET_WORKER_ERR_DOUBLE_FREE;
				goto paint_green_and_exit;

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

		if (worker->flags & WORKER_FLAG_OWN_THREAD) {

			pthread_detach(worker->worker_thread);

		}


		/* \                                 /\
		\ */     self_dismiss:              /* \
		 \/     _______________________     \ */


		clear_schedule(&worker->shutdown_schedule);
		clear_schedule(&worker->listener_schedule);
		job_list_clear_locked(&worker->wishlist, &worker->wishes_mutex);
		job_list_unschedule_and_clear(&worker->schedules);
		GNUNET_WORKER_terminate(worker);
		requirement_paint_green(&worker->worker_is_disposable);
		/*  `GNUNET_WORKER_dispose()` will unlock `worker->kill_mutex`...  */
		GNUNET_WORKER_dispose(worker);
		return GNUNET_WORKER_SUCCESS;

	}

	/*  The user has **not** called this function from the worker thread  */

	if (worker->flags & WORKER_FLAG_OWN_THREAD) {

		pthread_detach(worker->worker_thread);

	}

	pthread_mutex_lock(&worker->wishes_mutex);
	worker->future_plans = WORKER_MUST_BE_DISMISSED;

	if (!worker->wishlist && write(worker->beep_fd[1], &BEEP_CODE, 1) != 1) {

		/*  Pipe is down...  */

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		retval = GNUNET_WORKER_ERR_SIGNAL;

	} else {

		retval = GNUNET_WORKER_SUCCESS;

	}

	pthread_mutex_unlock(&worker->wishes_mutex);
	pthread_mutex_unlock(&worker->kill_mutex);


	/* \                                 /\
	\ */     paint_green_and_exit:      /* \
	 \/     _______________________     \ */


	requirement_paint_green(&worker->worker_is_disposable);
	return retval;

}


/**

	@brief      Terminate a worker and free its memory, waiting for the
	            scheduler to complete the shutdown (synchronous)
	@param      worker          The worker to destroy            [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_DOUBLE_FREE`, `GNUNET_WORKER_ERR_UNKNOWN`
	            `GNUNET_WORKER_ERR_NOT_ALONE`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_INTERNAL_BUG`

*/
int GNUNET_WORKER_synch_destroy (
	GNUNET_WORKER_Handle * const worker
) {

	requirement_paint_red(&worker->worker_is_disposable);

	switch (atomic_load(&worker->state)) {

		case WORKER_IS_ZOMBIE:

			if (currently_serving_as == worker) {

				/*  The zombie will be unzombified...  */

				goto synch_self_destroy;

			}

			if (write(worker->beep_fd[1], &BEEP_CODE, 1) == 1) {

				/*  The zombie will be unzombified...  */

				requirement_paint_green(&worker->worker_is_disposable);
				return GNUNET_WORKER_SUCCESS;

			}

			requirement_paint_green(&worker->worker_is_disposable);
			return GNUNET_WORKER_ERR_SIGNAL;

		case WORKER_IS_ALIVE:

			if (pthread_mutex_trylock(&worker->kill_mutex)) {

				if (worker->on_terminate) {

		case WORKER_SAYS_BYE:

					/*  It was still safe to call this function...  */

					requirement_paint_green(&worker->worker_is_disposable);
					return GNUNET_WORKER_ERR_NOT_ALONE;

				}

		default:

				GNUNET_WORKER_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				requirement_paint_green(&worker->worker_is_disposable);
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

		if (worker->flags & WORKER_FLAG_OWN_THREAD) {

			pthread_detach(worker->worker_thread);

		}


		/* \                                 /\
		\ */     synch_self_destroy:        /* \
		 \/     _______________________     \ */


		clear_schedule(&worker->shutdown_schedule);
		clear_schedule(&worker->listener_schedule);
		job_list_clear_locked(&worker->wishlist, &worker->wishes_mutex);
		job_list_unschedule_and_clear(&worker->schedules);
		GNUNET_WORKER_terminate(worker);
		requirement_paint_green(&worker->worker_is_disposable);
		GNUNET_WORKER_dispose_if_guest(worker);
		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_SUCCESS;

		/*  `worker->kill_mutex` will be unlocked by `GNUNET_WORKER_dispose()`
			either now or later...  */

	}

	/*  The user has **not** called this function from the worker thread  */

	pthread_mutex_lock(&worker->wishes_mutex);
	worker->future_plans = WORKER_MUST_SHUT_DOWN;

	int tempval =
		!worker->wishlist &&
		write(worker->beep_fd[1], &BEEP_CODE, 1) != 1;

	pthread_mutex_unlock(&worker->wishes_mutex);

	if (tempval) {

		/*  Pipe is down...  */

		if (worker->flags & WORKER_FLAG_OWN_THREAD) {

			pthread_detach(worker->worker_thread);

		}

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		pthread_mutex_unlock(&worker->kill_mutex);
		requirement_paint_green(&worker->worker_is_disposable);
		return GNUNET_WORKER_ERR_SIGNAL;

	}

	pthread_mutex_unlock(&worker->kill_mutex);

	if (worker->flags & WORKER_FLAG_OWN_THREAD) {

		/*  We started the scheduler's thread: it must be joined  */

		pthread_t thread_ref_copy = worker->worker_thread;
		requirement_paint_green(&worker->worker_is_disposable);
		tempval = pthread_join(thread_ref_copy, NULL);

		if (tempval) {

			pthread_detach(thread_ref_copy);

		}

		switch (tempval) {

			case __EOK__: return GNUNET_WORKER_SUCCESS;

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

		/*  This should not happen with a decent C library...  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_(
				"`%s` has returned `%d` (unknown code) while waiting for the "
				"worker thread to terminate\n"
			),
			"pthread_join()",
			tempval
		);

		return GNUNET_WORKER_ERR_UNKNOWN;

	}

	/*  We did not start the scheduler's thread: it must live  */

	tempval = requirement_wait_for_green(&worker->scheduler_has_returned);
	requirement_paint_green(&worker->worker_is_disposable);

	switch (tempval) {

		case __EOK__: return GNUNET_WORKER_SUCCESS;

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

	/*  This should not happen with a decent C library...  */

	GNUNET_log(
		GNUNET_ERROR_TYPE_WARNING,
		_(
			"`%s` has returned `%d` (unknown code) while waiting for the "
			"worker thread to terminate\n"
		),
		"pthread_cond_wait()",
		tempval
	);

	return GNUNET_WORKER_ERR_UNKNOWN;

}


/**

	@brief      Terminate a worker and free its memory, waiting for the
	            scheduler to complete the shutdown (synchronous), but only if
	            this happens within a certain time, otherwise it will be
	            completed in parallel (asynchronous)
	@param      worker          The worker to destroy            [NON-NULLABLE]
	@param      absolute_time   The absolute time to wait until  [NON-NULLABLE]
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_DOUBLE_FREE`, `GNUNET_WORKER_ERR_EXPIRED`,
	            `GNUNET_WORKER_ERR_INVALID_TIME`, `GNUNET_WORKER_ERR_UNKNOWN`
	            `GNUNET_WORKER_ERR_NOT_ALONE`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_INTERNAL_BUG`

*/
int GNUNET_WORKER_timedsynch_destroy (
	GNUNET_WORKER_Handle * const worker,
	const struct timespec * const absolute_time
) {

	requirement_paint_red(&worker->worker_is_disposable);

	switch (atomic_load(&worker->state)) {

		case WORKER_IS_ZOMBIE:

			if (currently_serving_as == worker) {

				/*  The zombie will be unzombified...  */

				goto synch_self_destroy;

			}

			if (write(worker->beep_fd[1], &BEEP_CODE, 1) == 1) {

				/*  The zombie will be unzombified...  */

				requirement_paint_green(&worker->worker_is_disposable);
				return GNUNET_WORKER_SUCCESS;

			}

			requirement_paint_green(&worker->worker_is_disposable);
			return GNUNET_WORKER_ERR_SIGNAL;

		case WORKER_IS_ALIVE:

			if (pthread_mutex_trylock(&worker->kill_mutex)) {

				if (worker->on_terminate) {

		case WORKER_SAYS_BYE:

					/*  It was still safe to call this function...  */

					requirement_paint_green(&worker->worker_is_disposable);
					return GNUNET_WORKER_ERR_NOT_ALONE;

				}

		default:

				GNUNET_WORKER_log(
					GNUNET_ERROR_TYPE_ERROR,
					_("Double free detected\n")
				);

				requirement_paint_green(&worker->worker_is_disposable);
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

		if (worker->flags & WORKER_FLAG_OWN_THREAD) {

			pthread_detach(worker->worker_thread);

		}


		/* \                                 /\
		\ */     synch_self_destroy:        /* \
		 \/     _______________________     \ */


		clear_schedule(&worker->shutdown_schedule);
		clear_schedule(&worker->listener_schedule);
		job_list_clear_locked(&worker->wishlist, &worker->wishes_mutex);
		job_list_unschedule_and_clear(&worker->schedules);
		GNUNET_WORKER_terminate(worker);
		requirement_paint_green(&worker->worker_is_disposable);
		GNUNET_WORKER_dispose_if_guest(worker);
		GNUNET_SCHEDULER_shutdown();
		return GNUNET_WORKER_SUCCESS;

		/*  `worker->kill_mutex` will be unlocked by `GNUNET_WORKER_dispose()`
			either now or later...  */

	}

	/*  The user has **not** called this function from the worker thread  */

	pthread_mutex_lock(&worker->wishes_mutex);
	worker->future_plans = WORKER_MUST_SHUT_DOWN;

	int tempval =
		!worker->wishlist &&
		write(worker->beep_fd[1], &BEEP_CODE, 1) != 1;

	pthread_mutex_unlock(&worker->wishes_mutex);

	if (tempval) {

		/*  Pipe is down...  */

		if (worker->flags & WORKER_FLAG_OWN_THREAD) {

			pthread_detach(worker->worker_thread);

		}

		atomic_store(&worker->state, WORKER_IS_ZOMBIE);
		pthread_mutex_unlock(&worker->kill_mutex);
		requirement_paint_green(&worker->worker_is_disposable);
		return GNUNET_WORKER_ERR_SIGNAL;

	}

	pthread_mutex_unlock(&worker->kill_mutex);

	if (worker->flags & WORKER_FLAG_OWN_THREAD) {

		/*  We started the scheduler's thread: it must be joined  */

		pthread_t thread_ref_copy = worker->worker_thread;
		requirement_paint_green(&worker->worker_is_disposable);
		tempval = pthread_timedjoin_np(thread_ref_copy, NULL, absolute_time);

		if (tempval) {

			pthread_detach(thread_ref_copy);

		}

		switch (tempval) {

			case __EOK__: return GNUNET_WORKER_SUCCESS;

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

		/*  This should not happen with a decent C library...  */

		GNUNET_log(
			GNUNET_ERROR_TYPE_WARNING,
			_(
				"`%s` has returned `%d` (unknown code) while waiting for the "
				"worker thread to terminate\n"
			),
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

		case __EOK__: return GNUNET_WORKER_SUCCESS;

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

	/*  This should not happen with a decent C library...  */

	GNUNET_log(
		GNUNET_ERROR_TYPE_WARNING,
		_(
			"`%s` has returned `%d` (unknown code) while waiting for the "
			"worker thread to terminate\n"
		),
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
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_INVALID_HANDLE`

*/
int GNUNET_WORKER_push_load_with_priority (
	GNUNET_WORKER_Handle * const worker,
	const enum GNUNET_SCHEDULER_Priority job_priority,
	const GNUNET_CallbackRoutine job_routine,
	void * const job_data
) {

	requirement_paint_red(&worker->worker_is_disposable);

	int retval = GNUNET_WORKER_SUCCESS;

	switch (atomic_load(&worker->state)) {

		case WORKER_IS_ALIVE:

			break;

		case WORKER_IS_ZOMBIE:

			if (currently_serving_as == worker) {

				/*  The zombie will be unzombified...  */

				/*

				We return `GNUNET_WORKER_SUCCESS` here. It will appear as if
				the job was scheduled and then immediately cancelled by the
				shutdown, although none of it really took place...

				*/

				clear_schedule(&worker->listener_schedule);
				requirement_paint_green(&worker->worker_is_disposable);
				load_request_handler(worker);
				return GNUNET_WORKER_SUCCESS;

			}

			if (write(worker->beep_fd[1], &BEEP_CODE, 1) != 1) {

				retval = GNUNET_WORKER_ERR_SIGNAL;
				goto paint_green_and_exit;

			}

			/*  The zombie will be unzombified...  */

			/*  No case break (fallthrough)  */

		case WORKER_SAYS_BYE:

			/*

			We return `GNUNET_WORKER_SUCCESS` here. It will appear as if the
			job was scheduled and then immediately cancelled by the shutdown,
			although none of it really took place...

			*/

			goto paint_green_and_exit;

		default:

			GNUNET_WORKER_log(
				GNUNET_ERROR_TYPE_ERROR,
				_(
					"An attempt to push load into a destroyed worker has been "
					"detected\n"
				)
			);

			retval = GNUNET_WORKER_ERR_INVALID_HANDLE;
			goto paint_green_and_exit;

	}

	GNUNET_WORKER_JobList * new_job = malloc(sizeof(GNUNET_WORKER_JobList));

	if (!new_job) {

		retval = GNUNET_WORKER_ERR_NO_MEMORY;
		goto paint_green_and_exit;

	}

	new_job->routine = job_routine;
	new_job->data = job_data;
	new_job->priority = job_priority;
	new_job->assigned_to = worker;
	new_job->prev = NULL;

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		if (worker->schedules) {

			worker->schedules->prev = new_job;

		}

		new_job->next = worker->schedules;
		worker->schedules = new_job;

		new_job->scheduled_as = GNUNET_SCHEDULER_add_with_priority(
			job_priority,
			call_and_unlist_handler,
			new_job
		);

		goto paint_green_and_exit;

	}

	/*  The user has **not** called this function from the worker thread  */

	new_job->scheduled_as = NULL;
	pthread_mutex_lock(&worker->wishes_mutex);
	new_job->next = worker->wishlist;

	if (worker->wishlist) {

		worker->wishlist->prev = new_job;
		worker->wishlist = new_job;

	} else {

		worker->wishlist = new_job;

		if (write(worker->beep_fd[1], &BEEP_CODE, 1) != 1) {

			/*  Without a "beep" the list stays empty...  */

			worker->wishlist = NULL;
			free(new_job);
			retval = GNUNET_WORKER_ERR_SIGNAL;

		}

	}

	pthread_mutex_unlock(&worker->wishes_mutex);


	/* \                                 /\
	\ */     paint_green_and_exit:      /* \
	 \/     _______________________     \ */


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
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_THREAD_CREATE`

*/
int GNUNET_WORKER_create (
	GNUNET_WORKER_Handle ** const save_handle,
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
		WORKER_FLAG_OWN_THREAD
	);

	if (tempval) {

		return tempval;

	}

	if (
		pthread_create(
			(pthread_t *) &worker->worker_thread,
			NULL,
			&scheduler_launcher,
			worker
		)
	) {

		GNUNET_WORKER_unallocate(worker);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	if (save_handle) {

		*save_handle = worker;

	}

	return GNUNET_WORKER_SUCCESS;

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
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_ALREADY_SERVING`,
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_THREAD_CREATE`

*/
int GNUNET_WORKER_start_serving (
	GNUNET_WORKER_Handle ** const save_handle,
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
		WORKER_FLAG_NONE
	);

	if (tempval) {

		return tempval;

	}

	/*  This is not strictly necessary at the moment (`::worker_thread` can be
		left undefined if we are not the ones who created the worker thread).
		However we might want to add thread-related utilities to the GNUnet
		Worker API in the future...  */

	*((pthread_t *) &worker->worker_thread) = pthread_self();

	if (master_routine && thread_create_detached(&master_launcher, worker)) {

		GNUNET_WORKER_unallocate(worker);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	if (save_handle) {

		*save_handle = worker;

	}

	scheduler_launcher(worker);
	return GNUNET_WORKER_SUCCESS;

}


/**

	@brief      Install a load listener into an already running scheduler and
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
	@return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
	            `GNUNET_WORKER_ERR_ALREADY_SERVING`,
	            `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
	            `GNUNET_WORKER_ERR_THREAD_CREATE`

*/
int GNUNET_WORKER_adopt_running_scheduler (
	GNUNET_WORKER_Handle ** const save_handle,
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
		WORKER_FLAG_IS_GUEST
	);

	if (tempval) {

		return tempval;

	}

	/*  This is not strictly necessary at the moment (`::worker_thread` can be
		left undefined if we are not the ones who created the worker thread).
		However we might want to add thread-related utilities to the GNUnet
		Worker API in the future...  */

	*((pthread_t *) &currently_serving_as->worker_thread) = pthread_self();

	if (
		master_routine &&
		thread_create_detached(&master_launcher, currently_serving_as)
	) {

		GNUNET_WORKER_unallocate(currently_serving_as);
		return GNUNET_WORKER_ERR_THREAD_CREATE;

	}

	currently_serving_as->shutdown_schedule = GNUNET_SCHEDULER_add_shutdown(
		&unattended_shutdown_handler,
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

	if (save_handle) {

		*save_handle = currently_serving_as;

	}

	return GNUNET_WORKER_SUCCESS;

}


/**

	@brief      Get the handle of the current worker if this is a worker thread
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
	GNUNET_WORKER_Handle * const worker
) {

	if (currently_serving_as == worker) {

		/*  The user has called this function from the worker thread  */

		clear_schedule(&worker->listener_schedule);
		load_request_handler(worker);
		return true;

	}

	/*  The user has **not** called this function from the worker thread  */

	return write(worker->beep_fd[1], &BEEP_CODE, 1) == 1;

}


/*  EOF  */

