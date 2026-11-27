// lock_ctrl 단위 테스트 (SWE.4). 근거: 상세 설계 FN-020~FN-026, FN-028의 결정표.
// 하위 컴포넌트(input_guard, arbiter)는 스텁으로 대체해 방어 경로까지 도달한다(설계 3.1 "테스트
// 가능성").
#include <gtest/gtest.h>

#include "lock_ctrl/lock_ctrl_if.h"
#include "stub_control.h"

namespace
{

/** 스텁을 초기화하고 제어기를 만든다. */
lock_ctrl_t ready_ctrl(lock_outputs_t initial = {LOCK_RELEASE, LOCK_RELEASE})
{
    stub_reset();
    lock_ctrl_t ctrl = {};
    lock_ctrl_config_t config = {initial};
    EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, &config));
    return ctrl;
}

status_t evaluate(lock_ctrl_t *ctrl, uint32_t now_ms, const raw_command_t *command,
                  lock_ctrl_result_t *result)
{
    lock_ctrl_input_t input = {now_ms, nullptr, command};
    return lock_ctrl_evaluate(ctrl, &input, result);
}

} // namespace

/**
 * @test UT-040
 * @brief lock_ctrl_init은 ctrl이 NULL이면 거절한다 (FN-020 R1).
 * @par 입력값
 * ctrl = NULL, config = NULL
 * @par 절차
 * lock_ctrl_init 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM
 */
TEST(LockCtrlInit, NullCtrlIsRejected)
{
    stub_reset();
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_init(nullptr, nullptr));
}

/**
 * @test UT-041
 * @brief 초기 출력이 열거 범위 밖이면 거절하고 제어기를 바꾸지 않는다 (FN-020 R2, FN-028).
 * @par 입력값
 * config.initial_outputs = (2, RELEASE), (RELEASE, 2), (LOCK, 2). 비교용으로 (LOCK, RELEASE)는 유효
 * @par 절차
 * 값을 미리 채운 ctrl에 각각 lock_ctrl_init 호출
 * @par 예상 출력값
 * 무효 조합은 STATUS_ERR_PARAM이고 ctrl의 outputs 필드 불변. 유효 조합은 STATUS_OK
 */
TEST(LockCtrlInit, InvalidInitialOutputsAreRejected)
{
    stub_reset();
    lock_ctrl_t ctrl = {};
    ctrl.outputs = {LOCK_LOCK, LOCK_LOCK};
    const lock_outputs_t invalid[] = {{static_cast<lock_cmd_t>(2), LOCK_RELEASE},
                                      {LOCK_RELEASE, static_cast<lock_cmd_t>(2)},
                                      {LOCK_LOCK, static_cast<lock_cmd_t>(2)}};
    for (const lock_outputs_t &outputs : invalid)
    {
        lock_ctrl_config_t config = {outputs};
        EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_init(&ctrl, &config));
        EXPECT_EQ(LOCK_LOCK, ctrl.outputs.left);
        EXPECT_EQ(LOCK_LOCK, ctrl.outputs.right);
    }
    lock_ctrl_config_t good = {{LOCK_LOCK, LOCK_RELEASE}};
    EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, &good));
    EXPECT_EQ(LOCK_LOCK, ctrl.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, ctrl.outputs.right);
}

/**
 * @test UT-042
 * @brief 설정이 없으면 초기 출력은 RELEASE이고 초기 결과가 SWR-025대로 만들어진다 (FN-020 R3,
 * FN-026).
 * @par 입력값
 * config = NULL
 * @par 절차
 * lock_ctrl_init 후 lock_ctrl_get_result 호출
 * @par 예상 출력값
 * 출력 (RELEASE, RELEASE), state NORMAL, reason/priority_reason/warning/command_status 모두 NONE,
 * 모든 input_validity STALE
 */
TEST(LockCtrlInit, DefaultInitialResult)
{
    stub_reset();
    lock_ctrl_t ctrl = {};
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, nullptr));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(STATE_NORMAL, result.state);
    EXPECT_EQ(REASON_NONE, result.reason);
    EXPECT_EQ(REASON_NONE, result.priority_reason);
    EXPECT_EQ(WARNING_NONE, result.warning);
    EXPECT_EQ(COMMAND_NONE, result.command_status);
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        EXPECT_EQ(VALIDITY_STALE, result.input_validity[i]);
    }
}

