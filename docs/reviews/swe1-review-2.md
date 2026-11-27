# SWE.1 리뷰 2회

- 일자: 2026-11-27
- 대상 커밋: `8d46c7e` (브랜치 `feature/iter-1-input-guard`, 작업 트리 변경 없음 확인)
- 입력(상위 산출물): `OEM/OEM-SWR-001_OEM SW 요구사항 사양서.docx` (Rev 1.0, BL-OEM-1.0). 대조는 텍스트 추출본(370줄)으로 전수 수행
- 이전 리뷰: `docs/reviews/swe1-review-1.md` (부적합 21건, 의견 3건)
- 리뷰 대상: `docs/swe1/software-requirements.md`(v0.2), `docs/swe1/safety-requirements.md`(v0.2), `docs/iterations/iter-1.md`, `.claude/skills/requirements-analysis/`(`trace_check.py`, `test_trace_check.py`, `SKILL.md`, `references/templates.md`, `references/traceability.md`), `.claude/skills/aspice-stage-review/references/checklists.md`, `CLAUDE.md`
- 적용 체크리스트: CL-REQ-01~21, CL-SAF-01~06(이번 추가분 CL-REQ-16~21, CL-SAF-06 포함), CL-TRC, CL-COM
- 리뷰어: stage-reviewer
- 판정: **불통과** (중대 5건, 경미 15건, 미확인 0건, 의견 4건)

## 리뷰한 산출물과 적용 체크리스트

| 산출물 | 경로 | 체크리스트 | 항목 수 | 적합 | 부적합 | 해당 없음 | 미확인 |
|---|---|---|---|---|---|---|---|
| SW 요구사항 명세 | `docs/swe1/software-requirements.md` | CL-REQ | 21 | 7 | 14 | 0 | 0 |
| 기능 안전 요구사항 | `docs/swe1/safety-requirements.md` | CL-SAF | 6 | 4 | 1 | 1 | 0 |
| 추적성 검사 결과 | `trace_check.py --stage swe1` | CL-TRC | 3 | 3 | 0 | 0 | 0 |
| 공통 | 위 산출물 전체, `iter-1.md`, 스킬·체크리스트·CLAUDE.md 변경 | CL-COM | 4 | 3 | 1 | 0 | 0 |

`docs/iterations/iter-1.md`와 스킬·체크리스트 변경분은 여전히 대응표에 전용 체크리스트가 없다. 범위 선언은 CL-REQ-21, 스킬·체크리스트 변경은 CL-COM과 checklists.md "체크리스트 개발·개선" 3·5항, CLAUDE.md 3절로 판정했다.

## 도구 결과

| 도구 | 명령 | 결과(판정 줄) |
|---|---|---|
| 추적성 (SWE.1) | `docker compose run --rm dev python3 .claude/skills/requirements-analysis/scripts/trace_check.py --docs docs --src src --stage swe1` | `추적성 검사 (단계 swe1): SAF 4, SWR 20` / `판정: 통과 (양방향 추적성 100%)` (exit 0). 정의 수를 문서에서 직접 세어 대조: SWR 20(기능 12, 품질 2, 제한 1, 제약 5), SAF 4 일치 |
| 추적성 (전 구간, CL-TRC-02) | `docker compose run --rm dev tools/run.sh trace` | `판정: 실패 — 오류 24건` (전부 T1: SWR/SAF가 아키텍처에 연결되지 않음. SWE.2 산출물이 없는 SWE.1 시점의 예상 결과이며 SWE.1 판정에 쓰지 않음) |
| ID 정규식 회귀 시험 | `docker compose run --rm dev python3 .claude/skills/requirements-analysis/scripts/test_trace_check.py -v` | `Ran 4 tests` / `OK` (SWR 인식, `OEM-IF-002` 무시, `SWR-099를` 인식, `SWR-0071` 무시) |
| 검증 수단 존재 확인 (CL-REQ-19) | `grep -rnE "malloc\|recurs\|clock\|time\(\|gettimeofday\|순환\|시계" tools/run.sh .claude/skills/implementation/scripts/ .claude/skills/architecture-design/scripts/` | 금지 함수·재귀(호출 순환)·시계 함수 검사 없음. 일치는 Lizard 설명과 컴포넌트·모듈 의존 순환(I4, I5)뿐 → R-SWE1-2-15 |
| 브랜치·커밋 | `git log -1 --format=%B 8d46c7e`, `git merge-base HEAD develop`, `git status --short` | 브랜치 `feature/iter-1-input-guard`, 분기 원점 `98f78be`(= `develop`). 산출물·스킬·체크리스트·CLAUDE.md 변경이 커밋 하나에 들어 있음 → R-SWE1-2-20 |

