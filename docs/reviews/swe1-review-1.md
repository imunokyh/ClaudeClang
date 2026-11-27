# SWE.1 리뷰 1회

- 일자: 2026-11-27
- 대상 커밋: `98f78be` (브랜치 `feature/iter-1-input-guard`). 리뷰 대상 산출물은 **커밋되지 않은 작업 트리 상태**다(`docs/`, `OEM/` 미추적, `.claude/skills/requirements-analysis/` 3개 파일·`CLAUDE.md` 수정 상태).
- 입력(상위 산출물): `OEM/OEM-SWR-001_OEM SW 요구사항 사양서.docx` (Rev 1.0, BL-OEM-1.0). 대조는 텍스트 추출본(369줄)으로 전수 수행
- 보조 산출물: `docs/iterations/iter-1.md`, `.claude/skills/requirements-analysis/` 변경분(`trace_check.py`, `references/templates.md`, `references/traceability.md`)
- 리뷰어: stage-reviewer
- 판정: **불통과** (중대 6건, 경미 15건, 미확인 0건)

## 리뷰한 산출물과 적용 체크리스트

| 산출물 | 경로 | 체크리스트 | 항목 수 | 적합 | 부적합 | 해당 없음 | 미확인 |
|---|---|---|---|---|---|---|---|
| SW 요구사항 명세 | `docs/swe1/software-requirements.md` | CL-REQ | 15 | 5 | 10 | 0 | 0 |
| 기능 안전 요구사항 | `docs/swe1/safety-requirements.md` | CL-SAF | 5 | 3 | 1 | 1 | 0 |
| 추적성 검사 결과 | `trace_check.py --stage swe1` | CL-TRC | 3 | 1 | 1 | 1 | 0 |
| 공통 | 위 산출물 전체 | CL-COM | 4 | 3 | 0 | 1 | 0 |

`docs/iterations/iter-1.md`와 스킬 변경분은 대응 체크리스트가 없다(체크리스트 개선 제안 참고). 범위 선언 대조는 CL-REQ-12에, 스킬 변경 타당성은 CL-TRC-01과 "체크리스트 개발·개선" 3항에 묶어 판정했다.

## 도구 결과

| 도구 | 명령 | 결과(판정 줄) |
|---|---|---|
| 추적성 (SWE.1) | `docker compose run --rm dev python3 .claude/skills/requirements-analysis/scripts/trace_check.py --docs docs --src src --stage swe1` | `추적성 검사 (단계 swe1): SAF 3, SWR 15` / `판정: 통과 (양방향 추적성 100%)` (exit 0) |
| 추적성 (전 구간, 참고) | `docker compose run --rm dev tools/run.sh trace` (`--stage all`) | `판정: 실패 — 오류 18건` (전부 T1: 아키텍처 미작성. SWE.1 시점에서는 예상된 결과이며 SWE.1 판정에 쓰지 않음) |
| ID 정규식 경계 시험 | 컨테이너 `python3 -I`로 구·신 `ID_RE` 비교 | `OEM-IF-002` 신: 미인식(구: `IF-002` 오인식) — 수정 목적 달성. `SWR-023의`, `SWR-099를`, `REQ-F-001을` 신·구 모두 미인식(의견 O-01) |
| 동적 메모리·재귀 검사 도구 존재 여부 | `grep -rnE "malloc\|recurs\|재귀\|동적" tools/run.sh .claude/skills/implementation/scripts/ .claude/skills/architecture-design/scripts/` | 일치 없음 (R-SWE1-1-13 근거) |

## 체크리스트 판정 (산출물별)