/**
 * @test UT-043
 * @brief 지정한 초기 출력이 초기 결과와 직전 확정 출력으로 쓰인다 (FN-020 R4, SWR-039).
 * @par 입력값
 * config.initial_outputs = (LOCK, LOCK)
 * @par 절차
 * 초기화 후 결과 조회, 이어서 평가 한 번(스텁 결정은 직전 출력을 보지 않음)
 * @par 예상 출력값
 * 초기 결과의 출력 (LOCK, LOCK). 평가 때 arbiter에 넘기는 직전 출력도 (LOCK, LOCK)
 */
TEST(LockCtrlInit, ConfiguredInitialOutputs)
{
    lock_ctrl_t ctrl = ready_ctrl({LOCK_LOCK, LOCK_LOCK});
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(LOCK_LOCK, result.outputs.right);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, g_stub.last_previous.left);
    EXPECT_EQ(LOCK_LOCK, g_stub.last_previous.right);
}

/**
 * @test UT-044
 * @brief 하위 input_guard_init이 실패하면 그 오류를 반환한다 (FN-020 R5).
 * @par 입력값
 * 스텁 input_guard_init이 STATUS_ERR_PARAM을 반환하도록 설정
 * @par 절차
 * lock_ctrl_init 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM
 */
TEST(LockCtrlInit, GuardInitFailureIsPropagated)
{
    stub_reset();
    g_stub.guard_init_status = STATUS_ERR_PARAM;
    lock_ctrl_t ctrl = {};
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_init(&ctrl, nullptr));
}

/**
 * @test UT-045
 * @brief lock_ctrl_evaluate는 ctrl, input, result 중 NULL이 있으면 거절한다 (FN-021 R2~R4).
 * @par 입력값
 * (ctrl NULL), (input NULL), (result NULL)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM이며 하위 컴포넌트를 호출하지 않는다(update_calls 0)
 */
TEST(LockCtrlEvaluate, NullArgumentsAreRejected)
{
    lock_ctrl_t ctrl = ready_ctrl();
    lock_ctrl_input_t input = {10, nullptr, nullptr};
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_evaluate(nullptr, &input, &result));
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_evaluate(&ctrl, nullptr, &result));
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_evaluate(&ctrl, &input, nullptr));
    EXPECT_EQ(0, g_stub.update_calls);
}

/**
 * @test UT-046
 * @brief 평가 시각이 직전에 받아들여진 평가보다 작으면 거절하고 아무것도 바꾸지 않는다
 *        (FN-021 R5, FN-023).
 * @par 입력값
 * now_ms 100 평가(성공) 후 now_ms 99, 같은 100, 101
 * @par 절차
 * 순서대로 평가
 * @par 예상 출력값
 * 99는 STATUS_ERR_TIME(하위 호출 없음, 결과 불변). 100과 101은 STATUS_OK
 */
TEST(LockCtrlEvaluate, TimeRegressionIsRejected)
{
    lock_ctrl_t ctrl = ready_ctrl();
    lock_ctrl_result_t first = {};
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 100, nullptr, &first));
    ASSERT_EQ(1, g_stub.update_calls);
    EXPECT_EQ(STATUS_ERR_TIME, evaluate(&ctrl, 99, nullptr, &result));
    EXPECT_EQ(1, g_stub.update_calls);
    EXPECT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_EQ(first.state, result.state);
    EXPECT_EQ(STATUS_OK, evaluate(&ctrl, 100, nullptr, &result));
    EXPECT_EQ(STATUS_OK, evaluate(&ctrl, 101, nullptr, &result));
}

/**
 * @test UT-047
 * @brief 첫 평가는 now_ms가 0이어도 받아들인다 (FN-023 R1).
 * @par 입력값
 * 초기화 직후 now_ms = 0
 * @par 절차
 * 평가 호출
 * @par 예상 출력값
 * STATUS_OK
 */
TEST(LockCtrlEvaluate, FirstEvaluationAtZero)
{
    lock_ctrl_t ctrl = ready_ctrl();
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_OK, evaluate(&ctrl, 0, nullptr, &result));
}

/**
 * @test UT-048
 * @brief 성공한 평가는 결정과 입력 상태를 결과에 옮기고 직전 확정 출력을 갱신한다
 *        (FN-024 R6, FN-025).
 * @par 입력값
 * 스텁 결정 = 출력 (LOCK, RELEASE), FAULT, 이유 SENSOR_FAULT, 경고 SENSOR_FAULT. 입력 상태 일부
 * INVALID
 * @par 절차
 * 평가 두 번(명령 없음)
 * @par 예상 출력값
 * 결과 필드가 결정과 같고 priority_reason = reason, command_status NONE. 두 번째 평가에서
 * arbiter에 넘긴 직전 출력은 (LOCK, RELEASE). input_guard에 넘긴 시각은 now_ms
 */
