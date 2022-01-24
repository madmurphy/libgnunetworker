/*  -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| gnunet/gnunet_worker_lib.h
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


#ifndef __GNUNET_WORKER_LIB_H__
#define __GNUNET_WORKER_LIB_H__


#include <time.h>
#include <stdbool.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_common.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
    @brief      GNUnet Worker's Errors

    The actual numeric values are subject to change (they are pretty random at
    the moment). Please deal only with the enumeration labels at this stage.
**/
enum GNUNET_WORKER_ErrNo {

    /*  Not at all an error  */
    GNUNET_WORKER_ERR_OK = 0,               /**< Everything OK **/

    /*  Errors that need a change in the user's bad programming to be fixed  */
    GNUNET_WORKER_ERR_INVALID_HANDLE = 1,   /**< The handle is invalid **/
    GNUNET_WORKER_ERR_DOUBLE_FREE = 2,      /**< Double free detected **/
    GNUNET_WORKER_ERR_ALREADY_SERVING = 3,  /**< A worker thread is attempting
                                                 to redefine itself **/
    GNUNET_WORKER_ERR_INVALID_TIME = 4,     /**< Time is invalid **/

    /*  Errors that cannot be fixed (life is hard)  */
    GNUNET_WORKER_ERR_NO_MEMORY = 5,        /**< Not enough memory **/
    GNUNET_WORKER_ERR_THREAD_CREATE = 6,    /**< Unable to launch a new
                                                 thread **/
    GNUNET_WORKER_ERR_NOTIFICATION = 7,     /**< Unable to notify the
                                                 worker **/
    GNUNET_WORKER_ERR_EXPIRED = 8,          /**< Time has expired **/
    GNUNET_WORKER_ERR_UNKNOWN = 9,          /**< Unknown error **/

    /*  Errors that need a change in GNUnet Worker's bad code to be fixed  */
    GNUNET_WORKER_ERR_INTERNAL_BUG = 127    /**< Unexpected error, probably due
                                                 to a bug in GNUnet Worker's
                                                 code **/

};


/**
    @brief      A handle for a worker
**/
typedef struct GNUNET_WORKER_Handle_T GNUNET_WORKER_Handle;


/**
    @brief      Classic callback function
**/
typedef void (* GNUNET_CallbackRoutine) (
    void * data
);


/**
    @brief      Confirm callback function
**/
typedef bool (* GNUNET_ConfirmRoutine) (
    void * data
);


/**
    @brief      Callback function for handling a `GNUNET_WORKER_Handle`
**/
typedef void (* GNUNET_WorkerHandlerRoutine) (
    GNUNET_WORKER_Handle * worker_handle,
    void * data
);


/**
    @brief      Start the GNUnet scheduler in a separate thread
    @param      on_worker_start     The first routine invoked by the worker,
                                    with @p worker_data passed as argument; the
                                    scheduler will be immediately interrupted
                                    if this function returns `false` [NULLABLE]
    @param      on_worker_end       The last routine invoked by the worker,
                                    with @p worker_data passed as argument
                                                                     [NULLABLE]
    @param      worker_data         The custom data owned by the scheduler
                                                                     [NULLABLE]
    @return     A handle for the worker created, or `NULL` if an error occurred

    If you want to exploit the facilities offered by `GNUNET_PROGRAM_run()`
    together with the facilities offered by this framework, you can launch
    `GNUNET_WORKER_create()` inside the `main()` function of your program and
    pass the handle returned to `GNUNET_PROGRAM_run2()` -- called with
    `GNUNET_YES` as last argument, so that another scheduler is not run.
**/
extern GNUNET_WORKER_Handle * GNUNET_WORKER_create (
    const GNUNET_ConfirmRoutine on_worker_start,
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data
);


