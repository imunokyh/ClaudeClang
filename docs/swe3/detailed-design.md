# SW 상세 설계서

- 대상 SW: 전자식 차일드락 제어 SW (후석 좌, 우)   버전: 0.2 (이터레이션 1)   일자: 2026-11-27
- 입력: docs/swe2/architecture.md (v0.1)
- 면책: 교육용 가상 OEM 입력에 대한 설계이며 준수·인증·ASIL 달성을 주장하지 않는다.
- 인터페이스 필수 기준: M1 호출됨 / M2 공개 함수 3개 이상 / M3 안전 요구 구현 / M4 상태 소유 (detailed-design 스킬 기준)
- 컴포넌트마다 모듈 하나를 두고(컴포넌트가 작아 더 쪼개면 SRP가 아니라 파편화가 된다), 모듈 안의 비공개 함수는 `static`이다. 모든 모듈은 M1(호출됨), M3 또는 M4에 해당하므로 인터페이스를 가진다.

## 1. 모듈

| ID | 모듈 (경로) | 책임 (한 문장) | 소유 데이터 | 인터페이스 필요(근거) | 상위 ID |
|---|---|---|---|---|---|
| MOD-001 | input_guard (`src/input_guard/`) | 스냅샷과 운전자 명령의 유효성을 검사하고 입력 항목별 입력 상태를 정한다 | `input_guard_t`(항목별 저장 상태, 보관 값, 마지막 수신 시각, 직전 스냅샷 시각) | 예 (M1, M3, M4) | CMP-001 |
| MOD-002 | arbiter (`src/arbiter/`) | 정규화된 입력과 직전 확정 출력으로 규칙표에 따라 출력, state, 결정 이유, 경고를 정한다 | 없음 | 예 (M1, M3) | CMP-002 |
| MOD-003 | lock_ctrl (`src/lock_ctrl/`) | 평가 순서를 지휘하고 직전 확정 출력을 보관해 결과를 낸다 | `lock_ctrl_t`(직전 확정 출력, 직전 now_ms, 마지막 결과, input_guard_t) | 예 (M1, M3, M4) | CMP-003 |
| MOD-004 | common (`src/common/`) | 공유 타입과 상수를 정의한다 | 없음 | 예 (M1, 공유 타입 헤더) | CMP-004 |

## 2. 모듈 인터페이스

| ID | 인터페이스 (헤더) | 제공 모듈 | 사용 모듈 | 구현하는 컴포넌트 인터페이스 | 상위 ID |
|---|---|---|---|---|---|
| MIF-001 | `input_guard_if.h` | MOD-001 | MOD-003 | IF-001 | CMP-001, IF-001 |
| MIF-002 | `arbiter_if.h` | MOD-002 | MOD-003 | IF-002 | CMP-002, IF-002 |
| MIF-003 | `lock_ctrl_if.h` | MOD-003 | 호출자(범위 밖) | IF-003 | CMP-003, IF-003 |
| MIF-004 | `types_if.h` | MOD-004 | MOD-001, MOD-002, MOD-003 | IF-004 | CMP-004, IF-004 |

## 3. 함수

모든 함수는 `status_t` 또는 값(판정 결과)을 반환하며 `void`가 없다. 판정 함수(`bool`, 열거형, 실수 반환)는 순수 함수다. 모든 함수는 `return`을 끝에 한 번만 둔다.

