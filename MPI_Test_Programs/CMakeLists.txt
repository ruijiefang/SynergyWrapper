cmake_minimum_required(VERSION 3.3)
project(MPI_Test_Programs)
if (APPLE)
    set(CMAKE_C_COMPILER /usr/local/bin/mpicc)
else ()
    set(CMAKE_C_COMPILER mpicc)
endif ()

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c)
add_executable(MPI_Test_Programs ${SOURCE_FILES})
