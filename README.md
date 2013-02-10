watch_it
========

A Linux tool for executing a command on read or open of a file. 



IDE
===========

You can use any IDE but I've used eclipse and has therefore included the eclipse projectfiles. So it should only be to include this as an existing project if you use eclipse.

Build
=====

To build run make all

Logging will be done in syslog under watch_it. For more info on syslog run: >man syslog 

Config
======

To use this application you need a config file. The config file must be in the same dir as the executable. 
Currently the are six config options:
 1. watch_dir
 2. fire_on
 3. min_read_close
 4. recursive
 5. open_command
 6. close_command

	warch_it - holds a list of folders to watch
	fire_on - is the list of event to fire. OPEN or CLOSE
	min_read_close - is the minimum time in milliseconds between read and
   					 close events that must elapse before firing open-cmd
	recursive - is a boolean telling if we should recursivle watch all under
			 	folders.
	open_command - The command to execute on OPEN. @0 will be replaced with
				   the file that triggerd the event
	close_command - The command to execute on CLOSE. @0 will be replaced with 
				    the file that triggerd the event

Here is an example config file:

# Each rule in [folder] will be applied on watch dir.
# To make changes for this file to apply you must restart watch_it

[folder] # Tells us we are working on folders and not files
# The comma seperated list of directories to watch.
watch_dir="test, test2" 


# can be any of 
#    OPEN	this will fire once on starting to read or write
#	 CLOSE 
fire_on="OPEN|CLOSE"

# min_read_close is the minimum time in milliseconds between read and a close
# event that must elapse before firing an open event. This is because when 
# just open an directory all files are read and then close. So if you want 
# these events as well you must set this to 0. Also when open a text document
# it is not continual held open therefor it will not fire any open events if
# this is something other then 0.
# Example:
#  A file is read and close in under min_read_close -> This will not fire
min_read_close=200 ; time in milleseconds

recursive=true ; if set to true it will recursively watch all contents

# open_command is the command that will be called on open. @0 will be replaced
# with the file or dir that triggered to event
open_command=echo "this is echo open @0"    ; command called on open
close_command=echo "this is echo! close @0" ; command called on close



This is work in progress more to come

 * TODO: Add config-file explination
 * TODO: Add vidoe record command
