# SWE.1 리뷰 4회

- 일자: 2026-11-27
- 대상 커밋: `0181df1` (브랜치 `feature/iter-1-input-guard`, 산출물 커밋 `c74abbe` + `develop`(`286504f`) 병합. `git merge-base HEAD develop` = `286504f`, 작업 트리 변경 없음 확인)
- 입력(상위 산출물): `OEM/OEM-SWR-001_OEM SW 요구사항 사양서.docx` (Rev 1.0, BL-OEM-1.0). `word/document.xml`에서 텍스트를 직접 추출해 5절(SR, FR, NFR), 6절(IF), 8절, 9절을 대조
- 이전 리뷰: `docs/reviews/swe1-review-1.md`, `-2.md`, `-3.md` (3회차 부적합 15건 R-SWE1-3-01~15, 의견 O-01~06)
- 리뷰 대상: `docs/swe1/software-requirements.md`(개정 이력상 v0.4), `docs/swe1/safety-requirements.md`(v0.4), `docs/iterations/iter-1.md`, `OEM/README.md`. R-SWE1-3-08 확인을 위해 `develop`에 병합된 `61875ff`(PR #3)도 대조
- 적용 체크리스트: CL-REQ-01~27, CL-SAF-01~07, CL-TRC-01~03, CL-COM-01~04
- 리뷰어: stage-reviewer
- 판정: **불통과** (중대 0건, 경미 11건, 미확인 0건, 의견 9건)

## 리뷰한 산출물과 적용 체크리스트

| 산출물 | 경로 | 체크리스트 | 항목 수 | 적합 | 부적합 | 해당 없음 | 미확인 |
|---|---|---|---|---|---|---|---|
| SW 요구사항 명세 | `docs/swe1/software-requirements.md` | CL-REQ | 27 | 19 | 8 | 0 | 0 |
| 기능 안전 요구사항 | `docs/swe1/safety-requirements.md` | CL-SAF | 7 | 5 | 1 | 1 | 0 |
| 추적성 검사 결과 | `trace_check.py --stage swe1`, `--stage all` | CL-TRC | 3 | 3 | 0 | 0 | 0 |
| 공통 | 위 산출물 전체, `iter-1.md`, `OEM/README.md` | CL-COM | 4 | 3 | 1 | 0 | 0 |

- `docs/iterations/iter-1.md`, `OEM/README.md`는 여전히 대응표에 전용 체크리스트가 없다(R-SWE1-4-11). 범위 선언은 CL-REQ-21, 나머지는 CL-COM으로 판정했다.

## 도구 결과

| 도구 | 명령 | 결과(판정 줄) |
|---|---|---|
| 추적성 (SWE.1) | `docker compose run --rm dev python3 .claude/skills/requirements-analysis/scripts/trace_check.py --docs docs --src src --stage swe1` | `추적성 검사 (단계 swe1): SAF 4, SWR 29` / `판정: 통과 (양방향 추적성 100%)` (exit 0). 문서에서 직접 센 정의 수와 일치: SWR 29(기능 17, 품질 2, 제한 1, 제약 9), SAF 4. T11(중복)·T12(SAF 위치)·T13(표 안 범위 표기) 0건 |
| 추적성 (전 구간, CL-TRC-02) | `docker compose run --rm dev tools/run.sh trace` | `판정: 실패 — 오류 33건`. T1 외 오류 0건(필터 확인). 33건 = SWR 29 + SAF 4의 T1(아키텍처 미연결)로 SWE.2 전 예상 결과. SWE.1 판정에 쓰지 않음 |
| 검사 스크립트 자체 시험 | `docker compose run --rm dev tools/run.sh selftest` | `Ran 5 tests … OK`, `Ran 7 tests … OK` / `판정: 통과` |
| 금지 호출 검사 (CL-REQ-19) | `docker compose run --rm dev tools/run.sh rules`, `check_banned.py` 21~24줄 읽기 | `건너뜀: rules — src/ 에 C 소스가 없음`(SWE.1 시점 정상). B3의 CLOCK 집합에 `rand`, `srand`, `random`, `srandom`이 있어 SWR-047 검증 수단이 실제로 존재함을 확인 |
| v0.3 대비 변경 대조 (CL-REQ-15) | `git show a722ff7:docs/swe1/software-requirements.md`와 현재 파일의 SWR 행 diff | 바뀐 행: 007, 013, 021, 022, 023, 025, 028, 032, 034, 035, 039, 040, 041, 044, 신설 046, 047. 0.4 행 변경 ID와 정확히 일치. 다만 3줄 머리말 버전은 `0.3` 그대로 → R-SWE1-4-01 |
| 체크리스트 반영 확인 (R-SWE1-3-08) | `git show --stat 61875ff`, `git log -1 --format=%B 61875ff` | 체크리스트(+7줄)와 SKILL.md(1줄)만 담은 별도 chore 커밋. 본문에 항목별 출처(회차·지적 ID) 있음. CL-REQ-22~27, CL-SAF-07 추가. 2회차 제안 6번(이터레이션 계획 체크리스트)은 미반영 → R-SWE1-4-11 |
| OEM 원문 추출 | `py -3 -I`로 docx `word/document.xml` 추출(작업 디렉터리 `D:/tmp/oemx`, 저장소 밖) | SR-001·003·004, FR-007, NFR-001, IF-001~009 원문 대조. 9절 추적표: NFR-001 → SWR-016, FR-007 → SWR-020 |
| ID 고유성 (CL-COM-02) | `grep -oE "^\| (Q\|A\|SWR\|SAF)-[0-9]+" … \| uniq -d` | 중복 0건. Q-01~20(20건), A-01~15(15건) |

## 체크리스트 판정 (산출물별)

### docs/swe1/software-requirements.md — CL-REQ

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-REQ-01 | 적합 | `분류` 열로 기능 17, 비기능 12. 판별 맞음 |
| CL-REQ-02 | 적합 | 품질 2(026, 027), 제한 1(031), 제약 9(028, 041, 037, 029, 042, 038, 043, 032, 047) |
| CL-REQ-03 | 적합 | 5줄 "ISO 25010 적용 판: 2023". SWR-026, 027 특성·부특성 있음 |
| CL-REQ-04 | 적합 | SWR-026(300 ms, 10 ms 주기, 고정 시계), SWR-027(T+200 VALID, T+210 STALE, T+300 이내). 용어·조건 결함은 R-SWE1-4-04 |
| CL-REQ-05 | 적합 | 2023판 9개 특성 전부 판정(적용 2, 해당 없음 7, 사유 있음) |
| CL-REQ-06 | 적합 | T12 0건 |
| CL-REQ-07 | 적합 | 29건 전수. SWR-032(난수 분리로 시계 금지 하나 + SWR-044 참조 문장), SWR-047 분리 확인. SWR-013 검증의 SWR-045 중복 제거 확인 |
| CL-REQ-08 | 부적합 | now_s·timestamp_s·source_timestamp_s의 "소수 넷째 자리 이하가 0이 아니면 형식 오류"는 실수(이진 부동소수) 원시 값(A-12, "NaN 아님")에서 판정할 수 없음 → R-SWE1-4-02. 출력 input_validity의 원소(입력 단위)가 정해지지 않음 → R-SWE1-4-06 |
| CL-REQ-09 | 적합 | "적절히·빠르게·가능하면" 등 없음(A-01 근거 열의 "충분히"는 요구가 아닌 근거 서술) |
| CL-REQ-10 | 적합 | 범위 밖, 무효, 누락, 타임아웃, 시각 역행·미래 시각 모두 정의(SWR-022, 034, 024, 013, 040, 046). 지속적 now_s 역행·상한 도달의 안전 영향은 의견 O-01 |
| CL-REQ-11 | 적합 | 29건 모두 검증 기준 있음. 전제 도달성 결함은 CL-REQ-23에서 판정 |
| CL-REQ-12 | 적합 | OEM ID(SR 4, FR 7, NFR 2, IF 9) 전수: 0.1절이 모두 처리 |
| CL-REQ-13 | 부적합 | SWR-026은 "CONFIRMED 스냅샷이 전달된 평가에서 RELEASE"를 무조건 요구해 SWR-024(오래된 스냅샷 → STALE 유지)·SWR-046(역행 스냅샷 → INVALID 유지)과 어긋남 → R-SWE1-4-04. SWR-045 "첫 스냅샷을 받기 전"과 0.2절 INVALID 지속이 첫 스냅샷 거절 뒤 평가에서 서로 다른 결과 → R-SWE1-4-05 |
| CL-REQ-14 | 적합 | 열린 질의마다 적용 가정 또는 이터레이션 지정이 있음. SWR-007, SAF-001(A-04, Q-17), SAF-004(A-03, Q-16), SWR-035(A-15, Q-20) 출처 표시 확인 |
| CL-REQ-15 | 부적합 | 머리말 3줄 "버전: 0.3"이 개정 이력 최신 행(0.4)·iter-1.md "v0.4"와 다름 → R-SWE1-4-01 |
| CL-REQ-16 | 적합 | 입력 상태(INVALID > STALE > VALID, 해제 조건), state(FAULT > DEGRADED > NORMAL, 평가마다 재결정), 결정 규칙(5.4절 처음 성립) 모두 진입·탈출·우선순위 있음 |
| CL-REQ-17 | 부적합 | "스냅샷 생성 시각"(SWR-026) 미정의 잔존 → R-SWE1-4-04. "첫 스냅샷을 받기 전"(SWR-045)이 0.2절 "받아들인 스냅샷"과 다름 → R-SWE1-4-05. "입력"(input_validity의 단위)이 요구마다 다르게 쓰임 → R-SWE1-4-06 |
| CL-REQ-18 | 적합 | INVALID(지속·해제), 스냅샷 거절(SWR-046), 평가 거절(SWR-040), 명령 거절(SWR-033)의 상태·출력이 정의됨 |
| CL-REQ-19 | 적합 | SWR-029(B1), 042(B2), 032·047(B3, `rand`·`random` 포함 확인), 028·041·037(`build`), 038(RT1), 043(RT2, RT3). `selftest` 통과 |
| CL-REQ-20 | 적합 | OEM 3절(실행 환경 → Q-11, A1·A2), 7절(Q-13), 8절(iter-1.md 인도 증거 표, A-09 일치), 9절(7줄 번호 체계) |
| CL-REQ-21 | 부적합 | SWR-047(출처 OEM-NFR-001)이 이터레이션 1에 있는데 0.1절 24줄과 iter-1.md 6줄은 NFR-001 중 SWR-032만 선행한다고 선언 → R-SWE1-4-08 |
| CL-REQ-22 | 부적합 | 5.3절 값은 모두 정해짐(OFF는 미사용 명시). 그러나 SWR-040이 출력으로 둔 "반환 상태"의 값 집합과 정상·스냅샷 거절 평가의 반환 값이 없음 → R-SWE1-4-07 |
| CL-REQ-23 | 부적합 | v0.4 입력 상태 정의 아래에서 "일부 입력만 STALE", "crash VALID + 다른 입력 STALE", "FAULT 조건 + STALE"은 도달 불가인데 SWR-024·SWR-007·SAF-003 검증 기준과 A-15·Q-20이 이를 전제로 함 → R-SWE1-4-03 |
| CL-REQ-24 | 부적합 | A-04 첫 문장("필수 입력이 STALE 또는 INVALID이면 직전 확정 출력을 유지")이 무조건이라 A-04를 출처로 둔 SWR-007("다른 입력 INVALID여도 RELEASE")·규칙 1과 어긋남 → R-SWE1-4-09 |
| CL-REQ-25 | 적합 | 0.2절: INVALID는 "유효한 값으로 갱신될 때(원인 해소)에만" 풀리고 새 스냅샷이 없다는 이유로 풀리지 않음. STALE은 갱신으로 풀림. 판정 값은 VALID 입력의 보관 값뿐 |
| CL-REQ-26 | 적합 | 초기값: SWR-025가 5.3절 출력 전부(lock 2, state, input_validity, reason_code, priority_reason, warning_code, command_status) 정의. 평가 거절: SWR-040 "5.3절의 모든 출력 … 바꾸지 않는다". 스냅샷 거절: SWR-046 + 규칙표. 명령 거절: SWR-033 |
| CL-REQ-27 | 적합 | 표 밖 본문 7줄은 금지 예시를 백틱으로 인용한 것뿐이며 ID 범위 표기는 "SWR-001부터 SWR-021까지"로 고쳐짐. OEM ID 범위 표기(표 안 24줄, iter-1.md)는 의견 O-07 |

### docs/swe1/safety-requirements.md — CL-SAF

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-SAF-01 | 적합 | SAF-001/003/004/005 모두 ASIL B, 안전 목표(미제공 — OEM-A 할당), 안전 상태, FTTI |
| CL-SAF-02 | 적합 | 근거 OEM-SWR-001 5절. SR-001 → SAF-001, SR-003 → SAF-003·005, SR-004 → SAF-004, SR-002 → 002 예약 |
| CL-SAF-03 | 적합 | 네 SAF 모두 고장 주입 조건과 관측 가능한 기대 반응. SAF-003 검증 일부 전제의 도달 불가는 CL-REQ-23(R-SWE1-4-03)으로 지적 |
| CL-SAF-04 | 적합 | 수동 대조: SAF-001 ← 007, 008, 036, 023, 026 / SAF-003 ← 013, 024, 035, 044, 027, 045, 040 / SAF-004 ← 021, 035, 023, 025 / SAF-005 ← 022, 034, 035, 044, 046. T1은 SWE.2부터 도구 판정 |
| CL-SAF-05 | 해당 없음 | ASIL 분해 없음(4줄) |
| CL-SAF-06 | 적합 | SAF마다 안전 상태·FTTI 하나 |
| CL-SAF-07 | 부적합 | SAF-003·005에 충돌 해제 예외가 요구 문장과 안전 상태 열에 모두 반영됨(R-SWE1-3-02 조치 확인). SAF-001 ↔ SWR-007/008, SAF-004 ↔ SWR-021 일치. 그러나 6줄이 결정 규칙표(SWR-023)를 SW 요구사항 명세 "0.2절"에 있다고 참조함(실제 5.4절) → R-SWE1-4-10 |

### 추적성 — CL-TRC

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-TRC-01 | 적합 | `--stage swe1` 오류 0건(직접 실행) |
| CL-TRC-02 | 적합 | 개정(v0.4) 뒤 `--stage all` 직접 실행. 33건 전부 T1(하위 산출물 없음) |
| CL-TRC-03 | 적합 | 출처 열의 OEM ID 전부가 원문에 존재(SR-001·003·004, FR-007, NFR-001, IF-001~009). 0.1절이 OEM ID 22개를 모두 처리 |

### 공통 — CL-COM

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-COM-01 | 적합 | 지정 경로, 한글 |
| CL-COM-02 | 적합 | SWR 29, SAF 4(T11 0건), Q 20, A 15 중복 없음 |
| CL-COM-03 | 부적합 | 3회차 지적 15건 중 3건 조치 불충분(R-SWE1-3-06, 08, 09 → R-SWE1-4-02, 11, 04) |
| CL-COM-04 | 적합 | `feature/iter-1-input-guard`, 분기 원점 `develop`, develop 최신(`286504f`) 병합 |

## 개정부 조합 점검 (리뷰어 직접 계산)

0.2절 정의, SWR-022·046(입력 상태), SWR-023 규칙표(5.4절), SWR-035, SWR-021, SWR-033, SWR-040, A-07로 계산했다.

### 도달 가능성 정리 (R-SWE1-4-03의 근거)

A-07("입력은 스냅샷 단위로 갱신하고 갱신 시각은 항상 스냅샷의 timestamp_s")과 v0.4의 INVALID 지속 정의를 함께 적용하면:

1. 받아들인 스냅샷은 모든 차량 입력을 담는다(빠지면 누락 → INVALID).
2. 어떤 입력이 INVALID가 아니면, 그 입력이 가장 최근에 받은 값(= 마지막으로 받아들인 스냅샷의 값)은 유효했고 그 스냅샷에서 갱신되었다. 스냅샷 거절(SWR-046)은 모든 입력을 INVALID로 만들므로 예외가 되지 않는다.
3. 따라서 **INVALID가 아닌 모든 차량 입력의 마지막 갱신 시각은 같은 값(마지막으로 받아들인 스냅샷의 timestamp_s)**이고, STALE/VALID는 이 입력들에 대해 항상 함께 바뀐다(첫 스냅샷 전에는 모두 STALE).

결과: "입력 하나만 STALE", "crash_status VALID + 다른 입력 STALE", "sensor_fault VALID(FAULT 조건) + 다른 입력 STALE"은 도달할 수 없다. v0.3에서는 INVALID가 다음 평가에 이어지지 않아 이 조합에 도달할 수 있었으므로, 이번 개정이 만든 변화다.

### 조합표

| 조합 | 출력 | reason_code | state | warning_code | 판정 |
|---|---|---|---|---|---|
| crash CONFIRMED(V) + sensor_fault TRUE(V) + 다른 입력 INVALID | RELEASE | CRASH_CONFIRMED | FAULT | SENSOR_FAULT | 일관 (SWR-007, 021, 023, 035) |
| crash CONFIRMED(V) + sensor_fault INVALID | RELEASE | CRASH_CONFIRMED | DEGRADED | NONE | 일관 |
| crash CONFIRMED(V) + 다른 입력 STALE | — | — | — | — | **도달 불가**. 그런데 SWR-007 검증, SAF-003 요구·안전 상태·검증이 전제로 씀 (R-SWE1-4-03) |
| crash PENDING(V) + sensor_fault TRUE(V) | 유지 | SENSOR_FAULT | FAULT | SENSOR_FAULT | 일관 (SWR-036 검증) |
| crash PENDING(V) + 다른 입력 INVALID | 유지 | INPUT_INVALID | DEGRADED | NONE | 일관 |
| crash INVALID + sensor_fault TRUE(V) | 유지 | SENSOR_FAULT | FAULT | SENSOR_FAULT | 일관 |
| sensor_fault TRUE(V) + 다른 입력 STALE | — | — | — | — | **도달 불가**. A-15·Q-20의 전제, SAF-003 "FAULT 동시이면 FAULT" (R-SWE1-4-03) |
| sensor_fault TRUE(V) + 다른 입력 INVALID | 유지 | SENSOR_FAULT | FAULT | SENSOR_FAULT | 일관 (A-15가 실제로 적용되는 유일한 경우) |
| 입력 하나 INVALID 후 200 ms 넘게 스냅샷 없음 | 유지 | INPUT_INVALID | DEGRADED | NONE | 일관 (규칙 3 > 4, 그 입력 INVALID, 나머지 STALE) |
| 모든 입력 STALE(스냅샷 끊김), 보관 값 CONFIRMED | 유지 | INPUT_STALE | DEGRADED | NONE | 일관 (Q-17 열림) |
| 차속만 STALE, 나머지 VALID | — | — | — | — | **도달 불가**. SWR-024 검증의 전제 (R-SWE1-4-03) |
| 일부 입력만 INVALID인 스냅샷(gear 미정 값, crash CONFIRMED 유효) | RELEASE | CRASH_CONFIRMED | DEGRADED | NONE | 일관. 이 스냅샷은 "받아들인 스냅샷"이라 역행 판정 기준이 됨 |
| source_timestamp_s > timestamp_s인 스냅샷 | 유지 | INPUT_INVALID | DEGRADED | NONE | 일관 (source_timestamp_s만 INVALID). input_validity에 source_timestamp_s 원소가 있는지는 불명 (R-SWE1-4-06) |
| 스냅샷 timestamp_s 역행/미래/누락 (SWR-046) | 유지 | INPUT_INVALID(sensor_fault도 INVALID이므로 규칙 2 불가) | DEGRADED | NONE | 일관. 반환 상태는 불명 (R-SWE1-4-07) |
| 초기화 직후(평가 전) | 지정값(기본 RELEASE) | NONE | NORMAL | NONE | 정의됨. 입력 모두 STALE인데 NORMAL (의견 O-02) |
| 초기화 후 스냅샷 없이 첫 평가 | 유지 | INPUT_STALE | DEGRADED | NONE | 일관 (SWR-045) |
| 첫 스냅샷이 SWR-046으로 거절된 뒤 스냅샷 없이 평가 | 유지 | INPUT_INVALID(0.2절) 또는 INPUT_STALE(SWR-045를 "받아들이기 전"으로 읽을 때) | DEGRADED | NONE | **두 가지로 읽힘** (R-SWE1-4-05) |
| now_s 100 → 50(거절) → 80 | 첫 평가 직후 값 전체 불변 | 불변 | 불변 | 불변 | 일관 (SWR-040, command_status 포함 불변) |
| now_s 거절 다음에 정상 평가(110) | 정상 계산, 기준 100 | — | — | — | 일관 |
| CONFIRMED 스냅샷이 timestamp_s 역행으로 도착 | 유지 (SWR-046 → crash INVALID) / RELEASE (SWR-026 문장) | INPUT_INVALID | DEGRADED | NONE | **SWR-026과 모순** (R-SWE1-4-04) |
| CONFIRMED 스냅샷의 timestamp_s가 now_s보다 250 ms 이전 | 유지 (SWR-024, STALE) / RELEASE (SWR-026 문장) | INPUT_STALE | DEGRADED | NONE | **SWR-026과 모순** (R-SWE1-4-04) |
| now_s가 크게 역행(시계 재설정)하거나 상한 4294967.295 s에 도달 | 이후 모든 평가 거절, 출력·state 동결 | 불변 | 불변(NORMAL일 수 있음) | 불변 | 정의는 있음. 충돌 해제·DEGRADED 보고 불가 (의견 O-01) |

## 지적 사항

| ID | 등급 | 산출물:위치 | 내용 | 근거(체크리스트 ID) | 조치(작성자 기입) |
|---|---|---|---|---|---|
| R-SWE1-4-01 | 경미 | software-requirements.md:3 | 머리말 "버전: 0.3 (이터레이션 1)"이 개정 이력 최신 행 0.4(206줄)와 iter-1.md 37줄 "v0.4"와 다르다. v0.3(`a722ff7`)에서 바뀌지 않은 줄이다 | CL-REQ-15 | |
| R-SWE1-4-02 | 경미 | software-requirements.md:157, 158, 161, 144 | R-SWE1-3-06 조치 불충분. 상한(4294967.295 s)과 넘침(범위 오류 → 거절)은 정해졌으나, 1 ms 미만 자리 규칙 "소수 넷째 자리 이하가 0이 아니면 형식 오류"는 판정할 수 없다. 5.1절이 "숫자(NaN 아님)"를 검사하고 A-12가 원시 값을 "정수 또는 실수"로 받으므로 시각은 이진 부동소수로 들어오는데, 0.001, 0.007, 0.210 같은 값 대부분은 이진 부동소수로 정확히 표현되지 않아 문자 그대로 적용하면 거의 모든 now_s가 형식 오류(SWR-040 거절)가 된다. 허용 오차나 반올림 규칙(예: ms로 반올림 후 차이가 x 이하이면 유효), 또는 시각을 정수 ms로만 받는다는 결정이 필요하다. 이 규칙은 STALE 경계(200 ms)와 역행 판정 결과를 바꾼다 | CL-REQ-08, CL-COM-03 | |
| R-SWE1-4-03 | 경미 | software-requirements.md:48, 55, 126, 146; safety-requirements.md:12 | v0.4 입력 상태 정의(INVALID 지속)와 A-07(스냅샷 단위 갱신)에 따라 INVALID가 아닌 모든 차량 입력의 마지막 갱신 시각은 항상 같다(위 "도달 가능성 정리"). 그래서 다음 검증 전제·서술에 도달할 수 없다. ① SWR-024 검증 "차속 입력만 STALE이면" ② SWR-007 검증 "다른 입력 … STALE여도 RELEASE"의 STALE 부분(crash가 VALID면 다른 비INVALID 입력도 VALID) ③ SAF-003 요구 문장·안전 상태·검증의 "FAULT 조건과 동시이면 FAULT", 검증 "같은 평가에 충돌 CONFIRMED(입력 상태 VALID)가 있으면 출력은 RELEASE"(STALE이 성립하면 crash와 sensor_fault도 STALE이거나 INVALID) ④ A-15·Q-20의 전제 "sensor_fault와 STALE이 동시일 때"(실제로 FAULT와 DEGRADED가 동시인 경우는 INVALID뿐). 검증 기준을 도달 가능한 전제로 바꾸거나(예: "입력 하나를 INVALID로 만든 뒤 200 ms 넘게 스냅샷 없음"), 도달 불가한 방어 서술임을 밝히고 검증 대상에서 뺄 것. 단순화 방향은 의견 O-09 ① | CL-REQ-23 | |
| R-SWE1-4-04 | 경미 | software-requirements.md:72 | R-SWE1-3-09 ③ 조치 불충분. 0.2절에 "스냅샷 시각(= timestamp_s)"을 정의했으나 SWR-026은 여전히 정의되지 않은 "스냅샷 생성 시각 T"를 쓴다. 또 SWR-026 요구 문장 "충돌 CONFIRMED를 담은 스냅샷이 전달된 평가에서 두 출력을 RELEASE로 확정"에는 입력 상태 조건(받아들인 스냅샷, crash_status 입력 상태 VALID)이 없어, timestamp_s가 now_s보다 200 ms 넘게 이전인 스냅샷(SWR-024: STALE → 유지)이나 timestamp_s가 역행한 스냅샷(SWR-046: INVALID → 유지)에서 SWR-024·046과 다른 출력을 요구한다. 측정 기준(now_s ≤ T+10 ms)만 그 경우를 피한다 | CL-REQ-17, CL-REQ-13, CL-COM-03 | |
| R-SWE1-4-05 | 경미 | software-requirements.md:60, 33, 35 | SWR-045 "첫 스냅샷을 받기 전의 평가에서 모든 필수 입력을 STALE로 취급"의 "받기"가 0.2절의 "받아들인 스냅샷"과 다른 말이다. 첫 스냅샷이 SWR-046으로 거절된 뒤 스냅샷 없이 평가하면, 0.2절은 모든 입력을 INVALID(지속)로, SWR-045를 "받아들이기 전"으로 읽으면 STALE로 정해 reason_code가 INPUT_INVALID와 INPUT_STALE로 갈린다 | CL-REQ-17, CL-REQ-13 | |
| R-SWE1-4-06 | 경미 | software-requirements.md:51, 53, 54, 55, 161, 182 | 출력 input_validity의 원소("입력별"의 입력 단위)가 정해지지 않았다. SWR-013은 "필수 입력(OEM-IF-001, 002, …)의 … 해당 입력의 input_validity"로 인터페이스 단위처럼, SWR-022·034와 SWR-024 검증("차속 입력만")은 필드 단위로 쓴다. 5.1절의 source_timestamp_s, rear_left/right_approach_risk(한 행) 등 어느 항목이 input_validity 원소인지, OEM-IF-001처럼 필드가 여럿인 인터페이스의 값이 어떻게 정해지는지 없다. SWE.2 인터페이스(출력 구조)를 정할 수 없다 | CL-REQ-17, CL-REQ-08 | |
| R-SWE1-4-07 | 경미 | software-requirements.md:62, 63, 176~185 | SWR-040은 "출력 반환 상태"를 두고 거절 시 "오류 상태를 반환"한다고 하지만, 반환 상태의 값 집합이 5.3절에 없고 받아들인 평가(정상)와 스냅샷이 SWR-046으로 거절된 평가가 무엇을 반환하는지 정한 요구가 없다. 호출자가 평가 거절과 스냅샷 거절을 구별할 수 있는지도 정해지지 않았다 | CL-REQ-22 | |
| R-SWE1-4-08 | 경미 | software-requirements.md:24, 102; iter-1.md:6 | SWR-047(난수 금지, 출처 OEM-NFR-001)이 이터레이션 1 요구인데, 0.1절 24줄은 "NFR-001~002는 이터레이션 2~4. 다만 초기 상태(SWR-025)와 시계 입력(SWR-032)은 이터레이션 1이 선행"으로, iter-1.md 6줄도 "OEM-NFR-001의 고정 시계 입력(SWR-032)은 선행"으로만 선언한다. iter-1.md 7줄은 SWR-047을 적어 같은 문서 안에서도 다르다. (OEM 9절은 NFR-001을 SWR-016에 할당했다는 점도 함께 기록할 것) | CL-REQ-21 | |
| R-SWE1-4-09 | 경미 | software-requirements.md:136, 48 | A-04 첫 문장 "필수 입력이 STALE 또는 INVALID이면 직전 확정 출력을 유지한다"는 조건 없는 규칙이라, crash_status가 VALID·CONFIRMED이고 다른 입력이 INVALID인 경우 RELEASE를 요구하는 SWR-007(출처에 A-04를 둠)·규칙 1과 어긋난다. R-SWE1-3-02가 SAF-003·005에서 고친 것과 같은 결함이 가정에 남았다(우선순위는 A-03에 있으나 A-04 자체가 예외를 말하지 않음) | CL-REQ-24 | |
| R-SWE1-4-10 | 경미 | safety-requirements.md:6 | "용어(…)와 결정 규칙표(SWR-023)는 software-requirements.md 0.2절을 따른다"에서 결정 규칙표는 0.2절이 아니라 5.4절에 있다. ASIL B 요구가 참조하는 규칙표 위치가 틀렸다 | CL-SAF-07 | |
| R-SWE1-4-11 | 경미 | `.claude/skills/aspice-stage-review/references/checklists.md`:9~27; docs/iterations/iter-1.md | R-SWE1-3-08 ③ 조치 불충분. `61875ff`로 2회차 제안 1~5와 3회차 제안을 CL-REQ-22~27, CL-SAF-07로 반영하고 커밋 본문에 항목별 출처를 남긴 것(①), SKILL.md 58줄(②)은 확인했다. 그러나 R-SWE1-3-08 ③이 명시한 6건 중 "이터레이션 계획 체크리스트"(2회차 제안 6, 1·3회차 재게시)가 반영되지 않아 `iter-1.md`가 여전히 대응표에 없고 전용 체크리스트 없이 리뷰된다 | CL-COM-03, CLAUDE.md 1절 "새 종류의 산출물이 생기면 체크리스트를 먼저 만든다", checklists.md "체크리스트 개발·개선" 1항 | |

## 이전 지적 조치 확인 (3회차)

| 이전 ID | 확인 결과 | 근거(산출물·도구) |
|---|---|---|
| R-SWE1-3-01 | 조치 확인 | 0.2절 "입력 상태": INVALID는 유효한 값이 갱신될 때만 풀리고 스냅샷이 없다는 이유로 풀리지 않음. A-14(도착 주기, 요구가 의존하지 않음), SWR-022·034 검증("스냅샷 없이 평가해도 계속 INVALID"), SAF-005("새 값이 유효하게 갱신될 때까지"), A-08 둘째 문장. 조합표의 "INVALID 후 스냅샷 없음" 계산 일관. 이 정의가 만든 도달 불가 전제는 R-SWE1-4-03 |
| R-SWE1-3-02 | 조치 확인 | SAF-003·005 요구 문장 "충돌 해제(SAF-001)는 예외로 우선한다", 안전 상태 "(충돌 CONFIRMED 입력 상태 VALID이면 RELEASE)", 검증에 동시 조건 추가. A-04 쪽 잔여는 R-SWE1-4-09 |
| R-SWE1-3-03 | 조치 확인 | SAF-001 둘째 문장·검증 "입력 상태가 VALID로 CONFIRMED가 유지되는 동안/유지되면" |
| R-SWE1-3-04 | 조치 확인 | SWR-007 출처 "가정 A-04, 질의 Q-17", SAF-001 출처 "가정 A-04, 질의 Q-17", SAF-004 출처 "가정 A-03, 질의 Q-16" |
| R-SWE1-3-05 | 조치 확인 | 7줄 "SWR-001부터 SWR-021까지", 0.3 행에서 SWR-031 제거(205줄) |
| R-SWE1-3-06 | **조치 불충분** | 상한 4294967.295 s, 정수 ms 내부 표현, 넘침 = 범위 오류 확인. 1 ms 미만 규칙이 실수 입력에서 판정 불가 → R-SWE1-4-02 |
| R-SWE1-3-07 | 조치 확인 | SWR-040: 스냅샷·명령 버림, 5.3절 모든 출력·입력 상태·보관 값·갱신 시각·직전 확정 출력·직전 now_s 불변, 기준은 "직전에 받아들여진 평가". 0.2절 "직전 확정 출력" 정의, SWR-044 ① "거절되면 ② 이후를 수행하지 않는다", 검증 100 → 50 → 80 |
| R-SWE1-3-08 | **조치 불충분** | ① `61875ff` 별도 커밋·항목별 출처 ② SKILL.md 58줄 "REQ-* 또는 SWR-*" 확인. ③ 이터레이션 계획 체크리스트 미반영 → R-SWE1-4-11 |
| R-SWE1-3-09 | **조치 불충분** | ① "DEGRADED 조건" 0.2절 정의 ② "fresh" 제거(grep 0건) ④ "받아들인 스냅샷" 정의 확인. ③ SWR-026 "스냅샷 생성 시각" 잔존 → R-SWE1-4-04 |
| R-SWE1-3-10 | 조치 확인 | SWR-021 요구 문장 "결정 이유를 SENSOR_FAULT로", 5.4절 결정 규칙표 신설, SWR-023 검증에서 "6과 7 동시" 제거 |
| R-SWE1-3-11 | 조치 확인 | A-15, Q-20 신설, SWR-035·SAF-003·SAF-005 출처에 표시. 전제 일부가 도달 불가인 점은 R-SWE1-4-03 ④ |
| R-SWE1-3-12 | 조치 확인 | SWR-046(역행·미래·누락 거절, 모든 입력 INVALID, 보관 값 불변, 검증 기준), A-11이 SWR-046을 가리킴 |
| R-SWE1-3-13 | 조치 확인 | SWR-025가 모든 출력의 초기값 정의, SWR-039와 "초기 출력 지정값(지정하지 않으면 RELEASE)"로 일치, 초기 state NORMAL은 "평가 전 초기값"으로 명시 |
| R-SWE1-3-14 | 조치 확인 | SWR-047 분리, SWR-013 검증에서 최초 갱신 전 문장 제거. SWR-032 둘째 문장은 SWR-044를 가리키는 참조 문장으로 봄 |
| R-SWE1-3-15 | 조치 확인 | iter-1.md 7줄 "검증 수단 준비 완료: … B1~B3, SWR-029, SWR-032, SWR-042, SWR-047 … PR #2로 병합" |
| O-01 (3회차) | 일부 반영 | A-13 "제품 동작에서는 기본값 RELEASE". 아키텍처 제한은 SWE.2에서 볼 일(의견 O-08로 유지) |
| O-02 (3회차) | 반영 확인 | SWR-021 "그 밖에는 warning_code를 NONE으로", 검증 기준에 INVALID·STALE 경우 |
| O-03 (3회차) | 반영 확인 | A-09가 iter-1.md 인도 증거 표와 같은 범위(G1, G2, G4) |
| O-04 (3회차) | 미반영 | OEM/README.md 8줄 "(미기재)" 그대로 (의견 O-05로 유지) |
| O-05 (3회차) | 미반영 | `swe1-review-3.md` "조치(작성자 기입)" 열이 비어 있음 (의견 O-06으로 유지) |
| O-06 (3회차) | 반영 확인 | SWR-028·041 출처에 "질의 Q-11" |

## 의견

| ID | 내용 |
|---|---|
| O-01 | now_s가 한 번 크게 역행(호출자 시계 재설정)하거나 상한 4294967.295 s(약 49.7일)에 도달하면 SWR-040에 따라 이후 모든 평가가 거절되어 출력·state가 동결된다. 그동안 충돌 CONFIRMED가 와도 해제되지 않고(SAF-001), state는 직전 값(NORMAL일 수 있음)으로 남아 DEGRADED가 보고되지 않는다. 정의는 있어 부적합으로 올리지 않지만, 시간 기준의 원점·재설정 가정과 지속 거절 시 안전 반응(예: 연속 거절을 DEGRADED로 보고)을 질의 사항으로 올리기를 권한다 |
| O-02 | 초기화 직후 state=NORMAL이면서 모든 input_validity=STALE인 출력은 SWR-035의 규칙과 맞지 않는 조합이다. 초기 출력을 "스냅샷 없는 첫 평가의 결과"(DEGRADED, INPUT_STALE)와 같게 두면 특수값이 하나 줄고 검증도 단순해진다 |
| O-03 | SWR-022 검증 "INVALID 평가 후 보관 값과 마지막 갱신 시각이 평가 전과 같다"는 5.3절 출력이 아닌 내부 값을 본다. 관측 가능한 결과(예: 그 뒤 유효 갱신 없이 이전 갱신 시각 + 200 ms를 넘긴 평가에서 STALE 대신 INVALID 유지, 또는 유효 갱신 시각 기준으로 STALE 경계가 계산됨)로 바꾸기를 권한다 |
| O-04 | 표 순서: 질의 Q-20이 Q-19 앞, 가정 A-14·A-15가 A-13 앞에 있다. A-11의 source_timestamp_s 검사 요약("숫자, 0 이상, 스냅샷 timestamp_s 이하")은 5.1절(상한, 1 ms 단위 포함)의 일부만 적었다. 정의는 5.1절 한 곳에 두고 가정은 참조만 하기를 권한다 |
| O-05 | OEM/README.md의 공개 승인 근거 위치가 "(미기재)"다(3회차 O-04 반복). 저장소가 Public이므로 근거를 기록하기를 권한다 |
| O-06 | 리뷰 기록의 "조치(작성자 기입)" 열을 채우면 재리뷰 증적이 분명해진다(2·3회차 반복) |
| O-07 | 0.1절 24줄 "FR-001~007, NFR-001~002"와 iter-1.md 5·6줄 "OEM-IF-001~005, OEM-IF-007~009", "OEM-FR-001~006"은 OEM ID 범위 표기다. 추적 ID(T13) 대상은 아니지만 7줄의 "범위 표기는 표 안팎 어디에도 쓰지 않는다" 방침과 맞추어 하나씩 쓰기를 권한다 |
| O-08 | SWR-039(초기 출력 LOCK 지정)는 안전 연결이 비어 있으나 SAF-001의 출력에 영향을 줄 수 있는 제품 인터페이스다. SWE.2에서 사용 제한과 근거를 적기를 권한다 |
| O-09 | **이터레이션 1 범위 대비 복잡도와 단순화 방향.** 1~4회차 지적의 대부분이 같은 영역(입력 상태 정의 ↔ 규칙표 ↔ state ↔ 초기·거절 처리)에서 반복되었다. 이번에도 그 영역에서 도달 불가 전제(R-SWE1-4-03), 용어 불일치(R-SWE1-4-05), 출력 구조 미정(R-SWE1-4-06)이 나왔다. 다음을 권한다. ① **freshness를 스냅샷 단위로 정의**: A-07에 따라 입력별 갱신 시각은 실제로 하나뿐이므로 "마지막으로 받아들인 스냅샷 시각" 하나로 STALE을 판정하고, 입력별로는 VALID/INVALID만 관리한다(입력별 input_validity의 STALE은 "스냅샷 STALE이고 INVALID 아님"으로 파생). 입력별 타이머, 도달 불가 조합, A-15·Q-20의 STALE 부분이 사라진다 ② **시각은 정수 ms로만 받는다**(A-12에서 시각 필드를 정수로 한정): 소수 자리 형식 규칙과 부동소수 판정 문제(R-SWE1-4-02)가 없어진다 ③ **입력 상태를 전이표 하나로 정의**: 사건(유효 값 수신, 무효·누락 수신, 스냅샷 거절, 스냅샷 없음, 200 ms 경과) × 현재 상태 → 다음 상태를 표로 두고 SWR-013, 022, 045, 046은 그 표의 행을 가리키게 하면 정의가 0.2절, SWR-045, SWR-046, A-14, A-08에 흩어져 생기는 불일치가 막힌다 ④ **초기 상태를 특수값 없이 정의**(O-02) ⑤ 규칙표의 규칙 5(이후 이터레이션 자리 표시)와 "새 명령" 서술은 이터레이션 2에서 추가해도 된다 ⑥ SWR-013과 SWR-027은 검증 기준(200 ms VALID, 210 ms STALE)이 같으므로 한쪽이 다른 쪽을 참조하게 하면 중복 검증이 줄어든다 |

## 체크리스트 개선 제안

| 체크리스트 | 제안 항목 | 계기가 된 결함 |
|---|---|---|
| CL-REQ | 상태 정의를 바꾼 개정에서는 기존 검증 기준·가정·질의의 전제 조합이 새 정의로도 도달 가능한지 다시 확인했다(CL-REQ-23 보강: 공통 시각을 공유하는 입력의 개별 STALE 등 조합 도달성) | R-SWE1-4-03 |
| CL-REQ | 실수로 받는 입력의 분해능·형식 규칙이 부동소수 표현에서 판정 가능하다(허용 오차·반올림 규칙이 있거나 정수 표현으로 받는다) | R-SWE1-4-02 |
| CL-REQ | 문서 머리말의 버전이 개정 이력의 최신 행과 같다 | R-SWE1-4-01 |
| CL-REQ | 입력별·항목별 출력(input_validity 등)의 원소 목록과 함수 반환 상태의 값 집합이 정의되어 있다(CL-REQ-22 보강) | R-SWE1-4-06, R-SWE1-4-07 |
| CL-SAF | SAF 문서가 참조하는 요구사항 명세의 절·표 위치와 용어가 실제와 일치한다 | R-SWE1-4-10 |
| 신규 (이터레이션 계획 `docs/iterations/iter-<n>.md`) | 2회차 제안 6 재게시(3번째): 대응표에 넣고, 범위 선언(대상·제외·선행 요구 ID)이 명세 0.1절·출처 열과 일치하는지 본다 | R-SWE1-4-08, R-SWE1-4-11 |
| 신규 (입력 문서 등록부 `OEM/README.md`) | 입력 문서의 판·베이스라인·배포 조건과 승인 근거가 기록되어 있다(3회차 제안 재게시) | 의견 O-05 |
