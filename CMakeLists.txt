cmake_minimum_required(VERSION 3.5)
project(synergy4)
set(CMAKE_C_STANDARD 11)
include(CMakeFiles/FindZeroMQ.cmake)
include(CMakeFiles/FindCZMQ.cmake)
include(CMakeFiles/FindBSON.cmake)
find_package(Threads)
if (APPLE)
    set(  "")
    set(CMAKE_C_COMPILER /usr/local/bin/mpicc)
else()
    set(CMAKE_C_COMPILER mpicc)
endif ()


# **** IMPO ANT *** Set CMake flags to optimize ourselves
set(CMAKE_C_FLAGS "-O3 -ffast-math -march=native")
set(CMAKE_DEBUG_FLAGS "-O3 -ffast-math -march=native")
set(CMAKE_RELEASE_FLAGS "-O3 -ffast-math -march=native")

if (NOT ${LIBZMQ_FOUND})
    message(FATAL_ERROR "Did not find libZMQ, abo ing.")
endif ()
if (NOT ${CZMQ_FOUND})
    message(FATAL_ERROR "Did not find CZMQ, abo ing.")
endif ()
if (NOT ${BSON_FOUND})
    message(FATAL_ERROR "Did not find BSON, abo ing.")
endif ()
include_directories(${LIBZMQ_INCLUDE_DIRS})
include_directories(${BSON_INCLUDE_DIRS})
set(THREADS_PREFER_PTHREAD_FLAG ON)
# our source files
file(GLOB p2pm_SRC "src/*.h" "src/*.c")                 # basic components
file(GLOB p2pmd_SRC "src/p2pmd/*.c" "src/p2pmd/*.h")    # p2pmd service
file(GLOB tsd_SRC "src/tsd/*.c" "src/tsd/*.h src/shared_memory/*.h")          # tsd service
file(GLOB libts_SRC "src/ts/*.c" "src/ts/*.h src/shared_memory/*.h")          # tuple space API
file(GLOB libparser_SRC "src/parser/*.c" "src/parser/*.h") # libparser API (with BSON)
set(sng_tsd_SRC "src/synergy4/synergy4_tsd.c")
# 3rd-pa y source files we compile
file(GLOB libchash_SRC "contrib/libchash/*.h" "contrib/libchash/*.c")
file(GLOB zlog_SRC "contrib/zlog/src/*.c" "contrib/zlog/src/*.h")
set(test2_SRC "src/p2pm_examples/test_2.c")
set(CLIENT_SOURCE_FILES "src/p2pm_examples/tclient.c")
set(SERVER_SOURCE_FILES "src/p2pm_examples/tserver.c")
set(TEST_SOURCE_FILES "src/p2pm_examples/test.c")
set(hello_SRC "src/synergy_examples/sng_hello.c")
set(dhello_master_SRC "src/synergy_examples/sng_dhello.c")
set(dhello_worker_SRC "src/synergy_examples/sng_dhello_worker.c")
set(dhm_master_SRC "src/synergy_examples/sng_hellom_master.c")
set(dhm_worker_SRC "src/synergy_examples/sng_hellom_worker.c")
set(hellob_master_SRC "src/synergy_examples/sng_hello_bcast.c")
set(hellob_worker_SRC "src/synergy_examples/sng_hello_bcast_worker.c")
set(matrix_master_SRC "src/synergy_examples/matrix_master.c")
set(matrix_worker_SRC "src/synergy_examples/matrix_worker.c")
add_library(p2pmd ${p2pmd_SRC} ${p2pm_SRC})
add_library(tsd ${tsd_SRC} ${p2pm_SRC} ${p2pmd_SRC})
add_library(ts ${libts_SRC})
add_library(parser ${libparser_SRC})
add_library(zlog ${zlog_SRC})
add_library(chash ${libchash_SRC})
add_executable(tclient ${CLIENT_SOURCE_FILES})
add_executable(tserver ${SERVER_SOURCE_FILES})
add_executable(ttest ${TEST_SOURCE_FILES})
add_executable(sng_tsd ${sng_tsd_SRC})
add_executable(sng_hello ${hello_SRC})
add_executable(test2 ${test2_SRC})
add_executable(sng_dhello_master ${dhello_master_SRC})
add_executable(sng_dhello_worker ${dhello_worker_SRC})
add_executable(sng_dhm_master ${dhm_master_SRC})
add_executable(sng_dhm_worker ${dhm_worker_SRC})
add_executable(sng_bcast_master ${hellob_master_SRC})
add_executable(sng_bcast_worker ${hellob_worker_SRC})
add_executable(matrix_master ${matrix_master_SRC})
add_executable(matrix_worker ${matrix_worker_SRC})
target_link_libraries(tsd ${LIBZMQ_LIBRARIES} ${CZMQ_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}  )
target_link_libraries(ts ${tsd}  )
target_link_libraries(parser ${BSON_LIBRARIES}  )
target_link_libraries(tclient p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(tserver p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(ttest p2pmd ts tsd parser chash zlog  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_tsd p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_hello p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test2 ${LIBZMQ_LIBRARIES}  
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_dhello_master p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_dhello_worker p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_dhm_master p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_dhm_worker p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_bcast_master p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sng_bcast_worker p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(matrix_master p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(matrix_worker p2pmd ts tsd parser zlog chash  
        ${LIBZMQ_LIBRARIES}
        ${CZMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
