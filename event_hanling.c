/*
 * event_hanling.c
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "event_handling.h"
#include "list.h"
#include <sys/time.h>
#include <sys/inotify.h>
#include <errno.h>

#define EOWNERDEAD_MSG "The mutex is a robust mutex and the process containing\
						the previous owning thread terminated while holding the\
						mutex lock. The mutex lock shall be acquired by the\
						calling thread and it is up to the new owner to make\
						the state consistent."
#define EPERM_MSG "The mutex type is PTHREAD_MUTEX_ERRORCHECK or the mutex is\
				    a robust mutex, and the current thread does not own the\
				    mutex."
#define EINVAL_MSG "The abstime argument specified a nanosecond value less\
					 than zero or greater than or equal to 1000 million."

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

long int event_handling_get_tick()
{
	struct timeval now;
	gettimeofday(&now,NULL);
	return now.tv_usec;
}
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

void print_event(EVENT *event)
{
	printf("{%s,%d,%s},",
			event->name,
			event->id,
			event->event_type & IN_CLOSE ? "CLOSE": "OPEN");
}

void print_list()
{
	EVENT_LIST *tmp;
	struct list_head *pos;
	printf("event_list=[");
	list_for_each(pos, &event_list.list){
		tmp= list_entry(pos, EVENT_LIST, list);
		print_event(tmp->element);
	}
	printf("]\n");
}

int equals_event(EVENT *e1, EVENT *e2)
{
	return e1->id == e2->id && !strcmp(e1->name,e2->name);
}

EVENT_LIST* list_get(EVENT *event, int of_type){
	EVENT_LIST *tmp;
	struct list_head *pos;
	list_for_each(pos, &event_list.list){
		tmp = list_entry(pos, EVENT_LIST, list);
		if (equals_event(tmp->element,event) &&
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
	EVENT_LIST *close_event;
	if ((close_event = list_get(cur->element, IN_CLOSE)))
	{
		long int delta = (cur->element->timestamp -
						   close_event->element->timestamp)/10;
//		printf("%s: has close event. (delta:%li)\n",cur->element->name,delta);
		if (is_time_to_fire(cur,delta))
		{
			printf("has close ");
			execute_event(cur->element);
		}
		else
		{
			close_event->remove = 1;
		}
		cur->remove = 1;
	}
	else
	{
		long int now = event_handling_get_tick();
		long int delta = (now - cur->element->timestamp)/10;
//		printf("%s: NO close event (delta:%f )\n",cur->element->name,delta);
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

void wait_for_producer(int max_wait_time)
{
	struct timespec timeToWait;
	struct timeval now;
	int wres = 0;
	gettimeofday(&now,NULL);
	timeToWait.tv_sec = now.tv_sec + max_wait_time;
	timeToWait.tv_nsec = 0;
//	printf("WAITING\n");
	if ((wres = pthread_cond_timedwait( &wait_cond, &mutex ,&timeToWait)))
	{
		switch(wres)
		{
		case ENOTRECOVERABLE:
			fprintf(stderr,
					"pthread_cond_timedwait failed to wait:\n\terr:%i\n\t%s\n",
					wres,
					"The state protected by the mutex is not recoverable.");
			break;
		case EOWNERDEAD:fprintf(stderr,
				"pthread_cond_timedwait failed to wait:\n\terr:%i\n\t%s\n",
				wres,
				EOWNERDEAD_MSG);
		break;
		case EPERM:
			fprintf(stderr,
					 "pthread_cond_timedwait failed to wait:\n\terr:%i\n\t%s\n",
					 wres,
					 EPERM_MSG);
			break;
		case EINVAL:
			fprintf(stderr,
				     "pthread_cond_timedwait failed to wait:\n\terr:%i\n\t%s\n",
				     wres,
				     EINVAL_MSG);
			break;
		default:break;
		}
	}
//	printf("WOKE_UP\n");
}

void *consumer(void *arg)
{
	int i = 0;
	EVENT_LIST *cur;
	struct list_head *pos, *q;
	int cur_wait = 600;
	while(1){
		pthread_mutex_lock( &mutex );
		if (!signaled)
		{
			wait_for_producer(cur_wait);
		}
		printf("start:");print_list();
		signaled = 0;
		list_for_each(pos, &event_list.list){
			cur= list_entry(pos, EVENT_LIST, list);
//			printf( "name:%s\tid:%i\n",cur->element->name,cur->element->id);
			if (!(cur->remove)){
				if (needs_filtering(cur))
				{
					filter_before_execute(cur);
					cur_wait = cur_wait > cur->element->min_read_close ?
									cur->element->min_read_close :
									cur_wait;
				}
				else
				{
					execute_event(cur->element);
				}
			}
		}

		list_for_each_safe(pos, q, &event_list.list){
			cur = list_entry(pos, EVENT_LIST, list);
			if (cur->remove){
//				printf("freeing item name:%s\tid:%i\n",cur->element->name,
//						cur->element->id);
				list_del(pos);
				deallocate_event(cur->element);
				cur->element = NULL;
				free(cur);
			}
		}

		printf("stop:");print_list();
		pthread_mutex_unlock( &mutex );



//		printf("running:%i\n\n", i);
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



