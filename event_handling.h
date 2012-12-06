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
	int id;
	char *name;
	int event_type;
	int min_read_close;
	double timestamp;
} EVENT;

int event_handling_add_event(EVENT *event);
int event_handling_notify();
int event_handling_init();



#endif /* EVENT_HANDLING_H_ */
