cmake_minimum_required(VERSION 3.21)
project(t_snake C)

set(CMAKE_C_STANDARD 11)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(t_snake
        main.c)

target_compile_options(t_snake PUBLIC -lcurses -Wall -Wextra -pedantic)

target_link_libraries(t_snake ${CURSES_LIBRARIES})
