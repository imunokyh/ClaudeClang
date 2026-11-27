/**
 * @file status_view_if.h
 * @brief 상태 표시용 직렬화 컴포넌트 인터페이스 (CMP-005, IF-005).
 * @trace SWR-014 SWR-015
 */
#ifndef STATUS_VIEW_STATUS_VIEW_IF_H
#define STATUS_VIEW_STATUS_VIEW_IF_H

#include <stddef.h>

#include "common/types_if.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 조회 결과를 널 종료 JSON 문자열로 직렬화한다. 형식은 요구사항 5.6절이다.
 * @trace FN-037
 * @param result 조회 결과 (NULL 불가)
 * @param buffer 출력: 문자열을 쓸 버퍼 (NULL 불가)
 * @param size 버퍼 크기 (널 문자를 포함, 1 이상)
 * @return STATUS_OK, STATUS_ERR_PARAM (NULL 이거나 size 가 0), STATUS_ERR_BUFFER (버퍼 부족, 이때
 *         buffer[0] 은 널 문자)
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t status_view_format(const lock_ctrl_result_t *result, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_VIEW_STATUS_VIEW_IF_H */
