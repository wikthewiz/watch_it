#
# watch_it Makefile
#

# compiler settings
CC = gcc
CFLAGS = -Wall -pedantic -std=c99

# Linker settings
LDLIBS = -lrt -lpthread
NAME = watch_it

SRCS = src/iniparser/iniparser.c src/iniparser/dictionary.c \
	   src/config.c src/watch_it.c src/event_handling.c
DEPS = $(SRCS:.c=.h)
OBJ_DIR = obj
OBJS = $(SRCS:.c=.o)

%.o: $(SRCS) $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJS)
	$(CC) $(CFLAGS) -O3 -fmessage-length=0 -o $(NAME) $(OBJS) $(LDLIBS) 

