# SWE.1 리뷰 3회

- 일자: 2026-11-27
- 대상 커밋: `a722ff7` (브랜치 `feature/iter-1-input-guard`, 분기 원점 `8379ca2` = `develop`, 작업 트리 변경 없음 확인)
- 입력(상위 산출물): `OEM/OEM-SWR-001_OEM SW 요구사항 사양서.docx` (Rev 1.0, BL-OEM-1.0). 대조는 텍스트 추출본으로 전수 수행
- 이전 리뷰: `docs/reviews/swe1-review-1.md`, `docs/reviews/swe1-review-2.md` (2회차 부적합 20건, 의견 4건)
- 리뷰 대상: `docs/swe1/software-requirements.md`(v0.3), `docs/swe1/safety-requirements.md`(v0.3), `docs/iterations/iter-1.md`, `OEM/README.md`. 2회차 R-SWE1-2-15/18/20 확인을 위해 `develop`에 병합된 PR #2(`98eae34`: `trace_check.py` T13, `check_banned.py` B1~B3, `tools/run.sh selftest`, 체크리스트, CLAUDE.md, 스킬)도 대조
- 적용 체크리스트: CL-REQ-01~21, CL-SAF-01~06, CL-TRC-01~03, CL-COM-01~04
- 리뷰어: stage-reviewer
- 판정: **불통과** (중대 2건, 경미 13건, 미확인 0건, 의견 6건)

## 리뷰한 산출물과 적용 체크리스트

| 산출물 | 경로 | 체크리스트 | 항목 수 | 적합 | 부적합 | 해당 없음 | 미확인 |
|---|---|---|---|---|---|---|---|
| SW 요구사항 명세 | `docs/swe1/software-requirements.md` | CL-REQ | 21 | 11 | 10 | 0 | 0 |
| 기능 안전 요구사항 | `docs/swe1/safety-requirements.md` | CL-SAF | 6 | 5 | 0 | 1 | 0 |
| 추적성 검사 결과 | `trace_check.py --stage swe1`, `--stage all` | CL-TRC | 3 | 3 | 0 | 0 | 0 |
| 공통 | 위 산출물 전체, `iter-1.md`, `OEM/README.md` | CL-COM | 4 | 3 | 1 | 0 | 0 |

- CL-SAF에는 SAF 간·SAF와 SWR 간 모순을 보는 항목이 없어, 안전 요구의 모순(R-SWE1-3-02)은 CL-REQ-13(요구사항 간 모순)을 근거로 지적했다(2회차와 같은 방식). 체크리스트 개선 제안에 올린다.
- `docs/iterations/iter-1.md`, `OEM/README.md`는 여전히 대응표에 전용 체크리스트가 없다. 범위 선언은 CL-REQ-21, 나머지는 CL-COM으로 판정했다.

## 도구 결과

| 도구 | 명령 | 결과(판정 줄) |
|---|---|---|
| 추적성 (SWE.1) | `docker compose run --rm dev python3 .claude/skills/requirements-analysis/scripts/trace_check.py --docs docs --src src --stage swe1` | `추적성 검사 (단계 swe1): SAF 4, SWR 27` / `판정: 통과 (양방향 추적성 100%)` (exit 0). 문서에서 직접 센 정의 수와 일치: SWR 27(기능 16: 007, 008, 036, 013, 021, 022, 034, 024, 035, 023, 044, 025, 045, 039, 040, 033 / 품질 2: 026, 027 / 제한 1: 031 / 제약 8: 028, 041, 037, 029, 042, 038, 043, 032), SAF 4 |
| 추적성 (전 구간, CL-TRC-02) | `docker compose run --rm dev tools/run.sh trace` | `판정: 실패 — 오류 31건`. T1이 아닌 오류를 걸러 보면 0건. 31건 = SWR 27 + SAF 4의 T1(아키텍처 미연결)로 SWE.2 전 예상 결과. SWE.1 판정에 쓰지 않음 |
| 검사 스크립트 자체 시험 | `docker compose run --rm dev tools/run.sh selftest` | `Ran 5 tests … OK`(trace_check: SWR 인식, 상위 ID 무시, 한글 조사, 4자리 숫자, 범위 표기 검출), `Ran 7 tests … OK`(check_banned: 정상, malloc, time, 직접·상호 재귀, 주석·문자열 무시, 비재귀 호출) / `판정: 통과` |
| 금지 호출 검사 연결 (CL-REQ-19) | `tools/run.sh` 107~115줄 읽기, `docker compose run --rm dev tools/run.sh rules` | `cmd_rules`가 `check_banned.py --src src`를 호출함을 확인. 실행 결과는 `건너뜀: rules — src/ 에 C 소스가 없음`(SWE.1 시점 정상). `check_banned.py`의 ALLOC·CLOCK 집합과 B2 호출 그래프 순환 검사가 SWR-029, SWR-042, SWR-032의 대상을 다룸을 코드로 확인 |
| 범위 표기 검사(T13) 적용 범위 | `trace_check.py` 80~100줄 읽기 | T13은 표 셀만 검사한다. 표 밖 본문(software-requirements.md 7줄)은 도구가 보지 않음 → R-SWE1-3-05에서 수동 판정 |
| v0.2 대비 변경 대조 (CL-REQ-15) | `git show 8d46c7e:docs/swe1/software-requirements.md`와 현재 파일의 ID 행 diff | 실제로 바뀐 행: 007, 008, 013, 021, 022, 023, 024, 025, 026, 027, 028, 029, 032, 033, 034, 035, 036, 038, 신설 039~045(039, 040, 041, 042, 043, 044, 045). SWR-031 행은 바뀌지 않음 → R-SWE1-3-05 |
| 브랜치·커밋 | `git log --oneline -8`, `git merge-base HEAD develop`, `git status --short`, `git diff develop...HEAD --stat`, `git log -1 --format=%B 98eae34`, `gh pr view 2` | 이터레이션 브랜치에는 산출물·리뷰 기록만 있음(7파일). 도구·체크리스트 변경은 PR #2(`98eae34`)로 분리됨. 다만 커밋·PR 본문에 체크리스트 항목별 출처 리뷰·지적 ID가 없음 → R-SWE1-3-08 |
| ID 고유성 (CL-COM-02) | `grep -oE "^\| (Q\|A)-[0-9]+"` 집계 | Q-01~19, A-01~13 32건 중복 없음. T11 0건 |

