/**
 * @file decision_log_if.h
 * @brief 제어 결정 이력 컴포넌트 인터페이스 (CMP-006, IF-006).
 * @trace SWR-010 SWR-011 SWR-012
 */
#ifndef DECISION_LOG_DECISION_LOG_IF_H
#define DECISION_LOG_DECISION_LOG_IF_H

#include <stdint.h>

#include "common/types_if.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** 이력에 보존하는 최대 건수 (SWR-010). */
#define DECISION_LOG_CAPACITY (100U)

/**
 * 이력 저장소(링 버퍼). 호출자가 메모리를 제공하고 필드는 decision_log 만 읽고 쓴다.
 * 메모리에만 있으며 파일 등에 저장하지 않는다(SWR-012).
 */
typedef struct
{
    decision_entry_t entries[DECISION_LOG_CAPACITY]; /**< 항목 */
    uint32_t count;                                  /**< 보존 중인 건수 (0~100) */
    uint32_t head;                                   /**< 다음에 쓸 위치 */
    uint32_t next_id;                                /**< 다음 이벤트 ID (1부터) */
} decision_log_t;

/**
 * @brief 이력을 비우고 이벤트 ID를 1로 되돌린다.
 * @trace FN-042
 * @param log 출력: 저장소 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (log 가 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t decision_log_init(decision_log_t *log);

/**
 * @brief 이력에 1건을 추가하고 이벤트 ID를 부여한다. 100건을 넘으면 가장 오래된 것을 덮어쓴다.
 * @trace FN-043
 * @param log 입출력: 저장소 (NULL 불가)
 * @param entry 추가할 항목 (NULL 불가, event_id 는 무시하고 이력이 부여한다)
 * @return STATUS_OK, STATUS_ERR_PARAM (log 또는 entry 가 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t decision_log_append(decision_log_t *log, const decision_entry_t *entry);

/**
 * @brief 보존 중인 이력 건수를 낸다.
 * @trace FN-044
 * @param log 저장소 (NULL 불가)
 * @param count 출력: 건수 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (log 또는 count 가 NULL)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t decision_log_count(const decision_log_t *log, uint32_t *count);

/**
 * @brief 오래된 것부터 센 index번째 항목을 낸다.
 * @trace FN-045
 * @param log 저장소 (NULL 불가)
 * @param index 0이 가장 오래된 항목 (건수 미만)
 * @param out 출력: 항목 (NULL 불가)
 * @return STATUS_OK, STATUS_ERR_PARAM (NULL 이거나 index 가 건수 이상, 이때 out 은 바뀌지 않는다)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t decision_log_get(const decision_log_t *log, uint32_t index,
                                    decision_entry_t *out);

#ifdef __cplusplus
}
#endif

#endif /* DECISION_LOG_DECISION_LOG_IF_H */
