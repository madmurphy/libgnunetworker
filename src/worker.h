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


/**

	@file       worker.h
	@brief      GNUnet Worker private header

**/


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
#include <gnunet_worker_lib.h>
#include "requirement.h"


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
	`GNUNET_SCHEDULER_PRIORITY_HIGH`, this would anyway have to face the
	listener's low priority bottleneck.

**/
#define WORKER_LISTENER_PRIORITY GNUNET_SCHEDULER_PRIORITY_URGENT


/**

	@brief      Possible states of a worker

**/
enum GNUNET_WorkerState {
	ALIVE_WORKER = 0,   /**< The worker is alive and well **/
	DYING_WORKER = 1,   /**< The worker is not listening (being shut down) **/
	ZOMBIE_WORKER = 2,  /**< The worker is unable to die (pipe is broken) **/
	DEAD_WORKER = 3     /**< The worker is dead, to be disposed soon **/
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

	@brief      Doubly linked list containing tasks for the scheduler

**/
typedef struct GNUNET_WORKER_JobList {
	struct GNUNET_WORKER_JobList * prev;
	struct GNUNET_WORKER_JobList * next;
	GNUNET_WORKER_Handle * assigned_to;
    void * data;
    GNUNET_CallbackRoutine routine;
	struct GNUNET_SCHEDULER_Task * scheduled_as;
	enum GNUNET_SCHEDULER_Priority priority;
} GNUNET_WORKER_JobList;


/**

	@brief      The entire scope of a worker

**/
typedef struct GNUNET_WORKER_Handle {
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
		worker_is_disposable;
	const pthread_t own_scheduler_thread;
	struct GNUNET_NETWORK_FDSet * const beep_fds;
	int beep_fd[2];
	_Atomic enum GNUNET_WorkerState state;
	enum GNUNET_WorkerDestiny future_plans;
	const bool scheduler_thread_is_owned;
} GNUNET_WORKER_Handle;


#endif


/*  EOF  */

