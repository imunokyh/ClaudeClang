/**
 * @file decision_log.c
 * @brief 제어 결정 이력 컴포넌트 구현 (CMP-006, MOD-006). 메모리 링 버퍼만 사용한다.
 * @trace SWR-010 SWR-011 SWR-012
 */
#include "decision_log/decision_log_if.h"

#include <stddef.h>

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t decision_log_init(decision_log_t *log)
{
    status_t status = STATUS_ERR_PARAM;
    const decision_entry_t empty = {
        0, 0, {LOCK_RELEASE, LOCK_RELEASE}, STATE_NORMAL, REASON_NONE, WARNING_NONE};

    if (log != NULL)
    {
        for (uint32_t index = 0; index < DECISION_LOG_CAPACITY; index++)
        {
            log->entries[index] = empty;
        }
        log->count = 0;
        log->head = 0;
        log->next_id = 1;
        status = STATUS_OK;
    }

    return status;
}

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t decision_log_append(decision_log_t *log, const decision_entry_t *entry)
{
    status_t status = STATUS_ERR_PARAM;

    if ((log != NULL) && (entry != NULL))
    {
        log->entries[log->head] = *entry;
        log->entries[log->head].event_id = log->next_id;
        log->head = (log->head + 1U) % DECISION_LOG_CAPACITY;
        log->next_id++;
        if (log->count < DECISION_LOG_CAPACITY)
        {
            log->count++;
        }
        status = STATUS_OK;
    }

    return status;
}

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t decision_log_count(const decision_log_t *log, uint32_t *count)
{
    status_t status = STATUS_ERR_PARAM;

    if ((log != NULL) && (count != NULL))
    {
        *count = log->count;
        status = STATUS_OK;
    }

    return status;
}

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t decision_log_get(const decision_log_t *log, uint32_t index, decision_entry_t *out)
{
    status_t status = STATUS_ERR_PARAM;

    if ((log != NULL) && (out != NULL) && (index < log->count))
    {
        const uint32_t start =
            (log->head + DECISION_LOG_CAPACITY - log->count) % DECISION_LOG_CAPACITY;

        *out = log->entries[(start + index) % DECISION_LOG_CAPACITY];
        status = STATUS_OK;
    }

    return status;
}
