/*  -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| gnunet_worker_lib.h
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

    @file       gnunet_worker_lib.h
    @brief      Multithreading with GNUnet

**/


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
    the moment); please deal only with the enumeration labels at this stage.

**/
enum GNUNET_WORKER_ErrNo {

    /*  Not at all an error  */
    GNUNET_WORKER_SUCCESS = 0,              /**< Everything OK [value=0] **/

    /*  Errors that need a change in the user's bad programming to be fixed  */
    GNUNET_WORKER_ERR_DOUBLE_FREE = 1,      /**< Double free detected **/
    GNUNET_WORKER_ERR_INVALID_HANDLE = 2,   /**< The handle is invalid **/
    GNUNET_WORKER_ERR_ALREADY_SERVING = 3,  /**< A worker thread is attempting
                                                 to redefine itself **/
    GNUNET_WORKER_ERR_INVALID_TIME = 4,     /**< Time is invalid **/

    /*  Errors that cannot be fixed (life is hard)  */
    GNUNET_WORKER_ERR_EXPIRED = 5,          /**< Time has expired **/
    GNUNET_WORKER_ERR_NOT_ALONE = 6,        /**< Another thread is waiting for
                                                 the same to happen  **/
    GNUNET_WORKER_ERR_NO_MEMORY = 7,        /**< Not enough memory **/
    GNUNET_WORKER_ERR_THREAD_CREATE = 8,    /**< Unable to launch a new
                                                 thread **/
    GNUNET_WORKER_ERR_SIGNAL = 9,           /**< Error in the communication
                                                 with the worker **/
    GNUNET_WORKER_ERR_UNKNOWN = 10,         /**< Unknown/unexpected error **/

    /*  Errors that need a change in GNUnet Worker's bad code to be fixed  */
    GNUNET_WORKER_ERR_INTERNAL_BUG = 127    /**< Unexpected error, probably due
                                                 to a bug in the GNUnet Worker
                                                 module **/

};


/**

    @brief      A handle for a worker

**/
typedef struct GNUNET_WORKER_Handle GNUNET_WORKER_Handle;


/**

    @brief      Generic callback function

**/
typedef void (* GNUNET_CallbackRoutine) (
    void * data
);


/**

    @brief      Generic confirm callback function

**/
typedef bool (* GNUNET_ConfirmRoutine) (
    void * data
);


/**

    @brief      Callback function for handling a worker thread

**/
typedef void (* GNUNET_WORKER_MasterRoutine) (
    GNUNET_WORKER_Handle * worker,
    void * data
);


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

    Any non-zero value indicates that the worker has not been created and the
    call was no-op.

    If you want to exploit the facilities offered by `GNUNET_PROGRAM_run()`
    together with the facilities offered by this framework, you can launch
    `GNUNET_PROGRAM_run2()` -- called with `GNUNET_YES` as last argument so
    that another GNUnet scheduler is not run -- and then invoke
    `GNUNET_WORKER_create()` inside the `GNUNET_PROGRAM_Main` function of your
    application. See the comments in `examples/gtk4-example/foobar-main.c` for
    a possible example.

    The @p on_worker_end routine is not like any routine, but represents a
    point of no return after which it becomes illegal to attempt to destroy a
    worker (and, if you are lucky, attempting to do so will result in an error
    message and a `GNUNET_WORKER_ERR_DOUBLE_FREE` error code). On the other
    hand, attempting to destroy a worker before @p on_worker_end has returned
    is always safe. Thus, if a program allows different threads to destroy a
    worker and these do not behave deterministically (e.g., user interaction),
    you must use the @p on_worker_end routine to set a global variable to some
    value that prevents other threads to use the worker's address ever again.

    If @p save_handle is not `NULL`, it will be used to store a handle for the
    new worker before exiting. If the function returns a non-zero value the
    original address of @p save_handle will be left untouched.

    If @p save_handle is `NULL`, the calling thread will have no way to access
    the worker created, unless the latter stores the return value of
    `GNUNET_WORKER_get_current_handle()` into a global/shared variable.

**/
extern int GNUNET_WORKER_create (
    GNUNET_WORKER_Handle ** const save_handle,
    const GNUNET_ConfirmRoutine on_worker_start,
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data
);


