cmake_minimum_required(VERSION 3.0.0)
project(term_control VERSION 0.1.0)

include(CTest)
enable_testing()

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(term_control main.cpp serial/src/serial.cc serial/src/impl/unix.cc serial/src/impl/list_ports/list_ports_linux.cc minipes/pes.cpp) 
target_link_libraries(term_control CONAN_PKG::fmt CONAN_PKG::cxxopts)
target_include_directories(term_control PRIVATE serial/include minipes)
set_target_properties(term_control PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
