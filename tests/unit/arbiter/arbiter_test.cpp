// arbiter 단위 테스트 (SWE.4). 근거: 상세 설계 FN-014~FN-019, FN-027, FN-029~FN-036의 결정표.
#include <gtest/gtest.h>

#include "arbiter/arbiter_if.h"

namespace
{

constexpr checked_command_t kNoCommand = {COMMAND_NONE, 0, 0};

/** 모든 항목이 VALID이고 값이 0이며 시동은 켜져 있는(ignition_on = 1) 입력. */
normalized_inputs_t all_valid()
{
    normalized_inputs_t inputs = {};
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        inputs.validity[i] = VALIDITY_VALID;
        inputs.value[i] = 0;
    }
    inputs.value[ITEM_IGNITION_ON] = 1;
    return inputs;
}

lock_outputs_t locked()
{
    return {LOCK_LOCK, LOCK_LOCK};
}

lock_outputs_t released()
{
    return {LOCK_RELEASE, LOCK_RELEASE};
}

arbiter_input_t make_input(const normalized_inputs_t *inputs,
                           lock_outputs_t previous = {LOCK_LOCK, LOCK_LOCK},
                           checked_command_t command = kNoCommand, uint32_t now_ms = 0,
                           approach_state_t approach = {})
{
    return {inputs, previous, command, now_ms, approach};
}

decision_t decide(const arbiter_input_t &input)
{
    decision_t out = {};
    EXPECT_EQ(STATUS_OK, arbiter_decide(&input, &out));
    return out;
}

decision_t decide(const normalized_inputs_t &inputs, lock_outputs_t previous)
{
    return decide(make_input(&inputs, previous));
}

constexpr checked_command_t command(int side, int action)
{
    return {COMMAND_ACCEPTED, side, action};
}

constexpr int kLeft = 0;
constexpr int kRight = 1;
constexpr int kAll = 2;
constexpr int kLock = 0;
constexpr int kUnlock = 1;

} // namespace

/**
 * @test UT-020
 * @brief arbiter_decide는 NULL 인자를 거절하고 출력을 바꾸지 않는다 (FN-014, R1~R3).
 * @par 입력값
 * (input = NULL), (out = NULL), (input.inputs = NULL)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM, out의 state는 호출 전 값(STATE_FAULT) 그대로
 */
TEST(ArbiterParams, NullArgumentsAreRejected)
{
    normalized_inputs_t inputs = all_valid();
    arbiter_input_t input = make_input(&inputs);
    arbiter_input_t no_inputs = make_input(nullptr);
    decision_t out = {};
    out.state = STATE_FAULT;
    EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(nullptr, &out));
    EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(&input, nullptr));
    EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(&no_inputs, &out));
    EXPECT_EQ(STATE_FAULT, out.state);
}

