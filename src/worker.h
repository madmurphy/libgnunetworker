/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| libgnunetworker/src/worker.h
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


#ifndef __GNUNET_WORKER_PRIVATE_HEADER__
#define __GNUNET_WORKER_PRIVATE_HEADER__


#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_common.h>
#include <gnunet/gnunet_scheduler_lib.h>
#include <gnunet/gnunet_network_lib.h>
#include "gnunet_worker_lib.h"


/**
	@brief      A generic "success" alias for the `pthread_*()` function family
**/
#define __EOK__ 0


/**
	@brief      The priority whereby the listener will be woken up after a beep

	What priority should we assign to this? The listener itself can schedule
	jobs with different priorities, including potentially high priority ones,
	and after detecting a shutdown it will invoke `GNUNET_SCHEDULER_shutdown()`
	directly, without scheduling it.

	Would it make sense to give low priority to it, instead of high priority?

	In favor of low priority: if threads push multiple jobs at once the
	listener will likely wake up less often and will schedule more tasks in a
	single run.

	Against low priority: if the user pushes a job with
	`GNUNET_SCHEDULER_PRIORITY_HIGH`, this will anyway have to face the
	listener's low priority bottleneck.
**/
#define WORKER_LISTENER_PRIORITY GNUNET_SCHEDULER_PRIORITY_URGENT


/**
	@brief      Possible states of a worker
**/
enum GNUNET_WorkerState {
	ALIVE_WORKER = 0,	/**< The worker is alive and well **/
	DYING_WORKER = 1,	/**< The worker is not listening (being shut down) **/
	ZOMBIE_WORKER = 2,	/**< The worker is unable to die (pipe is broken) **/
	DEAD_WORKER = 3		/**< The worker is dead, to be disposed soon **/
};


/**
	@brief      Possible future plans for a worker
**/
enum GNUNET_WorkerDestiny {
	WORKER_MUST_CONTINUE = 0,
	WORKER_MUST_SHUT_DOWN = 1,
	WORKER_MUST_BE_DISMISSED = 2
};


/**
	@brief      A `struct` that can be modified by one thread at a time
**/
typedef struct Requirement_T {
	pthread_mutex_t req_mutex;
	pthread_cond_t req_cond;
	unsigned int req_unfulfillment;
} Requirement;


/**
	@brief      Doubly linked list containing tasks for the scheduler
**/
typedef struct GNUNET_WORKER_JobList_T {
	struct GNUNET_WORKER_JobList_T * prev;
	struct GNUNET_WORKER_JobList_T * next;
	GNUNET_WORKER_Handle * owner;
    void * data;
    GNUNET_CallbackRoutine routine;
	struct GNUNET_SCHEDULER_Task * scheduled_as;
	enum GNUNET_SCHEDULER_Priority priority;
} GNUNET_WORKER_JobList;


/**
	@brief      The entire scope of a worker
**/
typedef struct GNUNET_WORKER_Handle_T {
	GNUNET_WORKER_JobList
		* wishlist,
		* schedules;
	struct GNUNET_SCHEDULER_Task
		* listener_schedule,
		* shutdown_schedule;
	const GNUNET_WorkerHandlerRoutine master;
	const GNUNET_ConfirmRoutine on_start;
	const GNUNET_CallbackRoutine on_terminate;
	void * const data;
	pthread_mutex_t
		tasks_mutex,
		state_mutex;
	Requirement
		scheduler_has_returned,
		handle_is_disposable;
	pthread_t own_scheduler_thread;
	struct GNUNET_NETWORK_FDSet * const beep_fds;
	int beep_fd[2];
	_Atomic enum GNUNET_WorkerState state;
	enum GNUNET_WorkerDestiny future_plans;
	const bool scheduler_thread_is_owned;
} GNUNET_WORKER_Handle;


/**
	@brief      Possible initialization values of a requirement
**/
enum REQUIREMENT_InitValue {
	REQ_INIT_GREEN = 0,
	REQ_INIT_RED = 1
};


/**
	@brief      Initialize a requirement
	@param      requirement     The requirement to init
	@return     Nothing
**/
static inline void requirement_init (
	Requirement * const requirement,
	enum REQUIREMENT_InitValue initial_value
) {
	pthread_mutex_init(&requirement->req_mutex, NULL);
	pthread_cond_init(&requirement->req_cond, NULL);
	requirement->req_unfulfillment = initial_value;
}


/**
	@brief      Uninitialize a requirement
	@param      requirement     The requirement to destroy
	@return     Nothing
**/
static inline void requirement_uninit (
	Requirement * const requirement
) {
	pthread_mutex_destroy(&requirement->req_mutex);
	pthread_cond_destroy(&requirement->req_cond);
}


/**
	@brief      Mark a requirement as "unfulfilled"
	@param      requirement     The requirement to mark as unfulfilled
	@return     Nothing
**/
static inline void requirement_paint_red (
	Requirement * const requirement
) {
	pthread_mutex_lock(&requirement->req_mutex);
	requirement->req_unfulfillment++;
	pthread_mutex_unlock(&requirement->req_mutex);
}


/**
	@brief      Mark a requirement as "fulfilled"
	@param      requirement     The requirement to mark as fulfilled
	@return     Nothing
**/
static inline void requirement_paint_green (
	Requirement * const requirement
) {
	pthread_mutex_lock(&requirement->req_mutex);
	if (requirement->req_unfulfillment > 0) {
		requirement->req_unfulfillment--;
	}
	pthread_cond_signal(&requirement->req_cond);
	pthread_mutex_unlock(&requirement->req_mutex);
}


/**
	@brief      Wait until a requirement is fulfilled
	@param      requirement     The requirement to wait for
    @param      absolute_time   The absolute time to wait until
	@return     The result of `pthread_cond_wait()`
**/
static inline int requirement_wait_for_green (
	Requirement * const requirement
) {
	int retval = 0;
	pthread_mutex_lock(&requirement->req_mutex);
	while (requirement->req_unfulfillment) {
		retval = pthread_cond_wait(
			&requirement->req_cond,
			&requirement->req_mutex
		);
	}
	pthread_mutex_unlock(&requirement->req_mutex);
	return retval;
}


/**
	@brief      Wait until a requirement is fulfilled, with a time limit
	@param      requirement     The requirement to wait for
    @param      absolute_time   The absolute time to wait until
	@return     The result of `pthread_cond_timedwait()`
**/
static inline int requirement_timedwait_for_green (
	Requirement * const requirement,
	const struct timespec * const absolute_time
) {
	int retval = 0;
	pthread_mutex_lock(&requirement->req_mutex);
	while (requirement->req_unfulfillment) {
		retval = pthread_cond_timedwait(
			&requirement->req_cond,
			&requirement->req_mutex,
			absolute_time
		);
	}
	pthread_mutex_unlock(&requirement->req_mutex);
	return retval;
}


#endif

/*  EOF  */