/**
    @brief      Launch the GNUnet scheduler in the current thread and turn the
                latter into a worker
    @param      master_routine      A function to call in a new thread, invoked
                                    with the newly created worker's handle and
                                    @p worker_data as arguments; this function
                                    will not belong to the scheduler, and
                                    therefore will not be able to invoke the
                                    scheduler's functions directly; however it
                                    will be able invoke all the functions
                                    available in this framework (such as
                                    `GNUNET_WORKER_push_load()`)     [NULLABLE]
    @param      on_worker_start     The first routine invoked by the worker,
                                    with @p worker_data passed as argument; the
                                    scheduler will be immediately interrupted
                                    if this function returns `false` [NULLABLE]
    @param      on_worker_end       The last routine invoked by the worker,
                                    with @p worker_data passed as argument
                                                                     [NULLABLE]
    @param      worker_data         The custom data owned by the scheduler
                                                                     [NULLABLE]
    @param      save_handle         A placeholder for the new
                                    `GNUNET_WORKER_Handle` created   [NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_ALREADY_SERVING`,
                `GNUNET_WORKER_ERR_NO_MEMORY` and
                `GNUNET_WORKER_ERR_THREAD_CREATE`

    This function will not return until the scheduler returns. The @p master
    thread, or equivalently any other thread, must eventually take care of
    destroying the worker after using it, or otherwise the caller thread will
    continue hanging indefinitely with the scheduler running.

    The caller thread, which has now become a worker thread, can also destroy
    shut down the scheduler and free the memory allocated for the worker by
    invoking `GNUNET_SCHEDULER_shutdown()`.

    If @p master_routine is non-`ǸULL`, the function passed will be launched in
    a new detached (non-joinable) thread. If @p master_routine is `NULL` there
    will be apparently little difference between invoking directly
    `GNUNET_SCHEDULER_run()` and invoking this function. However when later new
    threads will be manually launched there will be an important difference:
    `GNUNET_WORKER_push_load()` will now be at other threads' disposal.

    If @p save_handle is not `NULL`, it will be used to store a handle for the
    current session. The variable is set very early, before launching the
    @p master thread and before starting the scheduler.
**/
extern int GNUNET_WORKER_start_serving (
    const GNUNET_WorkerHandlerRoutine master_routine,
    const GNUNET_ConfirmRoutine on_worker_start,
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data,
    GNUNET_WORKER_Handle ** save_handle
);


/**
    @brief      Install a load listener for an already running scheduler and
                turn the latter into a worker
    @param      on_worker_end       The last routine invoked by the worker,
                                    with @p worker_data passed as argument
                                                                     [NULLABLE]
    @param      worker_data         The custom data owned by the scheduler
                                                                     [NULLABLE]
    @return     A handle for the worker created, or `NULL` if an error occurred

    This is the only function that requires that the user has already started
    the GNUnet scheduler manually (either via `GNUNET_SCHEDULER_run()` or
    `GNUNET_PROGRAM_run()`). Calling this function without the scheduler
    running will result in undefined behavior.

    If later the user wants to go back to using the scheduler without
    interferences from other threads the `GNUNET_WORKER_dismiss()` function is
    available.
**/
extern GNUNET_WORKER_Handle * GNUNET_WORKER_adopt_running_scheduler (
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data
);


/**
    @brief      Schedule a new function for the worker
    @param      worker_handle   A pointer to the worker where the task must be
                                scheduled
    @param      job_priority    The priority of the task
    @param      job_routine     The task to schedule
    @param      job_data        Custom data to pass to the task      [NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_NOTIFICATION`
                and `GNUNET_WORKER_ERR_INVALID_HANDLE`

    This function is identical to `GNUNET_WORKER_push_load()`, but allows to
    specify the priority whereby a job must be scheduled.

    The `GNUNET_SCHEDULER_Priority` enumeration type is defined in
    `/usr/include/gnunet/gnunet_common.h`. Possible priorities, from the lowest
    to the highest, are:

    * `GNUNET_SCHEDULER_PRIORITY_IDLE`
    * `GNUNET_SCHEDULER_PRIORITY_BACKGROUND`
    * `GNUNET_SCHEDULER_PRIORITY_DEFAULT`
    * `GNUNET_SCHEDULER_PRIORITY_HIGH`
    * `GNUNET_SCHEDULER_PRIORITY_UI`
    * `GNUNET_SCHEDULER_PRIORITY_URGENT`
    * `GNUNET_SCHEDULER_PRIORITY_SHUTDOWN`

    The functions pushed to the worker thread are free to use all the
    scheduler's utilities (such as `GNUNET_SCHEDULER_add_with_priority()`,
    `GNUNET_SCHEDULER_add_delayed_with_priority()`, etc.). Invoking
    `GNUNET_SCHEDULER_shutdown()` from the scheduler's thread will be
    equivalent to invoking `GNUNET_WORKER_*_destroy()` from another thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    A non-zero return value indicates that @p job_routine was not scheduled
    (the call was no-op and the user may attempt again).
**/
extern int GNUNET_WORKER_push_load_with_priority (
    GNUNET_WORKER_Handle * const worker_handle,
    enum GNUNET_SCHEDULER_Priority const job_priority,
    const GNUNET_CallbackRoutine job_routine,
    void * const job_data
);


