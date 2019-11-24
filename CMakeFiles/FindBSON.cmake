# FindBSON.cmake
# --
# Find the BSON-cpp library
#
# This module defines:
#   BSON_INCLUDE_DIRS - where to find bson/bson.h
#   BSON_LIBRARIES    - bson-cpp
#   BSON_FOUND        - True if BSON-cpp was found

# Include(FindModule)
# FIND_MODULE(BSON bson/bson.h "" "" bson-cpp "" "")

find_path(BSON_INCLUDE_DIR libbson-1.0/bson.h)
find_library(BSON_LIBRARY NAMES bson-1.0 libbson-1.0)
if(WIN32)
    find_program(BSON_DLL libbson-1.0.dll)
endif()

set(BSON_INCLUDE_DIR "${BSON_INCLUDE_DIR}/libbson-1.0")

set(BSON_LIBRARIES ${BSON_LIBRARY})
set(BSON_INCLUDE_DIRS ${BSON_INCLUDE_DIR})

if(WIN32)
    set(BSON_DLLS ${BSON_DLL})
endif()

include(FindPackageHandleStandardArgs)

if(WIN32)
    find_package_handle_standard_args(BSON DEFAULT_MSG BSON_LIBRARY BSON_INCLUDE_DIR BSON_DLL)
    mark_as_advanced(BSON_INCLUDE_DIR BSON_LIBRARY BSON_DLL)
else()
    find_package_handle_standard_args(BSON DEFAULT_MSG BSON_LIBRARY BSON_INCLUDE_DIR)
    mark_as_advanced(BSON_INCLUDE_DIR BSON_LIBRARY)
endif()