### docs/swe1/software-requirements.md — CL-REQ

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-REQ-01 | 적합 | `분류` 열로 기능 8건(SWR-007, 008, 013, 021~025), 비기능 7건 구분. 종류 판별 맞음. ID 체계가 CLAUDE.md의 REQ-F/Q/L/C와 다른 점은 R-SWE1-1-18 |
| CL-REQ-02 | 적합 | 품질 3(SWR-026, 027, 030), 제한 1(SWR-031), 제약 3(SWR-028, 029, 032) |
| CL-REQ-03 | 적합 | 머리 5줄 "ISO 25010 적용 판: 2023". 품질 3건 모두 특성·부특성 있음 |
| CL-REQ-04 | 부적합 | SWR-026, 027 적합. SWR-030은 "같은 벡터"가 무엇인지(조건) 없음 → R-SWE1-1-14 |
| CL-REQ-05 | 부적합 | 9개 특성 모두 언급은 됨. Maintainability "해당 없음"과 "제약이 대신한다"가 모순, 공통 사유 "외부 표시 없음"이 OEM-IF-006과 모순 → R-SWE1-1-15 |
| CL-REQ-06 | 적합 | T12 오류 0건. SAF 정의는 safety-requirements.md에만 있음 |
| CL-REQ-07 | 부적합 | SWR-008, 028, 029, 030 복수 요구 → R-SWE1-1-10 |
| CL-REQ-08 | 부적합 | 차속 표현 불일치, 출력 열거값 미정, timestamp_s 범위 없음 → R-SWE1-1-11, R-SWE1-1-03 |
| CL-REQ-09 | 부적합 | "갱신", "fresh", "직전 확정 출력", "정규화 입력", "새 명령", "안전 관련 입력" 미정의 → R-SWE1-1-12, R-SWE1-1-02 |
| CL-REQ-10 | 부적합 | INVALID 판정 뒤 동작, timestamp_s 이상값, DEGRADED/FAULT 복귀가 정의도 질의도 없음 → R-SWE1-1-02, 03, 04 |
| CL-REQ-11 | 부적합 | 15건 모두 검증 기준 열은 있음. SWR-029의 도구가 해당 내용을 검사하지 못하고, SWR-031 문구가 반대 의미 → R-SWE1-1-13 |
| CL-REQ-12 | 부적합 | 이터 1 범위 OEM ID 12개 전수 대조: SR-001/003/004, IF-001/002/003/007/008/009 처리됨. IF-004 입력 검증 누락(R-SWE1-1-01), IF-005/006 처리 없음(R-SWE1-1-07). 범위 선언과 내용 불일치(R-SWE1-1-08), OEM 8절 인도 증거 처리 없음(R-SWE1-1-09) |
| CL-REQ-13 | 부적합 | 우선순위·freshness 조건이 요구사항 간에 모순 → R-SWE1-1-05, 06. SWR-030↔SWR-032, SWR-030↔OEM 할당 SWR-016 중복 → R-SWE1-1-08 |
| CL-REQ-14 | 부적합 | 질의 13건, 가정 7건 표 있음. 가정에 기대는 SWR-008, SWR-013 행에 가정 표시 없음(R-SWE1-1-16), 복귀 조건은 질의에도 없음(R-SWE1-1-04) |
| CL-REQ-15 | 적합 | 97줄 개정 이력에 변경 ID·사유·영향 산출물 있음. 범위 표기 사용은 R-SWE1-1-17(traceability.md 규약) |

### docs/swe1/safety-requirements.md — CL-SAF

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-SAF-01 | 적합 | SAF-001/003/004 모두 ASIL B, 안전 목표(미제공 — OEM-A 할당 입력), 안전 상태, FTTI 있음 |
| CL-SAF-02 | 적합 | 근거 문서 OEM-SWR-001 5절 명시. HARA·안전 목표 미제공은 OEM 1.2·2절이 범위 제외로 정했고 Q-10으로 관리. OEM ASIL B 입력 4건 중 3건 SAF화, SR-002는 이터 2(SAF-002 예약) |
| CL-SAF-03 | 부적합 | SAF-003 "형식·범위 오류 값은 출력에 영향 없음"은 기대 안전 반응이 관측 가능한 값이 아님. SAF-001 "이후 입력 주기에도 유지"는 관측 구간 없음 → R-SWE1-1-21 |
| CL-SAF-04 | 적합 | 수동 대조: SAF-001 ← SWR-007, 008, 023, 026 / SAF-003 ← SWR-013, 022, 024, 027 / SAF-004 ← SWR-021, 023, 025. 모든 SAF에 기능 요구 있음(T1은 SWE.2부터 도구 판정) |
| CL-SAF-05 | 해당 없음 | ASIL 분해 없음(3줄) |

