/**
 * @file lock_ctrl_if.h
 * @brief 차일드락 제어 컴포넌트 인터페이스 (CMP-003, IF-003). 호출자가 쓰는 공개 API.
 * @trace SWR-025 SWR-026 SWR-027 SWR-031 SWR-039 SWR-040 SWR-044
 */
#ifndef LOCK_CTRL_LOCK_CTRL_IF_H
#define LOCK_CTRL_LOCK_CTRL_IF_H

#include <stdbool.h>
#include <stdint.h>

#include "common/types_if.h"
#include "input_guard/input_guard_if.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * 제어기 상태. 호출자가 메모리를 제공하고 필드는 lock_ctrl 만 읽고 쓴다.
 */
typedef struct
{
    input_guard_t guard;       /**< 입력 상태 저장소 */
    lock_outputs_t outputs;    /**< 직전 확정 출력 */
    approach_state_t approach; /**< 문별 접근위험 억제·override 기록 */
    uint32_t last_now_ms;      /**< 직전에 받아들여진 평가의 now_ms */
    bool has_evaluated;        /**< 받아들여진 평가가 있는가 */
    lock_ctrl_result_t result; /**< 마지막 결과 (초기화 직후에는 초기 결과) */
} lock_ctrl_t;

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
NODISCARD status_t lock_ctrl_init(lock_ctrl_t *ctrl, const lock_ctrl_config_t *config);

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
NODISCARD status_t lock_ctrl_evaluate(lock_ctrl_t *ctrl, const lock_ctrl_input_t *input,
                                      lock_ctrl_result_t *result);

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
NODISCARD status_t lock_ctrl_get_result(const lock_ctrl_t *ctrl, lock_ctrl_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* LOCK_CTRL_LOCK_CTRL_IF_H */
