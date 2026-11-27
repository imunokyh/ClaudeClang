# SW 통합 테스트 명세

- 대상 SW: 전자식 차일드락 제어 SW   버전: 0.1 (이터레이션 1)   일자: 2026-11-27
- 입력: docs/swe2/architecture.md (v0.1) 3절 인터페이스, 7절 동적 동작
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
| IT-008 | IF-001, IF-003 | lock_ctrl_integration_test: IntegrationFlow.CommandDoesNotChangeOutputs | 운전자 명령 검사 연동 | 동치 클래스 | 유효, 무효, 없음 | ACCEPTED/REJECTED/NONE, 출력 불변 |
| IT-009 | IF-002 | lock_ctrl_integration_test: IntegrationFlow.PendingDoesNotRelease | PENDING 처리 | 규칙 조합 | PENDING, +sensor_fault | 유지, 이유 우선순위 |
| IT-010 | IF-001, IF-002 | lock_ctrl_integration_test: IntegrationFlow.CrashNotAppliedWhenStaleOrInvalid | STALE·INVALID 충돌 입력 | 오류 주입 | STALE 경과, NaN | 해제를 새로 적용하지 않음 |
| IT-011 | IF-003 | lock_ctrl_integration_test: IntegrationFlow.InitialAndFirstEvaluation | 초기 결과와 첫 평가 | 초기 상태 | 초기화 직후 | 초기 결과, 첫 평가 STALE |
| IT-012 | IF-004 | lock_ctrl_integration_test: IntegrationTypes.ContractValues | 공유 타입 계약 | 계약 값 확인 | 타입 상수 | 열거 번호·항목 수 일치 |
| IT-013 | IF-003 | lock_ctrl_integration_test: IntegrationPerformance.EvaluationTimeWithinCycle | 실행 시간 상한 | 반복 측정 | 혼합 입력 1,000회 | 최댓값 10 ms 이하 |

## 2. 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | IT-001, IT-002, IT-003, IT-004, IT-005, IT-006, IT-007, IT-008, IT-009, IT-010, IT-011, IT-012, IT-013 | 이터레이션 1 최초 작성 (단계 리뷰는 사용자 지시로 생략) | 없음 |
