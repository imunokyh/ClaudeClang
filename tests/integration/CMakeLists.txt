# 통합 테스트 (SWE.5). 실제 컴포넌트를 모두 연결해 인터페이스(IF-001~IF-004)를 검증한다.
# 이 SW 안에는 HW·OS 호출이 없으므로 스텁을 쓰지 않는다.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration)

add_executable(it_lock_ctrl lock_ctrl/lock_ctrl_integration_test.cpp)
target_link_libraries(it_lock_ctrl PRIVATE cl_product GTest::gtest_main)
add_test(NAME it_lock_ctrl COMMAND it_lock_ctrl)
