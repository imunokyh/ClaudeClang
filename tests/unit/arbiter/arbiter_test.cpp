// arbiter 단위 테스트 (SWE.4). 근거: 상세 설계 FN-014~FN-019, FN-027의 결정표.
#include <gtest/gtest.h>

#include "arbiter/arbiter_if.h"

namespace
{

/** 모든 항목이 VALID이고 값이 0인 입력. */
normalized_inputs_t all_valid()
{
    normalized_inputs_t inputs = {};
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        inputs.validity[i] = VALIDITY_VALID;
        inputs.value[i] = 0;
    }
    return inputs;
}

lock_outputs_t locked()
{
    return {LOCK_LOCK, LOCK_LOCK};
}

decision_t decide(const normalized_inputs_t &inputs, lock_outputs_t previous)
{
    decision_t out = {};
    EXPECT_EQ(STATUS_OK, arbiter_decide(&inputs, &previous, &out));
    return out;
}

} // namespace

/**
 * @test UT-020
 * @brief arbiter_decide는 NULL 인자를 거절하고 출력을 바꾸지 않는다 (FN-014, R2~R4).
 * @par 입력값
 * (inputs = NULL), (previous = NULL), (out = NULL)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM, out의 state는 호출 전 값(STATE_FAULT) 그대로
 */
TEST(ArbiterParams, NullArgumentsAreRejected)
{
    normalized_inputs_t inputs = all_valid();
    lock_outputs_t previous = locked();
    decision_t out = {};
    out.state = STATE_FAULT;
    EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(nullptr, &previous, &out));
    EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(&inputs, nullptr, &out));
    EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(&inputs, &previous, nullptr));
    EXPECT_EQ(STATE_FAULT, out.state);
}

/**
 * @test UT-021
 * @brief previous의 출력 값이 열거 범위 밖이면 거절한다 (FN-014 R5, FN-027).
 * @par 입력값
 * previous (left, right) = (RELEASE, LOCK), (LOCK, RELEASE) 는 유효.
 * (2, RELEASE), (RELEASE, 2), (LOCK, 2) 는 무효
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * 유효 조합은 STATUS_OK, 무효 조합은 STATUS_ERR_PARAM이며 out을 바꾸지 않는다
 */
TEST(ArbiterParams, OutputRangeValidation)
{
    normalized_inputs_t inputs = all_valid();
    decision_t out = {};
    out.state = STATE_FAULT;
    const lock_outputs_t valid[] = {{LOCK_RELEASE, LOCK_RELEASE},
                                    {LOCK_RELEASE, LOCK_LOCK},
                                    {LOCK_LOCK, LOCK_RELEASE},
                                    {LOCK_LOCK, LOCK_LOCK}};
    for (const lock_outputs_t &previous : valid)
    {
        EXPECT_EQ(STATUS_OK, arbiter_decide(&inputs, &previous, &out));
    }
    out.state = STATE_FAULT;
    const lock_outputs_t invalid[] = {{static_cast<lock_cmd_t>(2), LOCK_RELEASE},
                                      {LOCK_RELEASE, static_cast<lock_cmd_t>(2)},
                                      {LOCK_LOCK, static_cast<lock_cmd_t>(2)}};
    for (const lock_outputs_t &previous : invalid)
    {
        EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(&inputs, &previous, &out));
        EXPECT_EQ(STATE_FAULT, out.state);
    }
}

/**
 * @test UT-022
 * @brief 규칙 1: 유효한 충돌 CONFIRMED는 두 출력을 RELEASE로 확정한다 (FN-017 R1, FN-018).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), 충돌 항목 VALID 값 CONFIRMED(2), 나머지 VALID 0
 * @par 절차
 * arbiter_decide 호출
 * @par 예상 출력값
 * (RELEASE, RELEASE), reason CRASH_CONFIRMED, state NORMAL, warning NONE
 */
