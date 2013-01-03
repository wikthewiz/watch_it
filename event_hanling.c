/*
 * event_hanling.c
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "event_handling.h"
#include "list.h"
#include <time.h>
#include <sys/time.h>
#include <sys/inotify.h>
#include <errno.h>
#include <math.h>



#define EOWNERDEAD_MSG "The mutex is a robust mutex and the process\
						 containing the previous owning thread terminated\
						 while holding the mutex lock. The mutex lock shall be\
						 acquired by the calling thread and it is up to the\
						 new owner to make the state consistent."
#define EPERM_MSG "The mutex type is PTHREAD_MUTEX_ERRORCHECK or the mutex is\
				    a robust mutex, and the current thread does not own the\
				    mutex."
#define EINVAL_MSG "The abstime argument specified a nanosecond value less\
					 than zero or greater than or equal to 1000 million."

typedef struct
{
	int id; // This is an unique id for each item that is watched: i.e.
			// use this to distinguish between files in directories
	char *name;
	long unsigned int open_timestamp; /*The time stamp it was opend*/
	long unsigned int close_timestamp; /*The time stamp it was opend*/
	int is_closed;
	int has_fired_open;
	int has_fired_closed;
} FILE_EVENT;

typedef struct
{
	FILE_EVENT *file;
	struct list_head list;
	int remove;
} FILE_LIST;

static FILE_LIST file_event_list;

static pthread_t consumer_thread;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wait_cond = PTHREAD_COND_INITIALIZER;
static int signaled;
static int max_delay_before_fire;

// Used as fall back if clock_gettime should have a strange value
static long unsigned int last_tick;

void *consumer();

static inline int equals_file(FILE_EVENT* f1, FILE_EVENT *f2)
{
	return f1->id == f2->id && !strcmp(f1->name,f2->name);
}

static inline int is_close_event(EVENT *other)
{
	return (other->event_type & IN_CLOSE) ||
			(other->event_type & IN_CLOSE_NOWRITE) ||
			(other->event_type & IN_CLOSE_WRITE) ||
			(other->event_type & IN_DELETE) ||
			(other->event_type & IN_DELETE_SELF);
}

static inline long unsigned int calc_open_delta(FILE_LIST* orig)
{
	long unsigned int cur_tick = event_handling_get_tick();
	if (orig == NULL || orig->file == NULL)
	{
		fprintf(stderr,"NULL error in calc_open_delta\n");
		return 0;
	}

	if (cur_tick < orig->file->open_timestamp )
	{
		fprintf(stderr,"cur_tick is before timestamp: calc_open_delta\n");
		return 0;
	}

	long unsigned int val = cur_tick - orig->file->open_timestamp ;
	return val;
}

static inline long unsigned int calc_close_delta(FILE_LIST* orig)
{
	long unsigned int cur_tick = event_handling_get_tick();

	if (orig == NULL || orig->file == NULL)
	{
		fprintf(stderr,"NULL error in calc_close_delta\n");
		return 0;
	}

	if (cur_tick < orig->file->close_timestamp )
	{
		fprintf(stderr,"cur_tick is before timestamp: calc_close_delta\n");
		return 0;
	}

	long unsigned int val = cur_tick - orig->file->close_timestamp ;
	return val;
}

static inline int has_open_past_delay(FILE_LIST *orig)
{
	return calc_open_delta(orig) > max_delay_before_fire;
}

static inline int is_valid_close(FILE_LIST *orig)
{
	return orig->file->close_timestamp != 	 UINT32_MAX;
}

static inline int has_close_past_delay(FILE_LIST *orig)
{
	if (!is_valid_close(orig)) return 0;
	return calc_close_delta(orig) > max_delay_before_fire;
}

static inline int is_open_event(EVENT *other)
{
	return (other->event_type & IN_OPEN) ||
			(other->event_type & IN_ACCESS);
}

static inline int is_file_open(FILE_LIST *other)
{
	return !(other->file->is_closed);
}

static inline int is_file_closed(FILE_LIST *other)
{
	return other->file->is_closed;
}

static inline int copy_event_to_file(EVENT *e, FILE_EVENT *f)
{
	f->name = e->name;
	f->id = e->id;
	if (is_close_event(e))
	{
		if (f->close_timestamp != UINT32_MAX &&
			e->timestamp < f->open_timestamp )
		{
			fprintf(stderr,"trying to set a cose_timestamp that is before open!\n");
		}

		f->close_timestamp = e->timestamp;
	}
	else if (is_open_event(e))
	{
		if (f->close_timestamp != UINT32_MAX &&
			f->close_timestamp < e->timestamp )
		{
			fprintf(stderr,"trying to set a open_timestamp that is after close!\n");
		}
		f->open_timestamp = e->timestamp;
	}
	else
	{
		fprintf(stderr,"FAILED to copy event to a file\n");
		return -1;
	}
	return 0;
}