TEST(LockCtrlEvaluate, SuccessCommitsResult)
{
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.decision = {
        {LOCK_LOCK, LOCK_RELEASE}, STATE_FAULT, REASON_SENSOR_FAULT, WARNING_SENSOR_FAULT, {}};
    g_stub.normalized.validity[ITEM_GEAR] = VALIDITY_INVALID;
    g_stub.normalized.validity[ITEM_CRASH_STATUS] = VALIDITY_STALE;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(STATE_FAULT, result.state);
    EXPECT_EQ(REASON_SENSOR_FAULT, result.reason);
    EXPECT_EQ(REASON_SENSOR_FAULT, result.priority_reason);
    EXPECT_EQ(WARNING_SENSOR_FAULT, result.warning);
    EXPECT_EQ(COMMAND_NONE, result.command_status);
    EXPECT_EQ(VALIDITY_INVALID, result.input_validity[ITEM_GEAR]);
    EXPECT_EQ(VALIDITY_STALE, result.input_validity[ITEM_CRASH_STATUS]);
    EXPECT_EQ(VALIDITY_VALID, result.input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(20u, g_stub.last_update_now_ms);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 30, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, g_stub.last_previous.left);
    EXPECT_EQ(LOCK_RELEASE, g_stub.last_previous.right);
}

/**
 * @test UT-049
 * @brief 명령이 없으면 명령 검사를 호출하지 않고, 있으면 그 결과를 command_status로 낸다
 *        (FN-024 R2, R6).
 * @par 입력값
 * 명령 없음 / 명령 있음(스텁 결과 REJECTED)
 * @par 절차
 * 각각 평가
 * @par 예상 출력값
 * 명령 없음: check_calls 0, COMMAND_NONE. 명령 있음: check_calls 1, COMMAND_REJECTED
 */
TEST(LockCtrlEvaluate, CommandStatusFlow)
{
    lock_ctrl_t ctrl = ready_ctrl();
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_EQ(0, g_stub.check_calls);
    EXPECT_EQ(COMMAND_NONE, result.command_status);
    g_stub.checked = {COMMAND_REJECTED, 0, 0};
    raw_command_t command = {{true, 9.0}, {true, 0.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, &command, &result));
    EXPECT_EQ(1, g_stub.check_calls);
    EXPECT_EQ(COMMAND_REJECTED, result.command_status);
}

/**
 * @test UT-050
 * @brief input_guard_update가 실패하면 그 오류를 반환하고 이후 단계와 상태 갱신을 건너뛴다
 *        (FN-024 R1).
 * @par 입력값
 * 스텁 update가 STATUS_ERR_PARAM 반환, 명령 있음
 * @par 절차
 * 평가 호출 후 결과 조회, 이어서 더 작은 now_ms로 정상 평가
 * @par 예상 출력값
 * STATUS_ERR_PARAM, check/decide 호출 0회, 결과는 초기 결과 그대로. 실패한 시각은 기록되지 않아
 * 이후 더 작은 시각도 거절되지 않는다
 */
TEST(LockCtrlEvaluate, UpdateFailureStopsPipeline)
{
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.update_status = STATUS_ERR_PARAM;
    raw_command_t command = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, evaluate(&ctrl, 500, &command, &result));
    EXPECT_EQ(0, g_stub.check_calls);
    EXPECT_EQ(0, g_stub.decide_calls);
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_EQ(VALIDITY_STALE, result.input_validity[ITEM_GEAR]);
    g_stub.update_status = STATUS_OK;
    EXPECT_EQ(STATUS_OK, evaluate(&ctrl, 100, nullptr, &result));
}

/**
 * @test UT-051
 * @brief check_command가 실패하면 그 오류를 반환하고 decide를 호출하지 않는다 (FN-024 R2).
 * @par 입력값
 * 스텁 check가 STATUS_ERR_PARAM 반환, 명령 있음
 * @par 절차
 * 평가 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM, decide 호출 0회
 */
