/*
 * event_handling.h
 *
 *  Created on: 19 Nov 2012
 *      Author: CONSEO\mawi
 */

#ifndef EVENT_HANDLING_H_
#define EVENT_HANDLING_H_

#include "config.h"

typedef struct
{
	int id; // This is an unique id for each item that is watched: i.e.
			// use this to distinguish between files in directories
	char *name;
	int event_type;
	int min_read_close;
	long unsigned int timestamp;
} EVENT;

int event_handling_add_event(EVENT *event);
int event_handling_notify(void);
int event_handling_init(const struct conf const * config);
long unsigned int event_handling_get_tick(void);


#endif /* EVENT_HANDLING_H_ */