/**
 * @test UT-021
 * @brief previous의 출력 값이 열거 범위 밖이면 거절한다 (FN-014 R4, FN-027).
 * @par 입력값
 * previous (left, right) = 유효 4조합, 무효 (2, RELEASE), (RELEASE, 2), (LOCK, 2)
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
        arbiter_input_t input = make_input(&inputs, previous);
        EXPECT_EQ(STATUS_OK, arbiter_decide(&input, &out));
    }
    out.state = STATE_FAULT;
    const lock_outputs_t invalid[] = {{static_cast<lock_cmd_t>(2), LOCK_RELEASE},
                                      {LOCK_RELEASE, static_cast<lock_cmd_t>(2)},
                                      {LOCK_LOCK, static_cast<lock_cmd_t>(2)}};
    for (const lock_outputs_t &previous : invalid)
    {
        arbiter_input_t input = make_input(&inputs, previous);
        EXPECT_EQ(STATUS_ERR_PARAM, arbiter_decide(&input, &out));
        EXPECT_EQ(STATE_FAULT, out.state);
    }
}

/**
 * @test UT-022
 * @brief 규칙 1: 유효한 충돌 CONFIRMED는 두 출력을 RELEASE로 확정한다 (FN-017, FN-018).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), 충돌 항목 VALID 값 CONFIRMED(2)
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
 * @brief 규칙 2: 유효한 sensor_fault TRUE는 출력을 유지하고 FAULT와 경고를 낸다 (FN-017, FN-019).
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
 * @brief 규칙 3: INVALID 항목이 있으면 유지하고 DEGRADED와 INPUT_INVALID를 낸다 (FN-017, FN-016).
 * @par 입력값
 * 직전 출력 (LOCK, RELEASE), 차속 항목 INVALID
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
 * @brief 규칙 4: STALE 항목이 있으면 유지하고 DEGRADED와 INPUT_STALE을 낸다 (FN-017).
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
 * @brief 규칙 13·14: 충돌 PENDING은 출력을 바꾸지 않고 이유만 CRASH_PENDING으로 낸다 (FN-035).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), 충돌 VALID 값 PENDING(1) / NONE(0)
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
    c.validity[ITEM_ISOFIX_LEFT] = VALIDITY_STALE;
    EXPECT_EQ(REASON_INPUT_INVALID, decide(c, locked()).reason);

    normalized_inputs_t d = all_valid();
    d.validity[ITEM_ISOFIX_LEFT] = VALIDITY_STALE;
    d.value[ITEM_CRASH_STATUS] = CRASH_PENDING;
    EXPECT_EQ(REASON_INPUT_STALE, decide(d, locked()).reason);
}

/**
 * @test UT-028
 * @brief 충돌 CONFIRMED는 해당 입력이 VALID일 때만 적용되고, 다른 입력이 INVALID여도 적용된다
 *        (FN-015, SWR-007, SWR-024).
 * @par 입력값
 * 충돌 항목 값 CONFIRMED이고 입력 상태가 STALE / INVALID / VALID, 그리고 다른 항목 INVALID + 충돌
 * VALID
 * @par 절차
 * 각각 직전 출력 (LOCK, LOCK)으로 호출
 * @par 예상 출력값
 * 충돌이 STALE, INVALID이면 LOCK 유지, 이유 INPUT_STALE, INPUT_INVALID.
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
 * @brief state 결정표: DEGRADED 조건이 INVALID 또는 STALE 중 어느 쪽만 있어도 성립한다 (FN-019).
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
    stale.validity[ITEM_ISOFIX_LEFT] = VALIDITY_STALE;
    EXPECT_EQ(STATE_DEGRADED, decide(stale, locked()).state);
    normalized_inputs_t invalid = all_valid();
    invalid.validity[ITEM_ISOFIX_LEFT] = VALIDITY_INVALID;
    EXPECT_EQ(STATE_DEGRADED, decide(invalid, locked()).state);
    EXPECT_EQ(STATE_NORMAL, decide(all_valid(), locked()).state);
}

/**
 * @test UT-060
 * @brief 규칙 5: ignition_on이 VALID이고 FALSE이면 두 출력을 해제하고 IGNITION_OFF를 낸다
 *        (FN-017, FN-018).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), ignition_on VALID 값 0 / 값 1
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 값 0: (RELEASE, RELEASE), reason IGNITION_OFF, state OFF. 값 1: (LOCK, LOCK) 유지, NORMAL
 */
TEST(ArbiterGlobalRules, IgnitionOffReleases)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_IGNITION_ON] = 0;
    decision_t off = decide(inputs, locked());
    EXPECT_EQ(LOCK_RELEASE, off.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, off.outputs.right);
    EXPECT_EQ(REASON_IGNITION_OFF, off.reason);
    EXPECT_EQ(STATE_OFF, off.state);
    decision_t on = decide(all_valid(), locked());
    EXPECT_EQ(LOCK_LOCK, on.outputs.left);
    EXPECT_EQ(STATE_NORMAL, on.state);
}