/**
    @brief      Schedule a new function for the worker
    @param      worker_handle   A pointer to the worker where the task must be
                                scheduled
    @param      job_routine     The task to schedule
    @param      job_data        Custom data to pass to the task      [NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_NOTIFICATION`
                and `GNUNET_WORKER_ERR_INVALID_HANDLE`

    The functions pushed to the worker thread are free to use all the
    scheduler's utilities (such as `GNUNET_SCHEDULER_add_with_priority()`,
    `GNUNET_SCHEDULER_add_delayed_with_priority()`, etc.). Invoking
    `GNUNET_SCHEDULER_shutdown()` from the scheduler's thread will be
    equivalent to invoking `GNUNET_WORKER_*_destroy()` from another thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    A non-zero return value indicates that @p job_routine was not scheduled
    (the call was no-op and the user may attempt again).
**/
static inline int GNUNET_WORKER_push_load (
    GNUNET_WORKER_Handle * const worker_handle,
    const GNUNET_CallbackRoutine job_routine,
    void * const job_data
) {
    return GNUNET_WORKER_push_load_with_priority(
        worker_handle,
        GNUNET_SCHEDULER_PRIORITY_DEFAULT,
        job_routine,
        job_data
    );
}


/**
    @brief      Terminate a worker and free its memory, without waiting for the
                scheduler to return (asynchronous)
    @param      worker_handle   A pointer to the worker to destroy
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE`
                and `GNUNET_WORKER_ERR_NOTIFICATION`

    This function is used to launch `GNUNET_SCHEDULER_shutdown()` from another
    thread, without waiting for the scheduler to return.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. However, if the user had
    passed an `on_worker_end` routine during the creation of the worker, this
    will be launched now.

    This function can be invoked by any thread (including the worker thread).
    Directly launching `GNUNET_SCHEDULER_shutdown()` from a worker thread
    will have identical effects to launching this function from another thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the scheduler's shutdown (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error.

    Zero or `GNUNET_WORKER_ERR_DOUBLE_FREE` indicate that the scheduler will
    eventually shut down; `GNUNET_WORKER_ERR_NOTIFICATION` instead indicates
    that it was not possible to notify the scheduler about the shutdown (this
    error can be thrown only if this function was not invoked from a worker
    thread). In this case there is not much to do. The return value is caused
    by an error during `write()` into the worker's pipe. The only way to know
    whether the worker has received the message or not is by passing an
    `on_worker_end` routine during the creation of the worker and let it signal
    about the shutdown happening. If no signal arrives it is possible to try to
    wake up the worker for the shutdown by using `GNUNET_WORKER_ping()`. A pipe
    breaking is a very unlikely event to occur, and it might make sense to
    ignore the possible `GNUNET_WORKER_ERR_NOTIFICATION` return value, assume
    that the worker has been destroyed, and tolerate the rare events of workers
    turned into zombies.
**/
extern int GNUNET_WORKER_asynch_destroy (
    GNUNET_WORKER_Handle * const worker_handle
);


