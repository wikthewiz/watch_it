/*
 * event_hanling.c
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "event_handling.h"
#include "list.h"
#include <time.h>
#include <sys/inotify.h>


typedef struct
{
	EVENT *element;
	int remove;
	struct list_head list;
} EVENT_LIST;

static EVENT_LIST event_list;
static pthread_t consumer_thread;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wait_cond = PTHREAD_COND_INITIALIZER;
static int signaled;
void *consumer();

void event_copy(EVENT *to,const EVENT const * from)
{
	to->event_type = from->event_type;
	to->id = from->id;
	to->min_read_close = from->min_read_close;
	to->timestamp = from->timestamp;
	strcpy(to->name,from->name);
}

EVENT* allocate_event(char *name)
{
	EVENT *event = (EVENT*)malloc(sizeof(EVENT));
	if (event == NULL)
	{
		return NULL;
	}

	event->name = malloc(sizeof(char)* strlen(name));
	if (event->name == NULL)
	{
		free(event);
		return NULL;
	}
	return event;

}

void deallocate_event(EVENT *e){
	free(e->name);
	e->name = NULL;
	free(e);
}

int event_handling_add_event(EVENT *event)
{
	EVENT_LIST *tmp;
	EVENT *eventToBeAdded = allocate_event(event->name);
	if (eventToBeAdded == NULL)
	{
		return -1;
	}

	event_copy(eventToBeAdded,event);

	tmp = (EVENT_LIST *)malloc(sizeof(EVENT_LIST));
	tmp->element = eventToBeAdded;
	tmp->remove = 0;

	pthread_mutex_lock( &mutex );
	list_add_tail(&(tmp->list),&(event_list.list));
	pthread_cond_signal( &wait_cond );
	signaled=1;
	pthread_mutex_unlock( &mutex );

	return 0;
}

EVENT_LIST* list_get(int id, int of_type){
	EVENT_LIST *tmp;
	struct list_head *pos;
	list_for_each(pos, &event_list.list){
		tmp= list_entry(pos, EVENT_LIST, list);
		if (id == tmp->element->id &&
			(tmp->element->event_type & of_type)){
			return tmp;
		}
	}
	return NULL;
}

void execute_event(EVENT *event)
{
	if (event->event_type & IN_CLOSE)
	{
		printf("*CLOSE EVENT for file:%s \n", event->name);
	}
	else
	{
		printf("*EVENT for file:%s \n", event->name);
	}
}

int is_time_to_fire(EVENT_LIST *cur, int delta_time)
{
	return delta_time > cur->element->min_read_close;
}

void filter_before_execute(EVENT_LIST *cur)
{
	EVENT_LIST *other;
	if ((other = list_get(cur->element->id, IN_CLOSE)))
	{
		double delta = cur->element->timestamp -
					    other->element->timestamp;
		if (is_time_to_fire(cur,delta))
		{
			execute_event(cur->element);
		}
		else
		{
			other->remove = 1;
		}
		cur->remove = 1;
	}
	else
	{
		clock_t c1;
		c1 = clock();
		if (c1 < 0){
			fprintf(stderr,"clock() failed. Skip this event!");
			cur->remove = 1;
			return;
		}
		double delta = c1/CLOCKS_PER_SEC - cur->element->timestamp;
		if (is_time_to_fire(cur,delta))
		{
			cur->remove = 1;
			execute_event(cur->element);
		}
	}
}

int needs_filtering(EVENT_LIST* cur)
{
	return cur->element->min_read_close &&
			!(cur->element->event_type & IN_CLOSE);
}

void *consumer(void *arg)
{
	int i = 0;
	EVENT_LIST *cur,*other;
	struct list_head *pos, *q;
	while(1){
		pthread_mutex_lock( &mutex );
		if (!signaled)
		{
			struct timespec timeToWait;
			int rt;
			gettimeofday(&now,NULL);
			timeToWait.tv_sec = now.tv_sec + 1;
			printf("WAITING");
			pthread_cond_timedwait( &wait_cond, &mutex ,&timeToWait);
			printf("WOKE_UP");
		}
		signaled = 0;
		list_for_each(pos, &event_list.list){
			cur= list_entry(pos, EVENT_LIST, list);
			printf( "name:%s\tid:%i\n",cur->element->name,cur->element->id);
			if (!(cur->remove)){
				if (needs_filtering(cur))
				{
					filter_before_execute(cur);
				}
				else
				{
					execute_event(cur);
				}
			}
		}

		list_for_each_safe(pos, q, &event_list.list){
			cur = list_entry(pos, EVENT_LIST, list);
			if (cur->remove){
				printf("freeing item name:%s\tid:%i\n",cur->element->name,
						cur->element->id);
				list_del(pos);
				deallocate_event(cur->element);
				cur->element = NULL;
				free(cur);
			}
		}

		pthread_mutex_unlock( &mutex );



		printf("running:%i\n\n", i);
		i++;
	}
	return NULL;
}
int event_handling_init()
{
	INIT_LIST_HEAD(&event_list.list);
	signaled = 0;
	if ( pthread_create( &consumer_thread, NULL, &consumer, NULL) )
	{
		fprintf(stderr,"failed to create consumer thread\n");
		return -1;
	}
	return 0;
}



