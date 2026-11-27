/**
 * @file types_if.h
 * @brief 컴포넌트들이 함께 쓰는 공유 타입과 상태 코드 (CMP-004, IF-004).
 * @trace SWR-038
 *
 * 타입과 상수만 둔다. 로직은 넣지 않는다.
 */
#ifndef COMMON_TYPES_IF_H
#define COMMON_TYPES_IF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

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
    STATUS_ERR_PARAM = 1, /**< 매개변수 오류 (NULL, 열거 범위 밖) */
    STATUS_ERR_TIME = 2   /**< 평가 시각이 직전에 받아들여진 평가보다 작음 */
} status_t;

/** 잠금 출력 값 (OEM-IF-005). */
typedef enum
{
    LOCK_RELEASE = 0, /**< 해제 */
    LOCK_LOCK = 1     /**< 잠금 */
} lock_cmd_t;

/** 제어 상태. 이터레이션 1에서 STATE_OFF 는 쓰지 않는다. */
typedef enum
{
    STATE_NORMAL = 0,
    STATE_DEGRADED = 1,
    STATE_FAULT = 2,
    STATE_OFF = 3
} ctrl_state_t;

/** 입력 상태 (입력 항목별). */
typedef enum
{
    VALIDITY_VALID = 0,
    VALIDITY_INVALID = 1,
    VALIDITY_STALE = 2
} validity_t;

/** 결정 이유 (reason_code 와 priority_reason 은 같은 값). */
typedef enum
{
    REASON_NONE = 0,
    REASON_CRASH_CONFIRMED = 1,
    REASON_SENSOR_FAULT = 2,
    REASON_INPUT_INVALID = 3,
    REASON_INPUT_STALE = 4,
    REASON_CRASH_PENDING = 5
} reason_t;

/** 경고 코드. */
typedef enum
{
    WARNING_NONE = 0,
    WARNING_SENSOR_FAULT = 1
} warning_t;

/** 운전자 명령 처리 상태. */
typedef enum
{
    COMMAND_NONE = 0,
    COMMAND_ACCEPTED = 1,
    COMMAND_REJECTED = 2
} command_status_t;

/** 충돌 상태 (OEM-IF-002). 값은 입력 번호와 같다. */
typedef enum
{
    CRASH_NONE = 0,
    CRASH_PENDING = 1,
    CRASH_CONFIRMED = 2
} crash_status_t;

/** 기어 (OEM-IF-001). 값은 입력 번호와 같다. */
typedef enum
{
    GEAR_P = 0,
    GEAR_N = 1,
    GEAR_D = 2,
    GEAR_R = 3
} gear_t;

/** 입력 항목. 배열 첨자로 쓰며 ITEM_COUNT 가 항목 수다. */
typedef enum
{
    ITEM_VEHICLE_SPEED = 0,
    ITEM_GEAR = 1,
    ITEM_SOURCE_TIMESTAMP = 2,
    ITEM_CRASH_STATUS = 3,
    ITEM_REAR_LEFT_APPROACH_RISK = 4,
    ITEM_REAR_RIGHT_APPROACH_RISK = 5,
    ITEM_FIRE_DETECTED = 6,
    ITEM_OVERTEMPERATURE_DETECTED = 7,
    ITEM_ADULT_PRESENT = 8,
    ITEM_ISOFIX_LEFT = 9,
    ITEM_ISOFIX_RIGHT = 10,
    ITEM_IGNITION_ON = 11,
    ITEM_SENSOR_FAULT = 12,
    ITEM_COUNT = 13
} input_item_t;

/** 검사 전의 입력 값 하나. 단위는 항목마다 다르다(ms, km/h, 열거 번호, 0/1). */
typedef struct
{
    bool present; /**< 값이 주어졌는가 (false 이면 누락) */
    double value; /**< 원시 값 */
} raw_value_t;

/** 검사 전의 시각 값 (ms). */
typedef struct
{
    bool present;  /**< 값이 주어졌는가 */
    int64_t value; /**< 원시 값 (ms) */
} raw_time_t;

/** 검사 전의 차량 입력 스냅샷 (OEM-IF-001, 002, 003, 007, 008, 009). */
typedef struct
{
    raw_time_t timestamp_ms;      /**< 스냅샷 시각 (ms) */
    raw_value_t item[ITEM_COUNT]; /**< 입력 항목별 원시 값 */
} raw_snapshot_t;

/** 검사 전의 운전자 명령 (OEM-IF-004). 열거 번호: side 0~2, action 0~1, source 0~3. */
typedef struct
{
    raw_value_t side;   /**< 0 left, 1 right, 2 all */
    raw_value_t action; /**< 0 lock, 1 unlock */
    raw_value_t source; /**< 0 physical_button, 1 avn, 2 voice, 3 mobile_app */
} raw_command_t;

/** 입력 검사를 거친 입력. value 는 차속 0.1 km/h 정수, 열거는 번호, boolean 은 0/1. */
typedef struct
{
    validity_t validity[ITEM_COUNT]; /**< 입력 항목별 입력 상태 */
    int64_t value[ITEM_COUNT];       /**< 입력 항목별 판정 값 (입력 상태가 VALID 일 때 의미 있음) */
} normalized_inputs_t;

/** 좌, 우 잠금 출력. */
typedef struct
{
    lock_cmd_t left;  /**< 좌측 후석 */
    lock_cmd_t right; /**< 우측 후석 */
} lock_outputs_t;

/** arbiter 의 판정 결과. */
typedef struct
{
    lock_outputs_t outputs; /**< 확정한 출력 */
    ctrl_state_t state;     /**< 제어 상태 */
    reason_t reason;        /**< 결정 이유 */
    warning_t warning;      /**< 경고 코드 */
} decision_t;

/** lock_ctrl 초기화 설정. 초기 출력 지정은 시험 지원 목적이다(SWR-039). */
typedef struct
{
    lock_outputs_t initial_outputs; /**< 초기 출력 (두 값 모두 열거 범위 안) */
} lock_ctrl_config_t;

/** lock_ctrl 평가 입력. snapshot, command 는 없으면 NULL. */
typedef struct
{
    uint32_t now_ms;                /**< 평가 시각 (ms) */
    const raw_snapshot_t *snapshot; /**< 새 스냅샷 (없으면 NULL) */
    const raw_command_t *command;   /**< 새 운전자 명령 (없으면 NULL) */
} lock_ctrl_input_t;

/** lock_ctrl 평가 결과. */
typedef struct
{
    lock_outputs_t outputs;                /**< 확정한 출력 */
    ctrl_state_t state;                    /**< 제어 상태 */
    reason_t reason;                       /**< 결정 이유 */
    reason_t priority_reason;              /**< reason 과 같은 값 (OEM-IF-006) */
    warning_t warning;                     /**< 경고 코드 */
    command_status_t command_status;       /**< 운전자 명령 처리 상태 */
    validity_t input_validity[ITEM_COUNT]; /**< 입력 항목별 입력 상태 */
} lock_ctrl_result_t;

#ifdef __cplusplus
}
#endif

#endif /* COMMON_TYPES_IF_H */