/**
 * @test UT-061
 * @brief 규칙 6~8: 화재, 과온, 성인 탑승은 각각 두 출력을 해제하고 이유를 낸다. 동시이면 앞선 것이
 * 이유다 (FN-017, FN-018).
 * @par 입력값
 * 직전 출력 (LOCK, LOCK), 각 입력 단독 TRUE, (화재+과온+성인), (과온+성인)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 단독: 두 출력 RELEASE, 이유 FIRE, OVERTEMPERATURE, ADULT_PRESENT. 동시: FIRE, OVERTEMPERATURE
 */
TEST(ArbiterGlobalRules, ForcedReleaseInputs)
{
    const input_item_t items[] = {ITEM_FIRE_DETECTED, ITEM_OVERTEMPERATURE_DETECTED,
                                  ITEM_ADULT_PRESENT};
    const reason_t reasons[] = {REASON_FIRE, REASON_OVERTEMPERATURE, REASON_ADULT_PRESENT};
    for (int i = 0; i < 3; ++i)
    {
        normalized_inputs_t inputs = all_valid();
        inputs.value[items[i]] = 1;
        decision_t out = decide(inputs, locked());
        EXPECT_EQ(LOCK_RELEASE, out.outputs.left);
        EXPECT_EQ(LOCK_RELEASE, out.outputs.right);
        EXPECT_EQ(reasons[i], out.reason);
    }
    normalized_inputs_t all = all_valid();
    all.value[ITEM_FIRE_DETECTED] = 1;
    all.value[ITEM_OVERTEMPERATURE_DETECTED] = 1;
    all.value[ITEM_ADULT_PRESENT] = 1;
    EXPECT_EQ(REASON_FIRE, decide(all, locked()).reason);
    all.value[ITEM_FIRE_DETECTED] = 0;
    EXPECT_EQ(REASON_OVERTEMPERATURE, decide(all, locked()).reason);
}

/**
 * @test UT-062
 * @brief 전역 규칙의 우선순위: 충돌 > ... > STALE > ignition off > 화재 (FN-017).
 * @par 입력값
 * (ignition off + STALE), (충돌 CONFIRMED + ignition off), (ignition off + 화재),
 * (화재 + 좌 접근위험 TRUE)
 * @par 절차
 * 각각 직전 출력 (LOCK, LOCK)으로 arbiter_decide 호출
 * @par 예상 출력값
 * 이유 INPUT_STALE(출력 유지, state DEGRADED), CRASH_CONFIRMED(state OFF), IGNITION_OFF, FIRE(두
 * 출력 RELEASE)
 */
TEST(ArbiterGlobalRules, PriorityAmongGlobalRules)
{
    normalized_inputs_t a = all_valid();
    a.value[ITEM_IGNITION_ON] = 0;
    a.validity[ITEM_ISOFIX_LEFT] = VALIDITY_STALE;
    decision_t da = decide(a, locked());
    EXPECT_EQ(REASON_INPUT_STALE, da.reason);
    EXPECT_EQ(STATE_DEGRADED, da.state);
    EXPECT_EQ(LOCK_LOCK, da.outputs.left);

    normalized_inputs_t b = all_valid();
    b.value[ITEM_IGNITION_ON] = 0;
    b.value[ITEM_CRASH_STATUS] = CRASH_CONFIRMED;
    decision_t db = decide(b, locked());
    EXPECT_EQ(REASON_CRASH_CONFIRMED, db.reason);
    EXPECT_EQ(STATE_OFF, db.state);

    normalized_inputs_t c = all_valid();
    c.value[ITEM_IGNITION_ON] = 0;
    c.value[ITEM_FIRE_DETECTED] = 1;
    EXPECT_EQ(REASON_IGNITION_OFF, decide(c, locked()).reason);

    normalized_inputs_t d = all_valid();
    d.value[ITEM_FIRE_DETECTED] = 1;
    d.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    decision_t dd = decide(d, locked());
    EXPECT_EQ(REASON_FIRE, dd.reason);
    EXPECT_EQ(LOCK_RELEASE, dd.outputs.left);
}

