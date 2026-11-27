/**
 * @file arbiter.c
 * @brief 출력 결정 컴포넌트 구현 (CMP-002, MOD-002). 상태가 없는 순수 판정이다.
 * @trace SWR-001 SWR-002 SWR-003 SWR-005 SWR-006 SWR-007 SWR-008 SWR-009 SWR-017 SWR-018 SWR-020
 * @trace SWR-021 SWR-023 SWR-024 SWR-034 SWR-035 SWR-036 SWR-048
 */
#include "arbiter/arbiter_if.h"

#include <stddef.h>

/** 결정 규칙표의 전역 규칙 (SWR-023 규칙 1~8). RULE_NONE 이면 문 규칙(9~12)으로 넘어간다. */
typedef enum
{
    RULE_CRASH_CONFIRMED,
    RULE_SENSOR_FAULT,
    RULE_INPUT_INVALID,
    RULE_INPUT_STALE,
    RULE_IGNITION_OFF,
    RULE_FIRE,
    RULE_OVERTEMPERATURE,
    RULE_ADULT_PRESENT,
    RULE_NONE
} arbiter_rule_t;

/** 문 하나의 판정 결과. rank 가 작을수록 결정 이유의 우선순위가 높다. */
typedef struct
{
    lock_cmd_t output; /**< 그 문의 출력 */
    reason_t reason;   /**< 그 문의 결정 이유 */
    int rank; /**< 결정 이유의 순위 (0 접근위험, 1 ISOFIX, 2 자동 잠금, 3 override, 4 명령, 5 없음)
               */
} door_result_t;

/** 접근위험 기록을 한 단계 진행한 결과. */
typedef struct
{
    door_approach_t next; /**< 새 기록 */
    bool locked;          /**< 이번 평가에서 접근위험 규칙이 문을 잠그는가 */
    bool overridden;      /**< 이번 평가에서 override 가 성립했는가 */
} approach_step_t;

/** boolean 항목의 TRUE 값. */
#define BOOL_TRUE (1)
/** boolean 항목의 FALSE 값. */
#define BOOL_FALSE (0)
/** 자동 잠금 차속 기준 (0.1 km/h 단위 30 = 3.0 km/h, SWR-003). */
#define AUTO_LOCK_THRESHOLD (30)
/** 접근위험 해제 요청 억제 창 (ms, SWR-048). */
#define APPROACH_WINDOW_MS (10000U)
/** 명령의 action 값: 잠금. */
#define ACTION_LOCK (0)
/** 명령의 action 값: 해제. */
#define ACTION_UNLOCK (1)
/** 명령의 side 값: 두 문 모두. */
#define SIDE_ALL (2)
/** 결정 이유 순위: 접근위험. */
#define RANK_APPROACH (0)
/** 결정 이유 순위: ISOFIX. */
#define RANK_ISOFIX (1)
/** 결정 이유 순위: 자동 잠금. */
#define RANK_AUTO_LOCK (2)
/** 결정 이유 순위: override. */
#define RANK_OVERRIDE (3)
/** 결정 이유 순위: 운전자 명령. */
#define RANK_COMMAND (4)
/** 결정 이유 순위: 문 규칙 없음. */
#define RANK_NONE (5)

/** 문별 접근위험 입력 항목. */
static const input_item_t RISK_ITEMS[DOOR_COUNT] = {ITEM_REAR_LEFT_APPROACH_RISK,
                                                    ITEM_REAR_RIGHT_APPROACH_RISK};
/** 문별 ISOFIX 입력 항목. */
static const input_item_t ISOFIX_ITEMS[DOOR_COUNT] = {ITEM_ISOFIX_LEFT, ITEM_ISOFIX_RIGHT};

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
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
 * @brief 차속 판정 값이 기준 이상인지 판정한다. 호출 전에 전역 규칙 3, 4가 모든 입력의 VALID를
 * 보장한다.
 * @trace FN-034
 * @param inputs 입력 (NULL 불가)
 * @param threshold 기준 (0.1 km/h 단위)
 * @return 차속이 VALID이고 판정 값이 기준 이상이면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool is_speed_at_least(const normalized_inputs_t *inputs, int64_t threshold)
{
    return inputs->value[ITEM_VEHICLE_SPEED] >= threshold;
}

