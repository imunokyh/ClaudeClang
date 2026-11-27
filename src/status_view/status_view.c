/**
 * @file status_view.c
 * @brief 상태 표시용 직렬화 컴포넌트 구현 (CMP-005, MOD-005). 상태가 없는 순수 변환이다.
 * @trace SWR-014 SWR-015
 */
#include "status_view/status_view_if.h"

#include <stddef.h>
#include <string.h>

/** 문자열 조각 배열의 크기. 스칼라 조각 약 30개, 입력 항목마다 5개, 끝 1개를 담는다. */
#define MAX_PIECES (128)
/** 배열의 원소 수. */
#define NAME_COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))

/** 잠금 출력 이름 (lock_cmd_t 순서). */
static const char *const LOCK_NAMES[] = {"RELEASE", "LOCK"};
/** 제어 상태 이름 (ctrl_state_t 순서). */
static const char *const STATE_NAMES[] = {"NORMAL", "DEGRADED", "FAULT", "OFF"};
/** 결정 이유 이름 (reason_t 순서). */
static const char *const REASON_NAMES[] = {
    "NONE",          "CRASH_CONFIRMED",   "SENSOR_FAULT", "INPUT_INVALID",   "INPUT_STALE",
    "CRASH_PENDING", "IGNITION_OFF",      "FIRE",         "OVERTEMPERATURE", "ADULT_PRESENT",
    "APPROACH_RISK", "APPROACH_OVERRIDE", "ISOFIX",       "AUTO_LOCK",       "DRIVER_COMMAND"};
/** 경고 이름 (warning_t 순서). */
static const char *const WARNING_NAMES[] = {"NONE", "SENSOR_FAULT"};
/** 명령 상태 이름 (command_status_t 순서). */
static const char *const COMMAND_NAMES[] = {"NONE", "ACCEPTED", "REJECTED"};
/** 입력 상태 이름 (validity_t 순서). */
static const char *const VALIDITY_NAMES[] = {"VALID", "INVALID", "STALE"};
/** 입력 항목 이름 (input_item_t 순서, 요구사항 5.6절). */
static const char *const ITEM_NAMES[ITEM_COUNT] = {"vehicle_speed",
                                                   "gear",
                                                   "source_timestamp",
                                                   "crash_status",
                                                   "rear_left_approach_risk",
                                                   "rear_right_approach_risk",
                                                   "fire_detected",
                                                   "overtemperature_detected",
                                                   "adult_present",
                                                   "isofix_left",
                                                   "isofix_right",
                                                   "ignition_on",
                                                   "sensor_fault"};

