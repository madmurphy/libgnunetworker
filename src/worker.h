/*  -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */

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


#include <stdint.h>
#include <stdatomic.h>
#include <time.h>
#include <pthread.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_common.h>
#include <gnunet/gnunet_scheduler_lib.h>
#include <gnunet/gnunet_network_lib.h>
#include "include/gnunet_worker_lib.h"
#include "requirement.h"


/**

    @brief      The priority whereby the listener will wake up after a beep

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
#define WORKER_LISTENER_PRIORITY \
    GNUNET_SCHEDULER_PRIORITY_URGENT


/**

    @brief      An alternative to `GNUNET_log()` that prints the name of this
                module

**/
#define GNUNET_WORKER_log(kind, ...) \
	GNUNET_log_from(kind, "worker-lib", __VA_ARGS__)


/**

    @brief      A generic "success" alias for the `pthread_*()` function family

**/
#define __EOK__ 0


/**

    @brief      The function type requested by `pthread_create()`

**/
typedef void * (* __thread_ftype__) (
	void * thread_data
);


/**

    @brief      Possible states of a worker

**/
enum GNUNET_WORKER_State {
    WORKER_IS_ALIVE = 0,    /**< The worker is alive and well **/
    WORKER_SAYS_BYE = 1,    /**< The worker is calling its `on_worker_end` **/
    WORKER_IS_DYING = 2,    /**< The worker might die at any moment now **/
    WORKER_IS_ZOMBIE = 3,   /**< The worker is unable to die (pipe is down) **/
    WORKER_IS_DEAD = 4      /**< The worker is dead, to be disposed soon **/
};


/**

    @brief      Flags set during the creation of a worker

**/
enum GNUNET_WORKER_Flags {
    WORKER_FLAG_NONE = 0,       /**< No flags **/
    WORKER_FLAG_OWN_THREAD = 1, /**< The worker runs in a thread it owns **/
    WORKER_FLAG_IS_GUEST = 2    /**< The worker did not start the scheduler **/
};


/**

    @brief      Doubly linked list containing tasks for the scheduler

**/
typedef struct GNUNET_WORKER_JobList {
    struct GNUNET_WORKER_JobList
        * prev;                     /**< The previous job in the list **/
    struct GNUNET_WORKER_JobList
        * next;                     /**< The next job in the list **/
    GNUNET_WORKER_Handle
        assigned_to;                /**< The worker the job is assigned to **/
    void
        * data;                     /**< The user's custom data for the job **/
    GNUNET_CallbackRoutine
        routine;                    /**< The job's routine **/
    struct GNUNET_SCHEDULER_Task
        * scheduled_as;             /**< A handle for the scheduled task **/
    enum GNUNET_SCHEDULER_Priority
        priority;                   /**< The job's priority **/
} GNUNET_WORKER_JobList;


/**

    @brief      The entire scope of a worker

    Non-`const` fields can be modified either by the worker thread only or by
    multiple threads; in the latter case they are either atomic or a mutual
    exclusion mechanism is provided.

**/
typedef struct GNUNET_WORKER_Instance {
    Requirement
        scheduler_has_returned, /**< The scheduler has returned **/
        worker_is_disposable;   /**< `free()` can be launched on the worker **/
    pthread_mutex_t
        wishes_mutex,           /**< For `::wishlist` and `::future_plans` **/
        kill_mutex;             /**< For various shutting down operations **/
    GNUNET_WORKER_JobList
        * wishlist,             /**< Mutual exclusion via `::wishes_mutex` **/
        * schedules;            /**< Accessed only by the worker thread **/
    struct GNUNET_SCHEDULER_Task
        * listener_schedule,    /**< Accessed only by the worker thread **/
        * shutdown_schedule;    /**< Accessed only by the worker thread **/
    GNUNET_WORKER_MasterRoutine
        const master;           /**< See the `master_routine` argument **/
    GNUNET_WORKER_LifeRoutine
        const on_start;         /**< See the `on_worker_start` argument **/
    GNUNET_CallbackRoutine
        const on_terminate;     /**< See the `on_worker_end` argument **/
    void
        * const data;           /**< See the `worker_data` argument **/
    pthread_t
        const worker_thread;    /**< The worker's thread **/
    struct GNUNET_NETWORK_FDSet
        * const beep_fds;       /**< GNUnet's file descriptor set **/
    int
        const beep_fd[2];       /**< The worker's pipe **/
    unsigned int
        const flags;            /**< See `enum GNUNET_WORKER_Flags` **/
    atomic_int
        state;                  /**< Atomic; see `enum GNUNET_WORKER_State` **/
    GNUNET_WORKER_LifeInstructions
        future_plans;           /**< Mutual exclusion via `::wishes_mutex` **/
} GNUNET_WORKER_Instance;


#endif


/*  EOF  */