| ID | 함수 | 모듈 | 공개 | 목적 (한 문장) | 상위 ID |
|---|---|---|---|---|---|
| FN-001 | `status_t input_guard_init(input_guard_t *guard)` | MOD-001 | 예 | 입력 상태 저장소를 초기 상태로 만든다 | MOD-001, CMP-001, IF-001 |
| FN-002 | `status_t input_guard_update(input_guard_t *guard, uint32_t now_ms, const raw_snapshot_t *snapshot, normalized_inputs_t *out)` | MOD-001 | 예 | 스냅샷(있으면)을 검사해 저장 상태를 갱신하고 입력 항목별 입력 상태와 판정 값을 만든다 | MOD-001, CMP-001, IF-001 |
| FN-003 | `status_t input_guard_check_command(const raw_command_t *command, command_status_t *out)` | MOD-001 | 예 | 운전자 명령의 유효성을 검사해 명령 상태를 정한다 | MOD-001, CMP-001, IF-001 |
| FN-004 | `static bool is_snapshot_time_acceptable(const input_guard_t *guard, uint32_t now_ms, const raw_snapshot_t *snapshot)` | MOD-001 | 아니오 | 스냅샷 시각이 받아들일 수 있는 값인지 판정한다 | MOD-001, CMP-001 |
| FN-005 | `static bool is_item_acceptable(const item_spec_t *spec, const raw_value_t *raw, bool snapshot_accepted, double upper_bound, int64_t *value)` | MOD-001 | 아니오 | 스냅샷이 받아들여졌고 입력 항목이 규칙에 맞을 때만 값을 낸다 | MOD-001, CMP-001 |
| FN-006 | `static bool validate_item(const item_spec_t *spec, const raw_value_t *raw, double upper_bound, int64_t *value)` | MOD-001 | 아니오 | 입력 항목 하나가 종류별 규칙에 맞는지 검사하고 내부 값을 낸다 | MOD-001, CMP-001 |
| FN-007 | `static bool validate_boolean(const raw_value_t *raw, int64_t *value)` | MOD-001 | 아니오 | 값이 존재하고 0 또는 1인지 검사한다 | MOD-001, CMP-001 |
| FN-008 | `static bool validate_integer_range(const raw_value_t *raw, double lower, double upper, int64_t *value)` | MOD-001 | 아니오 | 값이 존재하고 정수이며 범위 안인지 검사한다 | MOD-001, CMP-001 |
| FN-009 | `static bool validate_speed(const raw_value_t *raw, int64_t *value)` | MOD-001 | 아니오 | 차속이 존재하고 0.0~300.0 km/h이면 0.1 km/h 정수로 변환한다 | MOD-001, CMP-001 |
| FN-010 | `static double item_upper_bound(const item_spec_t *spec, const raw_snapshot_t *snapshot)` | MOD-001 | 아니오 | 입력 항목의 상한을 정한다(시각 항목은 스냅샷 시각, 나머지는 표의 값) | MOD-001, CMP-001 |
| FN-011 | `static validity_t compute_validity(const input_guard_t *guard, input_item_t item, uint32_t now_ms)` | MOD-001 | 아니오 | 입력 항목의 표시 입력 상태(INVALID, STALE, VALID)를 정한다 | MOD-001, CMP-001 |
| FN-012 | `static bool is_snapshot_stale(const input_guard_t *guard, uint32_t now_ms)` | MOD-001 | 아니오 | 받아들인 스냅샷이 없거나 마지막 수신 후 200 ms를 초과했는지 판정한다 | MOD-001, CMP-001 |
| FN-013 | `static status_t fill_normalized_inputs(const input_guard_t *guard, uint32_t now_ms, normalized_inputs_t *out)` | MOD-001 | 아니오 | 입력 항목별 입력 상태와 보관 값을 출력 구조체에 채운다 | MOD-001, CMP-001 |
| FN-014 | `status_t arbiter_decide(const normalized_inputs_t *inputs, const lock_outputs_t *previous, decision_t *out)` | MOD-002 | 예 | 규칙표와 state 결정표로 출력, state, 결정 이유, 경고를 정한다 | MOD-002, CMP-002, IF-002 |
| FN-015 | `static bool has_item_value(const normalized_inputs_t *inputs, input_item_t item, int64_t expected)` | MOD-002 | 아니오 | 입력 항목이 VALID이고 판정 값이 기대값과 같은지 판정한다 | MOD-002, CMP-002 |
| FN-016 | `static bool has_any_validity(const normalized_inputs_t *inputs, validity_t validity)` | MOD-002 | 아니오 | 입력 항목 중 해당 입력 상태인 것이 하나라도 있는지 판정한다 | MOD-002, CMP-002 |
| FN-017 | `static arbiter_rule_t select_rule(const normalized_inputs_t *inputs)` | MOD-002 | 아니오 | 결정 규칙표에서 처음 성립하는 규칙을 고른다 | MOD-002, CMP-002 |
| FN-018 | `static status_t apply_rule(arbiter_rule_t rule, const lock_outputs_t *previous, decision_t *out)` | MOD-002 | 아니오 | 규칙에 따라 출력과 결정 이유를 채운다 | MOD-002, CMP-002 |
| FN-019 | `static ctrl_state_t decide_state(bool fault_condition, bool degraded_condition)` | MOD-002 | 아니오 | FAULT, DEGRADED 조건으로 state를 정한다 | MOD-002, CMP-002 |
| FN-020 | `status_t lock_ctrl_init(lock_ctrl_t *ctrl, const lock_ctrl_config_t *config)` | MOD-003 | 예 | 제어기를 초기 상태로 만들고 초기 출력을 정한다 | MOD-003, CMP-003, IF-003 |
| FN-021 | `status_t lock_ctrl_evaluate(lock_ctrl_t *ctrl, const lock_ctrl_input_t *input, lock_ctrl_result_t *result)` | MOD-003 | 예 | 평가 한 번을 정해진 순서로 수행하고 결과를 낸다 | MOD-003, CMP-003, IF-003 |
| FN-022 | `status_t lock_ctrl_get_result(const lock_ctrl_t *ctrl, lock_ctrl_result_t *result)` | MOD-003 | 예 | 마지막 평가 결과(또는 초기 결과)를 낸다 | MOD-003, CMP-003, IF-003 |
| FN-023 | `static bool is_time_regressed(const lock_ctrl_t *ctrl, uint32_t now_ms)` | MOD-003 | 아니오 | now_ms가 직전에 받아들여진 평가보다 작은지 판정한다 | MOD-003, CMP-003 |
| FN-024 | `static status_t run_evaluation(lock_ctrl_t *ctrl, const lock_ctrl_input_t *input, lock_ctrl_result_t *result)` | MOD-003 | 아니오 | 입력 검사, 명령 검사, 결정을 호출해 결과를 구성하고 상태를 갱신한다 | MOD-003, CMP-003 |
| FN-025 | `static status_t build_result(const decision_t *decision, const normalized_inputs_t *inputs, command_status_t command, lock_ctrl_result_t *result)` | MOD-003 | 아니오 | 결정과 입력 상태로 결과 구조체를 구성한다 | MOD-003, CMP-003 |
| FN-026 | `static status_t make_initial_result(const lock_outputs_t *initial, lock_ctrl_result_t *result)` | MOD-003 | 아니오 | 초기화 직후의 결과(SWR-025)를 만든다 | MOD-003, CMP-003 |
| FN-027 | `static bool is_valid_lock_outputs(const lock_outputs_t *outputs)` | MOD-002 | 아니오 | 두 출력 값이 모두 열거 범위 안인지 판정한다 | MOD-002, CMP-002 |
| FN-028 | `static bool is_valid_initial_outputs(const lock_outputs_t *outputs)` | MOD-003 | 아니오 | 초기 출력의 두 값이 모두 열거 범위 안인지 판정한다 | MOD-003, CMP-003 |

