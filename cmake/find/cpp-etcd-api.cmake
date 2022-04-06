set (ETCD_CPP_API_LIBRARY "etcd-cpp-api")
option (USE_ETCD_CPP_API "Use ETCD_CPP_API" ${ENABLE_LIBRARIES})

set (ETCD_CPP_API_INCLUDE_DIR "${ClickHouse_SOURCE_DIR}/contrib/etcd-cpp-apiv3")
if (NOT EXISTS "${ETCD_CPP_API_INCLUDE_DIR}/etcd/Client.hpp")
    message (WARNING "submodule contrib/etcd-cpp-apiv3 is missing. to fix try run: \n git submodule update --init")
    if (USE_ETCD_CPP_API)
         message (${RECONFIGURE_MESSAGE_LEVEL} "Can't find internal ETCD_CPP_API library")
    endif()
    return()
endif ()

#set (BUILD_SHARED_LIBS OFF)
message (STATUS "Using ETCD_CPP_API=${ETCD_CPP_API_INCLUDE_DIR}: ${ETCD_CPP_API_LIBRARY}")

#include_directories("${ETCD_CPP_API_INCLUDE_DIR}")
