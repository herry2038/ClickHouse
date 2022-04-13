
#include(${ClickHouse_SOURCE_DIR}/contrib/etcd-cmake/proto-gen.cmake)
add_subdirectory(proto)

set(LIBRARY_DIR "${ClickHouse_SOURCE_DIR}/contrib/etcd-cpp-apiv3")
set(CMAKE_CURRENT_SOURCE_DIR "${LIBRARY_DIR}/src")

file(GLOB_RECURSE CPP_CLIENT_SRC
                  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp")



set(CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
check_cxx_source_compiles("
#include <grpc++/grpc++.h>
namespace grpc { class Channel; }
int main() {}
" GRPC_CHANNEL_CLASS_FOUND)
if(GRPC_CHANNEL_CLASS_FOUND)
    add_definitions(-DWITH_GRPC_CHANNEL_CLASS)
endif()
add_definitions(-DWITH_GRPC_CHANNEL_CLASS)

set_source_files_properties(${PROTOBUF_GENERATES} PROPERTIES GENERATED TRUE)
foreach (PG ${PROTOBUF_GENERATES})
   message(STATUS "protobuf_generate files: ${PG}")
endforeach()
add_library(etcd-cpp-api ${CPP_CLIENT_SRC} ${PROTOBUF_GENERATES})
add_dependencies(etcd-cpp-api protobuf_generates)

target_link_libraries(etcd-cpp-api PUBLIC
                      ${Boost_LIBRARIES}
                      ${CPPRESTSDK_LIBRARY}
                      ${Protobuf_LIBRARY}
                      ${OPENSSL_LIBRARIES}
                      ${gRPC_LIBRARIES})

target_include_directories(etcd-cpp-api PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../gen ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(etcd-cpp-api PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/proto)
target_include_directories(etcd-cpp-api PRIVATE ${gRPC_INCLUDE_DIRS} ${Protobuf_INCLUDE_DIR})
target_include_directories(etcd-cpp-api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