### 3.1 공통 설계 사항

- **상태 구조체**: `input_guard_t`, `lock_ctrl_t`는 호출자가 메모리를 제공한다(동적 할당 없음). 필드는 소유 모듈만 읽고 쓴다.
- **입력 항목 표** `item_spec_t {kind, lower, upper}` (`input_guard.c`의 `static const`, 항목 순서는 `input_item_t`):

| 항목 | kind | lower | upper |
|---|---|---|---|
| ITEM_VEHICLE_SPEED | SPEED | 0.0 | 300.0 |
| ITEM_GEAR | ENUM | 0 | 3 |
| ITEM_SOURCE_TIMESTAMP | TIME | 0 | (스냅샷 시각, 실행 시 결정) |
| ITEM_CRASH_STATUS | ENUM | 0 | 2 |
| ITEM_REAR_LEFT_APPROACH_RISK ~ ITEM_SENSOR_FAULT (나머지 9개) | BOOL | 0 | 1 |

- **NaN·무한대**: 검사 함수는 NaN과 무한대를 별도 조건으로 검사하지 않는다. 모든 비교(`==`, `>=`, `<=`)가 거짓이거나 범위를 벗어나 자연히 INVALID가 된다. 별도 조건은 MC/DC로 독립 효과를 보일 수 없는 중복 조건이 되기 때문이다.
- **원자성**: `lock_ctrl_evaluate`는 now_ms 검사에 실패하면 어떤 상태도 바꾸지 않는다(SWR-040). 하위 호출이 `STATUS_ERR_PARAM`을 반환하면 직전 확정 출력과 직전 now_ms를 갱신하지 않는다.
- **오류 처리 정책**: 반환 `STATUS_ERR_*`는 호출 규약 위반(NULL, 시각 역행)만 알리고, 입력 값의 오류는 입력 상태로 결과에 나타난다.
- **테스트 가능성**: `lock_ctrl`의 방어 경로(하위 호출이 오류를 반환하는 경우)는 단위 테스트에서 스텁(`tests/unit/stubs/`)으로 하위 컴포넌트를 대체해 도달한다. 항상 성공하는 `status_t` 반환 체인과 도달할 수 없는 `default` 분기는 두지 않는다.

### FN-001 `input_guard_init`
- **매개변수**: `guard` 출력(NULL 불가)
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(guard가 NULL)
- **사후 조건**: 모든 항목의 저장 상태 OK(`invalid=false`), 보관 값 0, `has_snapshot=false`, `last_receive_ms=0`, `last_snapshot_ms=0`
- **부작용**: `*guard`만 변경. NULL이면 아무것도 바꾸지 않는다
- **오류 처리**: `guard == NULL` → `STATUS_ERR_PARAM`

