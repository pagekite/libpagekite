cmake_minimum_required(VERSION 2.8)
project(libpagekite)

include(FindOpenSSL)

include_directories("." ${OPENSSL_INCLUDE_DIR} include libpagekite)

file(GLOB libfiles libpagekite/*.c)
file(GLOB testfiles libpagekite/*test*)
list(REMOVE_ITEM libfiles ${testfiles})

message(libfiles ${libfiles})
message(testfiles ${testfiles})
add_library(pagekite SHARED ${libfiles})

target_link_libraries(pagekite ${OPENSSL_LIBRARIES} ev m pthread)

add_executable(httpkite contrib/backends/httpkite.c)
target_link_libraries(httpkite pagekite)

add_executable(pagekitec contrib/backends/pagekitec.c)
target_link_libraries(pagekitec pagekite)

add_executable(hellokite contrib/backends/hello.c)
target_link_libraries(hellokite pagekite)

add_executable(sshkite contrib/backends/ssh.c)
target_link_libraries(sshkite pagekite)

enable_testing()

add_executable(tests ${testfiles})
target_link_libraries(tests pagekite)
add_test(Tests tests)

install(TARGETS pagekitec httpkite hellokite sshkite DESTINATION bin)
install(TARGETS pagekite LIBRARY DESTINATION lib)

