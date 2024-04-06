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
target_link_libraries(task1 PRIVATE MPI::MPI_CXX)