### FN-002 `input_guard_update`
- **매개변수**: `guard` 입출력(NULL 불가), `now_ms` 0~4294967295 ms, `snapshot` 읽기 전용(없으면 NULL), `out` 출력(NULL 불가)
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(guard 또는 out이 NULL)
- **알고리즘**: ① 파라미터 검사. ② snapshot이 있으면 FN-004로 시각을 판정해 `accepted`를 정한다. `accepted`이면 `last_snapshot_ms`와 `last_receive_ms`를 스냅샷 시각으로, `has_snapshot=true`로 한다. 항목마다 FN-005(accepted이고 FN-006을 통과하는지)로 판정해 통과하면 저장 상태 OK와 보관 값 갱신, 아니면 저장 상태 INVALID(보관 값 불변)로 한다. `accepted`가 거짓이면 모든 항목이 INVALID가 된다. ③ FN-013으로 출력을 채운다.
- **사후 조건**: 반환 `STATUS_OK`일 때만 `*out`이 갱신된다. snapshot이 NULL이면 저장 상태는 바뀌지 않는다.

| 규칙 | C1 guard, out이 non-NULL | C2 snapshot != NULL | C3 시각 받아들임 (FN-004) | 동작 |
|---|---|---|---|---|
| R1 | F | - | - | STATUS_ERR_PARAM, 변경 없음 |
| R2 | T | F | - | 저장 상태 불변, 출력만 채움 |
| R3 | T | T | T | 마지막 수신 시각 갱신, 항목별로 검사해 저장 상태·보관 값 갱신 |
| R4 | T | T | F | 모든 항목 INVALID. 마지막 수신 시각·보관 값 불변 |

### FN-003 `input_guard_check_command`
- **매개변수**: `command` 읽기 전용(NULL 불가), `out` 출력(NULL 불가). 명령이 없는 평가에서는 호출하지 않는다
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(command 또는 out이 NULL)
- **알고리즘**: side(0~2), action(0~1), source(0~3)를 FN-008로 검사한다. 셋 다 유효하면 `COMMAND_ACCEPTED`, 하나라도 아니면 `COMMAND_REJECTED`.

| 규칙 | C0 command, out non-NULL | C1 side 유효 | C2 action 유효 | C3 source 유효 | `*out` |
|---|---|---|---|---|---|
| R0 | F | - | - | - | STATUS_ERR_PARAM, 변경 없음 |
| R1 | T | T | T | T | COMMAND_ACCEPTED |
| R2 | T | F | - | - | COMMAND_REJECTED |
| R3 | T | T | F | - | COMMAND_REJECTED |
| R4 | T | T | T | F | COMMAND_REJECTED |

### FN-004 `is_snapshot_time_acceptable`
- **알고리즘**: 스냅샷 시각 `t`가 모든 조건을 만족하면 true. 5.1절의 범위(0 이상, 4294967295 이하)는 `t >= last_snapshot_ms`(0 이상 보장)와 `t <= now_ms`(`uint32_t`이므로 4294967295 이하)가 함께 보장하므로 범위 조건을 따로 두지 않는다(독립 효과를 보일 수 없는 중복 조건을 피한다).

| 규칙 | C1 `timestamp_ms.present` | C2 `t >= guard->last_snapshot_ms` | C3 `t <= now_ms` | 결과 |
|---|---|---|---|---|
| R1 | T | T | T | true |
| R2 | F | - | - | false |
| R3 | T | F | - | false |
| R4 | T | T | F | false |

### FN-005 `is_item_acceptable`
- `snapshot_accepted && validate_item(spec, raw, upper_bound, value)`. 스냅샷이 거절되었으면 항목 검사를 하지 않는다.

| 규칙 | C1 snapshot_accepted | C2 validate_item | 결과 |
|---|---|---|---|
| R1 | T | T | true |
| R2 | F | - | false |
| R3 | T | F | false |

### FN-006 `validate_item`
- **알고리즘**: `spec->kind`에 따라 FN-007(BOOL), FN-008(ENUM, TIME: lower, upper_bound), FN-009(SPEED)를 호출한다. `item_kind_t`의 모든 값을 처리하므로 그 밖의 값은 없다(`default` 없음).

| kind | 호출 | 결과 |
|---|---|---|
| BOOL | FN-007 | 반환값 |
| ENUM | FN-008(lower, upper) | 반환값 |
| TIME | FN-008(lower, upper_bound) | 반환값 |
| SPEED | FN-009 | 반환값 |

### FN-007 `validate_boolean`
| 규칙 | C1 present | C2 value == 0 | C3 value == 1 | 결과 |
|---|---|---|---|---|
| R1 | T | T | - | true, `*value`=0 |
| R2 | T | F | T | true, `*value`=1 |
| R3 | F | - | - | false |
| R4 | T | F | F | false (예: 2, NaN) |