static inline void close_file(FILE_LIST *file_to_close,
								 long unsigned int timestamp)
{
	if (file_to_close->file->open_timestamp > timestamp)
	{
		fprintf(stderr, "trying to set a cose_timestamp that is before open!\n");
	}

	file_to_close->file->is_closed = 1;
	file_to_close->file->close_timestamp = timestamp;
}

static inline void reset_to_open(FILE_LIST *file_to_open,
								long unsigned int timestamp)
{
	if (file_to_open->file->close_timestamp == UINT32_MAX)
	{
		fprintf(stderr, "trying to reset file that has not been closed!?!?\n");
	}
	file_to_open->file->is_closed = 0;
	file_to_open->file->close_timestamp = UINT32_MAX;
	file_to_open->file->open_timestamp = timestamp;
}

long unsigned int event_handling_get_tick()
{
	struct timespec start;
	int errCode = 0;
	if ((errCode = clock_gettime(0, &start)))
	{
		fprintf(stderr,"FAILED to get current tick in: \
					     event_handling_get_tick. Error code: %i\n", errCode);
		return 0;
	}

	long int cur_tick = ((long int)start.tv_sec * 1000000) +
								(long int)start.tv_nsec / 1000000;

	if (last_tick > cur_tick)
	{
		fprintf(stderr,"FAILED last_tick is greater then cur tick!!\n");
		return last_tick;
	}

	last_tick = cur_tick;
	return last_tick;
}

int init_file(FILE_EVENT *f,
				char *name,
				int id,
				long unsigned int timestamp)
{
	if (name != NULL )
	{
		if (!(strcpy(f->name, name)))
		{
			return -1;
		}
	}
	else
	{
		f->name = NULL;
	}

	f->id = id;
	f->open_timestamp = timestamp;
	f->close_timestamp = UINT32_MAX;
	f->has_fired_closed = 0;
	f->has_fired_open = 0;
	f->is_closed = 0;
	return 0;
}

FILE_EVENT* allocate_file(FILE_EVENT *info)
{
	FILE_EVENT *file = (FILE_EVENT*)malloc(sizeof(FILE_EVENT));
	if (file == NULL)
	{
		return NULL;
	}

	file->name = malloc(sizeof(char)* strlen(info->name));
	if (file->name == NULL)
	{
		free(file);
		return NULL;
	}

	if (init_file(file, info->name, info->id, info->open_timestamp))
	{
		free(file->name);
		free(file);
		return NULL;
	}

	return file;
}

FILE_LIST* allocate_file_list(FILE_EVENT *file)
{
	FILE_LIST *tmp;
	tmp = NULL;

	if (!(tmp = (FILE_LIST *)malloc(sizeof(FILE_LIST))))
	{
		return NULL;
	}
	tmp->file = file;
	tmp->remove = 0;
	return tmp;
}

void deallocate_file(FILE_EVENT *f)
{
	free(f->name);
	f->name = NULL;
	free(f);
}

void deallocate_file_list(FILE_LIST *fl)
{
	deallocate_file(fl->file);
	fl->file = NULL;
	free(fl);
}

void remove_from_list(FILE_LIST *fl)
{
	list_del(&fl->list);
	deallocate_file_list(fl);
}

void print_file(FILE_EVENT *file)
{
	printf("{%s,%d},",file->name,file->id);
}

void print_list()
{
	FILE_LIST *tmp;
	struct list_head *pos;
	printf("file_event_list=[");
	list_for_each(pos, &file_event_list.list){
		tmp= list_entry(pos, FILE_LIST, list);
		print_file(tmp->file);
	}
	printf("]\n");
}


FILE_LIST* find_in_file_list(EVENT *event_find)
{
	FILE_LIST *tmp;
	struct list_head *pos;
	FILE_EVENT file_to_find;
	init_file(&file_to_find,NULL,0,0);

	if (copy_event_to_file(event_find, &file_to_find))
	{
		return NULL;
	}

	list_for_each(pos, &file_event_list.list)
	{
		tmp = list_entry(pos, FILE_LIST, list);
		if (equals_file(tmp->file,&file_to_find))
		{
			return tmp;
		}
	}
	return NULL;
}

void execute_open_event(FILE_EVENT *f)
{
	if (f->has_fired_open)
	{
		fprintf(stderr,
				"execute open, even thought it already has been executed\n");
		return;
	}

	if (f->has_fired_closed)
	{
		fprintf(stderr,
				"execute open, even thought close has already been executed\n");
		return;
	}

	printf("*OPEN file:%s \n", f->name);
	f->has_fired_open = 1;
}

void execute_close_event(FILE_EVENT *f)
{
	if (!f->has_fired_open)
	{
		fprintf(stderr,
				"execute close, even thought open hasn't been fired!\n");
		return;
	}

	if (f->has_fired_closed)
	{
		fprintf(stderr,
				"execute close, even thought close has already been executed\n");
		return;
	}
	printf("*CLOSE file:%s \n", f->name);
	f->has_fired_closed = 1;
}