TEST(LockCtrlEvaluate, CheckFailureStopsPipeline)
{
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.check_status = STATUS_ERR_PARAM;
    raw_command_t command = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, evaluate(&ctrl, 10, &command, &result));
    EXPECT_EQ(1, g_stub.check_calls);
    EXPECT_EQ(0, g_stub.decide_calls);
}

/**
 * @test UT-052
 * @brief arbiter_decide가 실패하면 그 오류를 반환하고 직전 확정 출력을 갱신하지 않는다
 *        (FN-024 R3, R4).
 * @par 입력값
 * 초기 출력 (LOCK, LOCK), 스텁 decide가 STATUS_ERR_PARAM 반환, 결정은 (RELEASE, RELEASE)
 * @par 절차
 * 명령 없는 평가와 명령 있는 평가를 각각 호출한 뒤 decide를 성공으로 바꿔 평가
 * @par 예상 출력값
 * 두 실패 평가는 STATUS_ERR_PARAM. 이후 평가에서 arbiter에 넘긴 직전 출력은 (LOCK, LOCK) 그대로
 */
TEST(LockCtrlEvaluate, DecideFailureKeepsPreviousOutputs)
{
    lock_ctrl_t ctrl = ready_ctrl({LOCK_LOCK, LOCK_LOCK});
    g_stub.decide_status = STATUS_ERR_PARAM;
    raw_command_t command = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_EQ(STATUS_ERR_PARAM, evaluate(&ctrl, 20, &command, &result));
    g_stub.decide_status = STATUS_OK;
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 30, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, g_stub.last_previous.left);
    EXPECT_EQ(LOCK_LOCK, g_stub.last_previous.right);
}

/**
 * @test UT-053
 * @brief lock_ctrl_get_result는 NULL 인자를 거절한다 (FN-022 R2, R3).
 * @par 입력값
 * (ctrl NULL), (result NULL)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM
 */
TEST(LockCtrlGetResult, NullArgumentsAreRejected)
{
    lock_ctrl_t ctrl = ready_ctrl();
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_result(nullptr, &result));
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_result(&ctrl, nullptr));
}

/**
 * @test UT-080
 * @brief 초기화는 접근위험 기록과 결과의 override 상태를 지운다 (FN-020, FN-026, SWR-025).
 * @par 입력값
 * 접근위험 기록(override true)이 채워진 ctrl
 * @par 절차
 * lock_ctrl_init 후 결과 조회
 * @par 예상 출력값
 * 결과의 override_active가 좌, 우 모두 false. 다음 평가에서 arbiter에 넘기는 기록도 모두 false
 */
TEST(LockCtrlApproach, InitClearsApproachState)
{
    stub_reset();
    lock_ctrl_t ctrl = {};
    ctrl.approach.door[DOOR_LEFT] = {true, true, 77};
    ctrl.approach.door[DOOR_RIGHT] = {true, true, 88};
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, nullptr));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_FALSE(result.override_active[DOOR_LEFT]);
    EXPECT_FALSE(result.override_active[DOOR_RIGHT]);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_FALSE(g_stub.last_approach_in.door[DOOR_LEFT].override_active);
    EXPECT_FALSE(g_stub.last_approach_in.door[DOOR_RIGHT].pending);
}

/**
 * @test UT-081
 * @brief arbiter가 낸 새 접근위험 기록이 결과의 override 상태가 되고 다음 평가의 입력으로 전달된다
 *        (FN-024, FN-025).
 * @par 입력값
 * 스텁 결정의 접근위험 기록: 좌 {pending false, override true, 500}, 우 {pending true, override
 * false, 600}
 * @par 절차
 * 평가 두 번(두 번째는 명령 포함, 스텁 검사 결과 ACCEPTED(side 1, action 1))
 * @par 예상 출력값
 * 첫 결과: override_active 좌 true, 우 false. 두 번째 평가에서 arbiter에 넘긴 기록이 첫 결정의
 * 기록과 같고, 검사한 명령(side 1, action 1)과 평가 시각(20)이 그대로 전달됨
 */
