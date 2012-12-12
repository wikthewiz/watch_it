/*
 * event_handling.h
 *
 *  Created on: 19 Nov 2012
 *      Author: CONSEO\mawi
 */

#ifndef EVENT_HANDLING_H_
#define EVENT_HANDLING_H_

#include <stdio.h>

typedef struct
{
	int id; // This is an unique id for each item that is watched: i.e.
			// use this to distinguish between files in directories
	char *name;
	int event_type;
	int min_read_close;
	long int timestamp;
} EVENT;

int event_handling_add_event(EVENT *event);
int event_handling_notify();
int event_handling_init();
long int event_handling_get_tick();


#endif /* EVENT_HANDLING_H_ */