void wait_for_producer(long unsigned int max_wait_time)
{
	struct timespec timeToWait,now;
//	struct timeval now;
	int wres = 0;
	clock_gettime(0, &now);

//	printf("wait:%is\n", max_wait_time/10);

	double seconds_int_part = 0, seconds_fract_part = 0;
	seconds_fract_part = modf( ((double)max_wait_time)/1000,
								&seconds_int_part);


	if (now.tv_nsec + (int)(seconds_fract_part * 1000000) >= 1000000)
	{
		timeToWait.tv_sec = now.tv_sec + (int)seconds_int_part + 1;
		timeToWait.tv_nsec = (now.tv_nsec +
				(int)(seconds_fract_part * 1000000)) -1000000;
	}
	else
	{
		timeToWait.tv_sec = now.tv_sec + (int)seconds_int_part;
			timeToWait.tv_nsec = now.tv_nsec +
					(int)(seconds_fract_part * 1000000);
	}


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
}

int add_new_file(FILE_EVENT *cur_file)
{
	FILE_EVENT *newly_opened_file;
	FILE_LIST *tmp;
	tmp = NULL;
	if (!(newly_opened_file = allocate_file(cur_file)))
	{
		return -1;
	}

	if (!(tmp = allocate_file_list(newly_opened_file)))
	{
		return -1;
	}

	list_add_tail(&(tmp->list),&(file_event_list.list));
	return 0;
}

int event_handling_add_event(EVENT *event)
{
	FILE_LIST *tmp_file;
	FILE_EVENT file_to_add;
	int res;
	res = 0;
	tmp_file = NULL;
	init_file(&file_to_add,NULL,0,0);

	pthread_mutex_lock( &mutex );

	if (is_close_event(event))
	{
		if ((tmp_file = find_in_file_list(event)) && is_file_open(tmp_file)) // File is open
		{
			if (has_open_past_delay(tmp_file))
			{
				close_file(tmp_file, event_handling_get_tick());
			}
			else
			{
				if (!tmp_file->file->has_fired_open)
				{
					remove_from_list(tmp_file);
				}
			}
		}
	}
	else if (is_open_event(event))
	{
		if ((tmp_file = find_in_file_list(event)) && is_file_closed(tmp_file)) // File is closed but exists
		{
			reset_to_open(tmp_file, event->timestamp);
		}
		else if (tmp_file == NULL) // File doesn't exists yet
		{
			if (copy_event_to_file(event,&file_to_add))
			{
				res = -1;
			}
			else
			{
				if (add_new_file(&file_to_add))
				{
					fprintf(stderr, "FAILED to open file. I will skip this!\n");
					res = -1;
				}
			}
		}
	}
	tmp_file = NULL;

	pthread_cond_signal( &wait_cond );
	signaled=1;

	pthread_mutex_unlock( &mutex );

	return res;
}

void *consumer(void *arg)
{
	int i = 0;
	FILE_LIST *cur;
	struct list_head *pos, *q;
	long unsigned int cur_wait;

	cur_wait = 6000;
	cur = NULL;
	pos = NULL;
	q = NULL;

	while(1){
		pthread_mutex_lock( &mutex );
		if (!signaled)
		{
			wait_for_producer(cur_wait);
			cur_wait = max_delay_before_fire;
		}
//		printf("(%i) start:",i);print_list();
		signaled = 0;
		list_for_each(pos, &file_event_list.list)
		{
			cur = list_entry(pos, FILE_LIST, list);
			if (!cur->file->has_fired_open)
			{
				if (has_open_past_delay(cur))
				{
					execute_open_event(cur->file);
				}
				else
				{
					if (cur_wait > calc_open_delta(cur))
					{
						cur_wait = calc_open_delta(cur);
					}
				}
			}

			if (!cur->file->has_fired_closed)
			{
				if (is_valid_close(cur) && cur->file->has_fired_open)
				{
					execute_close_event(cur->file);
					cur->remove = 1;
				}
//				else if (is_valid_close(cur))
//				{
//					if (cur_wait > calc_close_delta(cur))
//					{
//						cur_wait = calc_close_delta(cur);
//					}
//				}
			}
		}

		cur = NULL;

		list_for_each_safe(pos, q, &file_event_list.list){
			cur = list_entry(pos, FILE_LIST, list);
			if (cur->remove){
//				printf("freeing item name:%s\tid:%i\n",cur->element->name,
//						cur->element->id);
				list_del(pos);
				deallocate_file(cur->file);
				cur->file = NULL;
				free(cur);
			}
		}

		if (list_empty(&file_event_list.list))
		{
			cur_wait = 6000;
		}
//		printf("(%i) stop:",i);print_list();printf("\n\n");
		signaled = 0;
		pthread_mutex_unlock( &mutex );
		i++;
	}
	return NULL;
}
int event_handling_init(int min_read_close)
{
	INIT_LIST_HEAD(&file_event_list.list);
	max_delay_before_fire = min_read_close;
	signaled = 0;
	last_tick = 0;
	if ( pthread_create( &consumer_thread, NULL, &consumer, NULL) )
	{
		fprintf(stderr,"failed to create consumer thread\n");
		return -1;
	}
	return 0;
}