/**
 * @test UT-063
 * @brief state 결정표: FAULT > DEGRADED > OFF > NORMAL (FN-019 R1~R4).
 * @par 입력값
 * (ignition off만), (ignition off + INVALID 항목), (ignition off + sensor_fault)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * OFF, DEGRADED, FAULT
 */
TEST(ArbiterState, OffIsBelowFaultAndDegraded)
{
    normalized_inputs_t off = all_valid();
    off.value[ITEM_IGNITION_ON] = 0;
    EXPECT_EQ(STATE_OFF, decide(off, locked()).state);
    normalized_inputs_t degraded = off;
    degraded.validity[ITEM_GEAR] = VALIDITY_INVALID;
    EXPECT_EQ(STATE_DEGRADED, decide(degraded, locked()).state);
    normalized_inputs_t fault = off;
    fault.value[ITEM_SENSOR_FAULT] = 1;
    EXPECT_EQ(STATE_FAULT, decide(fault, locked()).state);
}

/**
 * @test UT-064
 * @brief 규칙 9: 접근위험 TRUE인 문만 LOCK이 되고 이유가 APPROACH_RISK이다 (FN-030, SWR-009).
 * @par 입력값
 * 직전 출력 (RELEASE, RELEASE), (좌 접근위험), (우 접근위험), (좌, 우 모두)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * (LOCK, RELEASE), (RELEASE, LOCK), (LOCK, LOCK). 이유는 모두 APPROACH_RISK. 접근위험이 없으면 NONE
 */
TEST(ArbiterDoorRules, ApproachRiskLocksOnlyThatDoor)
{
    normalized_inputs_t left = all_valid();
    left.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    decision_t dl = decide(left, released());
    EXPECT_EQ(LOCK_LOCK, dl.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, dl.outputs.right);
    EXPECT_EQ(REASON_APPROACH_RISK, dl.reason);

    normalized_inputs_t right = all_valid();
    right.value[ITEM_REAR_RIGHT_APPROACH_RISK] = 1;
    decision_t dr = decide(right, released());
    EXPECT_EQ(LOCK_RELEASE, dr.outputs.left);
    EXPECT_EQ(LOCK_LOCK, dr.outputs.right);
    EXPECT_EQ(REASON_APPROACH_RISK, dr.reason);

    normalized_inputs_t both = left;
    both.value[ITEM_REAR_RIGHT_APPROACH_RISK] = 1;
    decision_t db = decide(both, released());
    EXPECT_EQ(LOCK_LOCK, db.outputs.left);
    EXPECT_EQ(LOCK_LOCK, db.outputs.right);

    EXPECT_EQ(REASON_NONE, decide(all_valid(), released()).reason);
}

/**
 * @test UT-065
 * @brief 규칙 10: ISOFIX TRUE인 문만 LOCK이 되고 이유가 ISOFIX이다 (FN-030, SWR-018).
 * @par 입력값
 * 직전 출력 (RELEASE, RELEASE), isofix_left만 TRUE / isofix_right만 TRUE
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * (LOCK, RELEASE) / (RELEASE, LOCK), 이유 ISOFIX
 */
TEST(ArbiterDoorRules, IsofixLocksOnlyThatDoor)
{
    normalized_inputs_t left = all_valid();
    left.value[ITEM_ISOFIX_LEFT] = 1;
    decision_t dl = decide(left, released());
    EXPECT_EQ(LOCK_LOCK, dl.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, dl.outputs.right);
    EXPECT_EQ(REASON_ISOFIX, dl.reason);
    normalized_inputs_t right = all_valid();
    right.value[ITEM_ISOFIX_RIGHT] = 1;
    decision_t dr = decide(right, released());
    EXPECT_EQ(LOCK_RELEASE, dr.outputs.left);
    EXPECT_EQ(LOCK_LOCK, dr.outputs.right);
    EXPECT_EQ(REASON_ISOFIX, dr.reason);
}

