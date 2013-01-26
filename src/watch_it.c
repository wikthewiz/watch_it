/**
 * WATCH_IT.c
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "event_handling.h"
#include <syslog.h>
#include <unistd.h>
#include <sys/inotify.h>


/* Allow for 1024 simultaneous events */
#define BUFF_SIZE ((sizeof(struct inotify_event)+FILENAME_MAX)*1024)

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
		syslog(LOG_ERR,"failed to load config file");
		return -1;
	}

	init_name_map();
	int res = begin_watch(config);
	config_free(config);
	syslog(LOG_WARNING,"exit");
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
		syslog(LOG_ERR,"inotify_init failed");
		return -1;
	}

	if (config->fire_on == 0)
	{
		syslog(LOG_ERR,"nothing to fire on!?");
		return -1;
	}

	if (config->watch_dir_count == 0)
	{
		syslog(LOG_ERR,"nothing to watch!?");
		return -1;
	}

	if (config->type == 0)
	{
		syslog(LOG_ERR, "Must watch content, directory or both. You have selected neither!?");
		return -1;
	}


	if (event_handling_init(config))
	{
		syslog(LOG_ERR,"Failed to initiate event_handling ");
		return -1;
	}

	for (int i = 0; i < config->watch_dir_count; ++i)
	{
		if (strlen(config->watch_dir[i]) == 0)
		{
			syslog(LOG_WARNING,"empty watch_dir");
			continue;
		}

		syslog(LOG_DEBUG,"to be watched:%s",config->watch_dir[i]);

		wd = inotify_add_watch (fd, config->watch_dir[i], config->fire_on);

		if (wd < 0)
		{
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
		syslog(LOG_ERR,"TO MANY DIRS TO WATCH. MAX_WATCH = 1025");
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
			syslog(LOG_WARNING,"couldn't find the event that fired!?");
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

		event.id = pevent->wd;
		event.name = event_name;
		event.min_read_close = tuple->min_read_close;
		event.timestamp = event_handling_get_tick();
		event.event_type = pevent->mask;

		if (event_handling_add_event(&event)){
			syslog(LOG_ERR,"failed to add event!");
			return;
		}
	}

}