## 체크리스트 판정 (산출물별)

### docs/swe1/software-requirements.md — CL-REQ

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-REQ-01 | 적합 | `분류` 열로 기능 16, 비기능 11. 종류 판별 맞음(SWR-039는 시험 지원 기능이지만 SW가 제공하는 기능이므로 기능으로 본다. 의견 O-01) |
| CL-REQ-02 | 적합 | 품질 2, 제한 1, 제약 8 |
| CL-REQ-03 | 적합 | 5줄 "ISO 25010 적용 판: 2023", SWR-026(Performance efficiency/Time behaviour), SWR-027(Reliability/Fault tolerance) |
| CL-REQ-04 | 적합 | SWR-026: 300 ms, 10 ms 주기, 고정 시계. SWR-027: T+200 VALID, T+210 STALE, T+300 이내, 고정 시계 전제. (T의 정의 문제는 R-SWE1-3-09) |
| CL-REQ-05 | 적합 | 9개 특성 전부. Interaction capability 사유를 "운전자 명령 검사는 입력 검사 기능이며 사용성 요구가 아니다"로 고쳐 SWR-033과의 모순이 없어짐 |
| CL-REQ-06 | 적합 | T12 0건 |
| CL-REQ-07 | 부적합 | SWR-032에 독립 검증 내용 셋(시계 함수 호출 금지, 난수 함수 호출 금지, 시간을 호출자가 입력으로 넘김)과 검증 수단 둘 → R-SWE1-3-14 |
| CL-REQ-08 | 부적합 | 스냅샷 도착 주기(평가 주기와의 관계)가 요구·가정 어디에도 없음 → R-SWE1-3-01. now_s·timestamp_s 정수 ms 내부 표현의 상한·폭·넘침과 1 ms 미만 값의 변환 규칙 없음 → R-SWE1-3-06 |
| CL-REQ-09 | 적합 | 검증 불가능한 표현 없음 |
| CL-REQ-10 | 적합 | 범위 밖·무효·타임아웃·시각 역행 모두 정의(SWR-022, 034, 024, 013, 040, A-11)되어 있거나 질의(Q-07, Q-17, Q-18, Q-19)에 있음. 정의 내용의 결함은 CL-REQ-16·18에서 판정 |
| CL-REQ-11 | 부적합 | timestamp_s 거절(스냅샷 전체 거절)을 검증하는 기준이 어느 요구에도 없음 → R-SWE1-3-12. SWR-023 검증 "6과 7 동시"는 규칙 7(그 밖)의 정의상 성립할 수 없는 조합 → R-SWE1-3-10 |
| CL-REQ-12 | 적합 | OEM ID(SR 4, FR 7, NFR 2, IF 9) 전수 대조: 0.1절이 모두 처리 |
| CL-REQ-13 | 부적합 | SAF-003·SAF-005의 "직전 확정 출력 유지"에 충돌 해제 예외가 없어 SAF-001·SWR-007과 모순 → R-SWE1-3-02. SWR-025(초기 직전 확정 출력 RELEASE 고정)와 SWR-039(호출자 지정) 모순, 초기 state NORMAL과 A-05(모든 입력 STALE) 불일치 → R-SWE1-3-13. SAF-001 둘째 문장 → R-SWE1-3-03. SWR-013 검증 기준과 SWR-045 중복 → R-SWE1-3-14 |
| CL-REQ-14 | 부적합 | SWR-007, SAF-001(A-04 의존), SAF-004(A-03·Q-16 의존) 출처에 가정 표시 없음 → R-SWE1-3-04. state의 FAULT > DEGRADED 우선(OEM-SR-003 수용기준 "DEGRADED 상태"를 바꿈)에 대응하는 가정·질의 없음 → R-SWE1-3-11. timestamp_s 거절 동작이 가정(A-11)에만 있고 요구가 아님 → R-SWE1-3-12 |
| CL-REQ-15 | 부적합 | 7줄 범위 표기 `SWR-001~021` 잔존(2회차 지적 대상), 0.3 행 변경 ID에 바뀌지 않은 SWR-031 포함 → R-SWE1-3-05 |
| CL-REQ-16 | 부적합 | INVALID 표시가 다음 평가로 이어지지 않아, 스냅샷이 없는 다음 평가에서 INVALID였던 입력이 더 오래된 보관 값으로 VALID로 복귀(원인 해소 없이 DEGRADED 탈출) → R-SWE1-3-01. 초기 상태에서 reason_code, priority_reason, warning_code, input_validity, command_status 값 없음 → R-SWE1-3-13 |
| CL-REQ-17 | 부적합 | "DEGRADED 조건", "fresh", "스냅샷 생성 시각", "받아들인 스냅샷", SWR-040의 "출력"·"직전 평가"가 정의되지 않았거나 0.2절과 다르게 쓰임 → R-SWE1-3-09 |
| CL-REQ-18 | 부적합 | now_s 거절 평가에서 같이 들어온 스냅샷·운전자 명령의 처리, reason_code·warning_code·command_status 값, 다음 평가의 "직전 평가 now_s" 기준이 없음 → R-SWE1-3-07. INVALID 검출 뒤 결과의 지속 기간 → R-SWE1-3-01 |
| CL-REQ-19 | 적합 | SWR-029(B1), SWR-042(B2), SWR-032(B3)의 검증 수단이 존재하고 `rules`에 연결되며 자체 시험 통과(직접 실행). SWR-028(`-std=c11`), SWR-041, SWR-037(`build`), SWR-038(RT1), SWR-043(RT2, RT3)은 기존 도구 |
| CL-REQ-20 | 적합 | OEM 8절 G1~G4 증거를 iter-1.md 인도 증거 표가 모두 처리(산출물 지정 또는 범위 밖 사유). 3절 실행 환경(Python)은 Q-11·A1(언어 C 확정), Web은 A2, 7절 법규는 Q-13, 9절 번호는 7줄 번호 체계로 처리. A-09 문구 불일치는 의견 O-03 |
| CL-REQ-21 | 부적합 | iter-1.md 7줄 "구현 작업에 포함: rules에 금지 함수·호출 순환·시계 함수 검사 추가(SWR-029, SWR-032 검증 수단)"가 이미 병합된 PR #2와 맞지 않고 SWR-042(재귀)·B1~B3 체계를 반영하지 않음 → R-SWE1-3-15 |