/**
 * @brief 명령의 side가 이 문을 가리키는지 판정한다.
 * @trace FN-033
 * @param side 명령의 side (0 left, 1 right, 2 all)
 * @param door 문
 * @return side가 all이거나 이 문이면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool targets_door(int side, door_t door)
{
    return (side == SIDE_ALL) || (side == (int)door);
}

/**
 * @brief 유효한 명령이 이 문을 대상으로 하는 해당 동작인지 판정한다.
 * @trace FN-032
 * @param command 검사한 명령 (NULL 불가)
 * @param door 문
 * @param action 동작 (0 lock, 1 unlock)
 * @return 해당하면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool command_matches(const checked_command_t *command, door_t door, int action)
{
    return (command->status == COMMAND_ACCEPTED) && (command->action == action) &&
           targets_door(command->side, door);
}

/**
 * @brief 문의 출력 값을 꺼낸다.
 * @trace FN-036
 * @param outputs 출력 (NULL 불가)
 * @param door 문
 * @return 그 문의 출력 값
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static lock_cmd_t get_door_output(const lock_outputs_t *outputs, door_t door)
{
    return (door == DOOR_LEFT) ? outputs->left : outputs->right;
}

/**
 * @brief 접근위험 입력과 해제 요청으로 문의 억제·override 기록을 한 단계 진행한다.
 * @trace FN-031
 * @param current 현재 기록
 * @param risk 접근위험 입력이 TRUE인가(전역 규칙 3, 4를 통과했으므로 입력은 VALID이고 FALSE는
 * !risk)
 * @param release 이 문에 대한 해제 요청이 있는가
 * @param now_ms 평가 시각 (ms)
 * @return 새 기록, 잠금 여부, override 성립 여부
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static approach_step_t step_approach(door_approach_t current, bool risk, bool release,
                                     uint32_t now_ms)
{
    approach_step_t step = {current, false, false};

    if (!risk)
    {
        step.next.pending = false;
        step.next.override_active = false;
    }
    else if (!current.override_active)
    {
        step.locked = true;
        if (release)
        {
            if (current.pending && ((now_ms - current.pending_ms) <= APPROACH_WINDOW_MS))
            {
                step.next.override_active = true;
                step.next.pending = false;
                step.overridden = true;
                step.locked = false;
            }
            else
            {
                step.next.pending = true;
                step.next.pending_ms = now_ms;
            }
        }
    }

    return step;
}

/**
 * @brief 문 하나에 문 규칙 9~12를 적용하고 그 문의 접근위험 기록을 갱신한다.
 * @trace FN-030
 * @param input arbiter 입력 (NULL 불가)
 * @param door 문
 * @param approach 입출력: 그 문의 접근위험 기록 (NULL 불가)
 * @return 그 문의 출력, 결정 이유, 순위
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static door_result_t decide_door(const arbiter_input_t *input, door_t door,
                                 door_approach_t *approach)
{
    const normalized_inputs_t *inputs = input->inputs;
    const bool risk = has_item_value(inputs, RISK_ITEMS[door], BOOL_TRUE);
    const bool release = command_matches(&input->command, door, ACTION_UNLOCK);
    const bool lock = command_matches(&input->command, door, ACTION_LOCK);
    const approach_step_t step = step_approach(*approach, risk, release, input->now_ms);
    door_result_t result = {get_door_output(&input->previous, door), REASON_NONE, RANK_NONE};

    *approach = step.next;
    if (step.locked)
    {
        result = (door_result_t){LOCK_LOCK, REASON_APPROACH_RISK, RANK_APPROACH};
    }
    else if (has_item_value(inputs, ISOFIX_ITEMS[door], BOOL_TRUE))
    {
        result = (door_result_t){LOCK_LOCK, REASON_ISOFIX, RANK_ISOFIX};
    }
    else if (is_speed_at_least(inputs, AUTO_LOCK_THRESHOLD))
    {
        result = (door_result_t){LOCK_LOCK, REASON_AUTO_LOCK, RANK_AUTO_LOCK};
    }
    else if (release)
    {
        result = step.overridden
                     ? (door_result_t){LOCK_RELEASE, REASON_APPROACH_OVERRIDE, RANK_OVERRIDE}
                     : (door_result_t){LOCK_RELEASE, REASON_DRIVER_COMMAND, RANK_COMMAND};
    }
    else if (lock)
    {
        result = (door_result_t){LOCK_LOCK, REASON_DRIVER_COMMAND, RANK_COMMAND};
    }

    return result;
}

/**
 * @brief 문(좌, 우)마다 문 규칙을 적용해 출력, 결정 이유, 접근위험 기록을 정한다.
 * @trace FN-029
 * @param input arbiter 입력 (NULL 불가)
 * @param out 입출력: out->approach 는 현재 기록으로 채워져 있어야 한다 (NULL 불가)
 * @return STATUS_OK
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static status_t decide_doors(const arbiter_input_t *input, decision_t *out)
{
    const door_result_t left = decide_door(input, DOOR_LEFT, &out->approach.door[DOOR_LEFT]);
    const door_result_t right = decide_door(input, DOOR_RIGHT, &out->approach.door[DOOR_RIGHT]);

    out->outputs.left = left.output;
    out->outputs.right = right.output;
    out->reason = (left.rank <= right.rank) ? left.reason : right.reason;

    return STATUS_OK;
}

/**
 * @brief 전역 규칙 1~8에서 처음 성립하는 규칙을 고른다.
 * @trace FN-017
 * @param inputs 입력 (NULL 불가)
 * @return 처음 성립한 규칙, 없으면 RULE_NONE
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static arbiter_rule_t select_global_rule(const normalized_inputs_t *inputs)
{
    arbiter_rule_t rule = RULE_NONE;

    if (has_item_value(inputs, ITEM_CRASH_STATUS, CRASH_CONFIRMED))
    {
        rule = RULE_CRASH_CONFIRMED;
    }
    else if (has_item_value(inputs, ITEM_SENSOR_FAULT, BOOL_TRUE))
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
    else if (has_item_value(inputs, ITEM_IGNITION_ON, BOOL_FALSE))
    {
        rule = RULE_IGNITION_OFF;
    }
    else if (has_item_value(inputs, ITEM_FIRE_DETECTED, BOOL_TRUE))
    {
        rule = RULE_FIRE;
    }
    else if (has_item_value(inputs, ITEM_OVERTEMPERATURE_DETECTED, BOOL_TRUE))
    {
        rule = RULE_OVERTEMPERATURE;
    }
    else if (has_item_value(inputs, ITEM_ADULT_PRESENT, BOOL_TRUE))
    {
        rule = RULE_ADULT_PRESENT;
    }

    return rule;
}

/**
 * @brief 전역 규칙에 따라 출력과 결정 이유를 채운다. 규칙이 없으면 직전 출력 유지와 이유 NONE이다.
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
static status_t apply_global_rule(arbiter_rule_t rule, const lock_outputs_t *previous,
                                  decision_t *out)
{
    out->outputs = *previous;
    out->reason = REASON_NONE;
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
    case RULE_IGNITION_OFF:
        out->outputs.left = LOCK_RELEASE;
        out->outputs.right = LOCK_RELEASE;
        out->reason = REASON_IGNITION_OFF;
        break;
    case RULE_FIRE:
        out->outputs.left = LOCK_RELEASE;
        out->outputs.right = LOCK_RELEASE;
        out->reason = REASON_FIRE;
        break;
    case RULE_OVERTEMPERATURE:
        out->outputs.left = LOCK_RELEASE;
        out->outputs.right = LOCK_RELEASE;
        out->reason = REASON_OVERTEMPERATURE;
        break;
    case RULE_ADULT_PRESENT:
        out->outputs.left = LOCK_RELEASE;
        out->outputs.right = LOCK_RELEASE;
        out->reason = REASON_ADULT_PRESENT;
        break;
    case RULE_NONE:
        break;
    }

    return STATUS_OK;
}

/**
 * @brief 문 규칙이 없어 이유가 NONE이면 충돌 PENDING 이유를 적용한다.
 * @trace FN-035
 * @param inputs 입력 (NULL 불가)
 * @param reason 지금까지 정한 결정 이유
 * @return 최종 결정 이유
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static reason_t apply_fallback_reason(const normalized_inputs_t *inputs, reason_t reason)
{
    const bool pending = has_item_value(inputs, ITEM_CRASH_STATUS, CRASH_PENDING);

    return ((reason == REASON_NONE) && pending) ? REASON_CRASH_PENDING : reason;
}

/**
 * @brief FAULT, DEGRADED, OFF 조건으로 state를 정한다.
 * @trace FN-019
 * @param fault_condition FAULT 조건이 성립하는가
 * @param degraded_condition DEGRADED 조건이 성립하는가
 * @param off_condition ignition off 조건이 성립하는가
 * @return STATE_FAULT, STATE_DEGRADED, STATE_OFF, STATE_NORMAL
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static ctrl_state_t decide_state(bool fault_condition, bool degraded_condition, bool off_condition)
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
    else if (off_condition)
    {
        state = STATE_OFF;
    }

    return state;
}

/**
 * @brief 규칙표와 state 결정표로 출력, state, 결정 이유, 경고, 새 접근위험 기록을 정한다.
 * @trace FN-014
 * @param input arbiter 입력 (NULL 불가, inputs 는 NULL 불가, previous 의 값은 LOCK_RELEASE 또는
 * LOCK_LOCK)
 * @param out 출력: 판정 결과 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (NULL 이 있거나 previous 의 값이 범위 밖)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
status_t arbiter_decide(const arbiter_input_t *input, decision_t *out)
{
    status_t status = STATUS_ERR_PARAM;

    if ((input != NULL) && (out != NULL) && (input->inputs != NULL) &&
        is_valid_lock_outputs(&input->previous))
    {
        const normalized_inputs_t *inputs = input->inputs;
        const arbiter_rule_t rule = select_global_rule(inputs);
        const bool fault_condition = has_item_value(inputs, ITEM_SENSOR_FAULT, BOOL_TRUE);
        const bool degraded_condition =
            has_any_validity(inputs, VALIDITY_INVALID) || has_any_validity(inputs, VALIDITY_STALE);
        const bool off_condition = has_item_value(inputs, ITEM_IGNITION_ON, BOOL_FALSE);

        out->approach = input->approach;
        status = apply_global_rule(rule, &input->previous, out);
        if (rule == RULE_NONE)
        {
            status = decide_doors(input, out);
        }
        out->reason = apply_fallback_reason(inputs, out->reason);
        out->warning = fault_condition ? WARNING_SENSOR_FAULT : WARNING_NONE;
        out->state = decide_state(fault_condition, degraded_condition, off_condition);
    }

    return status;
}
