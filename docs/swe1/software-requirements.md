# SW 요구사항 명세

- 대상 SW: 전자식 차일드락 제어 SW (후석 좌, 우)   버전: 0.5 (이터레이션 1)   일자: 2026-11-27
- 입력 문서: OEM-SWR-001 OEM SW 요구사항 사양서, Rev 1.0, 베이스라인 BL-OEM-1.0 (`OEM/OEM-SWR-001_OEM SW 요구사항 사양서.docx`)
- ISO 25010 적용 판: 2023
- 안전 요구사항: [safety-requirements.md](safety-requirements.md)
- 번호 체계: OEM 추적표(OEM-SWR-001 9절)가 하위 요구사항 번호를 SWR-001부터 SWR-021까지로 정했으므로 `SWR-nnn`을 쓰고 `분류` 열로 기능/품질/제한/제약을 구분한다. 이터레이션 1은 OEM 번호 중 007, 008, 013, 021을 쓰고(OEM 007, 008에 해당하는 PENDING 처리는 036으로 분리) 나머지는 새로 매겼다. 번호는 재사용하지 않는다. 범위 표기(`SWR-021~025` 등)는 표 안팎 어디에도 쓰지 않는다.
- 면책: 교육용 가상 OEM 입력을 SW 요구로 옮긴 문서이며 준수·인증·적합성·ASIL 달성을 주장하지 않는다.
- 언어: OEM이 개발 언어를 C(Clang)로 확정했다(2026-11-27, 사용자 전달). 이전의 Python 3.12 PC/SIL 가정은 폐기되었다.

## 0. 범위와 용어

### 0.1 이터레이션 1 범위 대응

| OEM 항목 | 이터레이션 1에서의 처리 |
|---|---|
| OEM-SR-001 | 포함 (충돌 CONFIRMED 해제, PENDING 처리) |
| OEM-SR-003 | 포함 (유효성, freshness) |
| OEM-SR-004 | 포함 (sensor_fault) |
| OEM-IF-001, 002, 003, 007, 008, 009 | 포함: 입력 검사와 freshness(SWR-022, SWR-013). 판정에 쓰는 것은 충돌(002)과 sensor_fault(009)이고 나머지 입력은 검사만 한다 |
| OEM-IF-004 | 포함: 운전자 명령의 검사와 거절(SWR-033). 명령의 적용은 이터레이션 3 |
| OEM-IF-005 | 포함: 출력 lock_left, lock_right의 값 정의(SWR-007). 적용 feedback은 OEM이 범위 밖으로 정함 |
| OEM-IF-006 | 이터레이션 3 (OEM-FR-004). 이터레이션 1은 state, priority_reason, reason_code, input_validity 값을 만들어 두기만 하고 표시 형식·직렬화는 다루지 않는다 |
| OEM-SR-002, FR-001~007, NFR-001~002 | 이터레이션 2~4. 다만 초기 상태(SWR-025), OEM-NFR-001 중 시계 입력(SWR-032)과 난수 금지(SWR-047)는 이터레이션 1이 선행한다 |

### 0.2 용어

| 용어 | 정의 |
|---|---|
| 평가 | 입력 now_ms(평가 시각)와 선택 입력 둘(새 스냅샷, 새 운전자 명령)을 받아 SWR-044의 순서로 수행하는 한 번의 호출. 10 ms 주기로 호출한다(가정 A-01) |
| 스냅샷 | 차량 입력(OEM-IF-001, 002, 003, 007, 008, 009)과 timestamp_ms를 묶은 입력 한 건 |
| 스냅샷 시각 | 스냅샷의 timestamp_ms |
| 받아들인 스냅샷 | 5.1절의 timestamp_ms 검사를 통과한 스냅샷. 일부 입력 항목이 INVALID여도 받아들인 것이다(역행 판정의 기준은 받아들인 스냅샷 중 가장 최근 것의 timestamp_ms) |
| 마지막 수신 시각 | 받아들인 스냅샷 중 가장 최근 것의 스냅샷 시각. 받아들인 스냅샷이 아직 없으면 없음 |
| 입력 항목 | 5.1절 표에서 now_ms와 timestamp_ms를 뺀 모든 필드 각각(vehicle_speed_kph, gear, source_timestamp_ms, crash_status, sensor_fault 등). input_validity는 입력 항목마다 낸다 |
| 갱신 | 받아들인 스냅샷에서 어떤 입력 항목의 값이 5.1절 검사를 통과한 것. 그때의 값을 그 입력 항목의 보관 값이라 한다 |
| 입력 상태 | 평가마다 입력 항목별로 정하는 값(5.5절 전이표). INVALID: 그 입력 항목의 최근 수신이 5.1절 검사에 실패했거나 스냅샷이 거절되어(SWR-046) 그 뒤 유효한 갱신이 없는 경우. STALE: INVALID가 아니고, 받아들인 스냅샷이 아직 없거나 now_ms − 마지막 수신 시각이 200 ms를 초과한 경우(모든 입력 항목에 공통). VALID: 그 밖. 우선순위는 INVALID > STALE > VALID. INVALID는 그 입력 항목이 다시 유효한 값으로 갱신될 때(원인 해소)에만 풀리며, 새 스냅샷이 없다는 이유로 풀리지 않는다 |
| DEGRADED 조건 | 입력 항목 중 입력 상태가 INVALID 또는 STALE인 것이 하나라도 있는 것 |
| FAULT 조건 | sensor_fault의 입력 상태가 VALID이고 판정 값이 TRUE인 것 |
| 판정 값 | 입력 상태가 VALID인 입력 항목의 보관 값 |
| 직전 확정 출력 | 직전에 받아들여진(SWR-040에서 거절되지 않은) 평가가 확정해 낸 lock_left, lock_right. 초기값은 초기 출력 지정값이며 기본 RELEASE(SWR-025, SWR-039) |
| 유지(hold) | 출력을 직전 확정 출력과 같게 정하는 것 |
| 새 명령 | 충돌 해제를 제외한, 직전 확정 출력을 바꿀 수 있는 이후 이터레이션의 판정(운전자 명령, 자동 잠금 등) |
| 결정 이유 | 출력 결정에서 처음 성립한 규칙의 이유. reason_code와 priority_reason은 같은 값을 낸다(OEM-IF-006) |

