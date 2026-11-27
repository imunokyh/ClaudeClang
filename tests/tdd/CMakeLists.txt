# TDD 테스트: 구현을 이끄는 테스트. 단위 테스트(tests/unit)와 분리해 별도 결과·별도 커버리지로 낸다.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/tdd)

add_executable(tdd_lock_ctrl lock_ctrl/lock_ctrl_tdd_test.cpp)
target_include_directories(tdd_lock_ctrl PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tdd_lock_ctrl PRIVATE cl_product GTest::gtest_main)
add_test(NAME tdd_lock_ctrl COMMAND tdd_lock_ctrl)

add_executable(tdd_lock_ctrl_iter2 lock_ctrl/lock_ctrl_tdd_iter2_test.cpp)
target_include_directories(tdd_lock_ctrl_iter2 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tdd_lock_ctrl_iter2 PRIVATE cl_product GTest::gtest_main)
add_test(NAME tdd_lock_ctrl_iter2 COMMAND tdd_lock_ctrl_iter2)

add_executable(tdd_lock_ctrl_iter3 lock_ctrl/lock_ctrl_tdd_iter3_test.cpp)
target_include_directories(tdd_lock_ctrl_iter3 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tdd_lock_ctrl_iter3 PRIVATE cl_product GTest::gtest_main)
add_test(NAME tdd_lock_ctrl_iter3 COMMAND tdd_lock_ctrl_iter3)
