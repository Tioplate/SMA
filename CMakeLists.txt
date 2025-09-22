cmake_minimum_required(VERSION 4.0)
project(SMA_Back C)

set(CMAKE_C_STANDARD 11)
# Windows 下自动导出符号，便于 JNI 导出
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
#add_library(SMA_Back STATIC
#        sma.c
#        readMatrix.c
#        myjni.c
#        fitness.c
#        sma.h
#        myjni.h
#        jni_md.h
#        main.c)
# 统一列出算法相关的源文件（不要把 \*.h 放进来）
set(SMA_SOURCES
        sma.c
        readMatrix.c
        fitness.c
)
# 可选：提供一个静态库目标，满足 ninja 构建 'SMA_Back'
add_library(SMA_Back STATIC ${SMA_SOURCES})
# JNI 动态库，把 myjni.c 和算法实现一起编进 DLL
add_library(myjni SHARED
        myjni.c
        ${SMA_SOURCES}
)
target_include_directories(myjni PRIVATE $ENV{JAVA_HOME}/include $ENV{JAVA_HOME}/include/win32)
#add_executable(SMA_Back main.c sma.c readMatrix.c myjni.c fitness.c)

