#
# watch_it Makefile
#

# compiler settings
CC = gcc
CFLAGS = -Wall -pedantic -std=c99

# CMD
REMOVE = rm -v

# Linker settings
LDLIBS = -lrt -lpthread

# PATH
NAME = watch_it
SRC_DIR = src
INIPARSER_DIR = $(SRC_DIR)/iniparser

SRCS = $(INIPARSER_DIR)/iniparser.c $(INIPARSER_DIR)/dictionary.c \
	   $(SRC_DIR)/config.c $(SRC_DIR)/watch_it.c $(SRC_DIR)/event_handling.c
DEPS = $(SRCS:.c=.h)
OBJ_DIR = obj
OBJS = $(SRCS:.c=.o)

%.o: $(SRCS) $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJS)
	$(CC) $(CFLAGS) -O3 -fmessage-length=0 -o $(NAME) $(OBJS) $(LDLIBS) 

clean-ini:
	$(REMOVE) $(INIPARSER_DIR)/*.o
clean-watchit:
	$(REMOVE) $(SRC_DIR)/*.o
clean-bin:
	$(REMOVE) $(NAME)
clean: clean-ini clean-watchit clean-bin