## 1. 기능 요구사항

| ID | 분류 | 요구사항 | 입력·출력 (단위, 범위, 분해능) | 검증 기준 | 출처 | 안전 연결 |
|---|---|---|---|---|---|---|
| SWR-007 | 기능 | SW는 crash_status의 입력 상태가 VALID이고 판정 값이 CONFIRMED이면 lock_left와 lock_right를 RELEASE로 확정하고 결정 이유를 CRASH_CONFIRMED로 한다(SWR-023 규칙 1). | 입력 crash_status {NONE, PENDING, CONFIRMED} / 출력 lock_left, lock_right {LOCK, RELEASE} | 직전 확정 출력을 LOCK으로 설정(SWR-039)하고 CONFIRMED 스냅샷을 평가하면 두 출력이 RELEASE. sensor_fault=TRUE이거나 다른 입력 항목이 INVALID여도 RELEASE | OEM-SR-001, OEM-IF-002, OEM-IF-005, 가정 A-04, 질의 Q-17 | SAF-001 |
| SWR-008 | 기능 | SW는 crash_status가 입력 상태 VALID로 CONFIRMED인 상태로 유지되는 동안 매 평가마다 SWR-007을 적용해 두 출력을 RELEASE로 확정한다. | 입력 crash_status / 출력 lock_left, lock_right | CONFIRMED를 담은 스냅샷을 10 ms 간격으로 100번 평가하면 매번 두 출력이 RELEASE. 스냅샷이 200 ms를 넘게 오지 않아 모든 입력이 STALE이 되면 RELEASE를 적용하지 않고 SWR-024가 적용된다 | OEM-SR-001 | SAF-001 |
| SWR-036 | 기능 | SW는 crash_status의 입력 상태가 VALID이고 판정 값이 PENDING이면 이를 출력 결정에 쓰지 않고, 더 높은 순위의 규칙이 없을 때 결정 이유를 CRASH_PENDING으로 한다(SWR-023 규칙 6). | 입력 crash_status / 출력 lock_left, lock_right, reason_code | 직전 확정 출력을 LOCK으로 설정하고 PENDING 스냅샷을 평가하면 출력이 LOCK 그대로이고 reason_code=CRASH_PENDING. PENDING과 sensor_fault=TRUE가 같이 있으면 reason_code=SENSOR_FAULT | OEM-SR-001, 가정 A-10 | SAF-001 |
| SWR-013 | 기능 | SW는 받아들인 스냅샷이 아직 없거나 now_ms − 마지막 수신 시각이 200 ms를 초과하면, INVALID가 아닌 모든 입력 항목의 input_validity를 STALE로 표시한다. 이때 DEGRADED 조건이 성립한다. | 입력 now_ms, 마지막 수신 시각 (ms) / 출력 input_validity | now_ms − 마지막 수신 시각 = 200 ms인 평가는 VALID, 210 ms인 평가는 STALE. 초기화 후 스냅샷 없이 평가하면 모든 입력 항목이 STALE, state=DEGRADED, reason_code=INPUT_STALE, 출력은 초기 출력 그대로 | OEM-SR-003, 가정 A-07 | SAF-003 |
| SWR-021 | 기능 | SW는 sensor_fault의 입력 상태가 VALID이고 판정 값이 TRUE이면 warning_code를 SENSOR_FAULT로 내고 결정 이유를 SENSOR_FAULT로 하며 새 명령을 적용하지 않고 직전 확정 출력을 유지한다(SWR-023 규칙 2). 그 밖에는 warning_code를 NONE으로 낸다. 충돌 해제(규칙 1)가 성립하면 출력은 규칙 1이 정한다. | 입력 sensor_fault boolean / 출력 lock_left, lock_right, warning_code | 직전 확정 출력을 LOCK으로 설정하고 sensor_fault=TRUE를 평가하면 출력 LOCK 그대로, warning_code=SENSOR_FAULT, reason_code=SENSOR_FAULT. 같은 평가에 충돌 CONFIRMED(VALID)이면 출력은 RELEASE이고 warning_code=SENSOR_FAULT는 유지. sensor_fault=FALSE 평가에서, 또는 sensor_fault 입력 상태가 INVALID·STALE인 평가에서 warning_code=NONE | OEM-SR-004, OEM-IF-009, 가정 A-03 | SAF-004 |
| SWR-022 | 기능 | SW는 받아들인 스냅샷의 모든 차량 입력(timestamp_ms 제외)을 5.1절 규칙으로 검사하고, 누락, 형식 오류, 범위 오류, 미정 enum 값인 입력을 INVALID로 정해 input_validity에 표시하며, 그 값을 판정 값으로 쓰지 않고 갱신으로 치지 않는다(보관 값 불변). | 5.1절 입력 검사 규칙 / 출력 input_validity {VALID, INVALID, STALE} | 항목별 경계값 직전·직후 주입: 범위 안 VALID, 범위 밖 INVALID. INVALID 평가 후 보관 값이 평가 전과 같다. INVALID가 된 입력은 이후 스냅샷이 없는 평가에서도 INVALID이고, 유효한 값이 갱신된 평가에서 풀린다 | OEM-SR-003, OEM-IF-001, OEM-IF-002, OEM-IF-003, OEM-IF-007, OEM-IF-008, OEM-IF-009 | SAF-005 |
| SWR-034 | 기능 | SW는 차량 입력 중 입력 상태가 INVALID인 것이 하나라도 있고 규칙 1, 2가 성립하지 않으면 새 명령을 적용하지 않고 직전 확정 출력을 유지하며 결정 이유를 INPUT_INVALID로 한다(SWR-023 규칙 3). 이때 DEGRADED 조건이 성립한다. | 입력 input_validity / 출력 lock_left, lock_right, reason_code | 직전 확정 출력 LOCK에서 입력 하나를 범위 밖으로 주입하면 출력 LOCK 그대로, reason_code=INPUT_INVALID, 그 입력의 input_validity=INVALID, state=DEGRADED. 그 뒤 스냅샷 없이 평가해도 계속 INPUT_INVALID, 유효한 값을 갱신한 평가에서 INVALID가 풀린다 | OEM-SR-003, 가정 A-04 | SAF-005 |
| SWR-024 | 기능 | SW는 차량 입력 중 입력 상태가 STALE인 것이 하나라도 있고 규칙 1~3이 성립하지 않으면 새 명령을 적용하지 않고 직전 확정 출력을 유지하며 결정 이유를 INPUT_STALE로 한다(SWR-023 규칙 4). | 입력 전체 / 출력 lock_left, lock_right, reason_code | 직전 확정 출력 LOCK에서 스냅샷을 200 ms 넘게 받지 못해 모든 입력이 STALE이면 출력 LOCK 그대로, reason_code=INPUT_STALE. crash_status 입력이 STALE이면 CONFIRMED 보관 값이 있어도 RELEASE를 적용하지 않는다 | OEM-SR-003, 가정 A-04 | SAF-003 |
| SWR-035 | 기능 | SW는 평가마다 state를 다시 정한다: FAULT 조건이 성립하면 FAULT, 아니면 DEGRADED 조건이 성립하면 DEGRADED, 아니면 NORMAL. 원인이 없어진 평가(예: INVALID 입력 항목에 유효한 값이 갱신된 평가)에서 NORMAL이 되며 원인이 남아 있는 한 state를 유지한다. sensor_fault의 입력 상태가 INVALID나 STALE이면 FAULT가 아니라 DEGRADED다. | 입력 input_validity, sensor_fault / 출력 state {NORMAL, DEGRADED, FAULT, OFF} | 새 스냅샷이 도착해 STALE이 풀린 평가에서 state=NORMAL. FAULT 조건과 DEGRADED 조건(다른 입력 항목 INVALID) 동시이면 FAULT. sensor_fault 입력 항목이 INVALID이면 DEGRADED. 이터레이션 1에서 OFF는 쓰지 않는다(OEM-FR-007, 이터레이션 2) | OEM-SR-003, OEM-SR-004, 가정 A-08, 가정 A-15, 질의 Q-20 | SAF-003, SAF-004, SAF-005 |
| SWR-023 | 기능 | SW는 출력과 결정 이유를 5.4절 결정 규칙표의 규칙 중 처음 성립한 것으로 정한다. 규칙 2~7은 직전 확정 출력을 바꾸지 않는다(이터레이션 1). | 입력 전체 / 출력 lock_left, lock_right, reason_code, priority_reason | 규칙 조합 시험: 1과 2 동시 → RELEASE, CRASH_CONFIRMED. 2와 3 동시 → 유지, SENSOR_FAULT. 3과 4 동시 → INPUT_INVALID. 6만 성립 → CRASH_PENDING. 어느 규칙도 성립하지 않음 → NONE | OEM-SR-001, OEM-SR-004, 가정 A-03 | SAF-001, SAF-004 |
| SWR-044 | 기능 | SW는 평가마다 ① now_ms 검사(SWR-040, 거절되면 ② 이후를 수행하지 않는다) ② 스냅샷(timestamp_ms 포함)·운전자 명령 검사와 입력 상태 계산 ③ state 결정(SWR-035) ④ 출력·결정 이유 결정(SWR-023) 순서로 수행하고, ②의 결과를 같은 평가의 ③, ④에 쓴다. | 입력 now_ms, 스냅샷, 운전자 명령 / 출력 전체 | 범위 밖 입력을 담은 스냅샷을 평가한 그 평가의 출력에서 INVALID가 이미 반영되어 있다(다음 평가가 아니라) | OEM-SR-003, OEM-SR-001 | SAF-003, SAF-005 |
| SWR-025 | 기능 | SW는 초기화 직후(첫 평가 전)의 출력을 다음 값으로 둔다: lock_left, lock_right와 직전 확정 출력은 초기 출력 지정값(SWR-039, 지정하지 않으면 RELEASE), state는 NORMAL(평가 전 초기값이며 첫 평가에서 SWR-035가 정한다), input_validity는 입력마다 STALE, reason_code와 priority_reason과 warning_code는 NONE, command_status는 NONE. | 출력 전체 | 지정 없이 초기화한 직후 조회: RELEASE, RELEASE, NORMAL, 모든 input_validity=STALE, NONE, NONE, NONE, NONE | OEM-FR-007(초기 해제 상태에 한함), 가정 A-05 | SAF-004 |
| SWR-039 | 기능 | SW는 초기화할 때 초기 출력(lock_left, lock_right의 초기값이자 직전 확정 출력)을 호출자가 지정할 수 있어야 한다(시험 지원. 이터레이션 1에는 LOCK을 내는 요구가 없어 유지 동작 검증에 필요하다). | 입력 초기 출력 {LOCK, RELEASE} / 출력 lock_left, lock_right | 초기 출력을 LOCK으로 지정해 초기화한 직후 조회하면 lock_left=LOCK, lock_right=LOCK | 가정 A-13 | |
| SWR-040 | 기능 | SW는 now_ms가 5.1절 검사에 실패하면(직전에 받아들여진 평가의 now_ms보다 작은 경우 포함) 그 평가를 거절하고 반환 상태 STATUS_ERR_TIME을 반환한다. 받아들여진 평가는 STATUS_OK를 반환한다(SWR-046의 스냅샷 거절 평가 포함). 거절된 평가는 없었던 것으로 본다: 함께 들어온 스냅샷과 운전자 명령은 버리고, 5.3절의 모든 출력, 입력 상태, 보관 값, 갱신 시각, 직전 확정 출력, "직전 평가"의 now_ms를 바꾸지 않는다. | 입력 now_ms (ms) / 출력 반환 상태 {STATUS_OK, STATUS_ERR_TIME} | 평가 100 ms → 50 ms(거절) → 80 ms 순서로 호출하면 두 번째는 STATUS_ERR_TIME을 반환하고 출력 전체가 첫 평가 직후 값과 같다. 두 번째에 함께 넣은 스냅샷·명령은 반영되지 않는다. 세 번째도 직전에 받아들여진 평가(첫 번째)의 now_ms 100 ms보다 작으므로 거절된다 | OEM-SR-003, 가정 A-02 | SAF-003 |
| SWR-046 | 기능 | SW는 스냅샷의 timestamp_ms가 5.1절 검사에 실패하면(누락, 형식 오류, 범위 오류, 직전에 받아들인 스냅샷보다 작음, now_ms보다 큼) 그 스냅샷을 거절하고, 그 스냅샷에 담긴 모든 입력 항목의 입력 상태를 INVALID로 하며 보관 값을 바꾸지 않는다. 반환 상태는 STATUS_OK다. | 입력 timestamp_ms / 출력 input_validity | timestamp_ms 역행, 미래 시각(now_ms 초과), 누락을 각각 주입하면 모든 차량 입력의 input_validity=INVALID, state=DEGRADED, 출력이 직전 확정 출력과 같고, 다음 받아들인 스냅샷에서 유효한 값으로 풀린다 | OEM-SR-003, OEM-IF-001, 가정 A-11 | SAF-005 |
| SWR-033 | 기능 | SW는 운전자 명령(OEM-IF-004)의 side, action, source 중 누락이거나 5.2절의 값 집합 밖인 것이 있으면 그 명령을 거절하고 command_status를 REJECTED로 하며, 유효하면 command_status를 ACCEPTED로 한다(이터레이션 1에서는 어느 쪽도 출력과 state를 바꾸지 않는다). 명령이 없는 평가는 command_status를 NONE으로 한다. | 입력 side {left, right, all}, action {lock, unlock}, source {physical_button, avn, voice, mobile_app} / 출력 command_status {NONE, ACCEPTED, REJECTED} | 필드별 누락, 미등록 값, 형식 오류 주입 → REJECTED. 모두 유효 → ACCEPTED. 명령 없음 → NONE. 세 경우 모두 출력·state 불변 | OEM-IF-004 | |

