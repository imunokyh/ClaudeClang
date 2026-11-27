# SW 통합 테스트 명세

- 대상 SW: 전자식 차일드락 제어 SW   버전: 0.3 (이터레이션 1~3)   일자: 2026-11-27
- 입력: docs/swe2/architecture.md (v0.3) 3절 인터페이스, 7절 동적 동작
- 테스트 코드: `tests/integration/` (GoogleTest). 실제 컴포넌트(`lock_ctrl`, `input_guard`, `arbiter`, `common`)를 모두 연결하고, 호출자(차량 신호 어댑터) 역할의 시험 하네스가 IF-003을 호출한다.
- 기준: 테스트 성공률 100%, 함수 커버리지 100% (모든 `src/` 파일)
- 스텁: 없음. 이 SW 안에는 HW·OS 호출이 없어 아키텍처 6절의 경계는 호출자(시험 하네스)와 고정 시계(now_ms 인자)뿐이다.
- 면책: 교육용 가상 OEM 입력에 대한 시험이며 준수·인증·ASIL 달성을 주장하지 않는다.

## 1. 테스트 케이스

| ID | 대상 인터페이스 | 테스트 (파일: 이름) | 목적 | 기법 | 입력 | 예상 출력 |
|---|---|---|---|---|---|---|
| IT-001 | IF-001, IF-002, IF-003 | lock_ctrl_integration_test: IntegrationFlow.NormalInputKeepsOutputs | 정상 연동 | 인터페이스 호출 순서 | 유효 스냅샷 5주기 | 출력 유지, NORMAL, 모두 VALID |
| IT-002 | IF-001, IF-002, IF-003 | lock_ctrl_integration_test: IntegrationFlow.CrashConfirmedReleasesAndHolds | 충돌 해제와 유지 | 시나리오, 시간 경계 | CONFIRMED 100주기 | 매 주기 RELEASE, 300 ms 이내 |
| IT-003 | IF-002, IF-003 | lock_ctrl_integration_test: IntegrationFlow.SensorFaultAndCrashPriority | sensor_fault와 충돌 우선순위 | 규칙 조합 | fault → +충돌 → fault 해제 | 유지/FAULT, RELEASE/FAULT, RELEASE/NORMAL |
| IT-004 | IF-001, IF-003 | lock_ctrl_integration_test: IntegrationFlow.StaleAndRecovery | STALE 전이와 복귀 | 경계값, 상태 전이 | 스냅샷 중단 200/210 ms | VALID→STALE→복귀 |
| IT-005 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationFlow.InvalidInputPersistsAndRecovers | INVALID 지속과 해소 | 상태 전이 | 범위 밖 → 스냅샷 없음 → 정상 | INVALID 유지 후 NORMAL |
| IT-006 | IF-001, IF-003 | lock_ctrl_integration_test: IntegrationFlow.RejectedSnapshotTimeFlow | 스냅샷 시각 거절 | 오류 주입 | 역행, 미래 스냅샷 | 모든 입력 INVALID 후 복귀 |
| IT-007 | IF-003 | lock_ctrl_integration_test: IntegrationFlow.TimeRegressionKeepsResult | 평가 시각 역행 | 오류 주입 | 100 → 50 → 80 → 110 ms | ERR_TIME 두 번, 결과 유지 |
| IT-008 | IF-001, IF-003 | lock_ctrl_integration_test: IntegrationFlow.CommandAppliedOnlyWhenValid | 운전자 명령 검사와 적용 연동 | 동치 클래스 | 유효, 무효, 없음 | ACCEPTED는 적용, REJECTED는 유지, NONE |
| IT-009 | IF-002 | lock_ctrl_integration_test: IntegrationFlow.PendingDoesNotRelease | PENDING 처리 | 규칙 조합 | PENDING, +sensor_fault | 유지, 이유 우선순위 |
| IT-010 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationFlow.CrashNotAppliedWhenStaleOrInvalid | STALE·INVALID 충돌 입력 | 오류 주입 | STALE 경과, NaN | 해제를 새로 적용하지 않음 |
| IT-011 | IF-003 | lock_ctrl_integration_test: IntegrationFlow.InitialAndFirstEvaluation | 초기 결과와 첫 평가 | 초기 상태 | 초기화 직후 | 초기 결과, 첫 평가 STALE |
| IT-012 | IF-004 | lock_ctrl_integration_test: IntegrationTypes.ContractValues | 공유 타입 계약 | 계약 값 확인 | 타입 상수 | 열거 번호·항목 수 일치 |
| IT-013 | IF-003 | lock_ctrl_integration_test: IntegrationPerformance.EvaluationTimeWithinCycle | 실행 시간 상한 | 반복 측정 | 혼합 입력 1,000회 | 최댓값 10 ms 이하 |
| IT-014 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.IgnitionOffThenOn | ignition off와 복귀 | 상태 전이 | ignition_on 0 → 1 | RELEASE, OFF → 유지, NORMAL |
| IT-015 | IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.ForcedReleaseBeatsApproachRisk | 해제 입력이 접근위험보다 우선 | 규칙 조합 | 성인 + 좌 접근위험 | RELEASE, 이후 접근위험 적용 |
| IT-016 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.ApproachRiskSuppressesOnlyThatDoor | 접근위험 문별 억제 | 시나리오 | 좌 접근위험에서 좌·우 unlock | 좌 유지, 우 해제 |
| IT-017 | IF-001, IF-002, IF-003 | lock_ctrl_integration_test: IntegrationDoorRules.OverrideFlow | override와 10초 창 | 시간 경계, 상태 전이 | 10,000 / 10,010 ms 경과 | override / 새 첫 억제 |
| IT-018 | IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.IsofixAutoLockAndCommandPriority | 문 규칙 우선순위의 지속 | 시나리오 | ISOFIX, 주행, 정지 후 명령 | LOCK 유지, 명령 해제 |
| IT-019 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.CommandDroppedUnderGlobalRule | 전역 규칙 아래 명령 버림 | 오류 주입 | sensor_fault + 명령 | 유지, 이어지지 않음 |
| IT-020 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.SourceDoesNotMatter | source 무관 | 동치 클래스 | source 0~3 | 같은 결과 |
| IT-021 | IF-003, IF-005 | lock_ctrl_integration_test: IntegrationStatusView.ReflectsEvaluationResult | 조회와 직렬화 연동 | 시나리오 | 접근위험, override, STALE | 문자열에 결과가 반영 |
| IT-022 | IF-005 | lock_ctrl_integration_test: IntegrationStatusView.SmallBufferFails | 버퍼 부족 | 오류 주입 | 크기 10 버퍼 | STATUS_ERR_BUFFER, 빈 문자열 |
| IT-023 | IF-003, IF-006 | lock_ctrl_integration_test: IntegrationDecisionLog.RecordsDecisionChanges | 결정 변화 이력 | 시나리오 | fault, 반복, 충돌, 복귀 | 변화만 3건, ID와 필드 일치 |
| IT-024 | IF-003, IF-006 | lock_ctrl_integration_test: IntegrationDecisionLog.KeepsLatestHundredAndResets | 100건 보존과 초기화 | 경계값 | 결정 변화 101번, 초기화 | 최신 100건, 초기화 후 0건 |
| IT-025 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationDoorRules.GeneralReleaseAllowedOnlyWithoutBlockers | 일반 해제 허용 조건 | 결정 테이블 | 차단 조건 5조합 | 차단 시 유지, 없으면 해제 |

## 2. 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | IT-001, IT-002, IT-003, IT-004, IT-005, IT-006, IT-007, IT-008, IT-009, IT-010, IT-011, IT-012, IT-013 | 이터레이션 1 최초 작성 (단계 리뷰는 사용자 지시로 생략) | 없음 |
| 0.2 | 2026-11-27 | IT-008, IT-014, IT-015, IT-016, IT-017, IT-018, IT-019, IT-020 | 이터레이션 2: 운전자 명령 적용(IT-008 수정), 문 규칙·전역 해제·override 통합 시험 추가 | 아키텍처 설계서 |
| 0.3 | 2026-11-27 | IT-021, IT-022, IT-023, IT-024, IT-025 | 이터레이션 3: 직렬화, 이력, 일반 해제 허용 조건 통합 시험 추가 | 아키텍처 설계서 |
