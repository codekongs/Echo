cmake_minimum_required(VERSION 3.4.1)

add_library( echo-lib
             SHARED
             src/main/jni/echo-lib.c)

find_library(log-lib
             log )

target_link_libraries( echo-lib
                       ${log-lib} )