/**
 * @brief 열거값의 이름을 낸다. 범위 밖이면 "UNKNOWN"이다.
 * @trace FN-038
 * @param names 이름 배열 (NULL 불가)
 * @param count 이름 수
 * @param value 열거값
 * @return 이름 문자열
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
static const char *name_of(const char *const *names, int count, int value)
{
    const char *name = "UNKNOWN";

    if ((value >= 0) && (value < count))
    {
        name = names[value];
    }

    return name;
}

/**
 * @brief 스칼라 필드(출력, state, 이유, 경고, 명령 상태, override)의 문자열 조각을 채운다.
 * @trace FN-039
 * @param result 조회 결과 (NULL 불가)
 * @param pieces 출력: 조각 배열 (MAX_PIECES 이상의 크기, NULL 불가)
 * @return 채운 조각 수
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
static int fill_scalar_pieces(const lock_ctrl_result_t *result, const char **pieces)
{
    int n = 0;

    pieces[n++] = "{\"lock_left\":\"";
    pieces[n++] = name_of(LOCK_NAMES, NAME_COUNT(LOCK_NAMES), (int)result->outputs.left);
    pieces[n++] = "\",\"lock_right\":\"";
    pieces[n++] = name_of(LOCK_NAMES, NAME_COUNT(LOCK_NAMES), (int)result->outputs.right);
    pieces[n++] = "\",\"state\":\"";
    pieces[n++] = name_of(STATE_NAMES, NAME_COUNT(STATE_NAMES), (int)result->state);
    pieces[n++] = "\",\"priority_reason\":\"";
    pieces[n++] = name_of(REASON_NAMES, NAME_COUNT(REASON_NAMES), (int)result->priority_reason);
    pieces[n++] = "\",\"reason_code\":\"";
    pieces[n++] = name_of(REASON_NAMES, NAME_COUNT(REASON_NAMES), (int)result->reason);
    pieces[n++] = "\",\"warning_code\":\"";
    pieces[n++] = name_of(WARNING_NAMES, NAME_COUNT(WARNING_NAMES), (int)result->warning);
    pieces[n++] = "\",\"command_status\":\"";
    pieces[n++] = name_of(COMMAND_NAMES, NAME_COUNT(COMMAND_NAMES), (int)result->command_status);
    pieces[n++] = "\",\"override_left\":";
    pieces[n++] = result->override_active[DOOR_LEFT] ? "true" : "false";
    pieces[n++] = ",\"override_right\":";
    pieces[n++] = result->override_active[DOOR_RIGHT] ? "true" : "false";
    pieces[n++] = ",\"input_validity\":{";

    return n;
}

/**
 * @brief 입력 항목별 입력 상태의 문자열 조각을 채운다.
 * @trace FN-040
 * @param result 조회 결과 (NULL 불가)
 * @param pieces 출력: 조각 배열 (항목당 5개와 끝 1개가 들어갈 크기, NULL 불가)
 * @return 채운 조각 수
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
static int fill_validity_pieces(const lock_ctrl_result_t *result, const char **pieces)
{
    int n = 0;

    for (int index = 0; index < (int)ITEM_COUNT; index++)
    {
        pieces[n++] = (index == 0) ? "\"" : ",\"";
        pieces[n++] = ITEM_NAMES[index];
        pieces[n++] = "\":\"";
        pieces[n++] =
            name_of(VALIDITY_NAMES, NAME_COUNT(VALIDITY_NAMES), (int)result->input_validity[index]);
        pieces[n++] = "\"";
    }
    pieces[n++] = "}}";

    return n;
}

/**
 * @brief 조각을 이어 버퍼에 쓴다. 부족하면 빈 문자열과 STATUS_ERR_BUFFER를 낸다.
 * @trace FN-041
 * @param pieces 문자열 조각 (NULL 불가)
 * @param count 조각 수
 * @param buffer 출력 버퍼 (NULL 불가)
 * @param size 버퍼 크기 (1 이상)
 * @return STATUS_OK, STATUS_ERR_BUFFER
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
static status_t join_pieces(const char *const *pieces, int count, char *buffer, size_t size)
{
    status_t status = STATUS_ERR_BUFFER;
    size_t total = 0;

    for (int index = 0; index < count; index++)
    {
        total += strlen(pieces[index]);
    }
    if (total < size)
    {
        size_t position = 0;

        for (int index = 0; index < count; index++)
        {
            const size_t length = strlen(pieces[index]);

            memcpy(&buffer[position], pieces[index], length);
            position += length;
        }
        buffer[position] = '\0';
        status = STATUS_OK;
    }
    else
    {
        buffer[0] = '\0';
    }

    return status;
}

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
 * - 상태: 완료
 * - 리뷰어: imunokyh
 * - 일시: 2026-11-27
 */
status_t status_view_format(const lock_ctrl_result_t *result, char *buffer, size_t size)
{
    status_t status = STATUS_ERR_PARAM;

    if ((result != NULL) && (buffer != NULL) && (size > 0U))
    {
        const char *pieces[MAX_PIECES];
        int count = fill_scalar_pieces(result, pieces);

        count += fill_validity_pieces(result, &pieces[count]);
        status = join_pieces(pieces, count, buffer, size);
    }

    return status;
}