## 체크리스트 판정 (산출물별)

### docs/swe1/software-requirements.md — CL-REQ

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-REQ-01 | 적합 | `분류` 열: 기능 12(SWR-007, 008, 036, 013, 021, 022, 034, 033, 023, 024, 035, 025), 비기능 8. 종류 판별 맞음 |
| CL-REQ-02 | 적합 | 품질 2(SWR-026, 027), 제한 1(SWR-031), 제약 5(SWR-028, 037, 029, 038, 032). 체크리스트 문구 자체의 미갱신은 R-SWE1-2-18 |
| CL-REQ-03 | 적합 | 5줄 "ISO 25010 적용 판: 2023". SWR-026, 027 모두 특성·부특성 있음 |
| CL-REQ-04 | 적합 | SWR-026: 300 ms, 고정 시계, 10 ms 주기 가정. SWR-027: T+201 ms, T+300 ms 이내, 고정 시계. 수치·방법·조건 있음("도착 시점" 용어 문제는 R-SWE1-2-09) |
| CL-REQ-05 | 부적합 | 9개 특성 모두 언급. Interaction capability 미적용 사유 "이터레이션 1에는 사용자 인터페이스가 없다"가 SWR-033(운전자 명령 입력 검사)과 모순 → R-SWE1-2-07 |
| CL-REQ-06 | 적합 | T12 0건. SAF 정의는 safety-requirements.md에만 있음 |
| CL-REQ-07 | 부적합 | SWR-028, 029, 038 복수 요구 잔존, SWR-023(처리 순서 + 우선순위), SWR-025(초기값 + 첫 스냅샷 전 동작) → R-SWE1-2-13 |
| CL-REQ-08 | 부적합 | now_s 범위·내부 표현 없음, 누락·형식 오류의 C 표현 질의·가정 없음(R-SWE1-1-11 ④ 미조치) → R-SWE1-2-10. priority_reason·reason_code 값 결정 규칙 누락 → R-SWE1-2-03 |
| CL-REQ-09 | 적합 | "적절히/빠르게/가능하면" 류 없음. "곧바로"는 "다음 평가"로 수치화됨(다만 그 기준 평가가 모호한 점은 R-SWE1-2-08) |
| CL-REQ-10 | 부적합 | INVALID 표시의 지속·해제, INVALID와 STALE 동시, 현재 INVALID인데 직전 VALID 값이 fresh인 경우, timestamp_s 거절의 결과가 정의도 질의도 없음 → R-SWE1-2-01 |
| CL-REQ-11 | 부적합 | 검증 전제("이전 출력이 LOCK")가 이터레이션 1 요구로 도달 불가 → R-SWE1-2-11. SWR-023의 처리 순서 부분에 검증 기준 없음 → R-SWE1-2-13 |
| CL-REQ-12 | 적합 | OEM ID 24개(SR 4, FR 7, NFR 2, IF 9 — 추출본 143~258줄) 전수 대조: 0.1절 범위 대응표가 모두 처리(포함 또는 이터레이션 지정). 상세 불일치는 CL-REQ-21 |
| CL-REQ-13 | 부적합 | state 결정 모순(SWR-013/034/025 vs SWR-035) → R-SWE1-2-02. reason_code 모순(SWR-036 vs SWR-024, SWR-024 vs SWR-034) → R-SWE1-2-03. SWR-008 freshness 조건 누락 → R-SWE1-2-04. 갱신 시각 정의 모순(0.2절·A-07 vs A-11) → R-SWE1-2-05 |
| CL-REQ-14 | 부적합 | 가정에 기댄 요구 중 출처에 가정 표시가 없는 것 잔존(SWR-007 ← A-04, SWR-021 ← A-03/Q-16) → R-SWE1-2-16 |
| CL-REQ-15 | 부적합 | 개정 이력 범위 표기 잔존(170, 171줄), v0.2 변경 ID 누락(SWR-013, 028, 029, 031) → R-SWE1-2-17 |
| CL-REQ-16 | 부적합 | state 복귀는 SWR-035로 정의됨. 그러나 INVALID 원인의 해제 시점 미정(R-SWE1-2-01), state를 정하는 요구 간 우선 관계 모순(R-SWE1-2-02), warning_code·reason_code의 해제(복귀) 조건 없음(R-SWE1-2-03), "다음 평가" 기준 모호(R-SWE1-2-08) |
| CL-REQ-17 | 부적합 | "VALID"와 input_validity의 STALE 관계 모호(R-SWE1-2-01), "갱신 시각" 정의가 A-11과 다름(R-SWE1-2-05), "평가" 정의에 운전자 명령이 없음(R-SWE1-2-06), "도착 시점" 미정의(R-SWE1-2-09), "CONFIRMED로 유지되는 동안"이 VALID·fresh 조건과 다름(R-SWE1-2-04) |
| CL-REQ-18 | 부적합 | SWR-034가 INVALID의 기본 결과를 정했으나 INVALID 지속 기간, STALE과 동시일 때 input_validity·reason_code, timestamp_s 거절(A-11) 시 결과가 요구로 정의되지 않음 → R-SWE1-2-01 |
| CL-REQ-19 | 부적합 | SWR-029, SWR-032의 검증 수단(`tools/run.sh rules`의 금지 함수·호출 순환·시계 함수 검사)이 존재하지 않음(grep 확인) → R-SWE1-2-15 |
| CL-REQ-20 | 부적합 | OEM 8절 증거 중 정적분석 계획(G2), 미검증 범위·잔여문제·Web 결과(G4), 검토기록·양방향 추적(G1)의 처리 기록 없음 → R-SWE1-2-14 |
| CL-REQ-21 | 부적합 | iter-1.md 26줄 "OEM-IF-001~009 (입력 검증 부분)"과 27줄 "OEM-IF-006은 이터 3", 명세 0.1절 IF-005 "포함(값 정의)"이 서로 어긋남 → R-SWE1-2-12 |