### FN-008 `validate_integer_range`
| 규칙 | C1 present | C2 value가 정수(`value == floor(value)`) | C3 `lower <= value` | C4 `value <= upper` | 결과 |
|---|---|---|---|---|---|
| R1 | T | T | T | T | true, `*value`=(int64_t)value |
| R2 | F | - | - | - | false |
| R3 | T | F | - | - | false (소수, NaN) |
| R4 | T | T | F | - | false |
| R5 | T | T | T | F | false |

### FN-009 `validate_speed`
| 규칙 | C1 present | C2 `0.0 <= value` | C3 `value <= 300.0` | 결과 |
|---|---|---|---|---|
| R1 | T | T | T | true, `*value`=0.1 km/h 단위 정수(반올림, `(int64_t)(value*10.0 + 0.5)`) |
| R2 | F | - | - | false |
| R3 | T | F | - | false (음수, NaN) |
| R4 | T | T | F | false |

### FN-010 `item_upper_bound`
- `spec->kind == KIND_TIME`이면 `(double)snapshot->timestamp_ms.value`, 아니면 `spec->upper`. 스냅샷 시각이 받아들여지지 않았으면 결과를 쓰지 않는다(FN-005의 C1이 거짓).

| 규칙 | C1 `spec->kind == KIND_TIME` | 결과 |
|---|---|---|
| R1 | T | 스냅샷 시각 |
| R2 | F | `spec->upper` |

### FN-011 `compute_validity`
| 규칙 | C1 저장 상태 INVALID | C2 `is_snapshot_stale` (FN-012) | 결과 |
|---|---|---|---|
| R1 | T | - | VALIDITY_INVALID |
| R2 | F | T | VALIDITY_STALE |
| R3 | F | F | VALIDITY_VALID |

### FN-012 `is_snapshot_stale`
| 규칙 | C1 `!has_snapshot` | C2 `now_ms - last_receive_ms > 200` | 결과 |
|---|---|---|---|
| R1 | T | - | true |
| R2 | F | T | true |
| R3 | F | F | false |

(`now_ms >= last_receive_ms`가 보장되도록 평가 시각 역행은 FN-023이 거절하고, 스냅샷 시각은 now_ms 이하만 받아들인다.)

### FN-013 `fill_normalized_inputs`
- 항목마다 `out->validity[i] = compute_validity(...)`, `out->value[i] = 보관 값[i]`. 반환 `STATUS_OK`.

### FN-014 `arbiter_decide`
- **매개변수**: `inputs`, `previous` 읽기 전용(NULL 불가), `out` 출력(NULL 불가). `previous`의 값은 LOCK_RELEASE 또는 LOCK_LOCK이어야 한다
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(NULL 또는 previous의 값이 범위 밖)
- **알고리즘**: ① 검증(`inputs`, `previous`, `out`이 NULL이 아니고 `is_valid_lock_outputs(previous)`(FN-027)). ② `rule = select_rule(inputs)`(FN-017). ③ `apply_rule`(FN-018)로 출력과 결정 이유 설정. ④ `warning`: FAULT 조건(`has_item_value(sensor_fault, 1)`)이면 `WARNING_SENSOR_FAULT`, 아니면 `WARNING_NONE`. ⑤ `state = decide_state(fault, degraded)`(FN-019). degraded = INVALID 있음 또는 STALE 있음.
- **사후 조건**: `STATUS_OK`일 때만 `*out` 갱신. `out->state`는 항상 NORMAL, DEGRADED, FAULT 중 하나(이터레이션 1은 OFF 없음).

| 규칙 | C1 inputs != NULL | C2 previous != NULL | C3 out != NULL | C4 `is_valid_lock_outputs(previous)` | 결과 |
|---|---|---|---|---|---|
| R1 | T | T | T | T | 판정 수행, STATUS_OK |
| R2 | F | - | - | - | STATUS_ERR_PARAM |
| R3 | T | F | - | - | STATUS_ERR_PARAM |
| R4 | T | T | F | - | STATUS_ERR_PARAM |
| R5 | T | T | T | F | STATUS_ERR_PARAM |

### FN-015 `has_item_value`
- `inputs->validity[item] == VALIDITY_VALID && inputs->value[item] == expected`.

| 규칙 | C1 validity VALID | C2 value == expected | 결과 |
|---|---|---|---|
| R1 | T | T | true |
| R2 | F | - | false |
| R3 | T | F | false |

### FN-016 `has_any_validity`
- 입력 항목 0~`ITEM_COUNT-1` 중 `validity[i] == validity`인 것이 하나라도 있으면 true, 없으면 false. (루프, 종료 조건은 항목 수 상수)

