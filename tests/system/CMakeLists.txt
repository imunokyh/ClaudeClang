# SW 시스템 테스트 (SWE.6). 테스트 명세(docs/swe6/test-cases.json)의 TC를 공개 API(IF-003)로만 수행한다.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/system)

add_executable(st_lock_ctrl lock_ctrl/system_test.cpp)
target_link_libraries(st_lock_ctrl PRIVATE cl_product GTest::gtest_main)
add_test(NAME st_lock_ctrl COMMAND st_lock_ctrl)
