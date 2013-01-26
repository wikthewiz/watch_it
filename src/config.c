/*
 * config.c
 */
#include "config.h"
#include <sys/inotify.h>
#include "iniparser/iniparser.h"
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>

static inline int equals(const char * const val, const char * const other)
{
	return strcasecmp(val, other) == 0;
}

static inline int is_true(const char * const val)
{
	return equals(val, "true");
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

int is_dir(char *dirName)
{
	struct stat buf;
	if (lstat(dirName,&buf))
	{
		syslog(LOG_ERR,"FAILED: lstat(%s): \n\tcause:%s",dirName,strerror(errno));
		return -1;
	}

	return S_ISDIR(buf.st_mode);
}


int count_all_dirs(char *dir_name)
{
	DIR *dir;
	struct dirent *ent;
	int cur_count;
	cur_count = 0;

	dir = opendir(dir_name);
	if (dir == NULL)
	{
		return -1;
	}

	/* print all the files and directories within directory */
	while ((ent = readdir(dir)) != NULL )
	{
		int errRes = 0;
		char nextFile[FILENAME_MAX];
		memset(nextFile,'\0',FILENAME_MAX);
		strcat(nextFile,dir_name);
		strcat(nextFile,"/");
		strcat(nextFile,ent->d_name);

		if (equals(".", ent->d_name) || equals("..",ent->d_name)) continue;
		if ( !(errRes = is_dir(nextFile)) && errRes != -1) continue;
		if (errRes == -1) return -1;

		++cur_count;
		int count = count_all_dirs(nextFile);
		if (count < 0)
		{
			closedir(dir);
			return -1;
		}
		cur_count += count;
	}

	if (closedir(dir))
	{
		return -1;
	}
	return cur_count;
}

int write_to_watchdir(struct conf *config, char *pch)
{
	config->watch_dir[config->watch_dir_count] =
			(char*) malloc(sizeof(char) * strlen(pch) + 1);
	if (config->watch_dir[config->watch_dir_count] == NULL )
	{
		syslog(LOG_ERR,"FAILD: Faild to malloc:%s",strerror(errno));
		return -1;
	}
	strcpy((config->watch_dir[config->watch_dir_count]), pch);
	config->watch_dir_count++;
	return 0;
}

int load_watch_dir_rec(char *curDir, struct conf *config)
{
	DIR *dir;
	struct dirent *ent;
	char nextFile[FILENAME_MAX];

	dir = opendir(curDir);
	if (dir == NULL)
	{
		return -1;
	}

	/* print all the files and directories within directory */
	while ((ent = readdir(dir)) != NULL )
	{
		int errRes = 0;
		if (equals(".", ent->d_name) || equals("..",ent->d_name)) continue;

		memset(nextFile,'\0',FILENAME_MAX);
		strcat(nextFile,curDir);
		strcat(nextFile,"/");
		strcat(nextFile,ent->d_name);

		if ( !(errRes = is_dir(nextFile)) && errRes != -1) continue;
		if (errRes == -1) return -1;

		if (write_to_watchdir(config,nextFile))
		{
			closedir(dir);
			return -1;
		}

		if( load_watch_dir_rec(nextFile, config))
		{
			closedir(dir);
			return -1;
		}
	}

	return closedir(dir);
}

int load_watch_dir(dictionary *dict, struct conf *config)
{
	char copy_of_val[FILENAME_MAX];
	memset(copy_of_val,'\0',FILENAME_MAX);
	char *def = "";
	char *watch_dir = "folder:watch_dir";
	strcpy(copy_of_val, iniparser_getstring(dict, watch_dir, def));
	char* pch = strtok(copy_of_val, ", ");
	while (pch != NULL )
	{
		if (write_to_watchdir(config,pch))
		{
			return -1;
		}

		if ( is_recursive(dict) )
		{
			load_watch_dir_rec(pch, config);
		}

		pch = strtok(NULL, ", ");
	}

	return 0;
}

static inline int load_watch_content(dictionary* dict, struct conf* config)
{
	config->type |= WATCH_CONTENT;
	return 0;
}


int get_watch_dir_count(dictionary* dict)
{
	char *val = {0};
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
		if (is_recursive(dict))
		{
			int nbrOfRecFolders = count_all_dirs(pch);
			if (nbrOfRecFolders < 0) return -1;
			count += nbrOfRecFolders;
		}
		pch = strtok(NULL, ", ");
	}

	return count;
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
			syslog(LOG_ERR,"unknown option:%s",pch);
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
		memset(config->watch_dir,'\0',watch_dir_count);
	}
	config_init(config);
	return config;
}

static inline int alloc_cmd(const char const* cmd, dictionary *dict, char **to_be_malloc)
{
	char *def = "";
	int len = strlen(iniparser_getstring(dict, cmd, def));
	*to_be_malloc = (char*)malloc(sizeof(char*) * len + 1);
	if (*to_be_malloc == NULL)
	{
		return -1;
	}
	memset(*to_be_malloc,'\0',len);
	return 0;
}

static inline int load_cmd(const char const* cmd, dictionary *dict, char *to_be_loaded)
{
	char *def = "";
	return strcpy(to_be_loaded, iniparser_getstring(dict, cmd, def)) == NULL;
}

int load_commands(dictionary *dict, struct conf *config)
{
	if (alloc_cmd("folder:close_command", dict, &config->close_cmd))
	{
		return -1;
	}
	if (load_cmd("folder:close_command", dict, config->close_cmd))
	{
		return -1;
	}

	if (alloc_cmd("folder:open_command", dict, &config->open_cmd))
	{
		return -1;
	}
	return load_cmd("folder:open_command", dict, config->open_cmd);
}
struct conf* config_load()
{
	dictionary *dict;
	struct conf *config = NULL;
	int res = 0;
	if( (res = access( CONFIG_FILE, R_OK )))
	{
		syslog(LOG_ERR,"FAILED to access: %s\n\tcause:%s",CONFIG_FILE,strerror(errno));
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

	if (load_commands(dict,config))
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

	if (config->close_cmd)
	{
		free(config->close_cmd);
	}
	if (config->open_cmd)
	{
		free(config->open_cmd);
	}
	free(config);
}
void config_init(struct conf *config)
{
	config->type = 0;
	config->fire_on = 0;
	config->min_read_close = 0;
	config->watch_dir_count = 0;
	config->close_cmd = NULL;
	config->open_cmd = NULL;
}