### docs/swe1/safety-requirements.md — CL-SAF

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-SAF-01 | 적합 | SAF-001/003/004/005 모두 ASIL B, 안전 목표(미제공 — OEM-A 할당), 안전 상태, FTTI 있음 |
| CL-SAF-02 | 적합 | 근거 OEM-SWR-001 5절. SR-001/003/004 → SAF-001/003·005/004, SR-002 → SAF-002 예약 |
| CL-SAF-03 | 적합 | 네 SAF 모두 고장 주입 조건과 관측 가능한 기대 반응. SAF-001 관측 구간(100번, 1 s) 추가 확인. LOCK 전제는 SWR-039로 도달 가능. 용어 "fresh"는 R-SWE1-3-09 |
| CL-SAF-04 | 적합 | 수동 대조: SAF-001 ← SWR-007, 008, 036, 023, 026 / SAF-003 ← SWR-013, 024, 035, 027, 044, 045, 040 / SAF-004 ← SWR-021, 035, 023, 025 / SAF-005 ← SWR-022, 034, 035, 044. T1은 SWE.2부터 도구 판정 |
| CL-SAF-05 | 해당 없음 | ASIL 분해 없음(4줄) |
| CL-SAF-06 | 적합 | SAF마다 안전 상태·FTTI 하나 |

### 추적성 — CL-TRC

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-TRC-01 | 적합 | `--stage swe1` 오류 0건(직접 실행) |
| CL-TRC-02 | 적합 | 개정(v0.3) 뒤 `--stage all` 직접 실행. 31건 전부 T1(하위 산출물 없음) |
| CL-TRC-03 | 적합 | OEM ID 전수 대조, 출처 열·0.1절에 모두 연결 |

### 공통 — CL-COM

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-COM-01 | 적합 | 지정 경로, 한글 |
| CL-COM-02 | 적합 | SWR 27, SAF 4(T11 0건), Q 19, A 13 중복 없음 |
| CL-COM-03 | 부적합 | 2회차 지적 20건 중 8건 조치 불충분(아래 "이전 지적 조치 확인"). 2회차 체크리스트 개선 제안 미반영 → R-SWE1-3-08 |
| CL-COM-04 | 적합 | `feature/iter-1-input-guard`, 분기 원점 `develop`(`8379ca2`) |

## 핵심 개정부(입력 상태, 결정 규칙, state, 이유·경고·명령 코드) 조합 점검

리뷰어가 0.2절 정의, SWR-023 규칙 1~7, SWR-035, SWR-021, SWR-033, SWR-040, A-08, A-11로 아래 조합의 결과를 직접 계산했다. 결과가 하나로 정해지지 않거나 요구끼리 어긋나는 것만 지적으로 올렸다.