### 추적성 — CL-TRC

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-TRC-01 | 적합 | `--stage swe1` 오류 0건(직접 실행). 스크립트 변경은 목적(SWR 인식, `OEM-IF-nnn` 오인식 제거)에 맞게 동작함을 경계 시험으로 확인 |
| CL-TRC-02 | 해당 없음 | 최초 작성(개정 없음). 하위 산출물 없음 |
| CL-TRC-03 | 부적합 | 원 요구사항 출처 전수 대조에서 OEM-IF-004/005/006 미연결 → R-SWE1-1-01, 07 |

### 공통 — CL-COM

| 번호 | 판정 | 비고 |
|---|---|---|
| CL-COM-01 | 적합 | 지정 경로, 한글 |
| CL-COM-02 | 적합 | SWR 15, SAF 3 중복 없음(T11 0건). Q-01~13, A-01~07 중복 없음(수동) |
| CL-COM-03 | 해당 없음 | 1회차 |
| CL-COM-04 | 적합 | `feature/iter-1-input-guard`, merge-base = `develop` HEAD `98f78be` |

## 지적 사항

| ID | 등급 | 산출물:위치 | 내용 | 근거(체크리스트 ID) | 조치(작성자 기입) |
|---|---|---|---|---|---|
| R-SWE1-1-01 | 중대 | software-requirements.md:9, 19, 80~91 | 범위가 "OEM-IF-001~009의 입력 검증"인데 OEM-IF-004(Driver→SW: side, action, source; "누락, 형식, 미등록 enum 거절")의 입력 검증이 SWR-022 출처와 5절 검사 규칙에 없다. A-07은 freshness 대상에서만 뺐을 뿐 검증 처리·이연 근거가 없다. 요구사항 누락 | CL-REQ-12, CL-TRC-03 | |
| R-SWE1-1-02 | 중대 | software-requirements.md:17, 19; safety-requirements.md:12 | SWR-022는 INVALID 값을 "평가에 사용하지 않는다"고만 하고 그 결과 동작이 없다: INVALID인 입력이 있을 때 state/출력/warning이 무엇인지, INVALID 수신이 SWR-013의 "갱신"으로 치는지(→ 200 ms 뒤 STALE이 되는지), 예컨대 sensor_fault나 crash_status가 INVALID이면 어떻게 되는지. OEM-SR-003 "비정상 입력을 정의된 방식으로 처리"를 충족하는 정의가 없고 검증 기준("출력 결정에 영향이 없다")도 기대값을 계산할 수 없다. 질의에도 없음 | CL-REQ-10, CL-REQ-09, CL-REQ-11 | |
| R-SWE1-1-03 | 중대 | software-requirements.md:17, 19, 74, 80~91 | freshness 판정의 유일한 시간 기준인 timestamp_s(ASIL B, SAF-003)에 검사 규칙이 5절에 없고(SWR-022 "모든 입력"과 불일치), 범위가 없으며, 역행·정지·누락 시 동작이 정의도 질의도 없다(A-02는 단조 증가를 가정만 함). source_timestamp_s의 용도(freshness 판정에 쓰는지)도 정해지지 않았다 | CL-REQ-10, CL-REQ-08 | |
| R-SWE1-1-04 | 중대 | software-requirements.md:17, 18, 21 | DEGRADED·FAULT 상태의 복귀 조건이 없다. 입력이 다시 갱신되거나 sensor_fault가 FALSE가 되면 NORMAL로 돌아가는지, 래치되는지, 복귀 후 첫 주기 출력이 무엇인지 요구사항·질의·가정 어디에도 없다. 상태 전이 요구 누락 | CL-REQ-10, CL-REQ-14 | |
| R-SWE1-1-05 | 중대 | software-requirements.md:18, 20; safety-requirements.md:11, 13 | SWR-021과 SAF-004는 sensor_fault=TRUE이면 무조건 "새 명령을 적용하지 않고 직전 확정 출력을 유지"하고, SWR-023·SAF-001은 충돌 CONFIRMED가 sensor_fault보다 우선해 RELEASE한다. 두 요구가 같은 입력(충돌 CONFIRMED + sensor_fault TRUE)에서 서로 다른 출력을 요구한다(SWR-024처럼 예외 문구가 없음). 또 이 동시 성립 시 state(FAULT?)와 warning_code가 정의되지 않았고, sensor_fault+DEGRADED 시 state=FAULT는 SWR-023 검증 기준에만 있고 요구 문장에 없다 | CL-REQ-13 | |
| R-SWE1-1-06 | 중대 | software-requirements.md:15, 20, 21; 75 | SWR-007·SWR-023은 "유효한" 충돌 CONFIRMED면 RELEASE, SWR-024·A-04는 "유효하고 fresh한" 경우에만 DEGRADED보다 우선한다. crash_status가 STALE인 채 마지막 값이 CONFIRMED일 때 SWR-007은 RELEASE, SWR-024는 유지를 요구해 모순이다. "유효"가 freshness를 포함하는지 정의가 없다(ASIL B) | CL-REQ-13 | |
| R-SWE1-1-07 | 경미 | software-requirements.md:9, 15~22 | 범위 선언 "OEM-IF-001~009"에 포함된 OEM-IF-005(lock_left/right 출력), OEM-IF-006(state, priority_reason, reason_code, input_validity 출력, HTTP 500)이 어느 요구의 출처에도 없고 "범위 아님(사유)" 처리도 없다. 해당 출력은 SWR-007, 008, 013, 021, 023에서 이미 쓰이고 있다 | CL-REQ-12, CL-TRC-03 | |
| R-SWE1-1-08 | 경미 | iter-1.md:6; software-requirements.md:22, 32 | iter-1.md는 OEM-FR-001~007, OEM-NFR-001~002를 "하지 않는 것"으로 선언했는데 SWR-025는 OEM-FR-007, SWR-030은 OEM-NFR-001(부분)을 출처로 한다. 또 OEM 9절은 OEM-NFR-001을 SWR-016에 할당했으므로 SWR-030은 이후 SWR-016과 중복된다(관계 미기재) | CL-REQ-12, CL-REQ-13 | |
| R-SWE1-1-09 | 경미 | software-requirements.md 전체, iter-1.md | OEM 8절 G1 필수 증거(SW 요구사항, **Use Case**, 검토기록, 양방향 추적)와 G2~G4 증거(UML, SBOM, 정적분석 계획, Web 결과)가 어느 문서·이터레이션에서도 처리(수행 또는 범위 아님·사유)되지 않았다 | CL-REQ-12 | |
| R-SWE1-1-10 | 경미 | software-requirements.md:16, 32, 46, 47 | 원자성 위반: SWR-008(CONFIRMED 유지 + PENDING 처리), SWR-028(C11 + Clang 23 + 경고 0건), SWR-029(동적 메모리 + 재귀 + void 금지 + 단일 return), SWR-030(결정론 품질 + 난수·실시간 시계 금지 제약, SWR-032와 중복) | CL-REQ-07 | |
| R-SWE1-1-11 | 경미 | software-requirements.md:15~22, 77, 84~91 | 입·출력 명세 불완전: ① vehicle_speed_kph가 5절은 0.0~300.0(km/h), A-06은 0.1 km/h 단위 정수 0~3000으로 서로 다르고 분해능 미기재(경계값 시험 기준이 정해지지 않음) ② 출력 열거값 미정: input_validity(VALID/INVALID/STALE 외?), priority_reason, reason_code(CRASH_PENDING 외 충돌 해제·FAULT·DEGRADED 시 값), warning_code 전체 ③ 5절의 gear, crash_status, boolean 입력에 "존재"(누락) 조건이 빠져 OEM-IF 오류 처리 열("누락, 형식 오류 INVALID")과 다르다 ④ C 구현에서 "누락"·"형식 오류"를 어떻게 표현하는지 질의·가정 없음 | CL-REQ-08 | |
| R-SWE1-1-12 | 경미 | software-requirements.md:17, 18, 21, 22; safety-requirements.md:12, 13 | 정의 없는 용어: "마지막 갱신", "fresh", "직전 확정 출력"(무엇이 '확정'인가), "정규화 입력", "새 명령"(이터 1에는 명령 입력이 없음), SAF-003의 "안전 관련 입력"과 SWR-013의 "필수 입력"의 관계 | CL-REQ-09 | |
| R-SWE1-1-13 | 경미 | software-requirements.md:40, 47 | SWR-029 검증 기준 `tools/run.sh rules`, `static`은 동적 메모리·재귀를 검사하지 않는다(규칙 스크립트·run.sh에 해당 검사 없음, grep 확인). SWR-031 검증 기준 "루프 상한과 동적 메모리가 없음을 분석"은 '상한 없는 루프가 없음'의 반대 의미이고, 실행 시간 측정 플랫폼·최악 입력 조건이 없다 | CL-REQ-11 | |
| R-SWE1-1-14 | 경미 | software-requirements.md:32 | SWR-030 측정 기준에 재생할 벡터(입력 순서·길이·초기 상태)가 정해져 있지 않아 조건이 없다 | CL-REQ-04 | |
| R-SWE1-1-15 | 경미 | software-requirements.md:34 | ISO 25010 미적용 사유: Maintainability를 "해당 없음"에 넣고 같은 문장에서 "SWR-029와 CLAUDE.md가 대신한다"고 해 적용 여부가 모순. 공통 사유 "외부 표시 … 입력이 없다"는 범위에 든 OEM-IF-006(SW→Display)과 모순. Compatibility(OEM 인터페이스 계약 IF-001~009와의 상호운용), Flexibility(Python 3.12 PC/SIL→C11 편차 A1)는 특성별 사유가 없다 | CL-REQ-05 | |
| R-SWE1-1-16 | 경미 | software-requirements.md:16, 17 | 열린 질의에 기댄 요구가 가정 표시 없이 확정 요구처럼 쓰였다: SWR-008(Q-03 가정, 출처에 OEM-SR-001만), SWR-013(A-02 시간 표현, A-07 필수 입력 범위, 출처에 OEM-SR-003만). SWR-023·024·025는 가정을 출처에 적었으므로 일관성도 깨짐 | CL-REQ-14 | |
| R-SWE1-1-17 | 경미 | software-requirements.md:97 | 개정 이력 변경 ID에 범위 표기 `SWR-021~032`를 썼다. traceability.md "범위 표기는 인식하지 못하므로 쓰지 않는다" 위반(도구는 SWR-021만 인식함을 확인) | traceability.md 규약, CL-REQ-15 | |
| R-SWE1-1-18 | 경미 | .claude/skills/requirements-analysis/SKILL.md:12~20, 57; .claude/skills/aspice-stage-review/references/checklists.md:37~38; CLAUDE.md:108 | `SWR-nnn`+`분류` 열 규칙을 templates.md·traceability.md·trace_check.py에만 넣고, 같은 스킬의 SKILL.md 분류 체계·"모든 요구사항(REQ-*, SAF-*)", 체크리스트 CL-REQ-01/02, CLAUDE.md 1절 요구사항 분류(REQ-Q/L/C 표기)는 고치지 않았다. 승인 결정 A3(iter-1.md)과 최종 기준 문서가 불일치 | checklists.md "체크리스트 개발·개선" 3항, CLAUDE.md 1절 | |
| R-SWE1-1-19 | 경미 | CLAUDE.md:7 | "과제 요구사항이 정해지면 이 절에 과제 이름과 입력 문서 위치를 추가한다"를 지키지 않았다(과제 이름 '전자식 차일드락 제어 SW', 입력 `OEM/OEM-SWR-001_…docx` 미기재) | CLAUDE.md 프로젝트 절 | |
| R-SWE1-1-20 | 경미 | safety-requirements.md:12 | SAF-003 한 문장에 ① 유효성·freshness 사전 확인 ② 200 ms 초과 시 100 ms 이내 DEGRADED ③ 형식·범위 오류 거절의 세 요구가 있다. templates.md "안전 요구사항도 한 문장 한 요구 규칙을 따른다" 위반 | templates.md, CL-REQ-07(준용) | |
| R-SWE1-1-21 | 경미 | safety-requirements.md:11, 12 | 기대 안전 반응이 관측 가능하지 않음: SAF-003 "형식·범위 오류 값은 출력에 영향 없음"(어떤 출력·state가 관측되어야 하는지 없음), SAF-001 "이후 입력 주기에도 유지"(관측 구간 없음, SWR-008은 100주기) | CL-SAF-03 | |