/**

    @brief      Launch the GNUnet scheduler in the current thread and turn it
                into a worker
    @param      save_handle         A placeholder for storing a handle for the
                                    new worker created               [NULLABLE]
    @param      master_routine      A master function to call in a new detached
                                    thread, invoked with the newly created
                                    worker's handle and @p worker_data as
                                    arguments; this function will not belong to
                                    the scheduler's thread, and therefore will
                                    not be able to invoke the scheduler's
                                    functions directly; however it will be able
                                    to invoke all the functions available in
                                    this framework (such as
                                    `GNUNET_WORKER_push_load()`)     [NULLABLE]
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

    Any non-zero value indicates that the worker has not been created and the
    call was no-op.

    If you want to exploit the facilities offered by `GNUNET_PROGRAM_run()`
    together with the facilities offered by this framework, you can launch
    `GNUNET_PROGRAM_run2()` -- called with `GNUNET_YES` as last argument so
    that another GNUnet scheduler is not run -- and then invoke
    `GNUNET_WORKER_create()` inside the `GNUNET_PROGRAM_Main` function of your
    application. See `examples/gtk4-example/foobar-main.c` for a possible
    example.

    This function will not return until the scheduler returns. The @p master
    thread, or equivalently any other thread, must eventually take care of
    destroying the worker after using it, or the caller thread will continue
    hanging indefinitely with the scheduler running.

    The caller thread, which has now become a worker thread, can also shut down
    the scheduler and free the memory allocated for the worker by invoking
    `GNUNET_SCHEDULER_shutdown()`.

    If @p master_routine is non-`ǸULL`, it will be launched in a new detached
    (non-joinable) thread. If @p master_routine is `NULL` there will be
    apparently little difference between invoking directly
    `GNUNET_SCHEDULER_run()` and invoking this function. However, there will be
    an important difference when new threads will be manually launched later:
    `GNUNET_WORKER_push_load()` will now be at other threads' disposal.

    If @p save_handle is not `NULL`, it will be used to store a handle for the
    new worker, right after @p master_routine has been launched in a new thread
    (if it applies), but before the scheduler has been started. If this
    function returns a non-zero value the original address of @p save_handle
    will be left untouched.

    The @p on_worker_end routine is not like any routine, but represents a
    point of no return after which it becomes illegal to attempt to destroy a
    worker (and, if you are lucky, attempting to do so will result in an error
    message and a `GNUNET_WORKER_ERR_DOUBLE_FREE` error code). On the other
    hand, attempting to destroy a worker before @p on_worker_end has returned
    is always safe. Thus, if a program allows different threads to destroy a
    worker and these do not behave deterministically (e.g., user interaction),
    you must use the @p on_worker_end routine to set a global variable to some
    value that prevents other threads to use the worker's address ever again.

**/
extern int GNUNET_WORKER_start_serving (
    GNUNET_WORKER_Handle ** const save_handle,
    const GNUNET_WORKER_MasterRoutine master_routine,
    const GNUNET_ConfirmRoutine on_worker_start,
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data
);