| 조합 | 출력 | reason_code | state | warning_code | 판정 |
|---|---|---|---|---|---|
| 충돌 CONFIRMED(VALID) + sensor_fault TRUE(VALID) + 다른 입력 INVALID | RELEASE | CRASH_CONFIRMED | FAULT | SENSOR_FAULT | 일관 |
| 충돌 CONFIRMED(VALID) + 다른 입력 STALE | RELEASE (SWR-007, 023) / 유지 (SAF-003) | CRASH_CONFIRMED | DEGRADED | NONE | **SAF-003 모순** (R-SWE1-3-02) |
| 충돌 CONFIRMED(VALID) + 다른 입력 INVALID | RELEASE (SWR-007) / 유지 (SAF-005) | CRASH_CONFIRMED | DEGRADED | NONE | **SAF-005 모순** (R-SWE1-3-02) |
| 충돌 CONFIRMED, crash_status 자신이 STALE | 유지 | INPUT_STALE | DEGRADED | — | 일관(Q-17 열림) |
| PENDING(VALID) + sensor_fault TRUE(VALID) | 유지 | SENSOR_FAULT | FAULT | SENSOR_FAULT | 일관. 단 SENSOR_FAULT 이유를 정하는 요구 문장 없음 (R-SWE1-3-10) |
| PENDING(VALID) + 다른 입력 INVALID / STALE | 유지 | INPUT_INVALID / INPUT_STALE | DEGRADED | NONE | 일관 |
| sensor_fault INVALID(직전 VALID TRUE) | 유지 | INPUT_INVALID | DEGRADED | NONE(A-08 추론) | 일관. 그러나 스냅샷 없는 다음 평가에서 FAULT로 복귀 (R-SWE1-3-01) |
| sensor_fault STALE(보관 값 TRUE) + 다른 입력 VALID | 유지 | INPUT_STALE | DEGRADED | 요구 문장 없음(A-08 추론으로 NONE) | 의견 O-02 |
| sensor_fault TRUE(VALID) + 다른 입력 STALE | 유지 | SENSOR_FAULT | FAULT | SENSOR_FAULT | 결과는 일관. FAULT > DEGRADED 근거 없음 (R-SWE1-3-11) |
| 스냅샷 timestamp_s > now_s (A-11 거절) | 유지 | INPUT_INVALID | DEGRADED | NONE | 결과는 계산되나 거절 자체가 요구·검증 없음 (R-SWE1-3-12) |
| now_s 역행 (SWR-040 거절) | 불변 | 불명 | 불변 | 불명 | **불명** (R-SWE1-3-07) |
| now_s 거절 평가에 운전자 명령 동반 | 불변 | 불명 | 불변 | — | command_status **불명** (R-SWE1-3-07) |
| 초기화 직후(평가 전) | RELEASE 또는 SWR-039 지정값 | 불명 | NORMAL | 불명 | **SWR-025/039 모순, 초기값 불명** (R-SWE1-3-13) |
| 초기화 후 스냅샷 없이 첫 평가 | 유지 | INPUT_STALE | DEGRADED | NONE | 일관(SWR-045) |
| 평가 k에 crash_status INVALID(직전 갱신 50 ms 전 CONFIRMED), 평가 k+1 스냅샷 없음 | k: 유지 / k+1: RELEASE | k: INPUT_INVALID / k+1: CRASH_CONFIRMED | k: DEGRADED / k+1: NORMAL | — | 정의대로이나 더 새 정보(무효)를 버리고 오래된 값으로 판정 (R-SWE1-3-01) |

## 지적 사항

