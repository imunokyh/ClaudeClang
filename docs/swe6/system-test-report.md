# SW 시스템 테스트(자격 시험) 결과서

- 대상 SW: 전자식 차일드락 제어 SW   버전: 0.2 (이터레이션 1~2)   일자: 2026-11-27
- 시험 명세: [test-cases.json](test-cases.json), 엑셀 [test-spec.xlsx](test-spec.xlsx) (`build_workbook.py` 검증 산출물)
- 시험 환경: PC/SIL. Docker 이미지 `claudeclangbuild`(Clang 23.1.6). 시험 코드 `tests/system/`(공개 API만 사용하는 블랙박스), `tools/run.sh system`
- 범위: 이터레이션 1~2의 SW 요구사항 SWR 38건과 안전 요구사항 SAF 5건(테스트 베이시스 43건). HIL, 실차, 타깃 ECU 시험은 범위 밖(OEM-SWR-001 1.3절)
- 면책: 교육용 가상 OEM 입력에 대한 시험이며 준수·인증·ASIL 달성을 주장하지 않는다.

## 1. 요약

| 항목 | 기준 | 결과 | 판정 |
|---|---|---|---|
| 요구사항 커버리지 | 100% | 베이시스 43/43 (100.0%), 양방향 추적성 오류 0 | 통과 |
| 시험 케이스 | — | 52건(기능 45, 비기능 7), 제안 케이스 3건(FTC-008, FTC-043, FTC-044) | — |
| 시스템 테스트 성공률 | 100% | 실행 48건(FTC 45건, NTC 3건) 중 48건 통과 | 통과 |
| 정적 검증 TC | — | NTC-004~NTC-007 (빌드, 규칙) 통과 | 통과 |

`tools/run.sh system` 판정 줄: `베이시스 커버리지 43/43 (100.0%), TC 52건, 오류 0, 경고 0` / `판정: 통과 (시스템 테스트 성공률 100%)`

## 2. 케이스별 결과

| TC | 수행 방법 | 결과 |
|---|---|---|
| FTC-001 ~ FTC-045 (45건) | `tests/system` 시험 `SystemFunctional.Ftc001…Ftc045` | 모두 통과 |
| NTC-001 (충돌 해제 응답 시간) | `SystemNonFunctional.Ntc001CrashReleaseLatency` | 통과 |
| NTC-002 (STALE 검출 시간) | `SystemNonFunctional.Ntc002StaleDetectionTime` (첫 DEGRADED 평가 310 ms) | 통과 |
| NTC-003 (평가 실행 시간) | `SystemNonFunctional.Ntc003EvaluationTime` (1,000회 최댓값 10 ms 이하) | 통과 |
| NTC-004 (C11, Clang, 경고 0) | `tools/run.sh build` | 통과 |
| NTC-005 (동적 메모리, 재귀 금지) | `tools/run.sh rules` B1, B2 | 통과 |
| NTC-006 (void 금지, 단일 return) | `tools/run.sh rules` RT1~RT3 | 통과 |
| NTC-007 (시계·난수 금지) | `tools/run.sh rules` B3 | 통과 |

## 3. 미검증 범위와 잔여 문제

- **범위 밖(OEM 문서 1.3절)**: HIL, 실차, 타깃 ECU, 실제 시계·스케줄러 연동. 실행 시간(NTC-003)은 컨테이너(x86-64) 측정이다.
- **이터레이션 3~4 대상(미구현·미검증)**: OEM-FR-001의 일반 해제 허용 조건(SWR-004), OEM-FR-004(상태 조회·표시), OEM-NFR-001(결정론), OEM-NFR-002(이력). 이 결과서는 이터레이션 1~2 범위(SWR 38건, SAF 5건)만 다룬다.
- **OEM 확인이 필요한 가정(시험 기대값에 영향)**: 평가 주기 10 ms(질의 Q-01), 상태 복귀 즉시성(Q-14), STALE 충돌 입력의 해제 유지 여부(Q-17, FTC-008), 우선순위(Q-21, FTC-044), override 의미(Q-22, FTC-036~038), 주행 중 해제 명령(Q-04, FTC-043).
- **미해결 결함**: 없음.
- **Web 결과**: Web 시뮬레이터 제외(승인된 권고안)이므로 해당 없음.

## 4. 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | 없음 | 이터레이션 1 최초 작성 (단계 리뷰는 사용자 지시로 생략) | 없음 |
| 0.2 | 2026-11-27 | 없음 | 이터레이션 2 결과 추가(TC 52건, 베이시스 43/43, 실행 48건) | 없음 |