### FN-017 `select_rule`
- 조건: C1 `has_item_value(crash_status, CRASH_CONFIRMED)`, C2 `has_item_value(sensor_fault, 1)`, C3 `has_any_validity(INVALID)`, C4 `has_any_validity(STALE)`, C5 `has_item_value(crash_status, CRASH_PENDING)`

| 규칙 | C1 | C2 | C3 | C4 | C5 | 결과 `arbiter_rule_t` | 결정 이유 | 출력 |
|---|---|---|---|---|---|---|---|---|
| R1 | T | - | - | - | - | RULE_CRASH_CONFIRMED | CRASH_CONFIRMED | 두 출력 RELEASE |
| R2 | F | T | - | - | - | RULE_SENSOR_FAULT | SENSOR_FAULT | 유지 |
| R3 | F | F | T | - | - | RULE_INPUT_INVALID | INPUT_INVALID | 유지 |
| R4 | F | F | F | T | - | RULE_INPUT_STALE | INPUT_STALE | 유지 |
| R5 | F | F | F | F | T | RULE_CRASH_PENDING | CRASH_PENDING | 유지 |
| R6 | F | F | F | F | F | RULE_NONE | NONE | 유지 |

(규칙 5 "새 명령"은 이후 이터레이션에서 R4와 R5 사이에 추가한다.)

### FN-018 `apply_rule`
| 규칙 | `out->outputs` | `out->reason` |
|---|---|---|
| RULE_CRASH_CONFIRMED | left, right = LOCK_RELEASE | REASON_CRASH_CONFIRMED |
| RULE_SENSOR_FAULT | `*previous` | REASON_SENSOR_FAULT |
| RULE_INPUT_INVALID | `*previous` | REASON_INPUT_INVALID |
| RULE_INPUT_STALE | `*previous` | REASON_INPUT_STALE |
| RULE_CRASH_PENDING | `*previous` | REASON_CRASH_PENDING |
| RULE_NONE | `*previous` | REASON_NONE |

`arbiter_rule_t`의 모든 값을 처리하므로 그 밖의 값은 없다(`default` 없음). 반환 `STATUS_OK`.

### FN-019 `decide_state`
| 규칙 | C1 fault_condition | C2 degraded_condition | 결과 |
|---|---|---|---|
| R1 | T | - | STATE_FAULT |
| R2 | F | T | STATE_DEGRADED |
| R3 | F | F | STATE_NORMAL |

### FN-020 `lock_ctrl_init`
- **매개변수**: `ctrl` 출력(NULL 불가), `config` 읽기 전용(NULL이면 기본 초기 출력 RELEASE)
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(ctrl이 NULL, config의 초기 출력이 열거 범위 밖), 또는 하위 `input_guard_init`의 오류
- **알고리즘**: 초기 출력 결정(config 있으면 그 값, 없으면 RELEASE) → FN-028로 검사 → `input_guard_init(&ctrl->guard)` → `ctrl->outputs` = 초기 출력, `has_evaluated=false`, `last_now_ms=0` → `make_initial_result`(FN-026)
- **오류 처리**: 어떤 검사도 실패하면 `*ctrl`을 바꾸지 않는다. 단, 하위 `input_guard_init`이 실패한 경우 `ctrl->guard`는 정의되지 않은 채로 둔다(방어 경로, 호출자는 반환값을 확인해야 한다).

| 규칙 | C1 ctrl != NULL | C2 config가 NULL이 아님 | C3 초기 출력 유효 (FN-028) | C4 input_guard_init 성공 | 결과 |
|---|---|---|---|---|---|
| R1 | F | - | - | - | STATUS_ERR_PARAM |
| R2 | T | T | F | - | STATUS_ERR_PARAM |
| R3 | T | F | T | T | 기본 RELEASE로 초기화, STATUS_OK |
| R4 | T | T | T | T | 지정한 초기 출력으로 초기화, STATUS_OK |
| R5 | T | - | T | F | 하위 오류 반환 |

### FN-021 `lock_ctrl_evaluate`
- **매개변수**: `ctrl` 입출력, `input`(now_ms, snapshot(NULL 가능), command(NULL 가능)), `result` 출력
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(ctrl, input, result 중 NULL), `STATUS_ERR_TIME`(시각 역행)
- **알고리즘**: ① NULL 검사. ② 시각 역행이면(`is_time_regressed`) `STATUS_ERR_TIME`, 아무것도 바꾸지 않는다. ③ 아니면 `run_evaluation`(FN-024).

