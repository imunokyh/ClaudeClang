# 이터레이션 1 — 입력 검증, freshness, 충돌 해제, sensor_fault

- 브랜치: `feature/iter-1-input-guard`
- 입력 문서: OEM-SWR-001 Rev 1.0 (BL-OEM-1.0), 사용자 승인된 개발 계획(2026-11-27)
- 대상 OEM 요구사항: OEM-SR-001, OEM-SR-003, OEM-SR-004, OEM-IF-001~005, OEM-IF-007~009 (입력 검사와 출력 값 정의). OEM-IF-006(표시)은 이터 3
- 이번 이터레이션에서 하지 않는 것: OEM-SR-002, OEM-FR-001~006, OEM-NFR-001~002 (이터 2~4). 단 OEM-FR-007의 초기 해제 상태(SWR-025)와 OEM-NFR-001 중 고정 시계 입력(SWR-032)·난수 금지(SWR-047)는 선행한다. OEM-IF-004는 검사·거절만(적용은 이터 3), OEM-IF-006은 이터 3.
- 검증 수단 준비 완료: 금지 호출 검사(B1~B3, SWR-029, SWR-032, SWR-042, SWR-047)는 chore PR #2로 `tools/run.sh rules`에 병합되었다

## 인도 증거 처리 (OEM-SWR-001 8절, 가정 A-09)

| 증거 | 처리 |
|---|---|
| Use Case, UML | 설계 문서에 Mermaid로 포함 (이터 1부터) |
| SBOM | 이터레이션 4(G3)에서 생성. 제품 코드는 외부 라이브러리 없음, GoogleTest는 시험 전용 |
| 결함 연결 | 결함은 GitHub 이슈로 추적하고 커밋·PR에 연결 |
| G1 검토기록, 양방향 추적 | `docs/reviews/`의 단계 리뷰 기록, `tools/run.sh trace` 결과 |
| G2 정적분석 계획 | 상세 설계서에 "정적 분석 계획" 절(Cppcheck 룰, Lizard 기준, 금지 호출 검사)을 둔다 |
| G4 Web 결과 | Web 시뮬레이터 제외(승인된 권고안)이므로 해당 없음 |
| G4 미검증 범위, 잔여문제 | 이터레이션 4의 SW 검증 결과서에 기록 |

## 승인된 결정 (사용자 승인 2026-11-27, "추천하는 방법대로 진행")

| ID | 결정 |
|---|---|
| A1 | 구현 언어는 C11/Clang. OEM이 개발 언어를 C(Clang)로 확정했다(2026-11-27). 문서의 "Python 3.12 PC/SIL"은 폐기 |
| A2 | Web 시뮬레이터는 범위 제외. 상태 조회 API(구조체)까지 제공한다 |
| A3 | 하위 요구사항 ID는 OEM 추적표를 따라 `SWR-nnn`, 기능 안전은 `SAF-nnn` |
| A4 | OEM SR은 `safety-requirements.md`(SAF), 파생 SW 요구사항은 `software-requirements.md`(SWR) |
| A5 | 산출물에 "준수·인증 주장 없음" 면책을 넣는다. MC/DC 100%는 자체 기준이다 |
| B | 질의 사항은 제안값을 가정으로 두고 OEM 질의 목록으로 관리한다 (요구사항 명세 3·4절) |
| C | 법규는 이번 범위 제외. PR #1(환경 구성)은 병합 완료 |

## 단계 진행

| 단계 | 산출물 | 상태 |
|---|---|---|
| 1 요구사항 분석 | `docs/swe1/` | 리뷰 5회차 대기 (v0.5) |
| 2 아키텍처 | `docs/swe2/` | 대기 |
| 3 상세 설계 | `docs/swe3/` | 대기 |
| 4 구현 | `src/`, `tests/tdd/` | 대기 |
| 5 단위 테스트 | `tests/unit/`, `docs/swe4/` | 대기 |
| 6 통합 테스트 | `tests/integration/`, `docs/swe5/` | 대기 |
| 7 시스템 테스트 | `docs/swe6/` | 대기 |