/**
    @brief      Terminate a worker and free its memory (synchronous)
    @param      worker_handle   A pointer to the worker to destroy
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE`,
                `GNUNET_WORKER_ERR_INTERNAL_BUG`, `GNUNET_WORKER_ERR_UNKNOWN`
                and `GNUNET_WORKER_ERR_NOTIFICATION`

    This function is used to launch `GNUNET_SCHEDULER_shutdown()` from another
    thread, waiting for the scheduler to return.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. However, if the user had
    passed an `on_worker_end` routine during the creation of the worker, this
    will be launched now.

    This function can be invoked by any thread (including the worker thread).
    Directly launching `GNUNET_SCHEDULER_shutdown()` from a worker thread
    will have identical effects to launching this function from another thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    Any return value different than `GNUNET_WORKER_ERR_NOTIFICATION` indicate
    that the scheduler will shut down. However only zero grants that the
    scheduler is not running anymore (although the thread that hosted the
    scheduler might be still running for the very short time of freeing the
    allocated memory and finally return).

    Any non-zero return value indicates that the function
    has returned before waiting for the scheduler to return, but unless
    `GNUNET_WORKER_ERR_NOTIFICATION` was returned the scheduler _will_
    eventually return and the  passed `GNUNET_WORKER_Handle` _will_
    eventually be destroyed. In fact, any error code different than zero makes
    this function behave like to `GNUNET_WORKER_asynch_destroy()`.

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the scheduler's shutdown (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error.

    A return value of `GNUNET_WORKER_ERR_UNKNOWN` can only be due to a
    non-standard implementation of the `pthread_cond_wait()` function currently
    in use.

    A value of `GNUNET_WORKER_ERR_NOTIFICATION` indicates that it was not
    possible to notify the scheduler about the shutdown (this error can be
    thrown only if this function was not invoked from a worker thread). In this
    case there is not much to do. The return value is caused by an error during
    `write()` into the worker's pipe. The only way to know whether the worker
    has received the message or not is by passing an `on_worker_end` routine
    during the creation of the worker and let it signal about the shutdown
    happening. If no signal arrives it is possible to try to wake up the worker
    for the shutdown by using `GNUNET_WORKER_ping()`. A pipe breaking is a very
    unlikely event to occur, and it might make sense to ignore the possible
    `GNUNET_WORKER_ERR_NOTIFICATION` return value, assume that the worker has
    been destroyed, and tolerate the rare events of workers turned into
    zombies.

    A value of `GNUNET_WORKER_ERR_INTERNAL_BUG` should never be returned;
    please check the log and fill a bug report if it happens.
**/
extern int GNUNET_WORKER_synch_destroy (
    GNUNET_WORKER_Handle * const worker_handle
);


