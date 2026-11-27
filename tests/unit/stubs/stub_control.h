// 단위 테스트용 스텁 제어 구조체. lock_ctrl의 하위 컴포넌트(input_guard, arbiter)를 대체한다.
// 스텁은 상세 설계의 인터페이스(IF-001, IF-002)와 같은 시그니처를 가지며 반환 상태와 결과를 시험이
// 정한다.
#ifndef TESTS_UNIT_STUBS_STUB_CONTROL_H
#define TESTS_UNIT_STUBS_STUB_CONTROL_H

#include "common/types_if.h"

struct StubControl
{
    status_t guard_init_status;
    status_t update_status;
    status_t check_status;
    status_t decide_status;
    normalized_inputs_t normalized;
    checked_command_t checked;
    decision_t decision;
    int update_calls;
    int check_calls;
    int decide_calls;
    uint32_t last_update_now_ms;
    lock_outputs_t last_previous;
    checked_command_t last_command;
    uint32_t last_decide_now_ms;
    approach_state_t last_approach_in;
};

extern StubControl g_stub;

/** 스텁을 기본값(모든 호출 성공, 모든 입력 VALID, 결정 NORMAL)으로 되돌린다. */
void stub_reset();

#endif /* TESTS_UNIT_STUBS_STUB_CONTROL_H */