/**

    @brief      Install a load listener into an already running scheduler and
                turn the latter into a worker
    @param      save_handle         A placeholder for storing a handle for the
                                    new worker created               [NULLABLE]
    @param      master_routine      A master function to call in a new detached
                                    thread, invoked with the newly created
                                    worker's handle and @p worker_data as
                                    arguments; this function will not belong to
                                    the scheduler's thread, and therefore will
                                    not be able to invoke the scheduler's
                                    functions directly; however it will be able
                                    to invoke all the functions available in
                                    this framework (such as
                                    `GNUNET_WORKER_push_load()`)     [NULLABLE]
    @param      on_worker_end       The last routine invoked by the worker,
                                    with @p worker_data passed as argument
                                                                     [NULLABLE]
    @param      worker_data         Custom user data retrievable at any moment
                                                                     [NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
                `GNUNET_WORKER_ERR_ALREADY_SERVING`,
                `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
                `GNUNET_WORKER_ERR_THREAD_CREATE`

    Any non-zero value indicates that the worker has not been created and the
    call was no-op.

    This is the only function that requires that the user has already started
    the GNUnet scheduler manually (either via `GNUNET_SCHEDULER_run()` or
    `GNUNET_PROGRAM_run()`). Calling this function without the scheduler
    running will result in undefined behavior.

    The `on_worker_start` argument is missing by design, as differently than
    `GNUNET_WORKER_start_serving()` this function returns immediately (in what
    has now become a worker thread).

    If @p master_routine is non-`ǸULL`, it will be launched in a new detached
    (non-joinable) thread.

    If later the user wants to go back to using the scheduler without
    interferences from other threads the `GNUNET_WORKER_dismiss()` function is
    available.

    If @p save_handle is not `NULL`, it will be used to store a handle for the
    new worker, right after @p master_routine has been launched in a new thread
    (if it applies). If this function returns a non-zero value the original
    address of @p save_handle will be left untouched.

    The @p on_worker_end routine is not like any routine, but represents a
    point of no return after which it becomes illegal to attempt to destroy a
    worker (and, if you are lucky, attempting to do so will result in an error
    message and a `GNUNET_WORKER_ERR_DOUBLE_FREE` error code). On the other
    hand, attempting to destroy a worker before @p on_worker_end has returned
    is always safe. Thus, if a program allows different threads to destroy a
    worker and these do not behave deterministically (e.g., user interaction),
    you must use the @p on_worker_end routine to set a global variable to some
    value that prevents other threads to use the worker's address ever again.

**/
extern int GNUNET_WORKER_adopt_running_scheduler (
    GNUNET_WORKER_Handle ** const save_handle,
    const GNUNET_WORKER_MasterRoutine master_routine,
    const GNUNET_CallbackRoutine on_worker_end,
    void * const worker_data
);


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

    This function is identical to `GNUNET_WORKER_push_load()`, but allows to
    specify the priority whereby a job will be scheduled.

    The `GNUNET_SCHEDULER_Priority` enumeration type is defined in
    `/usr/include/gnunet/gnunet_common.h`. Possible priorities, from the lowest
    to the highest, are:

    - `GNUNET_SCHEDULER_PRIORITY_IDLE`
    - `GNUNET_SCHEDULER_PRIORITY_BACKGROUND`
    - `GNUNET_SCHEDULER_PRIORITY_DEFAULT`
    - `GNUNET_SCHEDULER_PRIORITY_HIGH`
    - `GNUNET_SCHEDULER_PRIORITY_UI`
    - `GNUNET_SCHEDULER_PRIORITY_URGENT`
    - `GNUNET_SCHEDULER_PRIORITY_SHUTDOWN`

    The additional `GNUNET_SCHEDULER_PRIORITY_KEEP` cannot be used in this
    context.

    The functions pushed into the worker thread are free to use all the
    scheduler's utilities (such as `GNUNET_SCHEDULER_add_with_priority()`,
    `GNUNET_SCHEDULER_add_delayed_with_priority()`, and so on). Invoking
    `GNUNET_SCHEDULER_shutdown()` from the scheduler's thread will be
    equivalent to invoking `GNUNET_WORKER_*_destroy()` from any other thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    A non-zero return value indicates that @p job_routine was not scheduled
    (the call was no-op and the user may attempt again).

**/
extern int GNUNET_WORKER_push_load_with_priority (
    GNUNET_WORKER_Handle * const worker,
    const enum GNUNET_SCHEDULER_Priority job_priority,
    const GNUNET_CallbackRoutine job_routine,
    void * const job_data
);


/**

    @brief      Schedule a new function for the worker, with default priority
    @param      worker          A pointer to the worker where the task must be
                                scheduled                        [NON-NULLABLE]
    @param      job_routine     The task to schedule             [NON-NULLABLE]
    @param      job_data        Custom data to pass to the task      [NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
                `GNUNET_WORKER_ERR_NO_MEMORY`, `GNUNET_WORKER_ERR_SIGNAL` and
                `GNUNET_WORKER_ERR_INVALID_HANDLE`

    Use this routine every time you want to run a function in a worker thread.
    For specifying a priority, the `GNUNET_WORKER_push_load_with_priority()`
    function is available.

    The functions pushed into the worker thread are free to use all the
    scheduler's utilities (such as `GNUNET_SCHEDULER_add_with_priority()`,
    `GNUNET_SCHEDULER_add_delayed_with_priority()`, etc.). Invoking
    `GNUNET_SCHEDULER_shutdown()` from the scheduler's thread will be
    equivalent to invoking `GNUNET_WORKER_*_destroy()` from any other thread
    (i.e., the memory previously allocated for the worker will be destroyed).

    A non-zero return value indicates that @p job_routine was not scheduled
    (the call was no-op and the user may attempt again).

    A return value of `GNUNET_WORKER_ERR_INVALID_HANDLE` indicates that the
    worker is beeing destroyed by this or another thread. If this happens, the
    caller's code is in a dangerous position: the next time, instead of a
    handle in the process of being destroyed, an already destroyed handle might
    be passed to this function, and that will cause undefined behavior
    (probably a crash). This error must be handled exactly like a double free
    or corruption error, and you should always pay attention that a worker's
    address is never used again after a worker has been destroyed. You must not
    handle `GNUNET_WORKER_ERR_INVALID_HANDLE` using runtime checks unless you
    are debugging your program; the way to handle this error is by writing code
    that does not use a worker's address after this has been destroyed. Please
    have a look at the documentation of either `GNUNET_WORKER_start_serving()`,
    `GNUNET_WORKER_create()` or `GNUNET_WORKER_adopt_running_scheduler()` for
    more information on how to avoid that his happens.

**/
static inline int GNUNET_WORKER_push_load (
    GNUNET_WORKER_Handle * const worker,
    const GNUNET_CallbackRoutine job_routine,
    void * const job_data
) {
    return GNUNET_WORKER_push_load_with_priority(
        worker,
        GNUNET_SCHEDULER_PRIORITY_DEFAULT,
        job_routine,
        job_data
    );
}


