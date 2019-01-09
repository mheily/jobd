# Experimental -- use the Makefile instead

cmake_minimum_required(VERSION 3.12)
project(jobd C)

set(CMAKE_C_STANDARD 11)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -I$(SQLITE_SRCDIR) -Ivendor")

include_directories(.)
include_directories(vendor)
include_directories(vendor/freebsd-init-r338454)
include_directories(vendor/sqlite-amalgamation-3240000)

#vendor/freebsd-init-r338454/getmntopts.c
#vendor/freebsd-init-r338454/init.c
#vendor/freebsd-init-r338454/mntopts.h
#vendor/freebsd-init-r338454/pathnames.h

#jobadm.c

#if(LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE") # for asprintf()
#endif()

add_library(static_sqlite STATIC
    vendor/sqlite-amalgamation-3240000/shell.c
    vendor/sqlite-amalgamation-3240000/sqlite3.c
    vendor/sqlite-amalgamation-3240000/sqlite3.h
    vendor/sqlite-amalgamation-3240000/sqlite3ext.h)

set_target_properties(static_sqlite PROPERTIES COMPILE_FLAGS "-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION")

add_executable(jobd
        vendor/flopen.c
        vendor/pidfile.c
        vendor/pidfile.h
        array.h
        config.h
        database.c
        database.h
        ipc.c
        ipc.h
        job.c
        job.h
        jobd.c
        logger.c
        logger.h
        parser.c
        parser.h
        queue.h
        toml.c
        toml.h
        )

target_link_libraries(jobd static_sqlite)

add_executable(jobadm
        database.c
        ipc.c
        jobadm.c
        logger.c)

target_link_libraries(jobadm static_sqlite)

add_executable(jobcfg
        database.c
        ipc.c
        job.c
        jobcfg.c
        logger.c
        parser.c
        toml.c)

target_link_libraries(jobcfg static_sqlite)

add_executable(jobstat
        database.c
        ipc.c
        jobstat.c
        logger.c)

target_link_libraries(jobstat static_sqlite)

add_executable(jobprop
        jobprop.c
        database.c
        ipc.c
        job.c
        logger.c
        )

target_link_libraries(jobprop static_sqlite)
