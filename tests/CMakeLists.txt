find_package(Threads REQUIRED)

file(GLOB LSM_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

if(NOT LSM_TEST_SOURCES)
  add_custom_target(lsm_tests_placeholder ALL
    COMMAND ${CMAKE_COMMAND} -E echo "No test sources yet."
    COMMENT "No tests registered"
  )
  return()
endif()

foreach(test_src ${LSM_TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE lsm Threads::Threads)
  target_include_directories(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/include")
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