### docs/swe1/safety-requirements.md — CL-SAF

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-SAF-01 | 적합 | SAF-001/003/004/005 모두 ASIL B, 안전 목표(미제공 — OEM-A 할당 입력), 안전 상태, FTTI 있음 |
| CL-SAF-02 | 적합 | 근거 OEM-SWR-001 5절. HARA 미제공은 OEM 1.2절·2절 범위 제외, Q-10. OEM ASIL B 입력 4건 중 SR-001/003/004 → SAF-001/003/005/004, SR-002 → SAF-002 예약(7줄) |
| CL-SAF-03 | 부적합 | SAF-003, 004, 005는 관측 가능한 기대값으로 고침. SAF-001 "이후 평가에서도 RELEASE를 유지한다"는 관측 구간이 여전히 없음 → R-SWE1-2-19. 검증 전제가 도달 불가 → R-SWE1-2-11 |
| CL-SAF-04 | 적합 | 수동 대조: SAF-001 ← SWR-007, 008, 036, 023, 026 / SAF-003 ← SWR-013, 024, 035, 027 / SAF-004 ← SWR-021, 023, 035, 025 / SAF-005 ← SWR-022, 034. T1은 SWE.2부터 도구 판정 |
| CL-SAF-05 | 해당 없음 | ASIL 분해 없음(4줄) |
| CL-SAF-06 | 적합 | SAF-003을 SAF-003(STALE, FTTI 100 ms)과 SAF-005(형식·범위, FTTI 1주기)로 분리. 네 SAF 각각 안전 상태·FTTI가 하나. SAF 간 state 모순은 R-SWE1-2-02 |

### 추적성 — CL-TRC

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-TRC-01 | 적합 | `--stage swe1` 오류 0건(직접 실행). 정규식 변경은 회귀 시험 4건 통과로 확인 |
| CL-TRC-02 | 적합 | 요구사항 개정(v0.2) 뒤 `--stage all`을 리뷰어가 실행. 오류 24건은 전부 T1(하위 산출물 없음)로 SWE.1 시점 예상 결과 |
| CL-TRC-03 | 적합 | 원 요구사항 출처 전수 대조: OEM ID 24개 모두 0.1절 또는 출처 열에 연결됨 |

### 공통 — CL-COM

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-COM-01 | 적합 | 지정 경로, 한글 |
| CL-COM-02 | 적합 | SWR 20, SAF 4 중복 없음(T11 0건). Q-01~16, A-01~11 수동 대조 중복 없음 |
| CL-COM-03 | 부적합 | 이전 지적 21건 중 10건 조치 불충분(아래 "이전 지적 조치 확인") |
| CL-COM-04 | 적합 | 브랜치 이름 `feature/iter-1-input-guard`, 분기 원점 `develop`(`98f78be`). 도구·설정 변경을 같은 브랜치에 넣은 것은 CLAUDE.md 3절 문제로 R-SWE1-2-20 |

## 지적 사항