TEST(ArbiterRules, CrashConfirmedReleases)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_CRASH_STATUS] = CRASH_CONFIRMED;
    decision_t out = decide(inputs, locked());
    EXPECT_EQ(LOCK_RELEASE, out.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, out.outputs.right);
    EXPECT_EQ(REASON_CRASH_CONFIRMED, out.reason);
    EXPECT_EQ(STATE_NORMAL, out.state);
    EXPECT_EQ(WARNING_NONE, out.warning);
}

/**
 * @test UT-023
 * @brief 규칙 2: 유효한 sensor_fault TRUE는 출력을 유지하고 FAULT와 경고를 낸다 (FN-017 R2, FN-019
 * R1).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), sensor_fault VALID 값 1
 * @par 절차
 * arbiter_decide 호출
 * @par 예상 출력값
 * (LOCK, LOCK), reason SENSOR_FAULT, state FAULT, warning SENSOR_FAULT
 */
TEST(ArbiterRules, SensorFaultHolds)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_SENSOR_FAULT] = 1;
    decision_t out = decide(inputs, locked());
    EXPECT_EQ(LOCK_LOCK, out.outputs.left);
    EXPECT_EQ(LOCK_LOCK, out.outputs.right);
    EXPECT_EQ(REASON_SENSOR_FAULT, out.reason);
    EXPECT_EQ(STATE_FAULT, out.state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, out.warning);
}

/**
 * @test UT-024
 * @brief 규칙 3: INVALID 항목이 있으면 유지하고 DEGRADED와 INPUT_INVALID를 낸다 (FN-017 R3,
 * FN-016).
 * @par 입력값
 * 직전 출력 (LOCK, RELEASE), 차속 항목 INVALID, 나머지 VALID 0
 * @par 절차
 * arbiter_decide 호출
 * @par 예상 출력값
 * (LOCK, RELEASE) 유지, reason INPUT_INVALID, state DEGRADED, warning NONE
 */
TEST(ArbiterRules, InvalidInputHolds)
{
    normalized_inputs_t inputs = all_valid();
    inputs.validity[ITEM_VEHICLE_SPEED] = VALIDITY_INVALID;
    decision_t out = decide(inputs, {LOCK_LOCK, LOCK_RELEASE});
    EXPECT_EQ(LOCK_LOCK, out.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, out.outputs.right);
    EXPECT_EQ(REASON_INPUT_INVALID, out.reason);
    EXPECT_EQ(STATE_DEGRADED, out.state);
    EXPECT_EQ(WARNING_NONE, out.warning);
}

/**
 * @test UT-025
 * @brief 규칙 4: STALE 항목이 있으면 유지하고 DEGRADED와 INPUT_STALE을 낸다 (FN-017 R4).
 * @par 입력값
 * 직전 출력 (RELEASE, LOCK), 모든 항목 STALE
 * @par 절차
 * arbiter_decide 호출
 * @par 예상 출력값
 * (RELEASE, LOCK) 유지, reason INPUT_STALE, state DEGRADED
 */
TEST(ArbiterRules, StaleInputHolds)
{
    normalized_inputs_t inputs = all_valid();
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        inputs.validity[i] = VALIDITY_STALE;
    }
    decision_t out = decide(inputs, {LOCK_RELEASE, LOCK_LOCK});
    EXPECT_EQ(LOCK_RELEASE, out.outputs.left);
    EXPECT_EQ(LOCK_LOCK, out.outputs.right);
    EXPECT_EQ(REASON_INPUT_STALE, out.reason);
    EXPECT_EQ(STATE_DEGRADED, out.state);
}

/**
 * @test UT-026
 * @brief 규칙 5·6: 충돌 PENDING은 출력을 바꾸지 않고 이유만 CRASH_PENDING으로 낸다 (FN-017 R5, R6).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), 충돌 VALID 값 PENDING(1) / 충돌 VALID 값 NONE(0)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 둘 다 (LOCK, LOCK) 유지, state NORMAL. 이유는 PENDING일 때 CRASH_PENDING, NONE일 때 NONE
 */
