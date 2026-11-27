cmake_minimum_required(VERSION 3.28)
project(ClaudeClang C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_COVERAGE "제품 코드에 LLVM 커버리지(MC/DC 포함) 계측을 붙인다" OFF)

find_package(GTest REQUIRED)

# 제품 코드 라이브러리: src/ 의 모든 .c 가 아래 타깃 중 하나에 들어 있어야 한다.
# 경고는 모두 오류다. 커버리지 계측은 제품 코드에만 붙이고 테스트 코드에는 붙이지 않는다.
function(cl_add_product_library name)
    add_library(${name} STATIC ${ARGN})
    target_include_directories(${name} PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(${name} PUBLIC m)
    if(ENABLE_COVERAGE)
        target_compile_options(${name} PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
        target_link_options(${name} PUBLIC -fprofile-instr-generate)
    endif()
endfunction()

cl_add_product_library(cl_input_guard src/input_guard/input_guard.c)
cl_add_product_library(cl_arbiter src/arbiter/arbiter.c)
cl_add_product_library(cl_lock_ctrl src/lock_ctrl/lock_ctrl.c)

# 모든 컴포넌트를 연결한 제품 전체 (통합 테스트, TDD 테스트용)
add_library(cl_product INTERFACE)
target_link_libraries(cl_product INTERFACE cl_lock_ctrl cl_input_guard cl_arbiter)

enable_testing()
foreach(kind tdd unit integration system)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/${kind}/CMakeLists.txt)
        add_subdirectory(tests/${kind})
    endif()
endforeach()
