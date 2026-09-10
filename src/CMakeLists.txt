file(GLOB LSM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_library(lsm STATIC ${LSM_SOURCES})

target_include_directories(lsm
  PUBLIC "${CMAKE_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_SOURCE_DIR}/src"
)

target_compile_definitions(lsm PUBLIC _POSIX_C_SOURCE=200809L)

target_link_libraries(lsm PUBLIC pthread m)
