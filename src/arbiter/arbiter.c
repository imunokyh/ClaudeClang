/**
 * @file arbiter.c
 * @brief 출력 결정 컴포넌트 구현 (CMP-002, MOD-002). 상태가 없는 순수 판정이다.
 * @trace SWR-007 SWR-008 SWR-021 SWR-023 SWR-024 SWR-034 SWR-035 SWR-036
 */
#include "arbiter/arbiter_if.h"

#include <stddef.h>

/** 결정 규칙표에서 처음 성립한 규칙 (SWR-023). 규칙 5(새 명령)는 이후 이터레이션이 추가한다. */
typedef enum
{
    RULE_CRASH_CONFIRMED,
    RULE_SENSOR_FAULT,
    RULE_INPUT_INVALID,
    RULE_INPUT_STALE,
    RULE_CRASH_PENDING,
    RULE_NONE
} arbiter_rule_t;

/** sensor_fault 의 TRUE 값. */
#define SENSOR_FAULT_TRUE (1)

/**
 * @brief 두 출력 값이 모두 열거 범위 안인지 판정한다.
 * @trace FN-027
 * @param outputs 출력 값 (NULL 불가)
 * @return 모두 LOCK_RELEASE 또는 LOCK_LOCK 이면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool is_valid_lock_outputs(const lock_outputs_t *outputs)
{
    return ((outputs->left == LOCK_RELEASE) || (outputs->left == LOCK_LOCK)) &&
           ((outputs->right == LOCK_RELEASE) || (outputs->right == LOCK_LOCK));
}

/**
 * @brief 입력 항목이 VALID이고 판정 값이 기대값과 같은지 판정한다.
 * @trace FN-015
 * @param inputs 입력 (NULL 불가)
 * @param item 입력 항목 (0 이상 ITEM_COUNT 미만)
 * @param expected 기대하는 판정 값
 * @return VALID이고 값이 같으면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool has_item_value(const normalized_inputs_t *inputs, input_item_t item, int64_t expected)
{
    return (inputs->validity[item] == VALIDITY_VALID) && (inputs->value[item] == expected);
}

/**
 * @brief 입력 항목 중 해당 입력 상태인 것이 하나라도 있는지 판정한다.
 * @trace FN-016
 * @param inputs 입력 (NULL 불가)
 * @param validity 찾는 입력 상태
 * @return 하나라도 있으면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool has_any_validity(const normalized_inputs_t *inputs, validity_t validity)
{
    bool found = false;

    for (int index = 0; index < (int)ITEM_COUNT; index++)
    {
        if (inputs->validity[index] == validity)
        {
            found = true;
        }
    }

    return found;
}

/**
 * @brief 결정 규칙표에서 처음 성립하는 규칙을 고른다.
 * @trace FN-017
 * @param inputs 입력 (NULL 불가)
 * @return 처음 성립한 규칙
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static arbiter_rule_t select_rule(const normalized_inputs_t *inputs)
{
    arbiter_rule_t rule = RULE_NONE;

    if (has_item_value(inputs, ITEM_CRASH_STATUS, CRASH_CONFIRMED))
    {
        rule = RULE_CRASH_CONFIRMED;
    }
    else if (has_item_value(inputs, ITEM_SENSOR_FAULT, SENSOR_FAULT_TRUE))
    {
        rule = RULE_SENSOR_FAULT;
    }
    else if (has_any_validity(inputs, VALIDITY_INVALID))
    {
        rule = RULE_INPUT_INVALID;
    }
    else if (has_any_validity(inputs, VALIDITY_STALE))
    {
        rule = RULE_INPUT_STALE;
    }
    else if (has_item_value(inputs, ITEM_CRASH_STATUS, CRASH_PENDING))
    {
        rule = RULE_CRASH_PENDING;
    }
    else
    {
        rule = RULE_NONE;
    }

    return rule;
}

/**
 * @brief 규칙에 따라 출력과 결정 이유를 채운다. 충돌 해제만 출력을 바꾸고 나머지는 유지한다.
 * @trace FN-018
 * @param rule 적용할 규칙
 * @param previous 직전 확정 출력 (NULL 불가)
 * @param out 출력: outputs 와 reason 이 채워진다 (NULL 불가)
 * @return STATUS_OK
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static status_t apply_rule(arbiter_rule_t rule, const lock_outputs_t *previous, decision_t *out)
{
    out->outputs = *previous;
    switch (rule)
    {
    case RULE_CRASH_CONFIRMED:
        out->outputs.left = LOCK_RELEASE;
        out->outputs.right = LOCK_RELEASE;
        out->reason = REASON_CRASH_CONFIRMED;
        break;
    case RULE_SENSOR_FAULT:
        out->reason = REASON_SENSOR_FAULT;
        break;
    case RULE_INPUT_INVALID:
        out->reason = REASON_INPUT_INVALID;
        break;
    case RULE_INPUT_STALE:
        out->reason = REASON_INPUT_STALE;
        break;
    case RULE_CRASH_PENDING:
        out->reason = REASON_CRASH_PENDING;
        break;
    case RULE_NONE:
        out->reason = REASON_NONE;
        break;
    }

    return STATUS_OK;
}

/**
 * @brief FAULT, DEGRADED 조건으로 state를 정한다.
 * @trace FN-019
 * @param fault_condition FAULT 조건이 성립하는가
 * @param degraded_condition DEGRADED 조건이 성립하는가
 * @return STATE_FAULT, STATE_DEGRADED, STATE_NORMAL
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static ctrl_state_t decide_state(bool fault_condition, bool degraded_condition)
{
    ctrl_state_t state = STATE_NORMAL;

    if (fault_condition)
    {
        state = STATE_FAULT;
    }
    else if (degraded_condition)
    {
        state = STATE_DEGRADED;
    }
    else
    {
        state = STATE_NORMAL;
    }

    return state;
}

/**
 * @brief 결정 규칙표와 state 결정표로 출력, state, 결정 이유, 경고를 정한다.
 * @trace FN-014
 * @param inputs 입력 검사를 거친 입력 (NULL 불가)
 * @param previous 직전 확정 출력 (NULL 불가, 값은 LOCK_RELEASE 또는 LOCK_LOCK)
 * @param out 출력: 판정 결과 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (NULL 이 있거나 previous 의 값이 범위 밖)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
status_t arbiter_decide(const normalized_inputs_t *inputs, const lock_outputs_t *previous,
                        decision_t *out)
{
    status_t status = STATUS_ERR_PARAM;

    if ((inputs != NULL) && (previous != NULL) && (out != NULL) && is_valid_lock_outputs(previous))
    {
        status = apply_rule(select_rule(inputs), previous, out);
        const bool fault_condition = has_item_value(inputs, ITEM_SENSOR_FAULT, SENSOR_FAULT_TRUE);
        const bool degraded_condition =
            has_any_validity(inputs, VALIDITY_INVALID) || has_any_validity(inputs, VALIDITY_STALE);
        out->warning = fault_condition ? WARNING_SENSOR_FAULT : WARNING_NONE;
        out->state = decide_state(fault_condition, degraded_condition);
    }

    return status;
}