| 규칙 | C1 ctrl non-NULL | C2 input non-NULL | C3 result non-NULL | C4 시각 역행 아님 | 결과 |
|---|---|---|---|---|---|
| R1 | T | T | T | T | run_evaluation 결과 |
| R2 | F | - | - | - | STATUS_ERR_PARAM |
| R3 | T | F | - | - | STATUS_ERR_PARAM |
| R4 | T | T | F | - | STATUS_ERR_PARAM |
| R5 | T | T | T | F | STATUS_ERR_TIME, 상태 불변 |

### FN-022 `lock_ctrl_get_result`
- `ctrl`, `result` non-NULL이면 `*result = ctrl->result`, `STATUS_OK`. 아니면 `STATUS_ERR_PARAM`.

| 규칙 | C1 ctrl non-NULL | C2 result non-NULL | 결과 |
|---|---|---|---|
| R1 | T | T | STATUS_OK |
| R2 | F | - | STATUS_ERR_PARAM |
| R3 | T | F | STATUS_ERR_PARAM |

### FN-023 `is_time_regressed`
| 규칙 | C1 `has_evaluated` | C2 `now_ms < last_now_ms` | 결과 |
|---|---|---|---|
| R1 | F | - | false |
| R2 | T | T | true |
| R3 | T | F | false |

### FN-024 `run_evaluation`
- **알고리즘**: ① `input_guard_update(&guard, now_ms, snapshot, &normalized)`. ② 성공이고 command가 있으면 `input_guard_check_command`로 `command_status`(없으면 `COMMAND_NONE`). ③ 성공이면 `arbiter_decide(&normalized, &ctrl->outputs, &decision)`. ④ 성공이면 `build_result`(FN-025)로 `ctrl->result`를 만든다. ⑤ 성공이면 `ctrl->outputs = decision.outputs`, `ctrl->last_now_ms = now_ms`, `ctrl->has_evaluated = true`로 갱신하고 `*result = ctrl->result`. 중간에 오류가 나면 이후 단계를 건너뛰고 `ctrl->outputs`, `last_now_ms`, `result`를 갱신하지 않는다(입력 가드의 저장 상태는 ①에서 이미 바뀌었을 수 있다. 하위 호출이 호출자가 보장한 non-NULL 인자로 불리므로 실제로는 도달하지 않는 방어 경로이며 단위 테스트는 스텁으로 도달한다).
- 오류 처리 단계는 `status`를 단계마다 확인하는 `if`로 구성하며 조기 `return`을 쓰지 않는다.

| 규칙 | C1 update 성공 | C2 command != NULL | C3 check_command 성공 | C4 decide 성공 | C5 build_result 성공 | 결과 |
|---|---|---|---|---|---|---|
| R1 | F | - | - | - | - | 하위 오류 반환 |
| R2 | T | T | F | - | - | 하위 오류 반환 |
| R3 | T | F | - | F | - | 하위 오류 반환 |
| R4 | T | T | T | F | - | 하위 오류 반환 |
| R5 | T | F 또는 T | T | T | F | 하위 오류 반환 |
| R6 | T | F 또는 T | T | T | T | 상태 갱신, STATUS_OK |

(C3은 C2가 참일 때만 평가한다. 상태 확인은 단계별 `if (status == STATUS_OK)`로 일관되게 쓴다.)

### FN-025 `build_result`
- `result->outputs = decision->outputs`, `state`, `reason`, `priority_reason = reason`, `warning`, `command_status = command`, `input_validity[i] = inputs->validity[i]`. 반환 `STATUS_OK`.

### FN-026 `make_initial_result`
- SWR-025: outputs = initial, state = NORMAL, reason = priority_reason = NONE, warning = NONE, command_status = NONE, input_validity = 모두 STALE. 반환 `STATUS_OK`.

### FN-027 `is_valid_lock_outputs`
- `(left == LOCK_RELEASE || left == LOCK_LOCK) && (right == LOCK_RELEASE || right == LOCK_LOCK)`

| 규칙 | C1 left == RELEASE | C2 left == LOCK | C3 right == RELEASE | C4 right == LOCK | 결과 |
|---|---|---|---|---|---|
| R1 | T | - | T | - | true |
| R2 | T | - | F | T | true |
| R3 | F | T | T | - | true |
| R4 | F | F | - | - | false |
| R5 | T | - | F | F | false |

### FN-028 `is_valid_initial_outputs`
- FN-027과 같은 판정(모듈 `lock_ctrl`의 비공개 함수. 컴포넌트 간 로직 공유를 피하려고 따로 둔다).

## 4. 의존 관계 (모듈 단위)

```mermaid
flowchart TD
  MOD003["MOD-003 lock_ctrl"] --> MIF001[["MIF-001"]]
  MOD003 --> MIF002[["MIF-002"]]
  MOD003 --> MIF004[["MIF-004"]]
  MIF001 --- MOD001["MOD-001 input_guard"]
  MIF002 --- MOD002["MOD-002 arbiter"]
  MOD001 --> MIF004
  MOD002 --> MIF004
  MIF004 --- MOD004["MOD-004 common"]
```