## 의견

| ID | 내용 |
|---|---|
| O-01 | `trace_check.py` 변경은 타당하다(앞쪽 `(?<![\w-])`로 `OEM-IF-002`의 `IF-002` 오인식 제거, `SWR` 접두어 추가, `REQ`·T11/T12 동작 유지 확인). 다만 끝의 `\b`는 기존과 같아 한글 조사가 바로 붙은 ID(`SWR-023의`, `SWR-099를`, `REQ-F-001을`)를 인식하지 못한다. 연결 누락(T1 등 거짓 오류)은 안전한 쪽이지만 존재하지 않는 ID 참조(T11)를 놓친다. 끝을 `(?!\d)`로 바꾸고 회귀 시험을 두는 것을 권한다. 현재 문서의 영향: 15줄 `SWR-023의`, 21줄 `SWR-007이`가 연결로 집계되지 않음 |
| O-02 | 산출물·입력 문서(`docs/`, `OEM/`)와 스킬 변경이 커밋되지 않은 상태로 리뷰되었다. 재리뷰는 커밋된 해시를 대상으로 해야 리뷰 기록의 증적이 성립한다 |
| O-03 | `tools/run.sh trace`는 `--stage all`이라 SWE.2 산출물이 생기기 전까지 실패한다(T1 18건). 단계 진행 중 `all` 결과 보고 시 이 점을 명시할 것 |

