/**
 * @file input_guard.c
 * @brief 입력 검사 컴포넌트 구현 (CMP-001, MOD-001).
 * @trace SWR-013 SWR-022 SWR-033 SWR-046
 */
#include "input_guard/input_guard_if.h"

#include <math.h>
#include <stddef.h>

/** 마지막 수신 후 이 시간(ms)을 초과하면 모든 입력이 STALE 이다 (SWR-013). */
#define STALE_THRESHOLD_MS (200U)
/** 차속 상한 (km/h, SWR-022). */
#define SPEED_MAX_KPH (300.0)
/** 0.1 km/h 단위 수 (SWR-022). */
#define SPEED_UNIT_PER_KPH (10.0)
/** 운전자 명령 열거 번호의 최대값 (SWR-033). */
/** side 열거 번호의 최대값. */
#define SIDE_MAX (2.0)
/** action 열거 번호의 최대값. */
#define ACTION_MAX (1.0)
/** source 열거 번호의 최대값. */
#define SOURCE_MAX (3.0)
/** gear 열거 번호의 최대값. */
#define GEAR_MAX (3.0)
/** crash_status 열거 번호의 최대값. */
#define CRASH_MAX (2.0)
/** boolean 항목의 최대값. */
#define BOOL_MAX (1.0)
/** 시각 항목의 하한 (ms). 상한은 스냅샷 시각이다. */
#define TIME_MIN (0.0)

/** 입력 항목의 검사 종류. */
typedef enum
{
    KIND_BOOL,
    KIND_ENUM,
    KIND_TIME,
    KIND_SPEED
} item_kind_t;

/** 입력 항목 하나의 검사 규칙. */
typedef struct
{
    item_kind_t kind; /**< 검사 종류 */
    double lower;     /**< 하한 (포함) */
    double upper;     /**< 상한 (포함). 시각 항목은 스냅샷 시각을 쓰므로 무시된다 */
} item_spec_t;

/** 입력 항목 표. 첨자는 input_item_t 와 같다. */
static const item_spec_t ITEM_SPECS[ITEM_COUNT] = {
    {KIND_SPEED, 0.0, SPEED_MAX_KPH}, /* ITEM_VEHICLE_SPEED */
    {KIND_ENUM, 0.0, GEAR_MAX},       /* ITEM_GEAR */
    {KIND_TIME, TIME_MIN, 0.0},       /* ITEM_SOURCE_TIMESTAMP (상한은 스냅샷 시각) */
    {KIND_ENUM, 0.0, CRASH_MAX},      /* ITEM_CRASH_STATUS */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_REAR_LEFT_APPROACH_RISK */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_REAR_RIGHT_APPROACH_RISK */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_FIRE_DETECTED */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_OVERTEMPERATURE_DETECTED */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_ADULT_PRESENT */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_ISOFIX_LEFT */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_ISOFIX_RIGHT */
    {KIND_BOOL, 0.0, BOOL_MAX},       /* ITEM_IGNITION_ON */
    {KIND_BOOL, 0.0, BOOL_MAX}        /* ITEM_SENSOR_FAULT */
};