TEST(ArbiterRules, PendingAndNone)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_CRASH_STATUS] = CRASH_PENDING;
    decision_t pending = decide(inputs, locked());
    EXPECT_EQ(REASON_CRASH_PENDING, pending.reason);
    EXPECT_EQ(LOCK_LOCK, pending.outputs.left);
    EXPECT_EQ(STATE_NORMAL, pending.state);
    inputs.value[ITEM_CRASH_STATUS] = CRASH_NONE;
    decision_t none = decide(inputs, locked());
    EXPECT_EQ(REASON_NONE, none.reason);
    EXPECT_EQ(LOCK_LOCK, none.outputs.right);
    EXPECT_EQ(STATE_NORMAL, none.state);
}

/**
 * @test UT-027
 * @brief 우선순위: 높은 규칙이 성립하면 낮은 규칙을 적용하지 않는다 (SWR-023, FN-017).
 * @par 입력값
 * (충돌 CONFIRMED + sensor_fault), (sensor_fault + INVALID), (INVALID + STALE),
 * (STALE + 충돌 PENDING)
 * @par 절차
 * 각각 직전 출력 (LOCK, LOCK)으로 arbiter_decide 호출
 * @par 예상 출력값
 * 이유는 각각 CRASH_CONFIRMED, SENSOR_FAULT, INPUT_INVALID, INPUT_STALE.
 * 충돌 + sensor_fault는 출력 RELEASE이고 state FAULT, warning SENSOR_FAULT
 */
TEST(ArbiterRules, PriorityOrder)
{
    normalized_inputs_t a = all_valid();
    a.value[ITEM_CRASH_STATUS] = CRASH_CONFIRMED;
    a.value[ITEM_SENSOR_FAULT] = 1;
    decision_t da = decide(a, locked());
    EXPECT_EQ(REASON_CRASH_CONFIRMED, da.reason);
    EXPECT_EQ(LOCK_RELEASE, da.outputs.left);
    EXPECT_EQ(STATE_FAULT, da.state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, da.warning);

    normalized_inputs_t b = all_valid();
    b.value[ITEM_SENSOR_FAULT] = 1;
    b.validity[ITEM_GEAR] = VALIDITY_INVALID;
    EXPECT_EQ(REASON_SENSOR_FAULT, decide(b, locked()).reason);

    normalized_inputs_t c = all_valid();
    c.validity[ITEM_GEAR] = VALIDITY_INVALID;
    c.validity[ITEM_IGNITION_ON] = VALIDITY_STALE;
    EXPECT_EQ(REASON_INPUT_INVALID, decide(c, locked()).reason);

    normalized_inputs_t d = all_valid();
    d.validity[ITEM_IGNITION_ON] = VALIDITY_STALE;
    d.value[ITEM_CRASH_STATUS] = CRASH_PENDING;
    EXPECT_EQ(REASON_INPUT_STALE, decide(d, locked()).reason);
}

/**
 * @test UT-028
 * @brief 충돌 CONFIRMED는 해당 입력이 VALID일 때만 적용되고, 다른 입력이 INVALID여도 적용된다
 *        (FN-015, SWR-007, SWR-024).
 * @par 입력값
 * 충돌 항목 값 CONFIRMED이고 검증 상태가 STALE / INVALID / VALID, 그리고 다른 항목 INVALID + 충돌
 * VALID
 * @par 절차
 * 각각 직전 출력 (LOCK, LOCK)으로 호출
 * @par 예상 출력값
 * 충돌이 STALE, INVALID이면 RELEASE로 바뀌지 않고(LOCK 유지) 이유는 INPUT_STALE, INPUT_INVALID.
 * 충돌이 VALID이면 다른 항목이 INVALID여도 RELEASE, state DEGRADED
 */
