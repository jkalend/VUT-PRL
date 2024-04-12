# This file handles building each task as sub-project.
#
# Each task becomes an independent project in which you can experiment.
#
# Tasks are added as separate sub-projects so as compilation errors in
# one task don't affect another task.

cmake_minimum_required(VERSION 3.16)
project(VUT-PRL)

set(CMAKE_CXX_STANDARD 20)

find_package(MPI REQUIRED)

#find_package(MPI REQUIRED)
add_executable(task1
        task1/pms.cpp
)

add_executable(task2
        task2/life.cpp
)

target_link_libraries(task1 PRIVATE MPI::MPI_CXX)
target_link_libraries(task2 PRIVATE MPI::MPI_CXX)