## 체크리스트 개선 제안

| 체크리스트 | 제안 항목 | 계기가 된 결함 |
|---|---|---|
| CL-REQ | 상태(모드)가 있으면 모든 상태의 진입·복귀 조건과 동시 성립 시 상태 우선순위가 정의되어 있다 | R-SWE1-1-04, 05 |
| CL-REQ | 요구사항이 쓰는 용어(상태명, 열거값, "유효", "fresh" 등)가 용어·데이터 사전에 정의되어 있다 | R-SWE1-1-11, 12 |
| CL-REQ | 이상 입력을 "사용하지 않는다"로 끝내지 않고 그 결과 출력·상태가 정의되어 있다 | R-SWE1-1-02 |
| CL-REQ | 검증 기준에 적은 도구·방법이 실제로 그 요구를 검사할 수 있다 | R-SWE1-1-13 |
| CL-REQ | 원 문서의 ID 없는 요구(제품 경계, 인도 증거, 법규)도 처리 또는 범위 아님(사유)으로 남아 있다 | R-SWE1-1-09 |
| CL-SAF | SAF가 한 문장 한 요구(원자성)이다 | R-SWE1-1-20 |
| 신규 (이터레이션 계획 `docs/iterations/iter-<n>.md`) | 범위 선언(대상·제외 ID)이 요구사항 명세의 출처 열과 일치한다 | R-SWE1-1-08 |
| 신규 (검사 스크립트 변경) | 스크립트 변경 시 경계 사례 시험(앞뒤 문자, 한글 조사)이 있고 관련 SKILL.md·체크리스트·CLAUDE.md가 같은 변경에서 갱신되었다 | O-01, R-SWE1-1-18 |