/**
 * @test UT-066
 * @brief 규칙 11: 차속 3.0 km/h(30) 이상이면 두 문이 LOCK이 된다. 29는 아니다 (FN-034 경계값).
 * @par 입력값
 * 직전 출력 (RELEASE, RELEASE), 차속 값 29, 30, 3000
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 29: RELEASE 유지, 이유 NONE. 30, 3000: (LOCK, LOCK), 이유 AUTO_LOCK
 */
TEST(ArbiterDoorRules, AutoLockBoundary)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_VEHICLE_SPEED] = 29;
    decision_t below = decide(inputs, released());
    EXPECT_EQ(LOCK_RELEASE, below.outputs.left);
    EXPECT_EQ(REASON_NONE, below.reason);
    inputs.value[ITEM_VEHICLE_SPEED] = 30;
    decision_t at = decide(inputs, released());
    EXPECT_EQ(LOCK_LOCK, at.outputs.left);
    EXPECT_EQ(LOCK_LOCK, at.outputs.right);
    EXPECT_EQ(REASON_AUTO_LOCK, at.reason);
    inputs.value[ITEM_VEHICLE_SPEED] = 3000;
    EXPECT_EQ(REASON_AUTO_LOCK, decide(inputs, released()).reason);
}

/**
 * @test UT-067
 * @brief 문 규칙의 우선순위: 접근위험 > ISOFIX > 자동 잠금 > 운전자 명령 (FN-030).
 * @par 입력값
 * 직전 출력 (RELEASE, RELEASE), (all, unlock) 명령. (좌 접근위험+ISOFIX+주행), (ISOFIX+주행),
 * (주행), (없음)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 좌 문의 이유가 순서대로 APPROACH_RISK, ISOFIX, AUTO_LOCK, DRIVER_COMMAND. 앞의 셋은 LOCK,
 * 마지막은 RELEASE
 */
TEST(ArbiterDoorRules, DoorRulePriority)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    inputs.value[ITEM_ISOFIX_LEFT] = 1;
    inputs.value[ITEM_VEHICLE_SPEED] = 100;
    const checked_command_t unlock_all = command(kAll, kUnlock);
    decision_t d1 = decide(make_input(&inputs, released(), unlock_all));
    EXPECT_EQ(LOCK_LOCK, d1.outputs.left);
    EXPECT_EQ(REASON_APPROACH_RISK, d1.reason);
    inputs.value[ITEM_REAR_LEFT_APPROACH_RISK] = 0;
    decision_t d2 = decide(make_input(&inputs, released(), unlock_all));
    EXPECT_EQ(LOCK_LOCK, d2.outputs.left);
    EXPECT_EQ(REASON_ISOFIX, d2.reason);
    inputs.value[ITEM_ISOFIX_LEFT] = 0;
    decision_t d3 = decide(make_input(&inputs, released(), unlock_all));
    EXPECT_EQ(LOCK_LOCK, d3.outputs.left);
    EXPECT_EQ(REASON_AUTO_LOCK, d3.reason);
    inputs.value[ITEM_VEHICLE_SPEED] = 0;
    decision_t d4 = decide(make_input(&inputs, locked(), unlock_all));
    EXPECT_EQ(LOCK_RELEASE, d4.outputs.left);
    EXPECT_EQ(REASON_DRIVER_COMMAND, d4.reason);
}

