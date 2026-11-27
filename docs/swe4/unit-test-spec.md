# SW 단위 테스트 명세

- 대상 SW: 전자식 차일드락 제어 SW   버전: 0.1 (이터레이션 1)   일자: 2026-11-27
- 입력: docs/swe3/detailed-design.md (v0.2)
- 테스트 코드: `tests/unit/` (GoogleTest). TDD 테스트(`tests/tdd/`)와 분리되어 있으며 단위 테스트는 상세 설계의 결정표에서 새로 도출했다.
- 기준: 테스트 성공률 100%, MC/DC 커버리지 100% (모든 `src/` 파일)
- 기법: 결정표 기반(MC/DC 벡터), 동치 클래스 분할, 경계값 분석, 스텁(`lock_ctrl`의 하위 컴포넌트만)
- 비공개(static) 함수는 직접 호출할 수 없으므로 공개 함수를 통해 시험한다(해당 FN 행에 연결).
- 면책: 교육용 가상 OEM 입력에 대한 시험이며 준수·인증·ASIL 달성을 주장하지 않는다.

## 1. 테스트 케이스

| ID | 대상 함수 | 테스트 (파일: 이름) | 목적 | 기법 | 입력 | 예상 출력 |
|---|---|---|---|---|---|---|
| UT-001 | FN-001 | input_guard_test: InputGuardInit.NullGuardIsRejected | NULL 저장소 거절 | 결정표 R1 | guard NULL | STATUS_ERR_PARAM |
| UT-002 | FN-001, FN-011, FN-012 | input_guard_test: InputGuardInit.InitialUpdateIsStale | 초기 상태 확인 | 결정표 | 초기화 후 스냅샷 없음 | 모든 항목 STALE, 값 0 |
| UT-003 | FN-002 | input_guard_test: InputGuardUpdate.NullArgumentsAreRejected | guard, out NULL 거절 | 결정표 R1 | NULL 각각 | STATUS_ERR_PARAM |
| UT-004 | FN-002, FN-005, FN-006, FN-013 | input_guard_test: InputGuardUpdate.ValidSnapshotIsNormalized | 유효 스냅샷 정규화 | 대표값 | 차속 123.4 등 | VALID, 1234 등 |
| UT-005 | FN-009 | input_guard_test: InputGuardUpdate.SpeedBoundaryValues | 차속 경계 | 경계값 분석 | 0.0, 300.0, -0.1, 300.1, NaN, 누락 | 경계 안 VALID, 밖 INVALID |
| UT-006 | FN-007 | input_guard_test: InputGuardUpdate.BooleanItemRules | boolean 규칙 | 동치 클래스, MC/DC | 0, 1, 2, 0.5, NaN, 누락 | 0,1만 VALID |
| UT-007 | FN-008 | input_guard_test: InputGuardUpdate.EnumItemRules | 열거 규칙 | 경계값, 동치 클래스 | 기어 -1~4, 1.5, 충돌 3 | 범위 안 정수만 VALID |
| UT-008 | FN-006, FN-010 | input_guard_test: InputGuardUpdate.SourceTimestampRules | 시각 항목 규칙 | 경계값 | 100, 0, 101, -1, 99.5 | 0~스냅샷 시각 정수만 VALID |
| UT-009 | FN-002, FN-004, FN-005 | input_guard_test: InputGuardUpdate.RejectedSnapshotMarksAllInvalid | 스냅샷 거절 | 결정표 R2~R4 | 시각 누락, 역행, 미래 | 모든 항목 INVALID, 보관 값 불변 |
| UT-010 | FN-011, FN-012 | input_guard_test: InputGuardUpdate.StaleBoundary | STALE 경계 | 경계값 | 경과 200, 201, 210 ms | VALID, STALE, STALE |
| UT-011 | FN-002, FN-011 | input_guard_test: InputGuardUpdate.InvalidPersistsUntilValidUpdate | INVALID 지속과 해소 | 상태 전이 | 무효 → 스냅샷 없음 → 유효 | INVALID 유지 후 VALID |
| UT-012 | FN-002, FN-004 | input_guard_test: InputGuardUpdate.EqualTimestampIsAccepted | 같은 시각 허용 | 경계값 | 같은 시각 두 번 | VALID |
| UT-013 | FN-003, FN-008 | input_guard_test: InputGuardCommand.FieldRules | 명령 필드 검사 | 결정표 R1~R4 | 필드별 무효·누락 | ACCEPTED/REJECTED |
| UT-014 | FN-003 | input_guard_test: InputGuardCommand.NullArgumentsAreRejected | NULL 거절 | 결정표 R0 | NULL 각각 | STATUS_ERR_PARAM, 출력 불변 |
| UT-020 | FN-014 | arbiter_test: ArbiterParams.NullArgumentsAreRejected | NULL 거절 | 결정표 R2~R4 | NULL 각각 | STATUS_ERR_PARAM |
| UT-021 | FN-014, FN-027 | arbiter_test: ArbiterParams.OutputRangeValidation | 직전 출력 범위 | 결정표 FN-027 | 열거 범위 밖 | STATUS_ERR_PARAM |
| UT-022 | FN-014, FN-017, FN-018 | arbiter_test: ArbiterRules.CrashConfirmedReleases | 규칙 1 | 결정표 R1 | 충돌 CONFIRMED | 두 출력 RELEASE |
| UT-023 | FN-017, FN-018, FN-019 | arbiter_test: ArbiterRules.SensorFaultHolds | 규칙 2 | 결정표 R2 | sensor_fault 1 | 유지, FAULT, 경고 |
| UT-024 | FN-016, FN-017, FN-018 | arbiter_test: ArbiterRules.InvalidInputHolds | 규칙 3 | 결정표 R3 | INVALID 항목 | 유지, DEGRADED |
| UT-025 | FN-017, FN-018 | arbiter_test: ArbiterRules.StaleInputHolds | 규칙 4 | 결정표 R4 | 모두 STALE | 유지, DEGRADED |
| UT-026 | FN-017, FN-018 | arbiter_test: ArbiterRules.PendingAndNone | 규칙 5, 6 | 결정표 R5, R6 | PENDING, NONE | 유지, 이유 구분 |
| UT-027 | FN-017 | arbiter_test: ArbiterRules.PriorityOrder | 규칙 우선순위 | 결정표 조합 | 규칙 동시 성립 4조합 | 높은 규칙의 이유 |
| UT-028 | FN-015 | arbiter_test: ArbiterRules.CrashRequiresValidInput | 충돌 해제의 입력 상태 조건 | MC/DC | 충돌 STALE/INVALID/VALID | VALID일 때만 RELEASE |
| UT-029 | FN-015, FN-019 | arbiter_test: ArbiterState.FaultCondition | FAULT 조건 | MC/DC | sensor_fault 상태 조합 | VALID TRUE일 때만 FAULT |
| UT-030 | FN-019 | arbiter_test: ArbiterState.DegradedCondition | DEGRADED 조건 | 결정표 R2, R3 | STALE만, INVALID만, 없음 | DEGRADED, DEGRADED, NORMAL |
| UT-040 | FN-020 | lock_ctrl_test: LockCtrlInit.NullCtrlIsRejected | NULL 거절 | 결정표 R1 | ctrl NULL | STATUS_ERR_PARAM |
| UT-041 | FN-020, FN-028 | lock_ctrl_test: LockCtrlInit.InvalidInitialOutputsAreRejected | 초기 출력 범위 | 결정표 R2, FN-028 | 범위 밖 3조합 | STATUS_ERR_PARAM, 불변 |
| UT-042 | FN-020, FN-026 | lock_ctrl_test: LockCtrlInit.DefaultInitialResult | 기본 초기 결과 | 결정표 R3 | config NULL | SWR-025의 초기값 |
| UT-043 | FN-020 | lock_ctrl_test: LockCtrlInit.ConfiguredInitialOutputs | 지정 초기 출력 | 결정표 R4 | (LOCK, LOCK) | 초기 결과·직전 출력에 반영 |
| UT-044 | FN-020 | lock_ctrl_test: LockCtrlInit.GuardInitFailureIsPropagated | 하위 오류 전파 | 결정표 R5, 스텁 | init 실패 스텁 | STATUS_ERR_PARAM |
| UT-045 | FN-021 | lock_ctrl_test: LockCtrlEvaluate.NullArgumentsAreRejected | NULL 거절 | 결정표 R2~R4 | NULL 각각 | STATUS_ERR_PARAM |
| UT-046 | FN-021, FN-023 | lock_ctrl_test: LockCtrlEvaluate.TimeRegressionIsRejected | 시각 역행 | 경계값 | 99, 100, 101 | ERR_TIME, OK, OK |
| UT-047 | FN-023 | lock_ctrl_test: LockCtrlEvaluate.FirstEvaluationAtZero | 첫 평가 | 결정표 R1 | now 0 | STATUS_OK |
| UT-048 | FN-024, FN-025 | lock_ctrl_test: LockCtrlEvaluate.SuccessCommitsResult | 성공 경로 | 결정표 R6, 스텁 | 스텁 결정 | 결과 필드와 직전 출력 갱신 |
| UT-049 | FN-024 | lock_ctrl_test: LockCtrlEvaluate.CommandStatusFlow | 명령 흐름 | 결정표 R2, R6, 스텁 | 명령 없음/있음 | NONE / 스텁 결과 |
| UT-050 | FN-024 | lock_ctrl_test: LockCtrlEvaluate.UpdateFailureStopsPipeline | update 실패 | 결정표 R1, 스텁 | update 실패 | 오류 반환, 상태 불변 |
| UT-051 | FN-024 | lock_ctrl_test: LockCtrlEvaluate.CheckFailureStopsPipeline | 명령 검사 실패 | 결정표 R2, 스텁 | check 실패 | 오류 반환, decide 호출 없음 |
| UT-052 | FN-024 | lock_ctrl_test: LockCtrlEvaluate.DecideFailureKeepsPreviousOutputs | 결정 실패 | 결정표 R3, R4, 스텁 | decide 실패 | 오류 반환, 직전 출력 불변 |
| UT-053 | FN-022 | lock_ctrl_test: LockCtrlGetResult.NullArgumentsAreRejected | NULL 거절 | 결정표 R2, R3 | NULL 각각 | STATUS_ERR_PARAM |

## 2. 스텁

| 스텁 | 대체하는 인터페이스 | 사유 |
|---|---|---|
| `tests/unit/stubs/component_stubs.cpp` | IF-001(input_guard), IF-002(arbiter) | `lock_ctrl`의 방어 경로(하위 호출 오류)는 실제 컴포넌트로는 만들 수 없어 스텁으로 오류를 주입한다. `input_guard`, `arbiter` 자신의 시험은 실제 구현으로 한다 |

## 3. 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | UT-001, UT-002, UT-003, UT-004, UT-005, UT-006, UT-007, UT-008, UT-009, UT-010, UT-011, UT-012, UT-013, UT-014, UT-020, UT-021, UT-022, UT-023, UT-024, UT-025, UT-026, UT-027, UT-028, UT-029, UT-030, UT-040, UT-041, UT-042, UT-043, UT-044, UT-045, UT-046, UT-047, UT-048, UT-049, UT-050, UT-051, UT-052, UT-053 | 이터레이션 1 최초 작성 (단계 리뷰는 사용자 지시로 생략) | 없음 |