/**
    @brief      Terminate a worker and free its memory (synchronous if it
                happens within a certain time)
    @param      worker_handle   A pointer to the worker to destroy
    @param      absolute_time   The absolute time to wait until
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE`, `GNUNET_WORKER_ERR_EXPIRED`,
                `GNUNET_WORKER_ERR_INVALID_TIME`,
                `GNUNET_WORKER_ERR_INTERNAL_BUG`, `GNUNET_WORKER_ERR_UNKNOWN`
                and `GNUNET_WORKER_ERR_NOTIFICATION`

    This function is used to launch `GNUNET_SCHEDULER_shutdown()` from another
    thread, waiting within a certain time for the scheduler to return.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. However, if the user had
    passed an `on_worker_end` routine during the creation of the worker, this
    will be launched now.

    This function can be invoked by any thread (including the worker thread).
    Directly launching `GNUNET_SCHEDULER_shutdown()` from a worker thread
    will have identical effects to launching this function from another thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    Any return value different than `GNUNET_WORKER_ERR_NOTIFICATION` indicate
    that the scheduler will shut down. However only zero grants that the
    scheduler is not running anymore (although the thread that hosted the
    scheduler might be still running for the very short time of freeing the
    allocated memory and finally return).

    Any non-zero return value indicates that the function
    has returned before waiting for the scheduler to return, but unless
    `GNUNET_WORKER_ERR_NOTIFICATION` was returned the scheduler _will_
    eventually return and the  passed `GNUNET_WORKER_Handle` _will_
    eventually be destroyed. In fact, any error code different than zero makes
    this function behave like to `GNUNET_WORKER_asynch_destroy()`.

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the scheduler's shutdown (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error.

    A return value of `GNUNET_WORKER_ERR_UNKNOWN` can only be due to a
    non-standard implementation of the `pthread_cond_wait()` function currently
    in use.

    A value of `GNUNET_WORKER_ERR_NOTIFICATION` indicates that it was not
    possible to notify the scheduler about the shutdown (this error can be
    thrown only if this function was not invoked from a worker thread). In this
    case there is not much to do. The return value is caused by an error during
    `write()` into the worker's pipe. The only way to know whether the worker
    has received the message or not is by passing an `on_worker_end` routine
    during the creation of the worker and let it signal about the shutdown
    happening. If no signal arrives it is possible to try to wake up the worker
    for the shutdown by using `GNUNET_WORKER_ping()`. A pipe breaking is a very
    unlikely event to occur, and it might make sense to ignore the possible
    `GNUNET_WORKER_ERR_NOTIFICATION` return value, assume that the worker has
    been destroyed, and tolerate the rare events of workers turned into
    zombies.

    A value of `GNUNET_WORKER_ERR_INTERNAL_BUG` should never be returned;
    please check the log and fill a bug report if it happens.
**/
extern int GNUNET_WORKER_timedsynch_destroy (
    GNUNET_WORKER_Handle * const worker_handle,
    const struct timespec * const absolute_time
);


/**
    @brief      Uninstall and destroy a worker without shutting down its
                scheduler
    @param      worker_handle   A pointer to the worker to dismiss
    @return     Possible return values are `GNUNET_WORKER_ERR_OK` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE` and
                `GNUNET_WORKER_ERR_NOTIFICATION`

    Once invoked, this function uninstalls the load listener from the scheduler
    and frees the memory allocated for the worker, but lets the scheduler live.

    Although it can be invoked on any worker, this function is often paired
    with `GNUNET_WORKER_adopt_running_scheduler()`, for making a scheduler
    serve as a worker only temporarily, before continuing its work without
    interferences from other threads.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. However, if the user had
    passed an `on_worker_end` routine during the creation of the worker, this
    will be launched now.
**/
extern int GNUNET_WORKER_dismiss (
    GNUNET_WORKER_Handle * const worker_handle
);


/**
    @brief      Retrieve the custom data initially passed to the worker
    @param      worker_handle   A pointer to the worker to query for the data
    @return     A pointer to the data initially passed to the worker

    The data returned is the `worker_data` argument previously passed to
    `GNUNET_WORKER_create()`, `GNUNET_WORKER_start_serving()` or
    `GNUNET_WORKER_adopt_running_scheduler()` (depending on how the worker was
    created).
**/
extern void * GNUNET_WORKER_get_data (
    const GNUNET_WORKER_Handle * const worker_handle
);


/**
    @brief      Get the handle of the current worker
    @return     A pointer to the `GNUNET_WORKER_Handle` that owns the current
                thread or `NULL` if this is not a worker thread

    In addition to retrieving the current `GNUNET_WORKER_Handle`, this utility
    can be used for detecting whether the current block of code is running
    in the worker thread or not (the returned value will be `NULL` if the
    function was not invoked from the worker thread).
**/
extern GNUNET_WORKER_Handle * GNUNET_WORKER_get_current_handle (void);


/**
    @brief      Ping the worker and wake up its listener function
    @param      worker_handle   A pointer to the worker to ping
    @return     A boolean: `true` if the ping was successful, `false` otherwise

    You will unlikely ever need this function. It is possible to use it to try
    and wake up the worker after a `GNUNET_WORKER_ERR_NOTIFICATION` error was
    returned by the `GNUNET_WORKER_*_destroy()` function family.
**/
extern bool GNUNET_WORKER_ping (
    const GNUNET_WORKER_Handle * const worker_handle
);


#ifdef __cplusplus
}
#endif


#endif

/*  EOF  */