## 2. 비기능 요구사항

### 2.1 품질 요구사항 (ISO 25010:2023)

| ID | 분류 | 특성 | 부특성 | 요구사항 | 측정 기준 (수치, 방법, 조건) | 출처 | 안전 연결 |
|---|---|---|---|---|---|---|---|
| SWR-026 | 품질 | Performance efficiency | Time behaviour | crash_status가 입력 상태 VALID로 CONFIRMED인 스냅샷(스냅샷 시각 T)이 전달된 평가에서 두 출력을 RELEASE로 확정해, T부터 출력 확정까지 300 ms 이내여야 한다. | 호출 주기 10 ms(A-01)에서 T 이후 처음 호출되는 평가(now_ms ≤ T+10 ms)의 출력이 RELEASE이고, 그 평가의 now_ms − T ≤ 300 ms. 고정 시계 입력으로 측정 | OEM-SR-001, 가정 A-01 | SAF-001 |
| SWR-027 | 품질 | Reliability | Fault tolerance | 마지막 수신 시각부터 200 ms를 초과한 시점부터 100 ms 이내에 모든 입력 항목이 STALE이 되어 DEGRADED 조건이 성립해야 한다. | SWR-013 검증과 같은 시험: 마지막 수신 시각 T에 대해 now_ms = T+210 ms의 평가에서 STALE(DEGRADED)이며 이는 T+300 ms 이내다 | OEM-SR-003, 가정 A-01 | SAF-003 |

