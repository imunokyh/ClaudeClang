/**
 * @file status_if.h
 * @brief 모든 함수가 공통으로 반환하는 상태 코드와 반환값 무시 방지 매크로.
 *
 * 이 파일은 양식이다. 프로젝트에 `src/common/status_if.h`로 복사해서 쓰고,
 * 오류 코드는 상세 설계서(docs/swe3/)의 함수 설계에 정의된 것만 추가한다.
 */
#ifndef COMMON_STATUS_IF_H
#define COMMON_STATUS_IF_H

#include <stdint.h>

/** 반환값을 버리는 호출을 컴파일 오류로 만든다 (-Wall -Werror 와 함께). */
#if defined(__GNUC__) || defined(__clang__)
#define NODISCARD __attribute__((warn_unused_result))
#else
#define NODISCARD
#endif

/** 함수 실행 결과. STATUS_OK 만 정상이다. */
typedef enum
{
    STATUS_OK = 0,        /**< 정상 실행 */
    STATUS_ERR_PARAM = 1, /**< 매개변수 오류 (NULL 등) */
    STATUS_ERR_RANGE = 2, /**< 값이 허용 범위를 벗어남 */
    STATUS_ERR_STATE = 3  /**< 현재 상태에서 허용되지 않는 호출 */
} status_t;

#endif /* COMMON_STATUS_IF_H */
