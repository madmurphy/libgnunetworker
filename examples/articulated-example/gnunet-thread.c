/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*  All functions in this document are launched in the scheduler's thread  */

#include <gnunet/platform.h>
#include <gnunet/gnunet_util_lib.h>
#include <gnunet/gnunet_scheduler_lib.h>

#include "common.h"
#include "gnunet-thread.h"


void task_for_the_scheduler_2 (
	void * const v_thread_data
) {

	printf(

		"This function has been scheduled by a function that was already "
		"running in the\nscheduler's thread. However, that function had been "
		"scheduled here by the\n\"%s\" thread.\n\n",

		((ThreadData *) v_thread_data)->name

	);

}


void task_for_the_scheduler_1 (
	void * const v_thread_data
) {

	printf(

		"We are in the scheduler's thread here. However, this function has "
		"been\nscheduled by the \"%s\" thread.\n\n",

		((ThreadData *) v_thread_data)->name

	);

	/*  We can safely invoke the scheduler - this is the scheduler's thread  */
	GNUNET_SCHEDULER_add_with_priority(
		GNUNET_SCHEDULER_PRIORITY_DEFAULT,
		&task_for_the_scheduler_2,
		v_thread_data
	);

}


/*  EOF  */