/**
 * @brief 값이 존재하고 0 또는 1인지 검사한다.
 * @trace FN-007
 * @param raw 원시 값 (NULL 불가)
 * @param value 출력: 0 또는 1 (유효할 때만 갱신, NULL 불가)
 * @return 유효하면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool validate_boolean(const raw_value_t *raw, int64_t *value)
{
    const bool is_zero = (raw->value == 0.0);
    const bool is_one = (raw->value == 1.0);
    const bool valid = raw->present && (is_zero || is_one);

    if (valid)
    {
        *value = is_one ? 1 : 0;
    }

    return valid;
}

/**
 * @brief 값이 존재하고 정수이며 범위 안인지 검사한다. NaN 은 모든 비교가 거짓이라 거절된다.
 * @trace FN-008
 * @param raw 원시 값 (NULL 불가)
 * @param lower 하한 (포함)
 * @param upper 상한 (포함)
 * @param value 출력: 정수 값 (유효할 때만 갱신, NULL 불가)
 * @return 유효하면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool validate_integer_range(const raw_value_t *raw, double lower, double upper,
                                   int64_t *value)
{
    const bool valid = raw->present && (raw->value == floor(raw->value)) && (raw->value >= lower) &&
                       (raw->value <= upper);

    if (valid)
    {
        *value = (int64_t)raw->value;
    }

    return valid;
}

/**
 * @brief 차속이 존재하고 0.0~300.0 km/h이면 0.1 km/h 정수로 변환한다.
 * @trace FN-009
 * @param raw 원시 값 (km/h, NULL 불가)
 * @param value 출력: 0.1 km/h 단위 정수 (유효할 때만 갱신, NULL 불가)
 * @return 유효하면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool validate_speed(const raw_value_t *raw, int64_t *value)
{
    const bool valid = raw->present && (raw->value >= 0.0) && (raw->value <= SPEED_MAX_KPH);

    if (valid)
    {
        *value = (int64_t)((raw->value * SPEED_UNIT_PER_KPH) + 0.5);
    }

    return valid;
}

/**
 * @brief 입력 항목 하나가 종류별 규칙에 맞는지 검사하고 내부 값을 낸다.
 * @trace FN-006
 * @param spec 항목 규칙 (NULL 불가)
 * @param raw 원시 값 (NULL 불가)
 * @param upper_bound 상한 (열거·시각 항목에 쓴다)
 * @param value 출력: 내부 값 (유효할 때만 갱신, NULL 불가)
 * @return 유효하면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool validate_item(const item_spec_t *spec, const raw_value_t *raw, double upper_bound,
                          int64_t *value)
{
    bool valid = false;

    switch (spec->kind)
    {
    case KIND_BOOL:
        valid = validate_boolean(raw, value);
        break;
    case KIND_ENUM:
        valid = validate_integer_range(raw, spec->lower, spec->upper, value);
        break;
    case KIND_TIME:
        valid = validate_integer_range(raw, spec->lower, upper_bound, value);
        break;
    case KIND_SPEED:
        valid = validate_speed(raw, value);
        break;
    }

    return valid;
}

/**
 * @brief 스냅샷이 받아들여졌고 입력 항목이 규칙에 맞을 때만 값을 낸다.
 * @trace FN-005
 * @param spec 항목 규칙 (NULL 불가)
 * @param raw 원시 값 (NULL 불가)
 * @param snapshot_accepted 스냅샷 시각이 받아들여졌는가
 * @param upper_bound 상한
 * @param value 출력: 내부 값 (NULL 불가)
 * @return 받아들일 수 있으면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool is_item_acceptable(const item_spec_t *spec, const raw_value_t *raw,
                               bool snapshot_accepted, double upper_bound, int64_t *value)
{
    return snapshot_accepted && validate_item(spec, raw, upper_bound, value);
}

/**
 * @brief 입력 항목의 상한을 정한다. 시각 항목은 스냅샷 시각, 나머지는 표의 값이다.
 * @trace FN-010
 * @param spec 항목 규칙 (NULL 불가)
 * @param snapshot 스냅샷 (NULL 불가)
 * @return 상한
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static double item_upper_bound(const item_spec_t *spec, const raw_snapshot_t *snapshot)
{
    double bound = spec->upper;

    if (spec->kind == KIND_TIME)
    {
        bound = (double)snapshot->timestamp_ms.value;
    }

    return bound;
}

/**
 * @brief 스냅샷 시각이 받아들일 수 있는 값인지 판정한다.
 * @trace FN-004
 * @param guard 저장소 (NULL 불가)
 * @param now_ms 평가 시각 (ms)
 * @param snapshot 스냅샷 (NULL 불가)
 * @return 받아들일 수 있으면 true. 직전에 받아들인 스냅샷 이상이고 평가 시각 이하여야 한다
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool is_snapshot_time_acceptable(const input_guard_t *guard, uint32_t now_ms,
                                        const raw_snapshot_t *snapshot)
{
    const raw_time_t time = snapshot->timestamp_ms;

    return time.present && (time.value >= (int64_t)guard->last_snapshot_ms) &&
           (time.value <= (int64_t)now_ms);
}

/**
 * @brief 받아들인 스냅샷이 없거나 마지막 수신 후 200 ms를 초과했는지 판정한다.
 * @trace FN-012
 * @param guard 저장소 (NULL 불가)
 * @param now_ms 평가 시각 (ms, 마지막 수신 시각 이상)
 * @return 오래되었으면 true
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static bool is_snapshot_stale(const input_guard_t *guard, uint32_t now_ms)
{
    return (!guard->has_snapshot) || ((now_ms - guard->last_receive_ms) > STALE_THRESHOLD_MS);
}

/**
 * @brief 입력 항목의 표시 입력 상태(INVALID, STALE, VALID)를 정한다.
 * @trace FN-011
 * @param guard 저장소 (NULL 불가)
 * @param item 입력 항목 (0 이상 ITEM_COUNT 미만)
 * @param now_ms 평가 시각 (ms)
 * @return 입력 상태
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static validity_t compute_validity(const input_guard_t *guard, input_item_t item, uint32_t now_ms)
{
    validity_t validity = VALIDITY_VALID;

    if (guard->invalid[item])
    {
        validity = VALIDITY_INVALID;
    }
    else if (is_snapshot_stale(guard, now_ms))
    {
        validity = VALIDITY_STALE;
    }

    return validity;
}

/**
 * @brief 입력 항목별 입력 상태와 보관 값을 출력 구조체에 채운다.
 * @trace FN-013
 * @param guard 저장소 (NULL 불가)
 * @param now_ms 평가 시각 (ms)
 * @param out 출력 (NULL 불가)
 * @return STATUS_OK
 * @par AI 작성
 * - 모델: claude-sonnet-5-5
 * - 일시: 2026-11-27
 * - 요청자: imunokyh
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
static status_t fill_normalized_inputs(const input_guard_t *guard, uint32_t now_ms,
                                       normalized_inputs_t *out)
{
    for (int index = 0; index < (int)ITEM_COUNT; index++)
    {
        out->validity[index] = compute_validity(guard, (input_item_t)index, now_ms);
        out->value[index] = guard->held[index];
    }

    return STATUS_OK;
}

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
status_t input_guard_init(input_guard_t *guard)
{
    status_t status = STATUS_ERR_PARAM;

    if (guard != NULL)
    {
        for (int index = 0; index < (int)ITEM_COUNT; index++)
        {
            guard->invalid[index] = false;
            guard->held[index] = 0;
        }
        guard->has_snapshot = false;
        guard->last_receive_ms = 0;
        guard->last_snapshot_ms = 0;
        status = STATUS_OK;
    }

    return status;
}

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
status_t input_guard_update(input_guard_t *guard, uint32_t now_ms, const raw_snapshot_t *snapshot,
                            normalized_inputs_t *out)
{
    status_t status = STATUS_ERR_PARAM;

    if ((guard != NULL) && (out != NULL))
    {
        if (snapshot != NULL)
        {
            const bool accepted = is_snapshot_time_acceptable(guard, now_ms, snapshot);
            for (int index = 0; index < (int)ITEM_COUNT; index++)
            {
                int64_t value = 0;
                const bool acceptable =
                    is_item_acceptable(&ITEM_SPECS[index], &snapshot->item[index], accepted,
                                       item_upper_bound(&ITEM_SPECS[index], snapshot), &value);
                guard->invalid[index] = !acceptable;
                if (acceptable)
                {
                    guard->held[index] = value;
                }
            }
            if (accepted)
            {
                guard->has_snapshot = true;
                guard->last_snapshot_ms = (uint32_t)snapshot->timestamp_ms.value;
                guard->last_receive_ms = guard->last_snapshot_ms;
            }
        }
        status = fill_normalized_inputs(guard, now_ms, out);
    }

    return status;
}

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
status_t input_guard_check_command(const raw_command_t *command, command_status_t *out)
{
    status_t status = STATUS_ERR_PARAM;

    if ((command != NULL) && (out != NULL))
    {
        int64_t unused = 0;
        const bool side_ok = validate_integer_range(&command->side, 0.0, SIDE_MAX, &unused);
        const bool action_ok = validate_integer_range(&command->action, 0.0, ACTION_MAX, &unused);
        const bool source_ok = validate_integer_range(&command->source, 0.0, SOURCE_MAX, &unused);
        *out = (side_ok && action_ok && source_ok) ? COMMAND_ACCEPTED : COMMAND_REJECTED;
        status = STATUS_OK;
    }

    return status;
}