/**

    @brief      Terminate a worker and free its memory, without waiting for the
                scheduler to return -- this will be completed in parallel
                (asynchronous)
    @param      worker          The worker to destroy            [NON-NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE` and `GNUNET_WORKER_ERR_SIGNAL`

    This function is used to launch `GNUNET_SCHEDULER_shutdown()` in the worker
    thread, without waiting for the scheduler to return.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. If the user had passed an
    `on_worker_end` routine during the creation of the worker, this will be
    launched now.

    This function can be invoked by any thread (including the worker thread as
    well). Directly launching `GNUNET_SCHEDULER_shutdown()` from the worker
    thread will have identical effects to launching this function from any
    other thread (i.e., the memory previously allocated for the worker will be
    destroyed).

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the worker's destruction (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens, the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error, and you should always pay attention that
    no more than one thread can destroy the worker. You must not handle
    `GNUNET_WORKER_ERR_DOUBLE_FREE` using runtime checks unless you are
    debugging your program; the way to handle this error is by writing code
    that does not destroy a worker twice. Please have a look at the
    documentation of either `GNUNET_WORKER_adopt_running_scheduler()`,
    `GNUNET_WORKER_start_serving()` or `GNUNET_WORKER_create()` for more
    information on how to avoid that his happens. It is very likely that this
    error code will be removed in the future and a call to `exit()` will appear
    in its place.

    A value of `GNUNET_WORKER_ERR_SIGNAL` indicates that it was not possible to
    notify the worker about the shutdown (this error can be thrown only if this
    function was not invoked from the worker thread). In this case there is not
    much to do. The return value was caused by an error during `write()` into
    the worker's pipe. The only way to know whether the worker has received the
    message or not is by passing an `on_worker_end` routine during the creation
    of the worker and let it signal about the shutdown happening. If no signal
    arrives it is possible to try and wake up the worker for the shutdown by
    using `GNUNET_WORKER_ping()` or by invoking this function again. A pipe
    breaking is a very unlikely event to occur, and it might make sense to
    ignore completely the possible `GNUNET_WORKER_ERR_SIGNAL` return value,
    assume that the worker has been destroyed, and tolerate the rare events of
    workers turned into zombies; or alternatively, it might be a good idea to
    launch `exit()`, if a worker turns into a zombie.

**/
extern int GNUNET_WORKER_asynch_destroy (
    GNUNET_WORKER_Handle * const worker
);


