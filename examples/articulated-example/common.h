/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */


#ifndef __COMMON_H__
#define __COMMON_H__


#include <gnunet/gnunet_worker_lib.h>


typedef struct ThreadData_T {
	const char * name;
	GNUNET_WORKER_Handle worker;
	pthread_t thread;
} ThreadData;


#endif


/*  EOF  */

