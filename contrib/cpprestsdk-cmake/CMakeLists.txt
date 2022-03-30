# cmake_policy(SET CMP0022 NEW)
set(LIBRARY_DIR "${ClickHouse_SOURCE_DIR}/contrib/cpprestsdk/Release")


file(GLOB HEADERS_CPPREST "${LIBRARY_DIR}/include/cpprest/*.h" "${LIBRARY_DIR}/include/cpprest/*.hpp" "${LIBRARY_DIR}/include/cpprest/*.dat")
file(GLOB HEADERS_PPLX "${LIBRARY_DIR}/include/pplx/*.h" "${LIBRARY_DIR}/include/pplx/*.hpp")
file(GLOB HEADERS_DETAILS "${LIBRARY_DIR}/include/cpprest/details/*.h" "${LIBRARY_DIR}/include/cpprest/details/*.hpp" "${LIBRARY_DIR}/include/cpprest/details/*.dat" "${LIBRARY_DIR}/include/pplx/*.hpp" "${LIBRARY_DIR}/include/pplx/*.dat")
source_group("Header Files\\cpprest" FILES ${HEADERS_CPPREST})
source_group("Header Files\\pplx" FILES ${HEADERS_PPLX})
source_group("Header Files\\cpprest\\details" FILES ${HEADERS_DETAILS})

file(GLOB HEADER_PPLX_THREADPOOL "${LIBRARY_DIR}/include/pplx/threadpool.h")
list(REMOVE_ITEM HEADERS_PPLX ${HEADER_PPLX_THREADPOOL})

set(SOURCES
  ${HEADERS_CPPREST}
  ${HEADERS_PPLX}
  ${HEADERS_DETAILS}
  ${LIBRARY_DIR}/src/pch/stdafx.h
  ${LIBRARY_DIR}/src/http/client/http_client.cpp
  ${LIBRARY_DIR}/src/http/client/http_client_impl.h
  ${LIBRARY_DIR}/src/http/client/http_client_msg.cpp
  ${LIBRARY_DIR}/src/http/common/connection_pool_helpers.h
  ${LIBRARY_DIR}/src/http/common/http_compression.cpp
  ${LIBRARY_DIR}/src/http/common/http_helpers.cpp
  ${LIBRARY_DIR}/src/http/common/http_msg.cpp
  ${LIBRARY_DIR}/src/http/common/internal_http_helpers.h
  ${LIBRARY_DIR}/src/http/listener/http_listener.cpp
  ${LIBRARY_DIR}/src/http/listener/http_listener_msg.cpp
  ${LIBRARY_DIR}/src/http/listener/http_server_api.cpp
  ${LIBRARY_DIR}/src/http/listener/http_server_impl.h
  ${LIBRARY_DIR}/src/http/oauth/oauth1.cpp
  ${LIBRARY_DIR}/src/http/oauth/oauth2.cpp
  ${LIBRARY_DIR}/src/json/json.cpp
  ${LIBRARY_DIR}/src/json/json_parsing.cpp
  ${LIBRARY_DIR}/src/json/json_serialization.cpp
  ${LIBRARY_DIR}/src/uri/uri.cpp
  ${LIBRARY_DIR}/src/uri/uri_builder.cpp
  ${LIBRARY_DIR}/src/utilities/asyncrt_utils.cpp
  ${LIBRARY_DIR}/src/utilities/base64.cpp
  ${LIBRARY_DIR}/src/utilities/web_utilities.cpp
)

add_library(cpprest ${SOURCES})
target_include_directories(cpprest
  PUBLIC
    ${LIBRARY_DIR}/include
  PRIVATE
    ${LIBRARY_DIR}/src/pch
)

set(CPPREST_WEBSOCKETS_IMPL none)
target_compile_definitions(cpprest PUBLIC -DCPPREST_EXCLUDE_WEBSOCKETS=1)


set(CPPREST_EXCLUDE_COMPRESSION OFF)
set(CPPREST_EXCLUDE_BROTLI ON)

target_link_libraries(cpprest
    PRIVATE ${ZLIB_LIBRARIES}
)

target_sources(cpprest PRIVATE ${LIBRARY_DIR}/src/pplx/pplxlinux.cpp ${LIBRARY_DIR}/src/pplx/pplx.cpp ${LIBRARY_DIR}/src/pplx/threadpool.cpp ${LIBRARY_DIR}/include/pplx/threadpool.h)

set(CPPREST_HTTP_CLIENT_IMPL asio)

#cpprest_find_boost()
#cpprest_find_openssl()
target_compile_definitions(cpprest PUBLIC -DCPPREST_FORCE_HTTP_CLIENT_ASIO)
target_sources(cpprest PRIVATE ${LIBRARY_DIR}/src/http/client/http_client_asio.cpp ${LIBRARY_DIR}/src/http/client/x509_cert_utilities.cpp)
target_link_libraries(cpprest PUBLIC ${OPENSSL_CRYPTO_LIBRARY} ${OPENSSL_SSL_LIBRARY})


set(CPPREST_FILEIO_IMPL posix)

target_sources(cpprest PRIVATE ${LIBRARY_DIR}/src/streams/fileio_posix.cpp)

set(CPPREST_HTTP_LISTENER_IMPL asio)
# http listener component
#cpprest_find_boost()
#cpprest_find_openssl()
#target_compile_definitions(cpprest PUBLIC -DCPPREST_FORCE_HTTP_LISTENER_ASIO)
target_sources(cpprest PRIVATE ${LIBRARY_DIR}/src/http/listener/http_server_asio.cpp)
#target_link_libraries(cpprest PUBLIC cpprestsdk_boost_internal cpprestsdk_openssl_internal)


#configure_pch(cpprest stdafx.h pch/stdafx.cpp /Zm120)
