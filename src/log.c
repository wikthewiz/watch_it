/*
 * log.c
 *
 *  Created on: 24 Jan 2013
 *      Author: CONSEO\mawi
 */
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#define LOG_FILE "stdout"

#define LOG_FORMAT "[%i-%i-%i %i:%i:%i][%s]:%s\n"
#define TIME_FORMAT (1900 + tm.tm_year), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec
#define DEBUG "d"
#define INFO "i"
#define WARNING "w"
#define ERROR "e"

static int stop_logging;
static inline FILE* open_file()
{
	if (stop_logging) return NULL;
	if (!strcmp(LOG_FILE,"stdout"))
	{
		return stdout;
	}
	return fopen(LOG_FILE,"w+");
}
static inline void close_file(FILE* file)
{
	if (stop_logging) return;
	if (!file) return;
	fflush(file);
	if (!strcmp( LOG_FILE,"stdout"))
	{
		return;
	}
	stop_logging = fclose(file);
}

static inline void write_to_log(const char const* lvl, const char const *msg)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t,&tm);
	FILE *f = open_file();
	if (f)
	{
		fprintf(f, LOG_FORMAT, TIME_FORMAT, lvl, msg);
		close_file(f);
	}
}

void log_init(void)
{
	stop_logging = 0;
}
void log_d(char *msg)
{
	write_to_log(DEBUG,msg);
}

void log_i(char *msg)
{
	write_to_log(INFO,msg);
}

void log_w(char *msg)
{
	write_to_log(WARNING,msg);
}

void log_e(char *msg)
{
	write_to_log(ERROR,msg);
}