| ID | 등급 | 산출물:위치 | 내용 | 근거(체크리스트 ID) | 조치(작성자 기입) |
|---|---|---|---|---|---|
| R-SWE1-2-01 | 중대 | software-requirements.md:31~34, 43, 47, 48, 49, 53, 125, 159; safety-requirements.md:13, 14 | INVALID·STALE·VALID 관계가 정의되지 않아 ASIL B 상태가 둘 이상으로 해석된다. ① 0.2절 "VALID = 5절 검사 만족"에는 시간 조건이 없고 SWR-007은 "VALID이고 fresh"를 따로 쓰는데, 5.3절 input_validity는 VALID/INVALID/STALE 중 하나다. 따라서 STALE이 된 sensor_fault=TRUE가 "VALID이고 TRUE"인지(SWR-021, 035, SAF-004 → FAULT) 아닌지(→ DEGRADED) 정해지지 않는다 ② INVALID 표시가 언제 해제되는지 없다. 새 스냅샷 없는 평가(29줄 "(있으면)")에서 직전 INVALID가 유지되는지, 그래서 SWR-034의 DEGRADED가 언제 끝나는지(SWR-035 "원인이 없어지면") 정할 수 없다 ③ 같은 입력이 INVALID(현재 스냅샷)이면서 200 ms 넘게 VALID 갱신이 없을 때 input_validity가 INVALID인지 STALE인지, reason_code·priority_reason이 무엇인지 없다 ④ 현재 스냅샷 값은 INVALID이나 직전 VALID 값이 아직 fresh일 때(예: 직전 crash_status=CONFIRMED) 그 값을 판정에 쓰는지 없다 ⑤ A-11의 timestamp_s 거절은 가정에만 있고, 그때 input_validity·state·reason_code가 무엇인지 어느 요구에도 없다(SWR-022·034는 "차량 입력"만 다루고 0.2절 스냅샷 정의상 timestamp_s는 차량 입력이 아님). 질의 사항에도 없다 | CL-REQ-10, CL-REQ-16, CL-REQ-17, CL-REQ-18 | |
| R-SWE1-2-02 | 중대 | software-requirements.md:46, 49, 53, 54; safety-requirements.md:12, 13, 14 | state를 정하는 요구가 여럿이고 서로 모순된다. SWR-013("STALE이면 state를 DEGRADED로 정하고"), SWR-034("INVALID이면 state를 DEGRADED로"), SWR-025("state는 DEGRADED가 된다")는 무조건 DEGRADED를 요구하고, SWR-035는 sensor_fault=TRUE이면 FAULT가 우선한다. sensor_fault=TRUE와 STALE(또는 INVALID)이 동시에 성립하면 SWR-013/034는 DEGRADED, SWR-035는 FAULT다(예외·참조 문구 없음). 안전 요구도 같다: SAF-003("state를 DEGRADED로 바꾸고"), SAF-005("DEGRADED로 보고") vs SAF-004(FAULT). 1회차 R-SWE1-1-05 조치(SWR-035 신설)로 새로 생긴 모순 | CL-REQ-13, CL-REQ-16 | |
| R-SWE1-2-03 | 중대 | software-requirements.md:45, 47, 49, 50, 51, 52, 160~164 | 이유·경고 출력의 결정 규칙이 모순되고 빠져 있다. ① 164줄 "reason_code는 SWR-023 우선순위가 높은 쪽"인데 SWR-023 순위에 CRASH_PENDING, COMMAND_REJECTED가 없고 INPUT_STALE과 INPUT_INVALID는 같은 순위③이다. 그래서 PENDING+STALE에서 SWR-036(CRASH_PENDING)과 SWR-024(INPUT_STALE), STALE+INVALID에서 SWR-024(INPUT_STALE)와 SWR-034(INPUT_INVALID)가 같은 입력에 다른 값을 요구한다 ② reason_code 값 CRASH_CONFIRMED, SENSOR_FAULT, NONE을 내는 요구가 없다(SWR-007·021은 reason_code를 정하지 않음) ③ priority_reason은 SWR-023 출력 열에만 있고 각 값(SENSOR_FAULT, INPUT_STALE, INPUT_INVALID, NONE)을 언제 내는지 요구 문장이 없다(검증 기준 예시 하나뿐) ④ warning_code=SENSOR_FAULT와 reason_code가 원인 해소 뒤 NONE으로 돌아가는지 유지되는지 없다(SWR-035는 state만 래치 없음을 정함) ⑤ SWR-036 "PENDING이면 출력을 바꾸지 않고"는 SWR-023 순위에 PENDING이 없어 ④순위(이후 이터레이션의 화재·과온 해제 등)를 막는지 정해지지 않았다 | CL-REQ-13, CL-REQ-08, CL-REQ-16 | |
| R-SWE1-2-04 | 중대 | software-requirements.md:44, 45; safety-requirements.md:11 | R-SWE1-1-06 조치 불충분. SWR-007은 "VALID이고 fresh하며 CONFIRMED"로 고쳤지만 SWR-008("CONFIRMED로 유지되는 동안 매 평가마다 RELEASE")과 SAF-001 둘째 문장("CONFIRMED가 유지되는 동안 매 평가에서 RELEASE")은 VALID·fresh 조건이 없다. crash_status가 STALE인 채 마지막 값이 CONFIRMED일 때 SWR-008은 RELEASE, SWR-024는 "CONFIRMED를 받았어도 RELEASE를 적용하지 않고 직전 확정 출력 유지"를 요구해 같은 모순이 남는다. SWR-036(PENDING)도 VALID·fresh 조건이 없다 | CL-REQ-13, CL-REQ-17 | |
| R-SWE1-2-05 | 중대 | software-requirements.md:32, 121, 125 | 갱신 시각 정의가 서로 다르다. 0.2절 "갱신: 그 스냅샷의 timestamp_s가 그 입력의 마지막 갱신 시각", A-07 "갱신 시각은 스냅샷의 timestamp_s"인데 A-11은 "차속의 갱신 시각은 source_timestamp_s로 잡는다". SAF-003(ASIL B)·SWR-013·SWR-027의 STALE 판정 기준 시각이 입력에 따라 무엇인지 확정할 수 없다(SWR-024 검증 "차속 입력만 STALE"도 어느 정의를 전제하는지 불명). gear와 source_timestamp_s 자체의 갱신 시각도 정해지지 않음 | CL-REQ-17, CL-REQ-13 | |
| R-SWE1-2-06 | 경미 | software-requirements.md:29, 30, 50 | SWR-033의 입력(운전자 명령)이 0.2절 "평가"(now_s와 스냅샷)·"스냅샷" 정의에 없어 명령이 어느 평가에서 어떻게 처리되는지 정해지지 않았다. 이터레이션 1에서 유효한 명령을 받았을 때의 동작(무시? reason_code?)이 없고, "출력과 state를 바꾸지 않는다"가 같은 평가의 SWR-035 상태 결정과 어떤 관계인지도 없다 | CL-REQ-17, CL-REQ-10 | |
| R-SWE1-2-07 | 경미 | software-requirements.md:68 | Interaction capability 미적용 사유 "이터레이션 1에는 사용자 인터페이스가 없다"가 같은 이터레이션의 SWR-033(Driver→SW 명령 입력의 누락·미등록 값 거절, OEM-IF-004)과 모순된다. 이는 사용자 오류 방지(User error protection) 성격이다(R-SWE1-1-15와 같은 종류의 재발) | CL-REQ-05 | |
| R-SWE1-2-08 | 경미 | software-requirements.md:53, 122 | SWR-035·A-08 "원인이 없어지면 다음 평가에서 곧바로 NORMAL"과 SWR-035 첫 문장·SWR-023 순서(입력 검사 → 상태 결정, 평가마다 state를 정함)가 어긋난다. fresh 스냅샷을 받은 그 평가에서 NORMAL인지, 그다음 평가에서 NORMAL인지 정할 수 없다(검증 기준 "주입한 다음 평가에서 NORMAL"도 같은 모호성) | CL-REQ-16, CL-REQ-17 | |
| R-SWE1-2-09 | 경미 | software-requirements.md:62 | SWR-026 요구 문장은 "스냅샷 도착 시점부터"인데 측정 기준은 "스냅샷 시각 T"(timestamp_s)를 기준으로 한다. "도착 시점"은 0.2절에 정의가 없고 평가 모델(now_s와 스냅샷을 함께 받음)에서 timestamp_s와 다른 값이다(A-11상 timestamp_s ≤ now_s) | CL-REQ-17 | |
| R-SWE1-2-10 | 경미 | software-requirements.md:46, 116, 135, 138 | ① now_s의 범위·내부 표현이 없다(SWR-013 "초, 0.001 s"만 있음). timestamp_s·source_timestamp_s의 상한이 "now_s 이하"뿐이라 정수 ms 내부 표현의 최대값·넘침 동작이 정해지지 않는다 ② R-SWE1-1-11 ④ 미조치: 5.1절 "존재", "숫자", "boolean" 검사를 C 입력 인터페이스에서 어떻게 표현하는지(누락·형식 오류 표현) 질의·가정이 없다(A-06은 차속 변환만 다룸) | CL-REQ-08 | |
| R-SWE1-2-11 | 경미 | software-requirements.md:43, 47, 49, 52, 54; safety-requirements.md:12, 13, 14 | 이터레이션 1 요구에는 출력을 LOCK으로 만드는 요구가 없다(초기 RELEASE, 충돌은 RELEASE, 나머지는 유지). 따라서 SWR-007 검증 전제 "이전 출력이 LOCK인 상태"에 도달할 수 없고, SWR-021·024·034, SAF-003·004·005의 "직전 확정 출력과 같다"는 항상 RELEASE여서 유지와 해제를 구별하지 못한다. 전제 상태를 만드는 방법(시험 수단 또는 이터레이션 이연)이 검증 기준에 없다 | CL-REQ-11, CL-SAF-03 | |
| R-SWE1-2-12 | 경미 | iter-1.md:26, 27; software-requirements.md:21, 22 | iter-1.md 26줄은 대상에 "OEM-IF-001~009 (입력 검증 부분)"을 넣고 27줄은 "OEM-IF-006은 이터 3"으로 제외해 계획서 안에서 모순된다. 출력 인터페이스 IF-005는 명세 0.1절에서 "포함(값 정의)"인데 계획서는 "입력 검증 부분"으로만 적었다. 또 0.1절 IF-006 행은 이터레이션 1이 만드는 값으로 state, reason_code, warning_code, input_validity만 들어 SWR-023의 priority_reason을 빠뜨렸다 | CL-REQ-21 | |
| R-SWE1-2-13 | 경미 | software-requirements.md:51, 54, 84, 86, 87 | R-SWE1-1-10 조치 불충분 및 추가. SWR-028(C11 작성 + Clang 빌드), SWR-029(동적 메모리 금지 + 재귀 금지, 검증 수단도 둘), SWR-038(void 금지 + 단일 return)에 독립 검증 가능한 내용이 둘씩 남아 있다. SWR-023은 처리 순서(입력 검사 → 상태 결정 → 출력 결정)와 출력 우선순위 두 요구를 담고, 처리 순서에는 검증 기준이 없다. SWR-025는 초기값과 첫 스냅샷 전 평가의 state(SWR-013의 귀결)를 함께 담았다 | CL-REQ-07, CL-REQ-11 | |
| R-SWE1-2-14 | 경미 | iter-1.md:30~36; software-requirements.md:108, 123 | R-SWE1-1-09 조치 불충분. OEM 8절 필수 증거 중 A-09·인도 증거 표가 다루는 것은 Use Case, UML, SBOM, 결함 연결뿐이다. G1 검토기록·양방향 추적, G2 정적분석 계획, G4 요구 기반 결과·Web 결과·미검증 범위·잔여문제의 처리(산출물 지정 또는 범위 밖 사유)가 없다 | CL-REQ-20 | |
| R-SWE1-2-15 | 경미 | software-requirements.md:86, 88; iter-1.md:28 | R-SWE1-1-13 조치 불충분. SWR-029 검증 기준 "`tools/run.sh rules`의 금지 함수·호출 순환 검사", SWR-032 "시계 함수 호출 금지 검사"는 저장소에 존재하지 않는다(grep: `tools/run.sh`, `check_return_rules.py`, `check_interfaces.py`에 해당 검사 없음. I4·I5는 컴포넌트·모듈 의존 순환이지 함수 호출 순환이 아님). 계획만으로는 "검증 수단이 실제로 그 요구를 검사한다"를 만족하지 않는다 | CL-REQ-19 | |
| R-SWE1-2-16 | 경미 | software-requirements.md:43, 47; safety-requirements.md:13 | R-SWE1-1-16 조치 불충분. SWR-007의 "VALID이고 fresh" 조건은 A-04에 기대는데 출처에 A-04가 없다(같은 내용의 SWR-024는 A-04 표시). SWR-021과 SAF-004의 "충돌 해제는 예외로 우선"은 A-03·Q-16(열림)에 기대는데 출처에 표시가 없다 | CL-REQ-14 | |
| R-SWE1-2-17 | 경미 | software-requirements.md:7, 171 | R-SWE1-1-17 미조치. 개정 이력 0.2 행에 범위 표기 `SWR-021~025`, `SWR-033~038`, 접두어 없는 `030`을 다시 썼다(도구는 `SWR-021`, `SWR-033`만 인식). 7줄 `SWR-001~021`도 같은 규약 위반. 또 0.2 행 변경 ID에 실제로 바뀐 SWR-013(출처에 A-07 추가, R-SWE1-1-16), SWR-028(분리), SWR-029(분리·검증 기준), SWR-031(검증 기준, R-SWE1-1-13)이 빠졌다 | CL-REQ-15, traceability.md "범위 표기는 … 쓰지 않는다" | |
| R-SWE1-2-18 | 경미 | CLAUDE.md:112, 1절 "양방향 추적성"; checklists.md:38, 83, 208; SKILL.md:58, 77 | R-SWE1-1-18 조치 불충분. SKILL.md 13줄과 CL-REQ-01은 고쳤으나 최종 기준인 CLAUDE.md 1절 요구사항 분류("품질(REQ-Q) … 제한(REQ-L), 제약사항(REQ-C)")는 그대로이고, CL-REQ-02·CL-ARC-10·CL-STS-05, SKILL.md 58·77줄도 REQ-Q/L/C·REQ-*만 기준으로 적고 있다 | checklists.md "체크리스트 개발·개선" 3항, CLAUDE.md 1절 | |
| R-SWE1-2-19 | 경미 | safety-requirements.md:11 | R-SWE1-1-21 조치 불충분. SAF-001 검증 기준 "이후 평가에서도 RELEASE를 유지한다"에 관측 구간(평가 횟수·시간)이 여전히 없다(SWR-008은 100번의 평가로 정함) | CL-SAF-03 | |
| R-SWE1-2-20 | 경미 | 커밋 8d46c7e; checklists.md 전체 diff; trace_check.py | 체크리스트 변경(CL-REQ-01, CL-REQ-16~21, CL-SAF-06 추가)을 요구사항 산출물과 같은 커밋에 넣었고 커밋 본문에 어느 리뷰 지적에서 비롯됐는지 항목별로 적지 않았다. 검사 도구(trace_check.py, 회귀 시험)·스킬 변경도 `feature/chore-` 브랜치가 아니라 이터레이션 브랜치에서 했다 | checklists.md "체크리스트 개발·개선" 5항, CLAUDE.md 3절 "도구·설정 변경은 `feature/chore-<설명>`" | |