> ISO 25010:2023의 나머지 특성은 이터레이션 1 범위에서 새 품질 요구가 없다. 해당 없음과 사유:
> - Functional suitability: 기능 요구사항(1절)이 다루고 OEM이 별도 품질 수치를 주지 않았다.
> - Compatibility: OEM-IF 인터페이스 계약은 기능 요구(5절 입력 검사)로 다루며 별도 공존성 요구가 없다.
> - Interaction capability: 사용자 표시·조작 품질은 OEM-IF-006과 OEM-FR-004(이터레이션 3)에서 다룬다. 이터레이션 1의 운전자 명령(SWR-033)은 입력 검사 기능이며 사용성 요구가 아니다.
> - Security: 보안 요구가 OEM 문서에 없다. 개인정보 금지(OEM-NFR-002)는 이터레이션 4.
> - Maintainability: OEM이 측정 가능한 유지보수성 요구를 주지 않았다. 코드 품질 기준(순환 복잡도·라인수)은 개발 규칙이며 제품 요구가 아니다.
> - Flexibility: 이식성·설치성 요구가 OEM 문서에 없다.
> - Safety: 안전 요구사항 문서(SAF)가 다룬다.

### 2.2 제한

| ID | 분류 | 요구사항 | 한계값 | 검증 기준 | 출처 |
|---|---|---|---|---|---|
| SWR-031 | 제한 | 평가 한 번의 실행 시간은 평가 주기 이하여야 한다. | 10 ms 이하 | 컨테이너(x86-64)에서 모든 분기를 지나는 입력 벡터로 평가를 1,000회 실행해 측정한 최댓값이 10 ms 이하 | 가정 A-01 |

