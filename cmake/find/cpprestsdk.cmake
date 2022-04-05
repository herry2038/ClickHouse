set (CPPRESTSDK_LIBRARY "cpprest")
option (USE_CPPRESTSDK "Use CPPRESTSDK" ${ENABLE_LIBRARIES})

set (CPPRESTSDK_INCLUDE_DIR "${ClickHouse_SOURCE_DIR}/contrib/cpprestsdk/Release/include")
if (NOT EXISTS "${CPPRESTSDK_INCLUDE_DIR}/cpprest/http_client.h")
    message (WARNING "submodule contrib/cpprestsdk is missing. to fix try run: \n git submodule update --init")
    if (USE_CPPRESTSDK)
         message (${RECONFIGURE_MESSAGE_LEVEL} "Can't find internal CPPRESTSDK library")
    endif()
    return()
endif ()

option (USE_CPPRESTSDK "Use CPPRESTSDK" ${ENABLE_LIBRARIES})
#set (BUILD_SHARED_LIBS OFF)
message (STATUS "Using CPPRESTSDK=${CPPRESTSDK_INCLUDE_DIR}: ${CPPRESTSDK_LIBRARY}")

#include_directories("${CPPRESTSDK_INCLUDE_DIR}")
