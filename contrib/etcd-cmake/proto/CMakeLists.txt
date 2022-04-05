set(LIBRARY_DIR "${ClickHouse_SOURCE_DIR}/contrib/etcd-cpp-apiv3")
#include(${LIBRARY_DIR}/cmake/GenerateProtobuf.cmake)
#include(${LIBRARY_DIR}/cmake/GenerateProtobufGRPC.cmake)

set(CMAKE_CURRENT_SOURCE_DIR "${LIBRARY_DIR}/proto")

file(GLOB_RECURSE PROTO_SRCS "${LIBRARY_DIR}/proto/*.proto")

foreach(f ${PROTO_SRCS})
  Message(WARNING, "find proto file: ${f}")
endforeach(example_file)

set(PROTO_GEN_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen/proto")
file(MAKE_DIRECTORY "${PROTO_GEN_OUT_DIR}")

# use `protobuf_generate` rather than `protobuf_generate_cpp` since we want to
# output the generated files to source dir, rather than binary dir.
#protobuf_generate_latest(
#    LANGUAGE cpp
#    OUT_VAR PROTO_GENERATES
#    PROTOC_OUT_DIR "${PROTO_GEN_OUT_DIR}"
#    PROTOS ${PROTO_SRCS}
#)
#compute_generated_srcs(etcd_PROTO_SOURCES "${PROTO_GEN_OUT_DIR}" false ${PROTO_SRCS})
set(PROTOBUF_GENERATE_CPP_APPEND_PATH FALSE)
set(PROTOBUF_IMPORT_DIRS "${ClickHouse_SOURCE_DIR}/contrib/protobuf/src")

set(PROTO_GRPC_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/rpc.proto"
                    "${CMAKE_CURRENT_SOURCE_DIR}/v3election.proto"
                    "${CMAKE_CURRENT_SOURCE_DIR}/v3lock.proto")

list(REMOVE_ITEM PROTO_SRCS ${PROTO_GRPC_SRCS})


PROTOBUF_GENERATE_CPP(etcd_PROTO_SOURCES etcd_PROTO_HEADERS ${PROTO_SRCS})



#grpc_generate_cpp(PROTO_GRPC_GENERATES PROTO_GRPC_GENERATES_HDRS
    #"${PROTO_GEN_OUT_DIR}"
    #${PROTO_GRPC_SRCS}
#)
#compute_generated_srcs(etcd_grpc_PROTO_SOURCES "${PROTO_GEN_OUT_DIR}" true ${PROTO_GRPC_SRCS})
PROTOBUF_GENERATE_GRPC_CPP(etcd_grpc_PROTO_SOURCES etcd_grpc_PROTO_HEADERS ${PROTO_GRPC_SRCS})


# populate `PROTOBUF_GENERATES` in the parent scope.
set(PROTOBUF_GENERATE_DEPS)
foreach(cxx_file ${etcd_PROTO_SOURCES})
    if(cxx_file MATCHES "cc$")
        list(APPEND PROTOBUF_GENERATE_DEPS ${cxx_file})
    endif()
endforeach()
foreach(cxx_file ${etcd_grpc_PROTO_SOURCES})
    list(APPEND PROTOBUF_GENERATE_DEPS ${cxx_file})
endforeach()

set(PROTOBUF_GENERATES ${etcd_PROTO_SOURCES} ${etcd_grpc_PROTO_SOURCES})
#set(PROTOBUF_GENERATES ${etcd_PROTO_SOURCES} PARENT_SCOPE)
set(PROTOBUF_GENERATES ${etcd_PROTO_SOURCES} ${etcd_grpc_PROTO_SOURCES} PARENT_SCOPE)
set_source_files_properties(${PROTOBUF_GENERATES} PROPERTIES GENERATED TRUE)
# 注释掉下面看看, 看起来是不行
add_custom_target(protobuf_generates DEPENDS ${PROTOBUF_GENERATE_DEPS})