### 2.3 제약사항

| ID | 분류 | 요구사항 | 근거 | 검증 기준 | 출처 |
|---|---|---|---|---|---|
| SWR-028 | 제약 | 제품 코드는 C11로 작성한다. | OEM이 개발 언어를 C로 확정(2026-11-27) | 컴파일 옵션 `-std=c11`, `tools/run.sh build` | 개발 규칙(CLAUDE.md), 질의 Q-11 |
| SWR-041 | 제약 | 제품 코드는 Clang(최신 안정 버전, 컨테이너 이미지 고정)으로 빌드한다. | OEM이 Clang 확정, 개발 환경(CLAUDE.md 4절) | `tools/run.sh build`가 Clang으로 성공 | 개발 규칙(CLAUDE.md), 질의 Q-11 |
| SWR-037 | 제약 | 제품 코드는 컴파일러 경고 없이 빌드되어야 한다. | 개발 규칙(`-Wall -Wextra -Werror`) | `tools/run.sh build` 경고 0건 | 개발 규칙(CLAUDE.md) |
| SWR-029 | 제약 | 제품 코드는 동적 메모리 할당 함수(malloc, calloc, realloc, free 등)를 호출하지 않는다. | 개발 규칙, 실행 시간 상한(SWR-031) | `tools/run.sh rules`의 B1 검사 | 개발 규칙(CLAUDE.md) |
| SWR-042 | 제약 | 제품 코드는 재귀 호출(직접, 상호)을 하지 않는다. | 개발 규칙, 실행 시간 상한(SWR-031) | `tools/run.sh rules`의 B2 검사 | 개발 규칙(CLAUDE.md) |
| SWR-038 | 제약 | 제품 코드의 모든 함수는 void 대신 상태 값을 반환한다. | 개발 규칙(CLAUDE.md 코딩 규칙) | `tools/run.sh rules` (RT1) | 개발 규칙(CLAUDE.md) |
| SWR-043 | 제약 | 제품 코드의 모든 함수는 return을 함수 끝에 한 번만 둔다. | 개발 규칙(CLAUDE.md 코딩 규칙) | `tools/run.sh rules` (RT2, RT3) | 개발 규칙(CLAUDE.md) |
| SWR-032 | 제약 | 제품 코드는 시스템 시계 함수(time, clock, clock_gettime 등)를 호출하지 않는다. 시간은 호출자가 now_ms, timestamp_ms 입력으로 넘긴다(SWR-044). | 재현 가능한 시험(고정 시계), 가정 A-02 | `tools/run.sh rules`의 B3 검사 | 가정 A-02, OEM-NFR-001(고정 시계) |
| SWR-047 | 제약 | 제품 코드는 난수 함수(rand, random 등)를 호출하지 않는다. | 결정론(OEM-NFR-001, 이터레이션 4) | `tools/run.sh rules`의 B3 검사 | OEM-NFR-001 |

## 3. 질의 사항 (OEM-A 확인 필요)