/**
 * @test UT-068
 * @brief 규칙 12: 운전자 명령은 선택한 문과 동작에만 적용되고, 거절·없는 명령은 무시한다
 *        (FN-030, FN-032, FN-033).
 * @par 입력값
 * 직전 출력 (LOCK, RELEASE), 명령 (left, lock), (left, unlock), (right, lock), (right, unlock),
 * (all, lock), (all, unlock), 상태 REJECTED인 (all, unlock), 상태 NONE
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * (LOCK, RELEASE), (RELEASE, RELEASE), (LOCK, LOCK), (LOCK, RELEASE), (LOCK, LOCK), (RELEASE,
 * RELEASE), 거절·없음은 (LOCK, RELEASE) 유지와 이유 NONE
 */
TEST(ArbiterDoorRules, DriverCommandMatrix)
{
    const normalized_inputs_t inputs = all_valid();
    const lock_outputs_t previous = {LOCK_LOCK, LOCK_RELEASE};
    struct Case
    {
        int side;
        int action;
        lock_cmd_t left;
        lock_cmd_t right;
    };
    const Case cases[] = {
        {kLeft, kLock, LOCK_LOCK, LOCK_RELEASE}, {kLeft, kUnlock, LOCK_RELEASE, LOCK_RELEASE},
        {kRight, kLock, LOCK_LOCK, LOCK_LOCK},   {kRight, kUnlock, LOCK_LOCK, LOCK_RELEASE},
        {kAll, kLock, LOCK_LOCK, LOCK_LOCK},     {kAll, kUnlock, LOCK_RELEASE, LOCK_RELEASE},
    };
    for (const Case &c : cases)
    {
        decision_t out = decide(make_input(&inputs, previous, command(c.side, c.action)));
        EXPECT_EQ(c.left, out.outputs.left);
        EXPECT_EQ(c.right, out.outputs.right);
        EXPECT_EQ(REASON_DRIVER_COMMAND, out.reason);
    }
    decision_t rejected = decide(make_input(&inputs, previous, {COMMAND_REJECTED, kAll, kUnlock}));
    EXPECT_EQ(LOCK_LOCK, rejected.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, rejected.outputs.right);
    EXPECT_EQ(REASON_NONE, rejected.reason);
    decision_t none = decide(make_input(&inputs, previous, {COMMAND_NONE, kAll, kUnlock}));
    EXPECT_EQ(REASON_NONE, none.reason);
}

/**
 * @test UT-069
 * @brief 접근위험 문에 대한 첫 해제 요청은 억제되고 기록이 남는다 (FN-031 R4, R5, SWR-005,
 * SWR-048).
 * @par 입력값
 * 직전 출력 (RELEASE, RELEASE), 좌 접근위험 TRUE, now 1000, (left, unlock) / (right, unlock) 명령
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * (left, unlock): 좌 LOCK(APPROACH_RISK), 기록 pending true, pending_ms 1000, override false, 우
 * 기록 불변. (right, unlock): 좌 LOCK, 우 RELEASE, 좌 기록은 pending false
 */
TEST(ArbiterApproach, FirstSuppressionIsRecorded)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    decision_t out = decide(make_input(&inputs, released(), command(kLeft, kUnlock), 1000));
    EXPECT_EQ(LOCK_LOCK, out.outputs.left);
    EXPECT_EQ(REASON_APPROACH_RISK, out.reason);
    EXPECT_TRUE(out.approach.door[DOOR_LEFT].pending);
    EXPECT_EQ(1000u, out.approach.door[DOOR_LEFT].pending_ms);
    EXPECT_FALSE(out.approach.door[DOOR_LEFT].override_active);
    EXPECT_FALSE(out.approach.door[DOOR_RIGHT].pending);

    decision_t other = decide(make_input(&inputs, released(), command(kRight, kUnlock), 1000));
    EXPECT_EQ(LOCK_LOCK, other.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, other.outputs.right);
    EXPECT_FALSE(other.approach.door[DOOR_LEFT].pending);
}