순환 없음(2026-11-27). 모듈 간 통신은 `*_if.h`로만 한다(`check_interfaces.py` I1~I5).

### 4.1 시퀀스 (한 번의 평가, 통합 테스트의 근거)

```mermaid
sequenceDiagram
  participant H as 호출자
  participant L as lock_ctrl
  participant G as input_guard
  participant A as arbiter
  H->>L: lock_ctrl_evaluate(input)
  L->>L: 시각 역행 검사
  L->>G: input_guard_update(now_ms, snapshot)
  G-->>L: normalized_inputs
  L->>G: input_guard_check_command(command) (명령이 있을 때)
  G-->>L: command_status
  L->>A: arbiter_decide(normalized, 직전 확정 출력)
  A-->>L: decision
  L-->>H: result (STATUS_OK)
```

### 4.2 Use Case (OEM 8절 G1~G2 증거, 이터레이션 1 범위)

```mermaid
flowchart LR
  OEM["호출자(차량 시스템)"] --- UC1(["차량 신호 평가"])
  OEM --- UC2(["운전자 명령 검사"])
  OEM --- UC3(["제어 결과 조회"])
  UC1 -. 포함 .-> UC4(["입력 검사·신선도"])
  UC1 -. 포함 .-> UC5(["충돌 해제·고장 유지 판정"])
```

## 5. 상수

| 이름 | 값 | 단위 | 근거 ID |
|---|---|---|---|
| `STALE_THRESHOLD_MS` | 200 | ms | SWR-013 |
| `SPEED_MAX_KPH` | 300.0 | km/h | SWR-022 |
| `SPEED_UNIT_PER_KPH` | 10 | 0.1 km/h 단위 수 | SWR-022 |
| `GEAR_MAX`, `CRASH_MAX`, `SIDE_MAX`, `ACTION_MAX`, `SOURCE_MAX` | 3, 2, 2, 1, 3 | 열거 번호 | SWR-022, SWR-033 |
| `ITEM_COUNT` | 13 | 개 | SWR-022 |

## 6. 정적 분석 계획 (OEM 8절 G2 증거)

| 검사 | 도구·명령 | 기준 |
|---|---|---|
| 정적 분석 | Cppcheck (`tools/run.sh static`) | warning, style, performance, portability, `--inconclusive`, 예외 없음, 지적 0건 |
| 순환 복잡도·함수 라인수 | Lizard (`tools/run.sh complexity`, Stop 훅) | 함수마다 순환 복잡도 10 이하, 80줄 이하 |
| 반환·금지 호출·인터페이스 규칙 | `tools/run.sh rules` | RT1~RT4, B1~B3, I1~I5 위반 0건 |
| 컴파일 | `-Wall -Wextra -Werror` | 경고 0건 |
| 포맷, 주석 | clang-format, Doxygen, HITL 주석 검사 | 차이·경고 0건 |

설계상 순환 복잡도 예상: `select_rule` 5, `validate_integer_range` 약 5, `arbiter_decide` 약 6, `input_guard_update` 약 8, `run_evaluation` 약 8. 모두 10 이하다. 매개변수는 5개인 `is_item_acceptable`(FN-005)을 제외하고 4개 이하다. FN-005는 항목 규칙, 입력 원본, 스냅샷 수용 여부, 상한, 출력의 값 하나씩이라 구조체로 묶으면 오히려 읽기 어려워 예외로 두며, 근거를 여기에 남긴다.

## 7. 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | MOD-001, MOD-002, MOD-003, MOD-004, MIF-001, MIF-002, MIF-003, MIF-004, FN-001, FN-002, FN-003, FN-004, FN-005, FN-006, FN-007, FN-008, FN-009, FN-010, FN-011, FN-012, FN-013, FN-014, FN-015, FN-016, FN-017, FN-018, FN-019, FN-020, FN-021, FN-022, FN-023, FN-024, FN-025, FN-026 | 이터레이션 1 최초 작성 (단계 리뷰는 사용자 지시로 생략) | 없음 |
| 0.2 | 2026-11-27 | FN-004, FN-005, FN-006, FN-007, FN-008, FN-009, FN-010, FN-014, FN-018, FN-020, FN-021, FN-024, FN-027, FN-028 | 구현 전 테스트 가능성 점검: MC/DC로 독립 효과를 보일 수 없는 중복 조건(범위, NaN) 제거, 항상 성공하는 상태 반환 체인과 도달 불가 default 제거, 매개변수 검사 분리(FN-027, 028) | 없음 |
