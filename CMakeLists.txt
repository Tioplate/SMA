cmake_minimum_required(VERSION 3.16)
project(SMA_Back C)

set(CMAKE_C_STANDARD 11)
# Windows 下自动导出符号，便于 JNI 导出（对 SHARED 目标生效）
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# 统一列出算法相关的源文件（不要把 *.h 或 main.c 放进库里）
set(SMA_SOURCES
        sma.c
        readMatrix.c
        fitness.c
)

# 可复用的静态库，不与可执行/共享库重名
add_library(sma_core STATIC ${SMA_SOURCES})

# JNI 动态库，把 myjni.c 和算法实现一起编进 DLL
add_library(myjni SHARED
        myjni.c
        ${SMA_SOURCES}
)
# 配置 JDK 头文件目录（要求设置 JAVA_HOME）
if (WIN32)
    target_include_directories(myjni PRIVATE $ENV{JAVA_HOME}/include $ENV{JAVA_HOME}/include/win32)
else()
    target_include_directories(myjni PRIVATE $ENV{JAVA_HOME}/include $ENV{JAVA_HOME}/include/linux)
endif()

# 可执行程序，便于本地直接运行/调试
add_executable(SMA_Back
        main.c
        ${SMA_SOURCES}
)

# 可选：将所有产物输出到构建根目录，便于查找（CLion 默认也在该目录下）
set_target_properties(SMA_Back PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_target_properties(myjni PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 如需在其他项目中链接，可公开头文件目录
# target_include_directories(sma_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