/**
 * @test UT-070
 * @brief 같은 문의 해제 요청이 첫 억제 후 10,000 ms 이하이면 override, 초과하면 새 첫 억제이다
 *        (FN-031 R6, R7, SWR-006, SWR-048).
 * @par 입력값
 * 좌 접근위험 TRUE, 기록 pending true pending_ms 1000, (left, unlock) 명령을 now 11000(경과 10000),
 * 11001(경과 10001)에 입력
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 11000: 좌 RELEASE, 이유 APPROACH_OVERRIDE, override true, pending false.
 * 11001: 좌 LOCK, 이유 APPROACH_RISK, pending true, pending_ms 11001, override false
 */
TEST(ArbiterApproach, OverrideWindowBoundary)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    approach_state_t approach = {};
    approach.door[DOOR_LEFT] = {true, false, 1000};
    decision_t in_window =
        decide(make_input(&inputs, released(), command(kLeft, kUnlock), 11000, approach));
    EXPECT_EQ(LOCK_RELEASE, in_window.outputs.left);
    EXPECT_EQ(REASON_APPROACH_OVERRIDE, in_window.reason);
    EXPECT_TRUE(in_window.approach.door[DOOR_LEFT].override_active);
    EXPECT_FALSE(in_window.approach.door[DOOR_LEFT].pending);

    decision_t expired =
        decide(make_input(&inputs, released(), command(kLeft, kUnlock), 11001, approach));
    EXPECT_EQ(LOCK_LOCK, expired.outputs.left);
    EXPECT_EQ(REASON_APPROACH_RISK, expired.reason);
    EXPECT_TRUE(expired.approach.door[DOOR_LEFT].pending);
    EXPECT_EQ(11001u, expired.approach.door[DOOR_LEFT].pending_ms);
    EXPECT_FALSE(expired.approach.door[DOOR_LEFT].override_active);
}

/**
 * @test UT-071
 * @brief override는 접근위험이 VALID로 FALSE가 될 때까지 유지되고 그때 기록이 지워진다
 *        (FN-031 R1, R3, SWR-006, 가정 A-19, A-21).
 * @par 입력값
 * 기록 override true + pending true, (접근위험 TRUE, 명령 없음), (접근위험 FALSE VALID),
 * (접근위험 항목이 FALSE가 아닌 INVALID)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * TRUE: 접근위험 규칙을 적용하지 않아 직전 출력 유지, override true 유지. FALSE: override false,
 * pending false. INVALID: 전역 규칙 INPUT_INVALID가 적용되고 기록은 그대로
 */
TEST(ArbiterApproach, OverridePersistsUntilRiskClears)
{
    approach_state_t approach = {};
    approach.door[DOOR_LEFT] = {true, true, 500};
    normalized_inputs_t risk = all_valid();
    risk.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    decision_t held = decide(make_input(&risk, released(), kNoCommand, 20000, approach));
    EXPECT_EQ(LOCK_RELEASE, held.outputs.left);
    EXPECT_EQ(REASON_NONE, held.reason);
    EXPECT_TRUE(held.approach.door[DOOR_LEFT].override_active);

    normalized_inputs_t cleared = all_valid();
    decision_t after = decide(make_input(&cleared, released(), kNoCommand, 20010, approach));
    EXPECT_FALSE(after.approach.door[DOOR_LEFT].override_active);
    EXPECT_FALSE(after.approach.door[DOOR_LEFT].pending);

    normalized_inputs_t invalid = all_valid();
    invalid.validity[ITEM_REAR_LEFT_APPROACH_RISK] = VALIDITY_INVALID;
    decision_t kept = decide(make_input(&invalid, released(), kNoCommand, 20020, approach));
    EXPECT_EQ(REASON_INPUT_INVALID, kept.reason);
    EXPECT_TRUE(kept.approach.door[DOOR_LEFT].override_active);
}

