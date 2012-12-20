/*
 * config.h
 *
 *  Created on: 16 Nov 2012
 *      Author: CONSEO\mawi
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdio.h>

#define WATCH_CONTENT 0x00000001 /*watch the content if it is dir*/
#define WATCH_DIR 0x00000002 /*watch the dir if it is dir*/

#define START_OF_READ 0x00100000
#define START_OF_WRITE 0x00200000

#define END_OF_READ 0x00300000
#define END_OF_WRITE 0x00400000

#define MAX_WATCH 1024

#define CONFIG_FILE "./config"

struct conf{
	char **watch_dir;
	int watch_dir_count;
	int type;
	int recursive;
	int fire_on;
	int min_read_close;
};
struct conf* config_load();
void config_init(struct conf *config);
void config_free(struct conf *config);

#endif /* CONFIG_H_ */