| ID | 등급 | 산출물:위치 | 내용 | 근거(체크리스트 ID) | 조치(작성자 기입) |
|---|---|---|---|---|---|
| R-SWE1-3-01 | 중대 | software-requirements.md:30, 33, 34, 50, 52, 133; safety-requirements.md:14 | 0.2절 "입력 상태"는 "이번 평가가 스냅샷을 받지 않았으면 INVALID는 없다. INVALID 표시는 다음 평가에 이어지지 않는다"로 정했다. 그 결과 ① 어떤 입력이 평가 k의 스냅샷에서 INVALID이고 그 전 VALID 갱신이 200 ms 이내였다면, 스냅샷이 없는 평가 k+1에서 그 입력은 다시 VALID가 되고 판정 값은 평가 k에 받은 정보(무효)보다 오래된 보관 값이 된다(예: sensor_fault TRUE → INVALID → 다음 평가 FAULT 복귀, 또는 sensor_fault FALSE 보관 값으로 NORMAL 복귀). ② SAF-005(ASIL B)의 안전 상태 "DEGRADED 조건, 직전 확정 출력 유지"와 SWR-034의 DEGRADED·INPUT_INVALID가 원인(그 입력의 최신 값이 무효)이 없어지지 않았는데도 스냅샷을 받은 평가 한 번만 지속된다. A-08 "원인이 없어진 평가에서 바로 NORMAL"과도 맞지 않는다(원인은 해소되지 않고 새 데이터가 없을 뿐이다). ③ 이 동작의 크기는 스냅샷이 평가(10 ms)마다 오는지에 달려 있는데, 0.2절 "평가"는 스냅샷을 선택 입력으로 두고 스냅샷 도착 주기나 평가 주기와의 관계를 요구·가정(A-01은 평가 주기만)·질의 어디에도 정하지 않았다. 해결 방향(작성자 선택): INVALID를 그 입력이 다시 VALID로 갱신될 때까지 유지하거나, "스냅샷은 매 평가마다 온다"를 가정으로 두고 질의에 올리거나, INVALID 이후 이전 보관 값을 판정에 쓰지 않도록 정의할 것 | CL-REQ-16, CL-REQ-18, CL-REQ-08 | |
| R-SWE1-3-02 | 중대 | safety-requirements.md:12, 14 | SAF-003 요구 문장("STALE이면 … DEGRADED 조건을 성립시키고 … 직전 확정 출력을 유지한다")과 안전 상태 열("직전 확정 출력 유지"), SAF-005 요구 문장("… DEGRADED 조건을 성립시키며 직전 확정 출력을 유지한다")과 안전 상태 열에 충돌 해제 예외가 없다. crash_status가 VALID·CONFIRMED이고 다른 필수 입력이 STALE/INVALID이면 SAF-001("다른 모든 명령보다 우선 … RELEASE")·SWR-007 검증("다른 입력 INVALID/STALE여도 RELEASE")·SWR-023 규칙 1은 RELEASE, SAF-003·005는 유지를 요구한다. SAF-004는 같은 상황을 "충돌 해제(SAF-001)는 예외로 우선한다", 안전 상태 "(충돌 CONFIRMED이면 RELEASE)"로 처리했는데 SAF-003·005만 빠졌다. SWR 쪽(SWR-034 "규칙 1, 2가 성립하지 않으면", SWR-024 "규칙 1~3이 성립하지 않으면")은 맞으므로 ASIL B 안전 요구가 하위 SWR과 어긋난다 | CL-REQ-13 | |
| R-SWE1-3-03 | 경미 | safety-requirements.md:11 | R-SWE1-2-04 조치 불충분. SWR-008은 "입력 상태 VALID로 CONFIRMED"로 고쳤고 SAF-001 첫 문장도 고쳤으나, 2회차가 지적한 SAF-001 둘째 문장 "CONFIRMED가 유지되는 동안 매 평가에서 RELEASE를 유지한다"는 그대로여서 입력 상태 조건이 없다. 검증 기준도 "CONFIRMED가 유지되면 매번 RELEASE"다. STALE인 CONFIRMED 보관 값에 대해 SWR-024는 RELEASE를 적용하지 않는다. 이터레이션 1에서는 규칙 3·4의 유지가 RELEASE를 내므로 관측값은 같지만, 요구 문장의 조건이 SWR-008과 다르다(등급은 관측 차이가 없어 경미로 내림) | CL-REQ-13, CL-REQ-17 | |
| R-SWE1-3-04 | 경미 | software-requirements.md:44; safety-requirements.md:11, 13 | R-SWE1-2-16 조치 불충분. SWR-021은 출처에 A-03을 넣어 조치됨. 그러나 SWR-007의 "입력 상태가 VALID(= freshness 포함)일 때만 해제"는 OEM-SR-001("유효 충돌 입력")을 A-04·Q-17로 해석한 것인데 출처는 "OEM-SR-001, OEM-IF-002, OEM-IF-005"뿐이다(같은 내용의 SWR-024는 A-04 표시). SAF-001(같은 VALID 조건)의 출처도 OEM-SR-001뿐이고, SAF-004의 "충돌 해제(SAF-001)는 예외로 우선한다"는 A-03·Q-16(열림)에 기대는데 출처는 OEM-SR-004뿐이다 | CL-REQ-14 | |
| R-SWE1-3-05 | 경미 | software-requirements.md:7, 184 | R-SWE1-2-17 조치 불충분. 개정 이력 표의 범위 표기는 고쳤으나 2회차가 함께 지적한 7줄 `SWR-001~021`이 남아 있다(T13은 표 셀만 검사하므로 도구로 잡히지 않음. traceability.md "범위 표기는 … 쓰지 않는다"). 또 0.3 행 "변경 ID"에 v0.2(`8d46c7e`)와 비교해 바뀌지 않은 SWR-031이 들어 있다(행 diff로 확인) | CL-REQ-15, traceability.md "표현 방법" | |
| R-SWE1-3-06 | 경미 | software-requirements.md:47, 58, 127, 148, 149, 152 | R-SWE1-2-10 ① 조치 불충분. 5.1절에 now_s·timestamp_s·source_timestamp_s의 내부 표현 "정수 ms"를 넣었으나 상한(또는 정수 폭)과 넘침 동작이 없고, A-12는 원시 값을 "정수 또는 실수"로 받는다면서 초 단위 실수를 정수 ms로 바꿀 때 1 ms 미만 자리의 처리(버림·반올림·거절)가 없다(A-02 "소수 셋째 자리가 1 ms"는 분해능만 말함). 이 변환은 STALE 경계(200 ms 초과)와 SWR-040의 역행 판정 결과를 바꾼다. ②(C 표현)는 A-12·Q-19로 조치 확인 | CL-REQ-08 | |
| R-SWE1-3-07 | 경미 | software-requirements.md:35, 54, 58, 59 | SWR-040의 거절 흐름이 정해지지 않았다. ① 거절 평가에 같이 들어온 스냅샷과 운전자 명령을 버리는지(SWR-040은 "보관 값을 바꾸지 않는다"만 있음), 그 평가의 command_status가 무엇인지(SWR-033 "명령이 없는 평가는 NONE"과 SWR-040 "출력을 바꾸지 않는다" 중 무엇이 적용되는지) ② "출력"의 범위: reason_code, priority_reason, warning_code, command_status, input_validity가 불변인지(5.3절은 모두 출력이다) ③ 거절된 평가 뒤의 "직전 평가의 now_s"가 거절된 평가의 값인지 마지막으로 받아들인 평가의 값인지(예: 100 → 50(거절) → 80) ④ 거절된 평가가 0.2절 "직전 확정 출력(직전 평가가 확정해 낸)"의 "직전 평가"인지. SWR-044도 ①에서 거절되면 ②~④를 수행하지 않는지 말하지 않는다 | CL-REQ-18, CL-REQ-17 | |
| R-SWE1-3-08 | 경미 | 커밋 `98eae34`, PR #2 본문; `.claude/skills/aspice-stage-review/references/checklists.md`; `.claude/skills/requirements-analysis/SKILL.md`:58 | 도구·체크리스트를 별도 chore PR로 분리한 것은 확인(R-SWE1-2-20 일부 조치). 남은 것: ① 체크리스트 변경이 도구·CLAUDE.md·CI 변경과 한 커밋에 있고, 커밋·PR 본문은 "이터레이션 1 SWE.1 리뷰에서 지적된"이라고만 적어 어느 회차·어느 지적에서 비롯됐는지 항목별로 없다(R-SWE1-2-20 미조치 부분. 병합된 이력은 고칠 수 없으므로 후속 chore 커밋 본문이나 이 기록의 조치 열에 항목별 출처를 남길 것) ② R-SWE1-2-18이 지적한 SKILL.md 58줄 "모든 요구사항(REQ-*, SAF-*)"이 그대로다(77줄은 조치됨) ③ 2회차 "체크리스트 개선 제안" 6건(출력 값별 생성 요구, 같은 출력을 정하는 요구 간 우선 관계, 검증 전제의 도달 가능성, 가정이 용어 정의를 바꾸지 않음, 상태 표시의 해제 조건, 이터레이션 계획 체크리스트)이 checklists.md에 반영되지 않았다(CL-REQ는 21에서 끝남) | checklists.md "체크리스트 개발·개선" 2·3·5항, CLAUDE.md 1절 "리뷰에서 체크리스트가 놓친 결함이 나오면 항목을 추가한다", CL-COM-03 | |
| R-SWE1-3-09 | 경미 | software-requirements.md:47, 50, 67, 68, 136, 149; safety-requirements.md:12, 13, 14 | 용어가 0.2절에 없거나 다르게 쓰인다. ① "DEGRADED 조건"(SWR-013, 034, 027, SAF-003·005의 요구 문장과 안전 상태 열): 정의 없음. SWR-035의 DEGRADED 판정 조건인지, state=DEGRADED인지 정해지지 않았고 SAF-003·005의 안전 상태가 관측 가능한 값이 아니다 ② "fresh"(SAF-004 검증 기준): 0.2절은 VALID/STALE로 바꿨는데 남아 있음 ③ "스냅샷 생성 시각 T"(SWR-026, R-SWE1-2-09의 "도착 시점"을 바꾼 말): 0.2절에 없고 timestamp_s와 같은지 적지 않았다(SAF-001은 "스냅샷 시각 T") ④ "직전에 받아들인 스냅샷"(A-11, 5.1절 timestamp_s 행): 일부 입력만 INVALID인 스냅샷이 "받아들인" 것인지 정의가 없어 다음 스냅샷의 역행 판정 기준이 둘로 읽힌다 | CL-REQ-17 | |
| R-SWE1-3-10 | 경미 | software-requirements.md:48, 53 | R-SWE1-2-03 ② 일부 미조치. 결정 이유 값 중 CRASH_CONFIRMED(SWR-007), INPUT_INVALID(SWR-034), INPUT_STALE(SWR-024), CRASH_PENDING(SWR-036), NONE(SWR-023 규칙 7)은 요구 문장이 정하지만 SENSOR_FAULT는 어느 요구 문장도 정하지 않고 검증 기준(SWR-021, 023, 036)에만 나온다(SWR-023 규칙 2는 "sensor_fault(SWR-021)"이라고만 적음). 또 SWR-023은 "다음 표의 규칙"이라 하지만 표가 없고, 검증 기준 "6과 7 동시 → CRASH_PENDING"은 규칙 7이 "그 밖"이라 동시에 성립할 수 없다 | CL-REQ-13, CL-REQ-11 | |
| R-SWE1-3-11 | 경미 | software-requirements.md:52, 128, 133; safety-requirements.md:12, 14 | SWR-035와 SAF-003·005는 sensor_fault(VALID TRUE)와 STALE/INVALID가 동시이면 state를 FAULT로 정한다. 이는 OEM-SR-003 수용기준 "100 ms 이내 DEGRADED 상태가 되고"를 바꾸는 해석인데 근거 가정·질의가 없다. SWR-035의 출처 A-08은 복귀(래치 없음)만 다루고, A-03·Q-02·Q-16은 출력 우선순위만 다룬다 | CL-REQ-14 | |
| R-SWE1-3-12 | 경미 | software-requirements.md:31, 33, 49, 136, 149 | 스냅샷 timestamp_s 검사와 스냅샷 전체 거절은 A-11(가정)과 0.2절 용어 정의에만 있고 이를 요구하는 SWR이 없다. SWR-022는 "스냅샷의 모든 차량 입력"을 검사한다고 하는데 0.2절 "스냅샷"은 timestamp_s를 차량 입력과 구분한다. 그 결과 timestamp_s 역행·미래 시각·누락을 주입해 모든 입력이 INVALID가 되는지 확인하는 검증 기준이 어디에도 없다 | CL-REQ-11, CL-REQ-14, CL-REQ-18 | |
| R-SWE1-3-13 | 경미 | software-requirements.md:35, 55, 57, 130, 138 | 초기 상태가 모순되고 빠져 있다. ① SWR-025는 "초기화 직후 … 직전 확정 출력을 RELEASE로" 무조건 정하고 SWR-039는 "직전 확정 출력을 호출자가 지정"하게 해 서로 어긋나며, SWR-039로 LOCK을 지정했을 때 초기화 직후 lock_left·lock_right 조회값이 무엇인지 없다(SWR-039 검증 "조회하면 LOCK"의 조회 대상 불명) ② SWR-025는 초기 state를 NORMAL로 두지만 A-05·SWR-045는 첫 스냅샷 전 모든 입력을 STALE로 보므로 SWR-035 규칙상 DEGRADED에 해당한다. NORMAL의 출처로 적은 OEM-FR-007은 ignition off 시 RELEASE·OFF를 말할 뿐 NORMAL을 정하지 않는다 ③ 초기화 직후 reason_code, priority_reason, warning_code, input_validity, command_status 값이 없다 | CL-REQ-13, CL-REQ-16 | |
| R-SWE1-3-14 | 경미 | software-requirements.md:47, 56, 96 | ① SWR-032에 독립 검증 가능한 내용 셋(시스템 시계 함수 호출 금지, 난수 함수 호출 금지, 시간을 호출자가 입력으로 넘김)과 검증 수단 둘(B3, 고정 시계 시험)이 있다(난수는 v0.3에서 추가) ② SWR-013 검증 기준 "최초 갱신 전 평가는 모든 입력이 STALE"이 SWR-045와 같은 내용이다(0.2절 STALE 정의 "한 번도 갱신되지 않은 경우"와도 중복) | CL-REQ-07, CL-REQ-13 | |
| R-SWE1-3-15 | 경미 | iter-1.md:7 | "구현 작업에 포함: `tools/run.sh rules`에 금지 함수(malloc 계열)·호출 순환·시계 함수 검사 추가(SWR-029, SWR-032 검증 수단)"가 사실과 다르다. 이 검사는 PR #2로 이미 `develop`에 병합되었고(B1~B3, `cmd_rules`), 재귀 금지는 SWR-042로 분리되어 계획서의 요구 ID와 명세가 어긋난다 | CL-REQ-21 | |

