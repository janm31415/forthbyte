cmake_minimum_required(VERSION 3.10.0)
project (forthbyte)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(cpp-rrb)
add_subdirectory(forthbyte)
add_subdirectory(forth.tests)
add_subdirectory(jtk)
add_subdirectory(pdcurses)


set_property(GLOBAL PROPERTY USE_FOLDERS ON)
