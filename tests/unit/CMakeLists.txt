# 단위 테스트 (SWE.4). 상세 설계의 결정표에서 도출한다. TDD 테스트(tests/tdd)와 분리되어 있다.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/unit)

# input_guard: 실제 구현을 시험한다.
add_executable(ut_input_guard input_guard/input_guard_test.cpp)
target_link_libraries(ut_input_guard PRIVATE cl_input_guard GTest::gtest_main)
add_test(NAME ut_input_guard COMMAND ut_input_guard)

# arbiter: 실제 구현을 시험한다.
add_executable(ut_arbiter arbiter/arbiter_test.cpp)
target_link_libraries(ut_arbiter PRIVATE cl_arbiter GTest::gtest_main)
add_test(NAME ut_arbiter COMMAND ut_arbiter)

# lock_ctrl: 하위 컴포넌트(input_guard, arbiter)를 스텁으로 대체해 방어 경로까지 도달한다.
add_executable(ut_lock_ctrl lock_ctrl/lock_ctrl_test.cpp stubs/component_stubs.cpp)
target_include_directories(ut_lock_ctrl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
target_link_libraries(ut_lock_ctrl PRIVATE cl_lock_ctrl GTest::gtest_main)
add_test(NAME ut_lock_ctrl COMMAND ut_lock_ctrl)

# decision_log: 실제 구현을 시험한다.
add_executable(ut_decision_log decision_log/decision_log_test.cpp)
target_link_libraries(ut_decision_log PRIVATE cl_decision_log GTest::gtest_main)
add_test(NAME ut_decision_log COMMAND ut_decision_log)

# status_view: 실제 구현을 시험한다.
add_executable(ut_status_view status_view/status_view_test.cpp)
target_link_libraries(ut_status_view PRIVATE cl_status_view GTest::gtest_main)
add_test(NAME ut_status_view COMMAND ut_status_view)