TEST(LockCtrlApproach, ApproachStateIsCommittedAndPassedOn)
{
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.decision.approach.door[DOOR_LEFT] = {false, true, 500};
    g_stub.decision.approach.door[DOOR_RIGHT] = {true, false, 600};
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_TRUE(result.override_active[DOOR_LEFT]);
    EXPECT_FALSE(result.override_active[DOOR_RIGHT]);
    g_stub.checked = {COMMAND_ACCEPTED, 1, 1};
    raw_command_t command = {{true, 1.0}, {true, 1.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, &command, &result));
    EXPECT_TRUE(g_stub.last_approach_in.door[DOOR_LEFT].override_active);
    EXPECT_FALSE(g_stub.last_approach_in.door[DOOR_LEFT].pending);
    EXPECT_EQ(500u, g_stub.last_approach_in.door[DOOR_LEFT].pending_ms);
    EXPECT_TRUE(g_stub.last_approach_in.door[DOOR_RIGHT].pending);
    EXPECT_EQ(600u, g_stub.last_approach_in.door[DOOR_RIGHT].pending_ms);
    EXPECT_EQ(COMMAND_ACCEPTED, g_stub.last_command.status);
    EXPECT_EQ(1, g_stub.last_command.side);
    EXPECT_EQ(1, g_stub.last_command.action);
    EXPECT_EQ(20u, g_stub.last_decide_now_ms);
}

/**
 * @test UT-082
 * @brief arbiter가 실패하면 새 접근위험 기록을 확정하지 않는다 (FN-024 R3, R4).
 * @par 입력값
 * 스텁 decide가 STATUS_ERR_PARAM과 override 기록을 반환, 이후 정상 결정(빈 기록)
 * @par 절차
 * 실패 평가 한 번 후 정상 평가 한 번
 * @par 예상 출력값
 * 정상 평가에서 arbiter에 넘긴 접근위험 기록이 모두 false(실패한 평가의 기록은 반영되지 않음)
 */
TEST(LockCtrlApproach, FailedDecideDoesNotCommitApproachState)
{
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.decide_status = STATUS_ERR_PARAM;
    g_stub.decision.approach.door[DOOR_LEFT] = {true, true, 5};
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, evaluate(&ctrl, 10, nullptr, &result));
    g_stub.decide_status = STATUS_OK;
    g_stub.decision.approach = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, nullptr, &result));
    EXPECT_FALSE(g_stub.last_approach_in.door[DOOR_LEFT].override_active);
    EXPECT_FALSE(g_stub.last_approach_in.door[DOOR_LEFT].pending);
}

/**
 * @test UT-110
 * @brief 결정이 바뀌지 않은 평가는 이력에 추가하지 않고, 바뀐 평가는 필드와 함께 추가한다
 *        (FN-049, FN-046 R5).
 * @par 입력값
 * 초기 결과와 같은 스텁 결정(출력 RELEASE, NORMAL, NONE), 이후 출력 (LOCK, RELEASE) · FAULT ·
 * SENSOR_FAULT · 경고 SENSOR_FAULT 결정
 * @par 절차
 * 평가 두 번(now 10, 20)
 * @par 예상 출력값
 * 첫 평가: 추가 0회. 둘째 평가: 추가 1회, 항목의 now_ms 20, 출력, state, reason, warning이 결정과
 * 같음
 */
TEST(LockCtrlLog, AppendsOnlyOnChange)
{
    lock_ctrl_t ctrl = ready_ctrl();
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_EQ(0, g_stub.log_append_calls);
    g_stub.decision = {
        {LOCK_LOCK, LOCK_RELEASE}, STATE_FAULT, REASON_SENSOR_FAULT, WARNING_SENSOR_FAULT, {}};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, nullptr, &result));
    EXPECT_EQ(1, g_stub.log_append_calls);
    EXPECT_EQ(20u, g_stub.last_appended.now_ms);
    EXPECT_EQ(LOCK_LOCK, g_stub.last_appended.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, g_stub.last_appended.outputs.right);
    EXPECT_EQ(STATE_FAULT, g_stub.last_appended.state);
    EXPECT_EQ(REASON_SENSOR_FAULT, g_stub.last_appended.reason);
    EXPECT_EQ(WARNING_SENSOR_FAULT, g_stub.last_appended.warning);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 30, nullptr, &result));
    EXPECT_EQ(1, g_stub.log_append_calls);
}

/**
 * @test UT-111
 * @brief 결정 변화는 출력 좌, 출력 우, state, 이유 중 어느 하나만 달라도 이력에 추가된다 (FN-046
 * R1~R4).
 * @par 입력값
 * 초기 결과 대비 좌 출력만 / 우 출력만 / state만 / 이유만 다른 스텁 결정
 * @par 절차
 * 각각 새 제어기로 평가
 * @par 예상 출력값
 * 네 경우 모두 이력 추가 1회. 경고만 다른 결정은 추가하지 않는다
 */
