cmake_minimum_required(VERSION 3.20)
project(lsm_tree C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra -Werror -pedantic)

set(CTEST_MEMORYCHECK_TYPE "Valgrind")
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS
    "--tool=memcheck --leak-check=full --error-exitcode=1 --errors-for-leak-kinds=definite,indirect --track-origins=yes --xml=yes")

include(CTest)

add_subdirectory(src)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()