## 이전 지적 조치 확인 (2회차)

| 이전 ID | 확인 결과 | 근거(산출물·도구) |
|---|---|---|
| R-SWE1-2-01 | 조치 확인(정의) | 0.2절 입력 상태(INVALID > STALE > VALID, INVALID 비지속, 스냅샷 거절 포함), 판정 값 정의, A-11. ①~⑤ 각각 정의로 결과가 계산됨. 정의가 만든 새 결함은 R-SWE1-3-01, 거절의 요구화·검증은 R-SWE1-3-12 |
| R-SWE1-2-02 | 조치 확인(state) | SWR-013·034는 "DEGRADED 조건"으로 바뀌고 state는 SWR-035 하나가 정함. SAF-003·005 검증에 "(FAULT 동시이면 FAULT)". 새 결함: 출력 쪽 충돌 예외 누락 R-SWE1-3-02, FAULT 우선 근거 R-SWE1-3-11, 용어 R-SWE1-3-09 |
| R-SWE1-2-03 | **조치 불충분** | ① 규칙 1~7(PENDING 포함, INVALID > STALE) 확인 ③ 0.2절 "결정 이유"로 priority_reason = reason_code ④ A-08 ⑤ A-10·규칙 순서. ② SENSOR_FAULT 이유를 정하는 요구 문장 없음 → R-SWE1-3-10 |
| R-SWE1-2-04 | **조치 불충분** | SWR-008 확인. SAF-001 둘째 문장 미조치 → R-SWE1-3-03 |
| R-SWE1-2-05 | 조치 확인 | A-07 "갱신 시각은 항상 스냅샷의 timestamp_s", A-11 "source_timestamp_s는 freshness에 쓰지 않음", Q-18 |
| R-SWE1-2-06 | 조치 확인 | 0.2절 "평가"에 운전자 명령, SWR-033에 ACCEPTED/REJECTED/NONE와 출력·state 불변. 거절 평가와의 관계는 R-SWE1-3-07 |
| R-SWE1-2-07 | 조치 확인 | 73줄 Interaction capability 사유 수정 |
| R-SWE1-2-08 | 조치 확인 | SWR-035 "같은 평가 안에서", SWR-044 순서, 검증 "갱신된 평가에서 NORMAL" |
| R-SWE1-2-09 | **조치 불충분** | "도착 시점"을 "스냅샷 생성 시각"으로 바꿨으나 이 용어도 정의가 없음 → R-SWE1-3-09 ③ |
| R-SWE1-2-10 | **조치 불충분** | ② A-12·Q-19 확인. ① 상한·폭·넘침·변환 규칙 없음 → R-SWE1-3-06 |
| R-SWE1-2-11 | 조치 확인 | SWR-039(초기 출력 지정)·A-13으로 LOCK 전제 도달 가능, 검증 기준에 "LOCK으로 설정" 명시. 새 모순은 R-SWE1-3-13 |
| R-SWE1-2-12 | 조치 확인 | iter-1.md 5·6줄과 0.1절 일치(IF-004 검사·거절만, IF-005 값 정의, IF-006 이터 3), 0.1절 IF-006 행에 priority_reason 포함 |
| R-SWE1-2-13 | 조치 확인 | 028→041, 029→042, 038→043, 023→044, 025→045 분리 확인. 새 복합 요구는 R-SWE1-3-14 |
| R-SWE1-2-14 | 조치 확인 | iter-1.md 인도 증거 표에 G1 검토기록·양방향 추적, G2 정적분석 계획, G4 Web 결과·미검증 범위·잔여문제 처리 추가 |
| R-SWE1-2-15 | 조치 확인 | `check_banned.py` B1~B3, `cmd_rules` 연결, `selftest` 7건 통과(직접 실행). SWR-029, 042, 032 검증 기준이 B1, B2, B3을 가리킴 |
| R-SWE1-2-16 | **조치 불충분** | SWR-021(A-03) 확인. SWR-007, SAF-001, SAF-004 미조치 → R-SWE1-3-04 |
| R-SWE1-2-17 | **조치 불충분** | 개정 이력 표의 범위 표기 제거, 0.2 행 변경 ID 보완 확인. 7줄 범위 표기 잔존, 0.3 행 SWR-031 과다 기재 → R-SWE1-3-05 |
| R-SWE1-2-18 | **조치 불충분** | CLAUDE.md 112줄, CL-REQ-02, CL-ARC-10, CL-STS-05, SKILL.md 18·19·77줄 확인. SKILL.md 58줄 미조치 → R-SWE1-3-08 ② |
| R-SWE1-2-19 | 조치 확인 | SAF-001 검증 "이후 100번의 평가(10 ms 간격, 1 s)" |
| R-SWE1-2-20 | **조치 불충분** | 도구·체크리스트를 PR #2(`feature/chore-` 브랜치)로 분리 확인. 항목별 출처 기록 없음, 체크리스트 변경이 별도 커밋 아님 → R-SWE1-3-08 ① |
| O-01 (2회차) | 반영 확인 | CLAUDE.md 15줄 요청자 이름과 위임 근거("알아서 해줘", 2026-11-27) 기재, iter-1.md의 "요청자 이름 확인 필요" 삭제. 호출자 전달에 따르면 사용자 메시지가 근거이며 리뷰어는 대화 원문을 직접 볼 수 없다 |
| O-02 (2회차) | 미반영 | `swe1-review-2.md` "조치(작성자 기입)" 열이 여전히 비어 있음(의견 O-05로 유지) |
| O-03 (2회차) | 반영 확인 | Q-17 신설(STALE인 충돌 입력) |
| O-04 (2회차) | 반영 확인 | T13과 `test_range_notation_is_detected` 추가, selftest 통과 |

