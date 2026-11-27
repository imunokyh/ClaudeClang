/**
 * @file arbiter_if.h
 * @brief 출력 결정 컴포넌트 인터페이스 (CMP-002, IF-002).
 * @trace SWR-001 SWR-002 SWR-003 SWR-005 SWR-006 SWR-007 SWR-008 SWR-009 SWR-017 SWR-018
 * @trace SWR-020 SWR-021 SWR-023 SWR-024 SWR-034 SWR-035 SWR-036 SWR-048
 */
#ifndef ARBITER_ARBITER_IF_H
#define ARBITER_ARBITER_IF_H

#include "common/types_if.h"

#ifdef __cplusplus
extern "C"
{
#endif

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
NODISCARD status_t arbiter_decide(const arbiter_input_t *input, decision_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ARBITER_ARBITER_IF_H */
