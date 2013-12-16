cmake_minimum_required(VERSION 2.8)
project(libpagekite)

include(FindOpenSSL)

include_directories("." ${OPENSSL_INCLUDE_DIR})
set(CMAKE_C_FLAGS "-g -O3 -std=c99 -pedantic -Wall -W -fpic -fno-strict-aliasing")

add_library(pagekite SHARED pkerror.c pkproto.c pkconn.c pkblocker.c pkmanager.c
    pklogging.c pkstate.c utils.c pd_sha1.c win_evwrap.c)
target_link_libraries(pagekite ${OPENSSL_LIBRARIES} ev m pthread)

add_executable(httpkite httpkite.c)
target_link_libraries(httpkite pagekite)

add_executable(pagekitec pagekitec.c)
target_link_libraries(pagekitec pagekite)

enable_testing()
add_executable(tests tests.c pkproto_test.c pkmanager_test.c sha1_test.c
    utils_test.c)
target_link_libraries(tests pagekite)
add_test(Tests tests)