| 번호 | 대상 | 내용 | 상태 | 답변 |
|---|---|---|---|---|
| Q-01 | 평가 주기 | OEM 문서에 "다음 평가주기"의 길이가 없다 | 열림 | 가정 A-01 적용 |
| Q-02 | 우선순위 | 충돌, sensor_fault, 화재·과온·성인(해제), ISOFIX(잠금), 접근위험, ignition off 동시 성립 시 우선순위가 없다 | 열림 | 가정 A-03 적용(이터레이션 1은 규칙 1~4, 6, 7) |
| Q-03 | 충돌 PENDING | 추적표에는 PENDING 처리가 있으나 본문 요구가 없다 | 열림 | 가정 A-10 적용 |
| Q-04 | 일반 해제 허용 | 주행 중 운전자 해제 허용 조건이 없다(추적표에만 있음) | 열림(이터레이션 3) | |
| Q-05 | SR-003의 시간 관계와 DEGRADED 동작 | "200 ms 초과 미갱신 시 100 ms 이내 DEGRADED"의 기준 시점, DEGRADED 때의 출력 동작이 없다 | 열림 | 가정 A-04, 요구 SWR-027 |
| Q-06 | 번호 결번 | OEM 추적표에 SWR 019번이 없다(의도된 결번인지 확인) | 열림 | 019는 사용하지 않고 예약 |
| Q-07 | 시계와 timestamp | OEM 인터페이스는 초 단위인데 C 인터페이스의 시간 표현, 고정 시계 재생 방법, 시각이 거꾸로 가거나 없을 때의 동작, 상한 도달(49.7일) 후 동작이 없다 | 열림 | 가정 A-02, A-11 적용 |
| Q-08 | 수치 타입 | 차속 float 여부, 분해능이 없다 | 열림 | 가정 A-06 적용 |
| Q-09 | 필수 입력 범위 | SR-003의 "필수 입력"이 어느 입력인지, OEM-IF-001 외에는 개별 타임스탬프가 없다 | 열림 | 가정 A-07 적용 |
| Q-10 | 안전 목표·FTTI | HARA, 안전 목표, FTTI 문서가 제공되지 않았다 | 열림 | OEM 응답 시간 기준을 FTTI로 대용 |
| Q-11 | 언어·실행 환경 | 개발 언어: OEM이 C(Clang)로 확정(2026-11-27). Web 시뮬레이터(OEM-IF-006 표시, HTTP 500)는 이터레이션 4 이후 필요 여부를 확인 | 언어는 닫힘, Web은 열림 | Web 시뮬레이터는 범위 제외(승인된 권고안 A2) |
| Q-12 | ignition off와 충돌 | 동시 성립 시 우선순위(이터레이션 2) | 열림(이터레이션 2) | |
| Q-13 | 법규 | 별표 14 등 적용성은 차종 확정 전이라 범위 제외 | 열림 | 범위 제외 |
| Q-14 | 상태 복귀 | DEGRADED, FAULT에서 NORMAL로 돌아오는 조건(즉시 복귀인지, 안정화 시간이 필요한지)이 없다 | 열림 | 가정 A-08 적용 |
| Q-15 | 인도 증거 | OEM 8절의 Use Case, UML, SBOM, 결함 연결의 형식과 인도 시점 | 열림 | 가정 A-09 적용 |
| Q-16 | 충돌과 sensor_fault 동시 | OEM-SR-001(최우선)과 SR-004(직전 출력 유지)가 동시에 성립할 때의 출력 | 열림 | 가정 A-03: 충돌 해제가 우선하고 warning_code는 SENSOR_FAULT |
| Q-17 | STALE인 충돌 입력 | crash_status가 STALE(200 ms 초과 미갱신)일 때 마지막 CONFIRMED 값으로 해제를 계속해야 하는지. 이 문서는 해제하지 않고 유지한다(SWR-024). OEM-SR-001 해석에 영향이 있어 확인이 필요하다 | 열림 | 가정 A-04 적용 |
| Q-18 | source_timestamp_ms의 용도 | OEM-IF-001의 source_timestamp_ms로 차속 freshness를 잴지 여부 | 열림 | 가정 A-11: 검사만 하고 freshness에는 쓰지 않음 |
| Q-20 | FAULT와 DEGRADED 동시 | OEM-SR-003 수용기준은 "DEGRADED 상태"인데 sensor_fault 입력이 유효한 TRUE이고 다른 입력 항목이 INVALID일 때 FAULT를 보고하면 문구와 다르다 | 열림 | 가정 A-15 적용 |
| Q-19 | 입력의 C 표현 | 누락·형식 오류를 C에서 어떻게 표현할지 OEM 계약이 없다 | 열림 | 가정 A-12 적용 |

## 4. 가정