## 이전 지적 조치 확인

| 이전 ID | 확인 결과 | 근거(산출물·도구) |
|---|---|---|
| R-SWE1-1-01 | 조치 확인 | SWR-033 신설, 5.2절 검사 규칙, 0.1절 IF-004 행. 새 결함은 R-SWE1-2-06 |
| R-SWE1-1-02 | 조치 확인(주 내용) | SWR-034(state=DEGRADED, reason_code=INPUT_INVALID, 출력 유지), SWR-022 "갱신으로 치지 않는다", 0.2절 갱신 정의. INVALID 지속·STALE 동시 등 잔여는 R-SWE1-2-01 |
| R-SWE1-1-03 | 조치 확인(주 내용) | 5.1절 timestamp_s 행, A-11(역행·미래 시각 거절, source_timestamp_s 용도). 그러나 A-11이 갱신 정의와 모순(R-SWE1-2-05), 거절 결과 미정(R-SWE1-2-01), now_s 범위 없음(R-SWE1-2-10) |
| R-SWE1-1-04 | 조치 확인(state) | SWR-035, A-08, Q-14. warning_code·reason_code 복귀는 R-SWE1-2-03, 기준 평가 모호는 R-SWE1-2-08 |
| R-SWE1-1-05 | 조치 확인 | SWR-021 예외 문구, SWR-023 순위, Q-16, SAF-004. 조치로 생긴 state 모순은 R-SWE1-2-02 |
| R-SWE1-1-06 | **조치 불충분** | SWR-007·024는 일치. SWR-008, SAF-001 둘째 문장 미반영 → R-SWE1-2-04 |
| R-SWE1-1-07 | 조치 확인 | 0.1절 IF-005(SWR-007 출처), IF-006(이터레이션 3, 사유) |
| R-SWE1-1-08 | 조치 확인 | iter-1.md 27줄 선행 예외 명시, SWR-030 삭제·사유 기록. 새 불일치는 R-SWE1-2-12 |
| R-SWE1-1-09 | **조치 불충분** | A-09, iter-1.md 인도 증거 표 추가. 정적분석 계획 등 누락 → R-SWE1-2-14 |
| R-SWE1-1-10 | **조치 불충분** | SWR-008 → 036, 028 → 037, 029 → 038 분리, 030 삭제. 분리 후에도 복합 잔존 → R-SWE1-2-13 |
| R-SWE1-1-11 | **조치 불충분** | ① 차속 표현 5.1절·A-06 일치 ② 5.3절 값 집합 ③ 5.1절 "존재" 추가 확인. ④ 누락·형식 오류의 C 표현 질의·가정 없음 → R-SWE1-2-10 |
| R-SWE1-1-12 | 조치 확인 | 0.2절 용어 9개 정의. 정의 간 충돌은 R-SWE1-2-01, 05, 09 |
| R-SWE1-1-13 | **조치 불충분** | SWR-031 문구·측정 조건 수정 확인. SWR-029·032 검증 수단 부재 → R-SWE1-2-15 |
| R-SWE1-1-14 | 조치 확인 | SWR-030 삭제(OEM-NFR-001은 SWR-016, 이터레이션 4) |
| R-SWE1-1-15 | **조치 불충분** | Maintainability·Compatibility·Flexibility 사유 수정 확인. Interaction capability 사유가 새로 모순 → R-SWE1-2-07 |
| R-SWE1-1-16 | **조치 불충분** | SWR-013(A-07), SWR-036(A-10), SWR-035(A-08) 표시 확인. SWR-007, SWR-021, SAF-004 누락 → R-SWE1-2-16 |
| R-SWE1-1-17 | **미조치** | 0.2 행에 범위 표기 재사용 → R-SWE1-2-17 |
| R-SWE1-1-18 | **조치 불충분** | SKILL.md 13줄, CL-REQ-01 수정 확인. CLAUDE.md 1절 등 미수정 → R-SWE1-2-18 |
| R-SWE1-1-19 | 조치 확인 | CLAUDE.md 프로젝트 절에 과제·입력 문서·이터레이션 계획 위치 기재 |
| R-SWE1-1-20 | 조치 확인 | SAF-003/SAF-005 분리 |
| R-SWE1-1-21 | **조치 불충분** | SAF-003 관측값 확인. SAF-001 관측 구간 없음 → R-SWE1-2-19 |
| O-01 | 반영 확인 | 정규식 끝 `(?!\d)`, 회귀 시험 4건 통과(직접 실행) |
| O-02 | 반영 확인 | 커밋 8d46c7e 대상으로 리뷰, 작업 트리 깨끗함 |
| O-03 | 유지 | `tools/run.sh trace`는 SWE.2 전까지 T1 24건으로 실패(예상) |