/**

    @brief      Terminate a worker and free its memory, waiting for the
                scheduler to complete the shutdown (synchronous)
    @param      worker          The worker to destroy            [NON-NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE`, `GNUNET_WORKER_ERR_UNKNOWN`
                `GNUNET_WORKER_ERR_NOT_ALONE`, `GNUNET_WORKER_ERR_SIGNAL` and
                `GNUNET_WORKER_ERR_INTERNAL_BUG`

    This function is used to launch `GNUNET_SCHEDULER_shutdown()` in the worker
    thread, waiting for the scheduler to return.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. If the user had passed an
    `on_worker_end` routine during the creation of the worker, this will be
    launched now.

    This function can be invoked by any thread (including the worker thread as
    well). Directly launching `GNUNET_SCHEDULER_shutdown()` from the worker
    thread will have identical effects to launching this function from any
    other thread (i.e., the memory previously allocated for the worker will be
    destroyed).

    Any return value different than `GNUNET_WORKER_ERR_SIGNAL` indicate that
    the scheduler will eventually shut down. However only zero grants that the
    scheduler is not running anymore when this function returns. In fact, any
    error code different than zero makes this function behave like
    `GNUNET_WORKER_asynch_destroy()`.

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the worker's destruction (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens, the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error, and you should always pay attention that
    no more than one thread can destroy the worker. You must not handle
    `GNUNET_WORKER_ERR_DOUBLE_FREE` using runtime checks unless you are
    debugging your program; the way to handle this error is by writing code
    that does not destroy a worker twice. Please have a look at the
    documentation of either `GNUNET_WORKER_adopt_running_scheduler()`,
    `GNUNET_WORKER_start_serving()` or `GNUNET_WORKER_create()` for more
    information on how to avoid that his happens. It is very likely that this
    error code will be removed in the future and a call to `exit()` will appear
    in its place.

    A return value of `GNUNET_WORKER_ERR_UNKNOWN` can only be due to a
    non-standard implementation of the `pthread_join()` and
    `pthread_cond_wait()` functions currently in use.

    A value of `GNUNET_WORKER_ERR_SIGNAL` indicates that it was not possible to
    notify the worker about the shutdown (this error can be thrown only if this
    function was not invoked from the worker thread). In this case there is not
    much to do. The return value was caused by an error during `write()` into
    the worker's pipe. The only way to know whether the worker has received the
    message or not is by passing an `on_worker_end` routine during the creation
    of the worker and let it signal about the shutdown happening. If no signal
    arrives it is possible to try and wake up the worker for the shutdown by
    using `GNUNET_WORKER_ping()` or by invoking this function again. A pipe
    breaking is a very unlikely event to occur, and it might make sense to
    ignore completely the possible `GNUNET_WORKER_ERR_SIGNAL` return value,
    assume that the worker has been destroyed, and tolerate the rare events of
    workers turned into zombies; or alternatively, it might be a good idea to
    launch `exit()`, if a worker turns into a zombie.

    A value of `GNUNET_WORKER_ERR_INTERNAL_BUG` should never be returned;
    please fill a bug report if it happens.

**/
extern int GNUNET_WORKER_synch_destroy (
    GNUNET_WORKER_Handle * const worker
);


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

    This function is used to launch `GNUNET_SCHEDULER_shutdown()` in the worker
    thread, waiting until a certain time for the scheduler to return.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. If the user had passed an
    `on_worker_end` routine during the creation of the worker, this will be
    launched now.

    This function can be invoked by any thread (including the worker thread as
    well). Directly launching `GNUNET_SCHEDULER_shutdown()` from the worker
    thread will have identical effects to launching this function from any
    other thread (i.e., the memory previously allocated for the worker will be
    destroyed). When this function is called from the worker thread the
    @p absolute_time argument is ignored.

    Any return value different than `GNUNET_WORKER_ERR_SIGNAL` indicate that
    the scheduler will eventually shut down. However only zero grants that the
    scheduler is not running anymore. In fact, any error code different than
    zero makes this function behave like `GNUNET_WORKER_asynch_destroy()`.

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the worker's destruction (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens, the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error, and you should always pay attention that
    no more than one thread can destroy the worker. You must not handle
    `GNUNET_WORKER_ERR_DOUBLE_FREE` using runtime checks unless you are
    debugging your program; the way to handle this error is by writing code
    that does not destroy a worker twice. Please have a look at the
    documentation of either `GNUNET_WORKER_adopt_running_scheduler()`,
    `GNUNET_WORKER_start_serving()` or `GNUNET_WORKER_create()` for more
    information on how to avoid that his happens. It is very likely that this
    error code will be removed in the future and a call to `exit()` will appear
    in its place.

    A return value of `GNUNET_WORKER_ERR_UNKNOWN` can only be due to a
    non-standard implementation of the `pthread_timedjoin_np()` and
    `pthread_cond_timedwait()` functions currently in use.

    A value of `GNUNET_WORKER_ERR_SIGNAL` indicates that it was not possible to
    notify the worker about the shutdown (this error can be thrown only if this
    function was not invoked from the worker thread). In this case there is not
    much to do. The return value was caused by an error during `write()` into
    the worker's pipe. The only way to know whether the worker has received the
    message or not is by passing an `on_worker_end` routine during the creation
    of the worker and let it signal about the shutdown happening. If no signal
    arrives it is possible to try and wake up the worker for the shutdown by
    using `GNUNET_WORKER_ping()` or by invoking this function again. A pipe
    breaking is a very unlikely event to occur, and it might make sense to
    ignore completely the possible `GNUNET_WORKER_ERR_SIGNAL` return value,
    assume that the worker has been destroyed, and tolerate the rare events of
    workers turned into zombies; or alternatively, it might be a good idea to
    launch `exit()`, if a worker turns into a zombie.

    A value of `GNUNET_WORKER_ERR_INTERNAL_BUG` should never be returned;
    please fill a bug report if it happens.