| 번호 | 대상 | 가정 | 근거 |
|---|---|---|---|
| A-01 | 평가 주기 | 평가 주기는 10 ms이다 | 300 ms, 100 ms, 200 ms 기준을 충분히 만족하는 값. Q-01 확정 필요 |
| A-02 | 시간 | now_ms, timestamp_ms, source_timestamp_ms는 32비트 부호 없는 정수 ms로 받는다. OEM 인터페이스의 단위 s(source_timestamp_s, VehicleSnapshot.timestamp_s)는 호출자가 ms 정수로 환산해 넘기며 소수 처리는 호출자 책임이다. now_ms는 비감소여야 한다(SWR-040). 호출자는 연속 가동 약 49.7일(상한 4294967295 ms)에 이르기 전에 재초기화한다 | 재현 가능한 시험, 부동소수 형식 판정 문제 회피, OEM-IF-001의 단위 s |
| A-03 | 우선순위 | 충돌 CONFIRMED 해제 > sensor_fault 유지 > INVALID 유지 > STALE 유지 > 이후 이터레이션 규칙(화재·과온·성인 해제 > 접근위험 잠금 > ISOFIX 잠금 > 자동 잠금 > 운전자 명령). ignition off는 이터레이션 2에서 정한다. 충돌 해제가 성립해도 state와 warning_code는 SWR-035, SWR-021에 따라 보고한다 | 사용자 승인된 제안(2026-11-27) |
| A-04 | STALE·INVALID | 충돌 해제(규칙 1)를 제외하고, 입력 항목 중 STALE 또는 INVALID가 있으면 직전 확정 출력을 유지한다. 충돌 해제는 crash_status 입력 항목이 VALID일 때만 적용하며 다른 입력 항목이 INVALID여도 적용한다 | sensor_fault와 같은 안전 상태(유지) |
| A-05 | 초기 상태 | 초기 출력은 RELEASE이고 첫 스냅샷 전에는 모든 필수 입력이 STALE이다 | OEM-FR-007의 "초기 해제 상태" |
| A-06 | 수치 | 차속은 0.1 km/h 단위의 부호 없는 정수로 표현한다(0~3000). 외부 값의 변환은 상세 설계에서 정한다 | 범위 0.0~300.0 km/h, MC/DC 시험 용이 |
| A-07 | freshness 대상 | OEM-IF-001, 002, 003, 007, 008, 009의 차량 입력을 필수 입력으로 본다. 입력은 스냅샷 단위로 도착하며 freshness는 스냅샷 단위(마지막 수신 시각)로 판정한다 | OEM-IF-004의 "요청 시각은 VehicleSnapshot.timestamp_s 사용" |
| A-08 | 상태 복귀 | state, reason_code, warning_code는 평가마다 현재 입력 상태에서 다시 정하며 별도의 안정화 시간 없이 원인이 없어진 평가에서 바로 NORMAL, NONE이 된다. INVALID 입력은 유효한 값이 갱신될 때 풀린다(새 스냅샷이 없다는 이유로 풀리지 않는다) | 가장 단순한 결정. Q-14로 확인 |
| A-09 | 인도 증거 | Use Case와 UML은 설계 문서에 Mermaid로 포함하고, SBOM은 이터레이션 4에서 만들며(제품 코드는 외부 라이브러리 없음, GoogleTest는 시험 전용), 결함은 GitHub 이슈로 추적해 커밋·PR에 연결한다. G1 검토기록·추적은 `docs/reviews/`와 `tools/run.sh trace`, G2 정적분석 계획은 상세 설계서의 절, G4 미검증 범위·잔여문제는 이터레이션 4 검증 결과서로 한다(iter-1.md 표와 같음) | OEM 8절 G1~G4 증거 |
| A-10 | PENDING | 충돌 PENDING은 출력을 바꾸지 않고 이유코드만 남긴다. 이후 이터레이션의 새 명령 규칙을 막지 않는다 | SR-001은 CONFIRMED만 규정 |
| A-11 | timestamp | 스냅샷 timestamp_ms 검사 실패 시 스냅샷 전체 거절은 SWR-046이 요구한다. source_timestamp_ms는 검사(0~4294967295, 스냅샷 timestamp_ms 이하)만 하고 freshness에는 쓰지 않는다 | OEM-IF-001, Q-18 |
| A-12 | C 표현 | 외부 입력은 필드마다 "존재" 플래그와 원시 값(정수 또는 실수)으로 전달한다. 형식 오류는 원시 값이 정의된 열거 집합 밖이거나 실수(차속)가 NaN인 경우다. 시간 값은 정수로 받으므로 형식 오류가 없고 범위 오류만 있다 | 가장 단순한 C 표현. Q-19로 확인 |
| A-14 | 스냅샷 도착 주기 | 스냅샷은 보통 평가 주기(10 ms)마다 오지만 요구는 이에 의존하지 않는다. 스냅샷이 없는 평가는 입력 상태 정의(0.2절)의 STALE·INVALID 규칙을 따른다 | 도착 주기가 OEM 문서에 없음(Q-01과 함께 확인) |
| A-15 | FAULT와 DEGRADED 동시 | FAULT 조건과 DEGRADED 조건이 동시에 성립하면(sensor_fault가 유효한 TRUE이고 다른 입력 항목이 INVALID) state는 FAULT다. STALE일 때는 sensor_fault 입력도 STALE이라 FAULT 조건이 성립하지 않으므로 FAULT와 STALE의 동시는 없다. OEM-SR-003의 "DEGRADED 상태"는 DEGRADED 조건 성립으로 읽고, FAULT가 더 심각한 상태로서 이를 포함한다고 본다 | 둘 다 직전 확정 출력을 유지하며 FAULT가 더 심각. Q-20으로 확인 |
| A-13 | 시험 지원 | 초기 출력 지정(SWR-039)은 시험 목적이며 제품 동작에서는 기본값 RELEASE를 쓴다 | LOCK을 내는 요구가 이터레이션 2부터라 이터레이션 1 시험에 필요 |

## 5. 입력 검사 규칙 (SWR-022, SWR-033, SWR-040 상세)

모든 입력은 먼저 "존재(누락 아님)"를 검사하고, 다음에 형식, 다음에 범위를 본다. 하나라도 어긋나면 INVALID다(운전자 명령은 REJECTED).

### 5.1 차량 입력과 평가 시각

| 입력 | OEM 인터페이스 | 유효 조건 (존재 + 형식 + 범위) | 내부 표현 |
|---|---|---|---|
| now_ms (평가 시각) | — | 존재, 정수 ms, 0~4294967295, 직전에 받아들여진 평가의 now_ms 이상 (SWR-040) | 32비트 부호 없는 정수 ms |
| timestamp_ms (스냅샷) | OEM-IF-004의 요청 시각 | 존재, 정수 ms, 0~4294967295, 직전에 받아들인 스냅샷 이상, now_ms 이하 (SWR-046) | 32비트 부호 없는 정수 ms |
| vehicle_speed_kph | OEM-IF-001 | 존재, 숫자(NaN 아님), 0.0~300.0 | 0.1 km/h 단위 정수 0~3000 |
| gear | OEM-IF-001 | 존재, P, N, D, R 중 하나 | 열거형 |
| source_timestamp_ms | OEM-IF-001 | 존재, 정수 ms, 0~4294967295, 스냅샷 timestamp_ms 이하 | 32비트 부호 없는 정수 ms |
| crash_status | OEM-IF-002 | 존재, NONE, PENDING, CONFIRMED 중 하나 | 열거형 |
| rear_left_approach_risk, rear_right_approach_risk | OEM-IF-003 | 존재, boolean | bool |
| fire_detected, overtemperature_detected, adult_present | OEM-IF-007 | 존재, boolean | bool |
| isofix_left, isofix_right | OEM-IF-008 | 존재, boolean | bool |
| ignition_on, sensor_fault | OEM-IF-009 | 존재, boolean | bool |

### 5.2 운전자 명령 (SWR-033)

| 입력 | 유효 조건 |
|---|---|
| side | 존재, left, right, all 중 하나 |
| action | 존재, lock, unlock 중 하나 |
| source | 존재, physical_button, avn, voice, mobile_app 중 하나 |

### 5.3 출력 값 집합

| 출력 | 값 |
|---|---|
| lock_left, lock_right | LOCK, RELEASE |
| state | NORMAL, DEGRADED, FAULT, OFF |
| input_validity (입력 항목별) | VALID, INVALID, STALE |
| 반환 상태 | STATUS_OK, STATUS_ERR_TIME |
| reason_code, priority_reason (같은 값) | NONE, CRASH_CONFIRMED, SENSOR_FAULT, INPUT_INVALID, INPUT_STALE, CRASH_PENDING |
| warning_code | NONE, SENSOR_FAULT |
| command_status | NONE, ACCEPTED, REJECTED |

