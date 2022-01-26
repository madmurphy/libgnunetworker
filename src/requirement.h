/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| requirement.h
|*|
|*| https://github.com/madmurphy/libgnunetworker
|*|
|*| Copyright (C) 2022 madmurphy <madmurphy333@gmail.com>
|*|
|*| **requirement.h** is free software: you can redistribute it and/or modify
|*| it under the terms of the GNU General Public License as published by the
|*| Free Software Foundation, either version 3 of the License, or (at your
|*| option) any later version.
|*|
|*| **requirement.h** is distributed in the hope that it will be useful, but
|*| WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|*| or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
|*| License for more details.
|*|
|*| You should have received a copy of the GNU General Public License along
|*| with this program. If not, see <http://www.gnu.org/licenses/>.
|*|
\*/


/**

	@file       requirement.h
	@brief      Blocking requirements using POSIX Threads

**/


#ifndef __REQUIREMENT_H__
#define __REQUIREMENT_H__


#include <pthread.h>


/**
	@brief      A `Requirement` is a data type that can be read or updated by
	            one thread at a time, and can be unfulfilled to a variable
	            degree (red) or fullfilled (green)
**/
typedef struct Requirement {
	pthread_mutex_t req_mutex;
	pthread_cond_t req_cond;
	unsigned int req_unfulfillment;
} Requirement;


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
	@param      initial_value   The initial value to assign to the requirement
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