/**
 * @test UT-072
 * @brief override가 성립한 문은 접근위험 규칙 대신 하위 규칙을 따른다 (FN-030, SWR-006).
 * @par 입력값
 * 기록 override true(좌), 좌 접근위험 TRUE. (left, unlock) 명령 / ISOFIX(좌) TRUE
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 명령: 좌 RELEASE, 이유 DRIVER_COMMAND(이번 평가에 override가 새로 성립한 것이 아님). ISOFIX: 좌
 * LOCK, 이유 ISOFIX
 */
TEST(ArbiterApproach, OverriddenDoorFollowsLowerRules)
{
    approach_state_t approach = {};
    approach.door[DOOR_LEFT] = {false, true, 0};
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    decision_t cmd = decide(make_input(&inputs, locked(), command(kLeft, kUnlock), 100, approach));
    EXPECT_EQ(LOCK_RELEASE, cmd.outputs.left);
    EXPECT_EQ(REASON_DRIVER_COMMAND, cmd.reason);
    inputs.value[ITEM_ISOFIX_LEFT] = 1;
    decision_t iso = decide(make_input(&inputs, released(), kNoCommand, 100, approach));
    EXPECT_EQ(LOCK_LOCK, iso.outputs.left);
    EXPECT_EQ(REASON_ISOFIX, iso.reason);
}

/**
 * @test UT-073
 * @brief 두 문의 이유 중 순위가 높은 쪽이 결정 이유이고, 순위가 같으면 좌측이다 (FN-029).
 * @par 입력값
 * (좌 접근위험, 우 lock 명령), (우 접근위험, 좌 lock 명령), (좌, 우 모두 ISOFIX), (좌 override 성립
 * 명령, 우 lock 명령)
 * @par 절차
 * 각각 arbiter_decide 호출
 * @par 예상 출력값
 * 이유 APPROACH_RISK, APPROACH_RISK, ISOFIX, APPROACH_OVERRIDE
 */
TEST(ArbiterDoorRules, ReasonRankSelection)
{
    normalized_inputs_t left_risk = all_valid();
    left_risk.value[ITEM_REAR_LEFT_APPROACH_RISK] = 1;
    EXPECT_EQ(REASON_APPROACH_RISK,
              decide(make_input(&left_risk, released(), command(kRight, kLock))).reason);
    normalized_inputs_t right_risk = all_valid();
    right_risk.value[ITEM_REAR_RIGHT_APPROACH_RISK] = 1;
    EXPECT_EQ(REASON_APPROACH_RISK,
              decide(make_input(&right_risk, released(), command(kLeft, kLock))).reason);
    normalized_inputs_t both_iso = all_valid();
    both_iso.value[ITEM_ISOFIX_LEFT] = 1;
    both_iso.value[ITEM_ISOFIX_RIGHT] = 1;
    EXPECT_EQ(REASON_ISOFIX, decide(both_iso, released()).reason);

    approach_state_t approach = {};
    approach.door[DOOR_LEFT] = {true, false, 100};
    EXPECT_EQ(
        REASON_APPROACH_OVERRIDE,
        decide(make_input(&left_risk, released(), command(kAll, kUnlock), 200, approach)).reason);
}

/**
 * @test UT-074
 * @brief 문 규칙이 이유를 정했으면 충돌 PENDING 이유로 바꾸지 않는다 (FN-035 R2).
 * @par 입력값
 * 충돌 PENDING + isofix_left TRUE
 * @par 절차
 * arbiter_decide 호출
 * @par 예상 출력값
 * 이유 ISOFIX (CRASH_PENDING 아님)
 */
TEST(ArbiterDoorRules, PendingDoesNotOverrideDoorReason)
{
    normalized_inputs_t inputs = all_valid();
    inputs.value[ITEM_CRASH_STATUS] = CRASH_PENDING;
    inputs.value[ITEM_ISOFIX_LEFT] = 1;
    EXPECT_EQ(REASON_ISOFIX, decide(inputs, released()).reason);
}