### 5.4 결정 규칙표 (SWR-023)

| 규칙 | 조건 | 출력 | 결정 이유 | 근거 요구 |
|---|---|---|---|---|
| 1 | crash_status 입력 상태 VALID, 판정 값 CONFIRMED | lock_left, lock_right = RELEASE | CRASH_CONFIRMED | SWR-007, SWR-008 |
| 2 | FAULT 조건 | 유지 | SENSOR_FAULT | SWR-021 |
| 3 | 차량 입력 중 INVALID가 있음 | 유지 | INPUT_INVALID | SWR-034 |
| 4 | 차량 입력 중 STALE이 있음 | 유지 | INPUT_STALE | SWR-024 |
| 5 | (이후 이터레이션) 새 명령 규칙 | (이후 정의) | (이후 정의) | 이터레이션 2, 3 |
| 6 | crash_status 입력 상태 VALID, 판정 값 PENDING | 유지 | CRASH_PENDING | SWR-036 |
| 7 | 위 어느 규칙도 성립하지 않음 | 유지 | NONE | SWR-023 |

### 5.5 입력 상태 전이표 (SWR-013, SWR-022, SWR-046)

입력 항목마다 저장 상태(OK 또는 INVALID, 초기값 OK)를 두고, 표시값 input_validity는 저장 상태와 스냅샷의 신선도로 정한다.

| 사건 | 저장 상태의 변화 | 근거 |
|---|---|---|
| 받아들인 스냅샷에서 그 입력 항목의 값이 5.1절 검사를 통과 | OK (보관 값 갱신) | SWR-022 |
| 받아들인 스냅샷에서 그 입력 항목의 값이 5.1절 검사에 실패 | INVALID (보관 값 불변) | SWR-022 |
| 스냅샷 거절(timestamp_ms 검사 실패) | 모든 입력 항목 INVALID (보관 값 불변) | SWR-046 |
| 스냅샷 없음, 또는 now_ms 거절(SWR-040) | 변화 없음 | SWR-040 |

| 표시값 input_validity | 조건 |
|---|---|
| INVALID | 저장 상태가 INVALID |
| STALE | 저장 상태가 OK이고, 받아들인 스냅샷이 아직 없거나 now_ms − 마지막 수신 시각 > 200 ms |
| VALID | 그 밖 |

## 6. 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | SWR-007, SWR-008, SWR-013, SWR-021, SWR-022, SWR-023, SWR-024, SWR-025, SWR-026, SWR-027, SWR-028, SWR-029, SWR-030, SWR-031, SWR-032 | 이터레이션 1 최초 작성 | 없음 |
| 0.2 | 2026-11-27 | SWR-007, SWR-008, SWR-013, SWR-021, SWR-022, SWR-023, SWR-024, SWR-025, SWR-028, SWR-029, SWR-031, SWR-032, SWR-033, SWR-034, SWR-035, SWR-036, SWR-037, SWR-038 | SWE.1 리뷰 1회 부적합 21건 조치. 030번은 삭제(OEM-NFR-001에 할당된 016번과 겹침, 번호 재사용 안 함) | 안전 요구사항 문서, iter-1.md |
| 0.3 | 2026-11-27 | SWR-007, SWR-008, SWR-013, SWR-021, SWR-022, SWR-023, SWR-024, SWR-025, SWR-026, SWR-027, SWR-028, SWR-029, SWR-032, SWR-033, SWR-034, SWR-035, SWR-036, SWR-038, SWR-039, SWR-040, SWR-041, SWR-042, SWR-043, SWR-044, SWR-045 | SWE.1 리뷰 2회 부적합 20건 조치: 입력 상태 정의와 결정 규칙표(SWR-023)로 INVALID·STALE·state·이유 코드의 모순 제거, 복합 요구 분리(028→041, 029→042, 038→043, 023→044, 025→045), 시험 지원(039), now_ms 검사(040) 추가, 언어 확정 반영 | 안전 요구사항 문서, iter-1.md |
| 0.4 | 2026-11-27 | SWR-007, SWR-013, SWR-021, SWR-022, SWR-023, SWR-025, SWR-028, SWR-032, SWR-034, SWR-035, SWR-039, SWR-040, SWR-041, SWR-044, SWR-046, SWR-047 | SWE.1 리뷰 3회 부적합 15건 조치: INVALID가 유효 갱신까지 이어지도록 입력 상태 재정의, timestamp 거절 요구(046), now_ms 거절 흐름, 초기 출력 전체 정의, 결정 규칙표(5.4), FAULT·DEGRADED 동시 가정(A-15), SWR-032 분리(047), 용어(DEGRADED 조건, 스냅샷 시각, 받아들인 스냅샷) 정의 | 안전 요구사항 문서, iter-1.md |
| 0.5 | 2026-11-27 | SWR-007, SWR-008, SWR-013, SWR-022, SWR-024, SWR-026, SWR-027, SWR-032, SWR-035, SWR-040, SWR-046 | SWE.1 리뷰 4회 부적합 11건 조치와 단순화: freshness를 스냅샷 단위(마지막 수신 시각)로, 시간 입력을 정수 ms로, 입력 상태 전이표(5.5절) 도입, 반환 상태 값 정의, 도달 불가능한 조합(입력별 STALE) 제거. SWR-045는 SWR-013에 통합하고 번호는 재사용하지 않는다 | 안전 요구사항 문서, iter-1.md |