**/
extern int GNUNET_WORKER_timedsynch_destroy (
    GNUNET_WORKER_Handle * const worker,
    const struct timespec * const absolute_time
);


/**

    @brief      Uninstall and destroy a worker without shutting down its
                scheduler
    @param      worker          The worker to dismiss            [NON-NULLABLE]
    @return     Possible return values are `GNUNET_WORKER_SUCCESS` (`0`),
                `GNUNET_WORKER_ERR_DOUBLE_FREE` and `GNUNET_WORKER_ERR_SIGNAL`

    Once invoked, this function uninstalls the load listener from the scheduler
    and frees the memory allocated with the worker, but lets the scheduler
    live. In short: it turns a worker thread back into a classic GNUnet
    scheduler, without any multithreading facility and without a load listener.

    Although it can be invoked on any worker, independently from how this was
    created, `GNUNET_WORKER_dismiss()` is often paired with
    `GNUNET_WORKER_adopt_running_scheduler()`, for making a scheduler serve as
    a worker only temporarily, before continuing its work without interferences
    from other threads.

    Functions already scheduled via `GNUNET_WORKER_push_load()` that had no
    time to be invoked will be immediately cancelled. If the user had passed an
    `on_worker_end` routine during the creation of the worker, this will be
    launched now.

    A return value of `GNUNET_WORKER_ERR_DOUBLE_FREE` indicates that another
    thread has already triggered the worker's destruction (this includes having
    invoked `GNUNET_SCHEDULER_shutdown()` from the scheduler itself). If this
    happens, the caller's code is in a dangerous position: the next time,
    instead of a handle in the process of being destroyed, an already destroyed
    handle might be passed to this function, and that will cause undefined
    behavior (probably a crash). This error must be handled exactly like a
    double free or corruption error, and you should always pay attention that
    no more than one thread can destroy the worker. You must not handle
    `GNUNET_WORKER_ERR_DOUBLE_FREE` using runtime checks unless you are
    debugging your program; the way to handle this error is by writing code
    that does not destroy a worker twice. Please have a look at the
    documentation of either `GNUNET_WORKER_adopt_running_scheduler()`,
    `GNUNET_WORKER_start_serving()` or `GNUNET_WORKER_create()` for more
    information on how to avoid that his happens. It is very likely that this
    error code will be removed in the future and a call to `exit()` will appear
    in its place.

**/
extern int GNUNET_WORKER_dismiss (
    GNUNET_WORKER_Handle * const worker
);


/**

    @brief      Retrieve the custom data initially passed to the worker
    @param      worker          The worker to query for the data [NON-NULLABLE]
    @return     A pointer to the data initially passed to the worker

    The data returned is the `worker_data` argument previously passed to
    `GNUNET_WORKER_create()`, `GNUNET_WORKER_start_serving()` or
    `GNUNET_WORKER_adopt_running_scheduler()` (depending on how the worker was
    created).

**/
extern void * GNUNET_WORKER_get_data (
    const GNUNET_WORKER_Handle * const worker
);


/**

    @brief      Get the handle of the current worker if this is a worker thread
    @return     The worker installed in the current thread or `NULL` if this is
                not a worker thread

    In addition to retrieving the current `GNUNET_WORKER_Handle`, this utility
    can be used to detect whether the current block of code is running in the
    worker thread or not (the returned value will be `NULL` if the function is
    not invoked from the worker thread).

**/
extern GNUNET_WORKER_Handle * GNUNET_WORKER_get_current_handle (void);


/**

    @brief      Ping the worker and try to wake up its listener function
    @param      worker          The worker to ping               [NON-NULLABLE]
    @return     A boolean: `true` if the ping was successful, `false` otherwise

    This function will unlikely ever be needed. It is possible to use it to try
    and wake up a worker after a `GNUNET_WORKER_ERR_SIGNAL` error was returned
    by the `GNUNET_WORKER_*_destroy()` function family.

    If called from the worker thread `GNUNET_WORKER_ping()` never fails.

**/
extern bool GNUNET_WORKER_ping (
    GNUNET_WORKER_Handle * const worker
);


#ifdef __cplusplus
}
#endif


#endif

/*  EOF  */

