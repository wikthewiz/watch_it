#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>
#include "config.h"
#include "event_handling.h"
#include <time.h>


typedef struct
{
	int wd;
	char name[FILENAME_MAX];
	int type;
	int min_read_close;
} WD_NAME_TUPLE;



static WD_NAME_TUPLE name_map[MAX_WATCH];
static int name_map_len = 0;

void get_event (int fd);
void handle_error (int error);

int begin_watch(const struct conf * const config);

int add_to_map(const char * const to_be_watched,
				 int wd,
				 int type,
				 int min_read_close);

WD_NAME_TUPLE* find_in_map(int wd);

void init_name_map();
void init_wd_name_tuple(WD_NAME_TUPLE *tuple);


int main (int argc, char *argv[])
{
	struct conf *config = config_load();
	if (config == NULL)
	{
		fprintf(stderr,"failed to load config file");
		return -1;
	}

	init_name_map();
	for (int i = 0; i < config->watch_dir_count; ++i)
	{
		printf("conf->watch_dir[%i]:%s\n",i,config->watch_dir[i]);
	}
	int res = begin_watch(config);
	config_free(config);
	printf("exit");
	return res;

}



void init_name_map()
{
	for (int i = 0; i < MAX_WATCH; ++i)
	{
		init_wd_name_tuple(&(name_map[i]));
	}
}

void init_wd_name_tuple(WD_NAME_TUPLE *tuple)
{
	tuple->wd = -1;
	tuple->type = 0;
	tuple->min_read_close = 0;
	strcpy(tuple->name,"\0");
}

int begin_watch(const struct conf * const config)
{
	int fd;
	int wd;   /* watch descriptor */

	fd = inotify_init();
	if (fd < 0) {
		handle_error (errno);
		return -1;
	}

	if (config->fire_on == 0)
	{
		fprintf(stderr,"nothing to fire on!?\n");
		return -1;
	}

	if (config->watch_dir_count == 0)
	{
		fprintf (stderr, "nothing to watch!?\n");
		return -1;
	}

	if (config->type == 0)
	{
		fprintf (stderr, "Must watch content, directory or both. ");
		fprintf (stderr, "You have selected neither!?\n");
		return -1;
	}


	if (event_handling_init())
	{
		fprintf (stderr, "Failed to initiate event_handling ");
		return -1;
	}

	for (int i = 0; i < config->watch_dir_count; ++i)
	{
		if (strlen(config->watch_dir[i]) == 0)
		{
			fprintf(stderr,"empty watch_dir");
			continue;
		}

		printf("to be watched: %s\n",config->watch_dir[i]);

		wd = inotify_add_watch (fd, config->watch_dir[i], config->fire_on);
		if (wd < 0)
		{
			handle_error (errno);
			return -1;
		}

		if (add_to_map(config->watch_dir[i],
					   wd,
					   config->type,
					   config->min_read_close))
		{
			return -1;
		}

	}

	while (1)
	{
		get_event(fd);
	}
	return 0;
}

int add_to_map(const char * const to_be_watched,
				int wd,
				int type,
				int min_read_close)
{
	if (name_map_len >= MAX_WATCH)
	{
		handle_error(2);
		return -1;
	}
	strcpy(name_map[name_map_len].name, to_be_watched);
	name_map[name_map_len].wd = wd;
	name_map[name_map_len].type = type;
	name_map[name_map_len].min_read_close = min_read_close;
	name_map_len++;
	return 0;
}

WD_NAME_TUPLE* find_in_map(int wd)
{
	for (int i = 0; i < name_map_len; ++i)
	{
		if (name_map[i].wd == wd)
		{
			return &name_map[i];
		}
	}
	return NULL;
}

/* Allow for 1024 simultaneous events */
#define BUFF_SIZE ((sizeof(struct inotify_event)+FILENAME_MAX)*1024)

void get_event (int fd)
{
	ssize_t len, i = 0;
	char buff[BUFF_SIZE] = {0};
	len = read (fd, buff, BUFF_SIZE);

	while (i < len)
	{
		EVENT event;
		struct inotify_event *pevent = (struct inotify_event *)&buff[i];
		i += sizeof(struct inotify_event) + pevent->len;
		char event_name[81+FILENAME_MAX] = {0};
		const WD_NAME_TUPLE *tuple = NULL;
		if ((tuple = find_in_map(pevent->wd)) == NULL)
		{
			fprintf(stderr,"couldn't find the event that fired!?");
			continue;
		}

		if (pevent->len && !(tuple->type & WATCH_CONTENT))
		{
			continue;
		}

		if (pevent->len)
		{
			if (pevent->mask & IN_ISDIR)
			{
				continue;
			}
			strcpy (event_name, pevent->name);
		}
		else
		{
			if (!(tuple->type & WATCH_DIR))
			{
				continue;
			}
			strcpy (event_name, tuple->name);
		}

		clock_t c1;
		c1 = clock();
		if (c1 < 0){
			fprintf(stderr,"clock() failed. Skip this event!");
			continue;
		}

		event.id = pevent->wd;
		event.name = event_name;
		event.min_read_close = tuple->min_read_close;
		event.timestamp = c1 / CLOCKS_PER_SEC;
		event.event_type = pevent->mask;

		if (event_handling_add_event(&event)){
			fprintf(stderr,"failed to add event!");
			return;
		}
		/*
		 * //		if (pevent->mask & IN_ACCESS)
//		{
//			strcat(action, " was read");
//		}
//		if (pevent->mask & IN_CLOSE_NOWRITE)
//		{
//			strcat(action, " not opened for writing was closed");
//		}
//		if (pevent->mask & IN_OPEN)
//		{
//			strcat(action, " was opened");
//		}
//		if (pevent->mask & IN_ATTRIB)
//		{
//			strcat(action, " Metadata changed");
//		}
//		if (pevent->mask & IN_CLOSE_WRITE)
//		{
//			strcat(action, " opened for writing was closed");
//		}
//		if (pevent->mask & IN_CREATE)
//		{
//			strcat(action, " created in watched directory");
//		}
//		if (pevent->mask & IN_DELETE)
//		{
//			strcat(action, " deleted from watched directory");
//		}
//		if (pevent->mask & IN_DELETE_SELF)
//		{
//			strcat(action, "Watched file/directory was itself deleted");
//		}
//		if (pevent->mask & IN_MODIFY)
//		{
//			strcat(action, " was modified");
//		}
//		if (pevent->mask & IN_MOVE_SELF)
//		{
//			strcat(action, "Watched file/directory was itself moved");
//		}
//		if (pevent->mask & IN_MOVED_FROM)
//		{
//			strcat(action, " moved out of watched directory");
//		}
//		if (pevent->mask & IN_MOVED_TO)
//		{
//			strcat(action, " moved into watched directory");
//		}

		 *
      printf ("wd=%d mask=%#x cookie=%d len=%d dir=%s\n\n",
              pevent->wd, pevent->mask, pevent->cookie, pevent->len,
              (pevent->mask & IN_ISDIR)?"yes":"no");
		 */
//		if (pevent->len) printf ("name=%s\n", pevent->name);
//		printf("the action: %s \n",event_name);
	}

}

void handle_error (int error)
{
	fprintf (stderr, "Error: %s\n", strerror(error));

}