## 의견

| ID | 내용 |
|---|---|
| O-01 | CLAUDE.md "요청자 이름: imunokyh (… 사용자가 '알아서 해줘'로 위임 …)"은 CLAUDE.md의 "이름을 모르면 지어내지 말고 사용자에게 묻는다"와 관련해 리뷰어가 사용자 위임 사실을 확인할 수 없다. 구현(SWE.3) 시작 전에 사용자에게 직접 확인받을 것을 권한다. iter-1.md 57줄 "요청자 이름 확인 필요"도 CLAUDE.md와 맞춰 갱신할 것 |
| O-02 | `docs/reviews/swe1-review-1.md`의 "조치(작성자 기입)" 열이 비어 있다. 지적별 조치 내용을 기입하면 재리뷰 증적이 분명해진다 |
| O-03 | A-04·SWR-024는 STALE인 crash_status=CONFIRMED로는 해제하지 않는다고 정했다(예: 지연 도착한 충돌 스냅샷). OEM-SR-001(ASIL B)의 해석에 직접 영향을 주는 결정이므로 Q-05와 별도로 OEM 질의 항목으로 명시하는 것을 권한다 |
| O-04 | trace_check.py 정규식 회귀 시험에 범위 표기(`SWR-021~025`)가 첫 ID만 인식된다는 사례를 추가하면 규약 위반을 도구로 알 수 있다(현재는 문서 규약에만 의존) |

