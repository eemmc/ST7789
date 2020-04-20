cmake_minimum_required(VERSION 3.5)

project(demo01 LANGUAGES C)

set(DEPENDDENT_DIR "${PROJECT_SOURCE_DIR}/dependents")


add_library(avfilter STATIC IMPORTED)
set_target_properties(avfilter PROPERTIES
    IMPORTED_LOCATION "${DEPENDDENT_DIR}/lib/libavfilter.a")

add_library(swscale STATIC IMPORTED)
set_target_properties(swscale PROPERTIES
    IMPORTED_LOCATION "${DEPENDDENT_DIR}/lib/libswscale.a")

add_library(avformat STATIC IMPORTED)
set_target_properties(avformat PROPERTIES
    IMPORTED_LOCATION "${DEPENDDENT_DIR}/lib/libavformat.a")

add_library(avcodec STATIC IMPORTED)
set_target_properties(avcodec PROPERTIES
    IMPORTED_LOCATION "${DEPENDDENT_DIR}/lib/libavcodec.a")

add_library(avutil STATIC IMPORTED)
set_target_properties(avutil PROPERTIES
    IMPORTED_LOCATION "${DEPENDDENT_DIR}/lib/libavutil.a")


set(TARGET_DEPENDENCY_LDFLAGS)
list(APPEND TARGET_DEPENDENCY_LDFLAGS
    avfilter swscale avformat avcodec avutil
    pthread  m z
)

include_directories("${DEPENDDENT_DIR}/include")

add_library(ffmpeg slicer.c)
add_library(st7789 st7789.c bcm2835.c)

add_executable(demo01 main.c)
target_link_libraries(demo01
    ffmpeg st7789 ${TARGET_DEPENDENCY_LDFLAGS}
)