개발 언어: 3줄 머리말 9줄, Q-11(언어 닫힘), iter-1.md A1, SWR-028·041 근거 열에 "OEM이 C(Clang)로 확정(2026-11-27, 사용자 전달)"이 반영되었고 Python 3.12 가정은 폐기로 적혀 있다(OEM 원문 3절 "Python 3.12 PC/SIL"과의 차이가 기록됨). SWR-028·041의 `출처` 열은 CLAUDE.md만 적었다(의견 O-06).

## 의견

| ID | 내용 |
|---|---|
| O-01 | SWR-039(초기 직전 확정 출력 지정)는 시험 지원용인데 제품 인터페이스로 남는다. 제품 동작에서 LOCK으로 초기화될 수 있는 경로가 안전(SAF-001)에 영향을 주지 않도록 아키텍처에서 제한(시험 빌드 한정이 아닌 방법으로)하고 근거를 적기를 권한다 |
| O-02 | sensor_fault가 STALE/INVALID이고 보관 값이 TRUE일 때 warning_code는 SWR-021과 A-08로 NONE이 추론되지만 요구 문장에 없다. "그 밖에는 NONE"을 SWR-021에 쓰고 A-08을 출처에 넣으면 해석 여지가 없어진다 |
| O-03 | A-09(가정)는 Use Case, UML, SBOM, 결함 연결만 적어 iter-1.md 인도 증거 표(G1 검토기록, G2 정적분석 계획, G4)와 범위가 다르다. 맞추기를 권한다 |
| O-04 | OEM/README.md는 Synetics의 "과정 밖 배포·공개는 사전 서면 승인" 조건에 대해 승인 근거 위치를 "(미기재)"로 두었다. 저장소가 Public이므로 승인 근거를 기록하기 전까지 공개 여부를 사용자와 다시 확인하기를 권한다 |
| O-05 | 리뷰 기록의 "조치(작성자 기입)" 열을 채우면 재리뷰 증적이 분명해진다(2회차 O-02 반복) |
| O-06 | SWR-028·041의 `출처`에 Q-11(언어 확정, 사용자 전달 2026-11-27)을 함께 적으면 OEM 원문 3절(Python 3.12)과의 차이 근거가 추적된다 |

