/**
 * @file lock_ctrl.c
 * @brief 차일드락 제어 컴포넌트 구현 (CMP-003, MOD-003). 평가 순서를 지휘한다.
 * @trace SWR-025 SWR-026 SWR-027 SWR-031 SWR-039 SWR-040 SWR-044 SWR-048
 */
#include "lock_ctrl/lock_ctrl_if.h"

#include <stddef.h>

#include "arbiter/arbiter_if.h"
#include "input_guard/input_guard_if.h"

/**
 * @brief 초기 출력의 두 값이 모두 열거 범위 안인지 판정한다.
 * @trace FN-028
 * @param outputs 초기 출력 (NULL 불가)
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
static bool is_valid_initial_outputs(const lock_outputs_t *outputs)
{
    return ((outputs->left == LOCK_RELEASE) || (outputs->left == LOCK_LOCK)) &&
           ((outputs->right == LOCK_RELEASE) || (outputs->right == LOCK_LOCK));
}

/**
 * @brief 초기화 직후의 결과(SWR-025)를 만든다.
 * @trace FN-026
 * @param initial 초기 출력 (NULL 불가)
 * @param result 출력: 초기 결과 (NULL 불가)
 * @return STATUS_OK
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static status_t make_initial_result(const lock_outputs_t *initial, lock_ctrl_result_t *result)
{

    result->outputs = *initial;
    result->state = STATE_NORMAL;
    result->reason = REASON_NONE;
    result->priority_reason = REASON_NONE;
    result->warning = WARNING_NONE;
    result->command_status = COMMAND_NONE;
    result->override_active[DOOR_LEFT] = false;
    result->override_active[DOOR_RIGHT] = false;
    for (int index = 0; index < (int)ITEM_COUNT; index++)
    {
        result->input_validity[index] = VALIDITY_STALE;
    }

    return STATUS_OK;
}

/**
 * @brief now_ms가 직전에 받아들여진 평가보다 작은지 판정한다.
 * @trace FN-023
 * @param ctrl 제어기 (NULL 불가)
 * @param now_ms 평가 시각 (ms)
 * @return 시각이 거꾸로 갔으면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
static bool is_time_regressed(const lock_ctrl_t *ctrl, uint32_t now_ms)
{
    return ctrl->has_evaluated && (now_ms < ctrl->last_now_ms);
}

/**
 * @brief 결정과 입력 상태로 결과 구조체를 구성한다.
 * @trace FN-025
 * @param decision 판정 결과 (NULL 불가)
 * @param inputs 입력 검사를 거친 입력 (NULL 불가)
 * @param command 운전자 명령 처리 상태
 * @param result 출력: 결과 (NULL 불가)
 * @return STATUS_OK
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static status_t build_result(const decision_t *decision, const normalized_inputs_t *inputs,
                             command_status_t command, lock_ctrl_result_t *result)
{

    result->outputs = decision->outputs;
    result->state = decision->state;
    result->reason = decision->reason;
    result->priority_reason = decision->reason;
    result->warning = decision->warning;
    result->command_status = command;
    result->override_active[DOOR_LEFT] = decision->approach.door[DOOR_LEFT].override_active;
    result->override_active[DOOR_RIGHT] = decision->approach.door[DOOR_RIGHT].override_active;
    for (int index = 0; index < (int)ITEM_COUNT; index++)
    {
        result->input_validity[index] = inputs->validity[index];
    }

    return STATUS_OK;
}

/**
 * @brief 입력 검사, 명령 검사, 결정을 호출해 결과를 구성하고 상태를 갱신한다.
 * @trace FN-024
 * @param ctrl 입출력: 제어기 (NULL 불가)
 * @param input 평가 입력 (NULL 불가)
 * @param result 출력: 결과 (NULL 불가)
 * @return STATUS_OK 또는 하위 호출의 오류 코드. 오류이면 직전 확정 출력, 직전 now_ms, 결과를
 *         갱신하지 않는다
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static status_t run_evaluation(lock_ctrl_t *ctrl, const lock_ctrl_input_t *input,
                               lock_ctrl_result_t *result)
{
    normalized_inputs_t normalized = {{VALIDITY_VALID}, {0}};
    decision_t decision = {{LOCK_RELEASE, LOCK_RELEASE},
                           STATE_NORMAL,
                           REASON_NONE,
                           WARNING_NONE,
                           {{{false, false, 0}, {false, false, 0}}}};
    lock_ctrl_result_t built = ctrl->result;
    checked_command_t checked = {COMMAND_NONE, 0, 0};
    status_t status = input_guard_update(&ctrl->guard, input->now_ms, input->snapshot, &normalized);

    if ((status == STATUS_OK) && (input->command != NULL))
    {
        status = input_guard_check_command(input->command, &checked);
    }
    if (status == STATUS_OK)
    {
        const arbiter_input_t arbiter_input = {&normalized, ctrl->outputs, checked, input->now_ms,
                                               ctrl->approach};
        status = arbiter_decide(&arbiter_input, &decision);
    }
    if (status == STATUS_OK)
    {
        status = build_result(&decision, &normalized, checked.status, &built);
    }
    if (status == STATUS_OK)
    {
        ctrl->result = built;
        ctrl->outputs = decision.outputs;
        ctrl->approach = decision.approach;
        ctrl->last_now_ms = input->now_ms;
        ctrl->has_evaluated = true;
        *result = built;
    }

    return status;
}

/**
 * @brief 제어기를 초기 상태로 만들고 초기 출력을 정한다.
 * @trace FN-020
 * @param ctrl 출력: 제어기 (NULL 불가)
 * @param config 초기 설정 (NULL 이면 초기 출력 RELEASE)
 * @return STATUS_OK, STATUS_ERR_PARAM (ctrl 이 NULL 또는 초기 출력이 열거 범위 밖)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
status_t lock_ctrl_init(lock_ctrl_t *ctrl, const lock_ctrl_config_t *config)
{
    status_t status = STATUS_ERR_PARAM;
    lock_outputs_t initial = {LOCK_RELEASE, LOCK_RELEASE};

    if (config != NULL)
    {
        initial = config->initial_outputs;
    }
    if ((ctrl != NULL) && is_valid_initial_outputs(&initial))
    {
        status = input_guard_init(&ctrl->guard);
    }
    if (status == STATUS_OK)
    {
        ctrl->outputs = initial;
        ctrl->approach = (approach_state_t){{{false, false, 0}, {false, false, 0}}};
        ctrl->last_now_ms = 0;
        ctrl->has_evaluated = false;
        status = make_initial_result(&initial, &ctrl->result);
    }

    return status;
}

/**
 * @brief 평가 한 번을 정해진 순서로 수행하고 결과를 낸다. 10 ms 주기로 호출한다.
 * @trace FN-021
 * @param ctrl 입출력: 제어기 (NULL 불가)
 * @param input 평가 입력 (NULL 불가)
 * @param result 출력: 결과 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (NULL), STATUS_ERR_TIME (now_ms 가 직전에 받아들여진
 *         평가보다 작음. 이때 상태는 바뀌지 않는다)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t lock_ctrl_evaluate(lock_ctrl_t *ctrl, const lock_ctrl_input_t *input,
                            lock_ctrl_result_t *result)
{
    status_t status = STATUS_ERR_PARAM;

    if ((ctrl != NULL) && (input != NULL) && (result != NULL))
    {
        if (is_time_regressed(ctrl, input->now_ms))
        {
            status = STATUS_ERR_TIME;
        }
        else
        {
            status = run_evaluation(ctrl, input, result);
        }
    }

    return status;
}

/**
 * @brief 마지막 평가 결과(또는 초기화 직후의 초기 결과)를 낸다.
 * @trace FN-022
 * @param ctrl 제어기 (NULL 불가)
 * @param result 출력: 결과 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (ctrl 또는 result 가 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t lock_ctrl_get_result(const lock_ctrl_t *ctrl, lock_ctrl_result_t *result)
{
    status_t status = STATUS_ERR_PARAM;

    if ((ctrl != NULL) && (result != NULL))
    {
        *result = ctrl->result;
        status = STATUS_OK;
    }

    return status;
}
