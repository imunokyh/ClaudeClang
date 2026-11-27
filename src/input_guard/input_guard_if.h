/**
 * @file input_guard_if.h
 * @brief 입력 검사 컴포넌트 인터페이스 (CMP-001, IF-001).
 * @trace SWR-013 SWR-022 SWR-033 SWR-046
 */
#ifndef INPUT_GUARD_INPUT_GUARD_IF_H
#define INPUT_GUARD_INPUT_GUARD_IF_H

#include <stdbool.h>
#include <stdint.h>

#include "common/types_if.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * 입력 상태 저장소. 호출자가 메모리를 제공하고 필드는 input_guard 만 읽고 쓴다.
 */
typedef struct
{
    bool invalid[ITEM_COUNT];  /**< 항목별 저장 상태 (true 이면 INVALID) */
    int64_t held[ITEM_COUNT];  /**< 항목별 보관 값 */
    bool has_snapshot;         /**< 받아들인 스냅샷이 있는가 */
    uint32_t last_receive_ms;  /**< 마지막 수신 시각 (ms) */
    uint32_t last_snapshot_ms; /**< 직전에 받아들인 스냅샷 시각 (ms) */
} input_guard_t;

/**
 * @brief 입력 상태 저장소를 초기 상태로 만든다.
 * @trace FN-001
 * @param guard 출력: 저장소 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (guard 가 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t input_guard_init(input_guard_t *guard);

/**
 * @brief 스냅샷(있으면)을 검사해 저장 상태를 갱신하고 입력 항목별 입력 상태를 만든다.
 * @trace FN-002
 * @param guard 입출력: 저장소 (NULL 불가)
 * @param now_ms 평가 시각 (ms, 직전 평가 이상이어야 함. 호출자가 보장)
 * @param snapshot 새 스냅샷 (없으면 NULL)
 * @param out 출력: 입력 항목별 입력 상태와 판정 값 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (guard 또는 out 이 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t input_guard_update(input_guard_t *guard, uint32_t now_ms,
                                      const raw_snapshot_t *snapshot, normalized_inputs_t *out);

/**
 * @brief 운전자 명령의 유효성을 검사해 명령 상태를 정한다.
 * @trace FN-003
 * @param command 운전자 명령 (NULL 불가. 명령이 없으면 호출하지 않는다)
 * @param out 출력: COMMAND_ACCEPTED 또는 COMMAND_REJECTED (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (command 또는 out 이 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t input_guard_check_command(const raw_command_t *command, command_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_GUARD_INPUT_GUARD_IF_H */