## 체크리스트 개선 제안

| 체크리스트 | 제안 항목 | 계기가 된 결함 |
|---|---|---|
| CL-SAF | SAF끼리, SAF와 하위 SWR 사이에 모순이 없다(다른 SAF의 우선 예외가 모든 관련 SAF의 요구 문장·안전 상태 열에 반영되어 있다) | R-SWE1-3-02 |
| CL-REQ | 상태 표시(INVALID 등)의 해제가 "원인 해소"로만 일어나며, 새 데이터가 없다는 이유만으로 더 오래된 값이 다시 판정에 쓰이지 않는다. 입력 도착 주기와 평가 주기의 관계가 정해져 있다 | R-SWE1-3-01 |
| CL-REQ | 동작을 정하는 내용이 가정(A-nn)에만 있지 않고, 요구(SWR)와 검증 기준으로 옮겨져 있다 | R-SWE1-3-12 |
| CL-REQ | 모든 출력(5.3절 값 집합)의 초기값과, 평가가 거절될 때 각 출력의 값이 정해져 있다 | R-SWE1-3-07, R-SWE1-3-13 |
| CL-REQ | 표 밖 본문에서도 범위 표기를 쓰지 않는다(T13은 표만 검사하므로 `읽기`로 확인) | R-SWE1-3-05 |
| 신규 (이터레이션 계획) | 2회차 제안 재게시: 이터레이션 계획을 대응표에 넣고 범위 선언과 "구현 작업에 포함" 같은 계획 항목이 명세·병합 이력과 일치하는지 본다 | R-SWE1-3-15 |
| 신규 (입력 문서 등록부 `OEM/README.md`) | 입력 문서의 판·베이스라인·배포 조건과 승인 근거가 기록되어 있다 | 의견 O-04 |
| CL-REQ (2회차 제안 5건) | 2회차 "체크리스트 개선 제안" 표의 CL-REQ 5건을 반영할 것(미반영) | R-SWE1-3-08 ③ |