TEST(ArbiterRules, CrashRequiresValidInput)
{
    normalized_inputs_t stale = all_valid();
    stale.value[ITEM_CRASH_STATUS] = CRASH_CONFIRMED;
    stale.validity[ITEM_CRASH_STATUS] = VALIDITY_STALE;
    decision_t ds = decide(stale, locked());
    EXPECT_EQ(LOCK_LOCK, ds.outputs.left);
    EXPECT_EQ(REASON_INPUT_STALE, ds.reason);

    normalized_inputs_t invalid = all_valid();
    invalid.value[ITEM_CRASH_STATUS] = CRASH_CONFIRMED;
    invalid.validity[ITEM_CRASH_STATUS] = VALIDITY_INVALID;
    decision_t di = decide(invalid, locked());
    EXPECT_EQ(LOCK_LOCK, di.outputs.left);
    EXPECT_EQ(REASON_INPUT_INVALID, di.reason);

    normalized_inputs_t other = all_valid();
    other.value[ITEM_CRASH_STATUS] = CRASH_CONFIRMED;
    other.validity[ITEM_VEHICLE_SPEED] = VALIDITY_INVALID;
    decision_t dv = decide(other, locked());
    EXPECT_EQ(LOCK_RELEASE, dv.outputs.left);
    EXPECT_EQ(STATE_DEGRADED, dv.state);
}

/**
 * @test UT-029
 * @brief sensor_fault는 입력이 VALID이고 값이 1일 때만 FAULT 조건이다 (FN-015, FN-019, SWR-035).
 * @par 입력값
 * (값 1, INVALID), (값 1, STALE), (값 0, VALID), (값 1, VALID + 다른 항목 INVALID)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 앞의 셋은 warning NONE. 처음 둘은 state DEGRADED, 셋째는 NORMAL. 넷째는 state FAULT, warning
 * SENSOR_FAULT
 */
TEST(ArbiterState, FaultCondition)
{
    normalized_inputs_t a = all_valid();
    a.value[ITEM_SENSOR_FAULT] = 1;
    a.validity[ITEM_SENSOR_FAULT] = VALIDITY_INVALID;
    decision_t da = decide(a, locked());
    EXPECT_EQ(WARNING_NONE, da.warning);
    EXPECT_EQ(STATE_DEGRADED, da.state);

    normalized_inputs_t b = all_valid();
    b.value[ITEM_SENSOR_FAULT] = 1;
    b.validity[ITEM_SENSOR_FAULT] = VALIDITY_STALE;
    decision_t db = decide(b, locked());
    EXPECT_EQ(WARNING_NONE, db.warning);
    EXPECT_EQ(STATE_DEGRADED, db.state);

    decision_t dc = decide(all_valid(), locked());
    EXPECT_EQ(WARNING_NONE, dc.warning);
    EXPECT_EQ(STATE_NORMAL, dc.state);

    normalized_inputs_t d = all_valid();
    d.value[ITEM_SENSOR_FAULT] = 1;
    d.validity[ITEM_GEAR] = VALIDITY_INVALID;
    decision_t dd = decide(d, locked());
    EXPECT_EQ(WARNING_SENSOR_FAULT, dd.warning);
    EXPECT_EQ(STATE_FAULT, dd.state);
}

/**
 * @test UT-030
 * @brief state 결정표: DEGRADED 조건이 INVALID 또는 STALE 중 어느 쪽만 있어도 성립한다 (FN-019 R2,
 * R3).
 * @par 입력값
 * (STALE 항목 하나), (INVALID 항목 하나), (모두 VALID)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 앞의 둘은 DEGRADED, 마지막은 NORMAL
 */
TEST(ArbiterState, DegradedCondition)
{
    normalized_inputs_t stale = all_valid();
    stale.validity[ITEM_IGNITION_ON] = VALIDITY_STALE;
    EXPECT_EQ(STATE_DEGRADED, decide(stale, locked()).state);
    normalized_inputs_t invalid = all_valid();
    invalid.validity[ITEM_IGNITION_ON] = VALIDITY_INVALID;
    EXPECT_EQ(STATE_DEGRADED, decide(invalid, locked()).state);
    EXPECT_EQ(STATE_NORMAL, decide(all_valid(), locked()).state);
}