## 체크리스트 개선 제안

| 체크리스트 | 제안 항목 | 계기가 된 결함 |
|---|---|---|
| CL-REQ | 출력 값 집합의 모든 값마다 그 값을 내는 요구가 있고, 여러 원인이 동시 성립할 때 선택 규칙이 모든 값에 대해 정해져 있다 | R-SWE1-2-03 |
| CL-REQ | 같은 출력(state 등)을 여러 요구가 정하면 우선·참조 관계가 각 요구 문장에 명시되어 있다 | R-SWE1-2-02 |
| CL-REQ | 검증 기준의 전제 상태가 해당 이터레이션 요구만으로 도달 가능하거나 도달 수단이 적혀 있다 | R-SWE1-2-11 |
| CL-REQ | 가정(A-nn)이 용어 정의를 바꾸지 않으며, 바꾸면 용어 정의를 함께 고쳤다 | R-SWE1-2-05 |
| CL-REQ | 상태 표시 값(INVALID, STALE 등)은 설정 조건과 함께 해제 조건·지속 기간이 정의되어 있다 | R-SWE1-2-01 |
| 신규 (이터레이션 계획 `docs/iterations/iter-<n>.md`) | 1회차 제안 재게시: 이터레이션 계획을 대응표에 넣고 범위 선언(대상·제외 ID)이 명세 0.1절·출처와 일치하는지 본다 | R-SWE1-2-12 |
