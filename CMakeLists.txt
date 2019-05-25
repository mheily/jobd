# Experimental -- use the Makefile instead

cmake_minimum_required(VERSION 3.10)
project(jobd C)

set(jobd_VERSION_MAJOR 0)
set(jobd_VERSION_MINOR 9)
set(jobd_VERSION_PATCH 0)
set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)

set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_RELEASE "-O3 -Wall")

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

set_target_properties(static_sqlite
        PROPERTIES
        COMPILE_FLAGS "-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_JSON1 -w")

#
# Local sources
#
add_executable(init init.c)

add_executable(jobd
        vendor/flopen.c
        vendor/pidfile.c
        vendor/pidfile.h
        array.h
        config.h
        database.c
        database.h
        event_loop.c
        event_loop.h
        ipc.c
        ipc.h
        job.c
        job.h
        jobd.c
        jsonrpc.c
        jsonrpc.h
        logger.c
        logger.h
        memory.h
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
        jsonrpc.c
        jsonrpc.h
        logger.c)

target_link_libraries(jobadm static_sqlite)

add_executable(jobcfg
        database.c
        ipc.c
        job.c
        jobcfg.c
        jsonrpc.c
        jsonrpc.h
        logger.c
        parser.c
        toml.c)

target_link_libraries(jobcfg static_sqlite)

add_executable(jobstat
        database.c
        ipc.c
        jobstat.c
        jsonrpc.c
        jsonrpc.h
        logger.c)

target_link_libraries(jobstat static_sqlite)

add_executable(jobprop
        jobprop.c
        database.c
        ipc.c
        job.c
        jsonrpc.c
        jsonrpc.h
        logger.c
        )

target_link_libraries(jobprop static_sqlite)

#
# Installation
#
install(TARGETS init
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME})

install(TARGETS jobadm jobcfg jobprop jobstat
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS jobd
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_SBINDIR})

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/jobd.8
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man8)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/job.5
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)

install(FILES schema.sql DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/manifests)

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/db/${PROJECT_NAME})

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/log)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/log/${PROJECT_NAME})

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_RUNSTATEDIR}/${PROJECT_NAME})
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_RUNSTATEDIR}/${PROJECT_NAME}/ipc)

enable_testing()

add_test(NAME run
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ./test/run.sh )

configure_file(config.h.in config.h @ONLY)
configure_file(config.inc.in config.inc @ONLY)
