/*
 * config.c
 */
#include "config.h"
#include <sys/inotify.h>
#include "iniparser.h"
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

static inline int equals(const char * const val, const char * const other)
{
	return strcasecmp(val, other) == 0;
}

static inline int is_true(const char * const val)
{
	return equals(val, "true");
}

int load_watch_dir(dictionary *dict, struct conf *config)
{
	char copy_of_val[FILENAME_MAX];
	char *def = "";
	char *watch_dir = "folder:watch_dir";
	strcpy(copy_of_val, iniparser_getstring(dict, watch_dir, def));
	printf("watch_dir val:%s\n", copy_of_val);
	char* pch = strtok(copy_of_val, ", ");
	while (pch != NULL )
	{
		config->watch_dir[config->watch_dir_count] = (char*) malloc(
				sizeof(char) * strlen(pch));
		if (config->watch_dir[config->watch_dir_count] == NULL )
		{
			return -1;
		}
		strcpy((config->watch_dir[config->watch_dir_count]), pch);
		config->watch_dir_count++;
		pch = strtok(NULL, ", ");
	}

	return 0;
}

int load_watch_content(dictionary* dict, struct conf* config)
{
	config->type |= WATCH_CONTENT;
	return 0;
}

int is_recursive(dictionary* dict)
{
    char* def = "";
    char* val = { 0 };
    val = iniparser_getstring(dict, "folder:recursive", def);
    if (val == NULL )
    {
    	return -1;
    }
    return is_true(val);
}

int get_watch_dir_count(dictionary* dict)
{
	char *val =
	{ 0 };
	char *def = "";
	int count = 0;
	char copy_of_val[FILENAME_MAX];
	char *watch_dir = "folder:watch_dir";

	val = iniparser_getstring(dict, watch_dir, def);
	strcpy(copy_of_val, val);
	char *pch = strtok(copy_of_val, ", ");
	while (pch != NULL )
	{
		count++;
		pch = strtok(NULL, ", ");
	}

	if (is_recursive(dict))
	{
		printf("recursive NOT implemented yet!\n");
	}

	return count;
}

int count_all_dirs()
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(".");
	if (dir == NULL)
	{
		return -1;
	}

	/* print all the files and directories within directory */
	while ((ent = readdir(dir)) != NULL )
	{
		if (equals(".", ent->d_name) || equals("..",ent->d_name))
		{
			continue;
		}
		printf("%s\n", ent->d_name);
	}
	return closedir(dir);
}

int load_fire_on(dictionary* dict, struct conf* config)
{
	char* def = "";
	char copy_of_val[1025];
	char* fire_on = "folder:fire_on";
	strcpy(copy_of_val, iniparser_getstring(dict, fire_on, def));
	if (copy_of_val == NULL )
	{
		return -1;
	}

	char* pch = strtok(copy_of_val, " |");
	while (pch != NULL )
	{
		if (equals(pch, "OPEN"))
		{
			config->fire_on |= IN_OPEN;
			config->fire_on |= IN_ACCESS;
		}
		else if (equals(pch, "CLOSE"))
		{
			config->fire_on |= IN_CLOSE;
			config->fire_on |= IN_DELETE;
			config->fire_on |= IN_DELETE_SELF;
		}
		else
		{
			fprintf(stderr, "unknown option:%s\n", pch);
			return -1;
		}
		pch = strtok(NULL, " |");
	}
	return 0;
}

int load_min_read_close(dictionary *dict, struct conf *config)
{
	config->min_read_close = iniparser_getint(dict,
											  "folder:min_read_close",
											  0);
	return 0;
}

struct conf* allocate_config(dictionary *dict)
{
	int watch_dir_count = get_watch_dir_count(dict);
	struct conf* config = (struct conf*) malloc(sizeof(struct conf));
	if (config != NULL )
	{
		int memsize = watch_dir_count * sizeof(char*);
		config->watch_dir = (char**) malloc(memsize);
		if (config->watch_dir == NULL )
		{
			free(config);
			return NULL ;
		}
	}
	config_init(config);
	return config;
}

struct conf* config_load()
{

	count_all_dirs();
	return NULL;
	dictionary *dict;
	struct conf *config = NULL;
	int res = 0;
	if( (res = access( CONFIG_FILE, R_OK )))
	{
		fprintf (stderr,"FAILED to access: %s\n\tcause:%s\n",
				  CONFIG_FILE,
				  strerror( errno ));
		return NULL;
	}

	dict = iniparser_load(CONFIG_FILE);
	if (!dict)
	{
		return NULL ;
	}

	config = allocate_config(dict);
	if (config == NULL )
	{
		goto clean_up;
	}

	if (load_fire_on(dict, config))
	{
		goto clean_up;
	}

	if (load_watch_dir(dict, config))
	{
		goto clean_up;
	}

	if (load_watch_content(dict, config))
	{
		goto clean_up;
	}

	if (load_fire_on(dict, config))
	{
		goto clean_up;
	}

	load_min_read_close(dict, config);

clean_up:
	iniparser_freedict(dict);
	return config;
}

void config_free(struct conf *config)
{
	if (config == NULL )
		return;
	if (config->watch_dir != NULL )
	{
		for (int i = 0; i < config->watch_dir_count; ++i)
		{
			free(*(config->watch_dir + i));
		}
		free(config->watch_dir);
	}
	free(config);
}
void config_init(struct conf *config)
{
	if (config->watch_dir != NULL )
	{
		strcpy(config->watch_dir[0], "\0");
	}
	config->type = 0;
	config->fire_on = 0;
	config->min_read_close = 0;
	config->watch_dir_count = 0;
}