TEST(LockCtrlLog, EachFieldChangeIsADecision)
{
    decision_t decisions[4];
    for (decision_t &d : decisions)
    {
        d = {{LOCK_RELEASE, LOCK_RELEASE}, STATE_NORMAL, REASON_NONE, WARNING_NONE, {}};
    }
    decisions[0].outputs.left = LOCK_LOCK;
    decisions[1].outputs.right = LOCK_LOCK;
    decisions[2].state = STATE_OFF;
    decisions[3].reason = REASON_FIRE;
    for (const decision_t &d : decisions)
    {
        lock_ctrl_t ctrl = ready_ctrl();
        g_stub.decision = d;
        lock_ctrl_result_t result = {};
        ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
        EXPECT_EQ(1, g_stub.log_append_calls);
    }
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.decision.warning = WARNING_SENSOR_FAULT;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, nullptr, &result));
    EXPECT_EQ(0, g_stub.log_append_calls);
}

/**
 * @test UT-112
 * @brief 이력 추가가 실패하면 그 오류를 반환하고 직전 확정 출력, 시각, 결과를 갱신하지 않는다
 * (FN-024 R6).
 * @par 입력값
 * 결정이 바뀌는 스텁 결정, 스텁 append가 STATUS_ERR_PARAM 반환
 * @par 절차
 * 평가 후 결과 조회, 이어서 더 작은 now_ms로 정상 평가
 * @par 예상 출력값
 * STATUS_ERR_PARAM, 조회 결과는 초기 결과(state NORMAL). 실패한 시각은 기록되지 않아 더 작은 시각도
 * 허용
 */
TEST(LockCtrlLog, AppendFailureIsNotCommitted)
{
    lock_ctrl_t ctrl = ready_ctrl();
    g_stub.decision = {
        {LOCK_LOCK, LOCK_LOCK}, STATE_FAULT, REASON_SENSOR_FAULT, WARNING_SENSOR_FAULT, {}};
    g_stub.log_append_status = STATUS_ERR_PARAM;
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_ERR_PARAM, evaluate(&ctrl, 500, nullptr, &result));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_EQ(STATE_NORMAL, result.state);
    g_stub.log_append_status = STATUS_OK;
    EXPECT_EQ(STATUS_OK, evaluate(&ctrl, 100, nullptr, &result));
}

/**
 * @test UT-113
 * @brief 이력 초기화가 실패하면 lock_ctrl_init이 그 오류를 반환한다 (FN-020).
 * @par 입력값
 * 스텁 decision_log_init이 STATUS_ERR_PARAM 반환
 * @par 절차
 * lock_ctrl_init 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM
 */
TEST(LockCtrlLog, LogInitFailureIsPropagated)
{
    stub_reset();
    g_stub.log_init_status = STATUS_ERR_PARAM;
    lock_ctrl_t ctrl = {};
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_init(&ctrl, nullptr));
}

/**
 * @test UT-114
 * @brief 이력 조회 함수는 ctrl이 NULL이면 거절하고, 아니면 이력 컴포넌트의 결과를 그대로 낸다
 *        (FN-047, FN-048).
 * @par 입력값
 * ctrl NULL / 스텁 이력 건수 7, 항목 이벤트 ID 42 / 스텁 오류
 * @par 절차
 * lock_ctrl_get_log_count, lock_ctrl_get_log_entry 호출
 * @par 예상 출력값
 * NULL: STATUS_ERR_PARAM. 정상: 건수 7, 항목 event_id 42, 스텁이 받은 index 3. 오류: 스텁의 오류
 * 코드
 */
TEST(LockCtrlLog, QueryPassThrough)
{
    lock_ctrl_t ctrl = ready_ctrl();
    uint32_t count = 0;
    decision_entry_t entry = {};
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_log_count(nullptr, &count));
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_log_entry(nullptr, 0, &entry));
    g_stub.log_count_value = 7;
    g_stub.log_get_entry.event_id = 42;
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(&ctrl, &count));
    EXPECT_EQ(7u, count);
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(&ctrl, 3, &entry));
    EXPECT_EQ(42u, entry.event_id);
    EXPECT_EQ(3u, g_stub.last_get_index);
    g_stub.log_count_status = STATUS_ERR_PARAM;
    g_stub.log_get_status = STATUS_ERR_PARAM;
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_log_count(&ctrl, &count));
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_log_entry(&ctrl, 3, &entry));
}
