cmake_minimum_required(VERSION 4.0)
project(SMA_Back C)

set(CMAKE_C_STANDARD 11)

#add_library(SMA_Back STATIC
#        sma.c
#        readMatrix.c
#        myjni.c
#        fitness.c
#        sma.h
#        myjni.h
#        jni_md.h
#        main.c)

add_executable(SMA_Back main.c sma.c readMatrix.c myjni.c fitness.c)

