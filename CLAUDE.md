# CLAUDE.md

이 파일은 Claude Code(claude.ai/code)가 이 저장소에서 작업할 때 참고하는 지침이다.

## 프로젝트

ClaudeClang — "AI를 이용한 C 코드". 차량 SW를 A-SPICE 절차에 따라 C로 개발하고, Clang/LLVM 툴체인으로 빌드·검증한다. 과제 요구사항이 정해지면 이 절에 과제 이름과 입력 문서 위치를 추가한다.

### 사람 개입(Human in the Loop) 설정

- **요청자 이름**: (미설정 — 사용자에게 확인해서 이 줄에 적는다)

코드 주석의 `AI 작성`의 `요청자`와 사용자 리뷰 기록에 이 이름을 쓴다. 이름을 모르면 지어내지 말고 사용자에게 묻는다.

### 디렉터리 구조

| 경로 | 내용 |
|---|---|
| `src/<컴포넌트>/` | 제품 코드 (C11, `.c`/`.h`) |
| `tests/tdd/<모듈>/` | TDD 테스트 (GoogleTest). 단위 테스트와 분리해 별도 결과·별도 커버리지로 낸다 |
| `tests/unit/<모듈>/` | 단위 테스트 (GoogleTest, SWE.4), `tests/unit/stubs/` 스텁 |
| `tests/integration/` | 통합 테스트 (GoogleTest, SWE.5), `tests/integration/stubs/` HW·OS 경계 스텁 |
| `docs/swe1/` ~ `docs/swe6/` | 단계별 산출물 (한글) |
| `docs/iterations/` | 이터레이션 계획·결과 (`iter-<n>.md`) |
| `docs/reviews/` | 단계 리뷰 기록 |
| `docker/Dockerfile`, `compose.yaml` | 빌드 이미지(`claudeclangbuild`)와 Jenkins 서비스 |
| `ci/jenkins/` | Jenkins 이미지, 플러그인 목록, 설정(JCasC) |
| `.github/workflows/ci.yml` | **CI(GitHub Actions)**: PR 게이트이자 매일 정기 점검 |
| `Jenkinsfile` | 사내 Jenkins 파이프라인(같은 검사를 주기적으로 재실행) |
| `tools/run.sh` | 컨테이너 안에서 빌드·테스트·품질 검사를 실행하는 진입점 (개발과 CI 공용) |
| `tools/Doxyfile`, `tools/cloc_report.py`, `tools/ci_summary.py` | Doxygen 설정, 주석률·커버리지 요약 |
| `.claude/hooks/check-metrics.sh` | 구현이 끝났을 때 Lizard 기준을 검사하는 Stop 훅 |
| `build/` | 빌드 산출물과 보고서 (`build/reports/`). 커밋하지 않는다 |

### Claude 스킬과 서브에이전트

A-SPICE 단계마다 작성 스킬이 있고, 모든 산출물은 `aspice-stage-review`로 리뷰한다. 검사 스크립트는 해당 스킬의 `scripts/`에 둔다.

| 단계 | 스킬 (`.claude/skills/`) | 스킬 안의 도구 |
|---|---|---|
| SWE.1 요구사항 분석 | `requirements-analysis` | `trace_check.py` (양방향 추적성) |
| SWE.2 아키텍처 설계 | `architecture-design` | `check_interfaces.py` (인터페이스 경유 접근) |
| SWE.3 상세 설계 | `detailed-design` | |
| SWE.3 구현 | `implementation`, `test-driven-development`, `static-analysis` | `check_return_rules.py` (반환 규칙), `check_hitl_comments.py` (HITL 주석) |
| SWE.4 단위 검증 | `unit-test-mcdc` | `llvm_cov_run.sh`, `coverage_gate.py`, `mcdc_gaps.py`, `pict_cases.py`, `check_test_docs.py` |
| SWE.5 통합 검증 | `integration-test-coverage` | (단위 검증 스킬의 도구 공용) |
| SWE.6 SW 검증 | `system-test-case-generator` | `build_workbook.py`, `run_pict.py` |
| 모든 단계 리뷰 | `aspice-stage-review` (산출물별 체크리스트) | |

| 서브에이전트 (`.claude/agents/`) | 역할 |
|---|---|
| `stage-reviewer` (Opus) | **리뷰어. 각 단계 산출물을 필수로 리뷰한다.** 봐주지 않고 엄격하게 본다 |
| `unit-test-engineer`, `integration-test-engineer`, `system-test-case-designer`, `static-analyzer` | 해당 단계 작업을 대신 수행할 수 있는 보조 에이전트. 기본은 메인 에이전트가 같은 이름의 스킬로 직접 한다 |

- `test-driven-development`: github.com/obra/superpowers의 TDD 스킬(MIT). ClaudePython 프로젝트에서 그대로 가져왔다. 본문 예시는 TypeScript/pytest이며, 이 저장소에서는 테스트 실행 명령을 `docker compose run --rm dev tools/run.sh tdd`(TDD 테스트)로 바꿔 읽는다.
- `system-test-case-generator`, `system-test-case-designer`: ClaudePython 프로젝트에서 그대로 가져왔다. 엑셀 생성 스크립트와 `pict.exe`는 Windows 호스트에서 실행할 수 있다(검증은 컨테이너에서도 `tools/run.sh system`으로 한다).

---

## 1. 개발 절차

개발은 **A-SPICE(SWE.1~SWE.6)**를 기준으로 한다.

### 생명주기: 애자일 이터레이션

- 개발 생명주기는 애자일 형식이다. **분석, 아키텍처 설계, 상세 설계, 구현, 단위 테스트, 통합 테스트, 시스템 테스트를 하나의 이터레이션**으로 한다. 이터레이션 하나가 다음 단계를 모두 통과해야 끝난다.
- 이터레이션은 작게 자른다. 시작할 때 **대상 요구사항 ID**를 정해 사용자와 합의하고 `docs/iterations/iter-<n>.md`에 범위를 적는다. 이후 이터레이션은 앞 이터레이션 산출물을 개정하며 이어 간다(요구사항이 바뀌면 영향받는 산출물을 함께 고친다).

| 순서 | 단계 | A-SPICE | 산출물 | 사용 스킬 |
|---|---|---|---|---|
| 1 | 요구사항 분석 | SWE.1 | `docs/swe1/software-requirements.md`, `safety-requirements.md` | `requirements-analysis` |
| 2 | 아키텍처 설계 | SWE.2 | `docs/swe2/architecture.md` | `architecture-design` |
| 3 | 상세 설계 | SWE.3 | `docs/swe3/detailed-design.md` | `detailed-design` |
| 4 | 구현 | SWE.3 | `src/` 소스 코드, `tests/tdd/` | `implementation`, `test-driven-development`, `static-analysis` |
| 5 | 단위 테스트 | SWE.4 | `tests/unit/`, `docs/swe4/` 명세·결과서 | `unit-test-mcdc` |
| 6 | 통합 테스트 | SWE.5 | `tests/integration/`, `docs/swe5/` 명세·결과서 | `integration-test-coverage` |
| 7 | 시스템 테스트 | SWE.6 | `docs/swe6/` 테스트 명세 엑셀과 결과 | `system-test-case-generator` |

### 수행 주체

- **개발은 기본적으로 메인 에이전트가 수행**하며, 각 단계에 맞는 스킬을 사용한다. 서브에이전트에 위임하는 것은 예외이고(컨텍스트가 큰 반복 작업을 분리할 때 등), **리뷰는 예외 없이 `stage-reviewer`**가 한다.
- 앞 단계 산출물이 없거나 리뷰를 통과하지 못했으면 다음 단계로 넘어가지 않는다. 작은 수정도 영향받는 산출물을 함께 고친다.

### 단계 리뷰 (매 단계 종료 시 필수)

- 분석, 아키텍처 설계 등 **각 단계가 완료되면 Reviewer 서브에이전트(`stage-reviewer`)가 그 단계의 모든 산출물을 리뷰**한다. 상세 설계(3)와 구현(4)은 따로 리뷰한다. 리뷰는 `aspice-stage-review` 스킬의 **산출물별 체크리스트**(`.claude/skills/aspice-stage-review/references/checklists.md`)를 기준으로 하며 항목마다 판정을 남긴다.
- **부적합 사항(중대, 경미 모두)이 전부 조치되어야 다음 단계로 진행**할 수 있다. 판정은 `통과`(부적합 0건, 미확인 0건)와 `불통과` 둘뿐이며 조건부 통과는 없다. `불통과`면 모든 부적합을 조치하고 회차를 올려 재리뷰를 받으며, 0건이 될 때까지 반복한다.
- 산출물을 만든 세션·에이전트는 자기 산출물을 리뷰하지 않는다. 리뷰 기록은 `docs/reviews/<단계>-review-<회차>.md`에 남긴다.
- `stage-reviewer`는 봐주지 않고 엄격하게 본다(전 항목, 전수 검사, 도구 직접 실행). 다만 모든 지적에는 위치와 근거가 있어야 하며, 근거 없는 것은 `의견`이다.
- 새 종류의 산출물이 생기면 체크리스트를 먼저 만든다. 리뷰에서 체크리스트가 놓친 결함이 나오면 항목을 추가한다.

### 이터레이션 완료와 PR

1. 이터레이션의 모든 단계가 리뷰를 통과하면 **사용자에게 결과를 보고하고 확인을 받는다.** 보고에는 범위(요구사항 ID), 단계별 리뷰 판정과 기록 경로, `tools/run.sh all` 결과(판정 줄), 남은 질의 사항을 담는다.
2. **사용자의 확인을 받은 뒤에야 PR 절차를 진행**한다. 확인 없이 PR을 만들지 않는다.
3. 사용자가 코드 리뷰를 마쳤다고 알려주면 그 함수들의 HITL 주석을 `완료`로 갱신한다(아래 2절 "주석"). PR 전에 `tools/run.sh hitl-reviewed`가 통과해야 한다.
4. PR을 만들고 CI(GitHub Actions)를 통과시킨다(3절).

### 구현은 TDD로, 테스트는 단위 테스트와 분리

- 구현(4)은 **TDD 스킬**(`test-driven-development`)을 이용해 TDD 방식(red-green-refactor)으로 한다.
- **TDD로 만든 테스트 코드는 단위 테스트 코드와 분리한다.** `tests/tdd/`에 두고, CMake 타깃(`tests/tdd/CMakeLists.txt`)도 따로 만든다. 단위 테스트(`tests/unit/`)는 5단계에서 상세 설계로부터 새로 도출하며, TDD 테스트를 옮기거나 복사하지 않는다.
- **결과도 별도**: `tools/run.sh tdd` → `build/reports/tdd/`(테스트 결과, 커버리지, HTML). 성공률 100%여야 한다.
- **커버리지도 별도로 측정**한다. TDD 테스트의 커버리지는 단위·통합 테스트의 커버리지와 합치지 않는다. 측정·보고만 하며 기준 판정은 하지 않는다(CI에서 "TDD 테스트 커버리지" 결과로 따로 보인다).

### 요구사항과 추적성

- **요구사항 분류**: 기능 / 비기능으로 나누고, 기능 안전 요구사항(SAF)은 별도 문서(`safety-requirements.md`)에서 관리한다. 비기능은 품질(REQ-Q, ISO 25010 준수), 제한(REQ-L), 제약사항(REQ-C)으로 나눈다.
- **양방향 추적성**: 모든 요구사항은 아키텍처(CMP/IF) → 상세 설계(MOD/MIF/FN) → 구현(`@trace`) → 단위·통합 테스트(UT/IT)와 양방향으로 연결한다. 단계마다 `trace_check.py`로 검증하고, 요구사항 개정 시 전 구간(`--stage all`)을 다시 검사한다.

---

## 2. 소스코드 품질 기준

아래 기준은 PR 병합 조건이며 CI(GitHub Actions)가 검사한다. 사내 Jenkins도 같은 기준을 주기적으로 검사한다. 하나라도 만족하지 못하면 병합하지 않는다.

### 테스트 커버리지

| 테스트 | 기준 | 명령 |
|---|---|---|
| 시스템 테스트 (SWE.6) | **요구사항 커버리지 100%** | `tools/run.sh system` (`build_workbook.py` 검증: 요구사항별 TC 연결, 양방향 추적성) |
| 통합 테스트 (SWE.5) | **함수 커버리지 100%**, **테스트 성공률 100%** | `tools/run.sh integration` |
| 단위 테스트 (SWE.4) | **MC/DC 커버리지 100%**, **테스트 성공률 100%** | `tools/run.sh unit` |

- 단위·통합 테스트는 기준에 도달할 때까지 테스트를 추가하고 다시 측정하는 것을 반복한다. 모든 `src/` 파일이 측정되어야 한다(테스트에 링크되지 않은 파일도 실패).
- 커버리지는 LLVM(`llvm-cov`, MC/DC 포함)으로 측정한다. 측정 제외(제외 주석, 필터 확대)는 금지한다.
- TDD 테스트는 별도로 측정·보고한다(1절).

### 정적 분석

- **Cppcheck 룰 대상 100% 만족**. 룰 대상은 `warning`, `style`, `performance`, `portability`이며 `--inconclusive --check-level=exhaustive`로 검사한다.
- **예외 처리는 없다.** `cppcheck-suppress`, 억제 파일, 억제 옵션을 쓰지 않는다(쓰면 그 자체로 실패). `src/`의 모든 `.c`는 빌드에 포함되어 분석되어야 한다.
- 명령: `tools/run.sh static`

### 소스코드 매트릭

| 항목 | 기준 | 도구 |
|---|---|---|
| 순환 복잡도 | 함수마다 **10 이하** | Lizard (`-C 10`) |
| 함수 라인수 | 함수마다 **80줄 이하** | Lizard (`-L 80`) |

- 명령: `tools/run.sh complexity`
- **구현이 끝났을 때 Hook으로 검사한다.** Claude Code Stop 훅(`.claude/hooks/check-metrics.sh`, `.claude/settings.json`)이 `src/`의 C 소스가 바뀌었으면 Lizard를 실행하고, 위반이 있으면 종료를 막고 고치게 한다. 훅이 한 번 막은 뒤에도 위반이 남으면 경고만 보인다(무한 반복 방지). 최종 게이트는 CI다.
- 초과한 함수는 분할한다(`static-analysis` 스킬). 기준을 올리지 않는다.

### 주석

- 주석은 **Doxygen 형식**으로 한글로 쓴다. 모든 함수(static 포함)에 `@brief`, `@param`, `@return`, `@trace`를 단다. 형식 오류·문서화 누락은 `tools/run.sh docs`(Doxygen 경고 = 실패)가 잡는다.
- **Human in the Loop 항목을 반드시 포함한다.** 모든 함수 정의 위 Doxygen 블록에 다음을 쓴다(`tools/run.sh hitl`이 검사, 형식도 Doxygen을 따른다).
  - **AI 작성**: 어느 AI 모델이(`모델`), 언제(`일시`), 누가 요청해서(`요청자`) 작성했는가
  - **사용자 리뷰**: 사용자가 리뷰를 끝낸 코드인가(`상태`: `미완료` | `완료`). 완료했다면 언제(`일시`), 누가(`리뷰어`) 했는가

  ```c
  /**
   * @brief 차속과 도어 상태로 잠금 명령을 결정한다.
   * @trace FN-001
   * @param speed_kph 차속 (km/h, 0~250)
   * @param door_closed 모든 도어가 닫혀 있으면 true
   * @param cmd 출력: 잠금 명령 (NULL 불가)
   * @return STATUS_OK, STATUS_ERR_PARAM (cmd가 NULL), STATUS_ERR_RANGE (speed_kph > 250)
   * @par AI 작성
   * - 모델: claude-opus-5-5
   * - 일시: 2026-11-27
   * - 요청자: (CLAUDE.md "요청자 이름")
   * @par 사용자 리뷰
   * - 상태: 미완료
   */
  ```

  - 사용자 리뷰가 끝나면 같은 구역을 `상태: 완료`, `리뷰어: <이름>`, `일시: <날짜>`로 바꾼다. **`완료`는 사용자가 그 리뷰를 했다고 직접 말했을 때만** 쓴다.
  - 리뷰가 끝난 함수를 AI가 수정하면 `AI 작성`을 갱신하고 `사용자 리뷰`를 `미완료`로 되돌린다.
  - PR 병합 전에는 모든 함수가 `완료`여야 한다(`tools/run.sh hitl-reviewed`, CI의 PR 빌드가 검사).
- 주석률은 CLOC으로 측정해 보고한다(`tools/run.sh cloc`). 현재는 기준 값이 없어 보고 전용이다. 기준을 정하면 `COMMENT_RATIO_MIN`으로 강제할 수 있다.

### 그 밖의 기준

| 항목 | 기준 | 명령 |
|---|---|---|
| 컴파일 경고 | 0건 (`-Wall -Wextra -Werror`) | `tools/run.sh build` |
| 포맷 | 차이 없음 (`.clang-format`) | `tools/run.sh format` |
| 반환 규칙 | `void` 금지, `return`은 마지막에 한 번, 반환값을 버리지 않음 | `tools/run.sh rules` (RT1~RT4) |
| 인터페이스 경유 | 컴포넌트·인터페이스 있는 모듈은 `*_if.h`로만 접근, 순환 의존 없음 | `tools/run.sh rules` (I1~I5) |
| 테스트 주석 | 모든 테스트에 목적·입력값·절차·예상 출력값 Doxygen 주석과 단언 | `tools/run.sh testdocs` |
| 추적성 | 요구사항 ↔ 설계 ↔ 코드 ↔ 테스트 양방향 100% | `tools/run.sh trace` |

### 테스트 작성 요건

- 단위 테스트: 근거는 상세 설계다. 예상값은 설계에서 도출하고 MC/DC 보강 테스트는 변이 확인(조건을 바꾸면 실패하는지)을 한다. 동치 클래스·경계값 분석·PICT 조합을 적극 활용한다(`unit-test-mcdc`).
- 통합 테스트: 근거는 아키텍처의 인터페이스와 동적 동작이다. 실제 컴포넌트를 연결하고 스텁은 HW·OS 경계에만 둔다. 인터페이스 중심의 연동 시험 위주로 한다(`integration-test-coverage`).
- **모든 테스트 함수**에 Doxygen 주석으로 목적(`@brief`), 입력값(`@par 입력값`), 절차(`@par 절차`), 예상 출력값(`@par 예상 출력값`)과 `@test <ID>`를 쓴다.
- Stub/Mock은 필요한 경우에만 쓰고 과하게 의존하지 않는다. 커버리지만 채우는 의미 없는 테스트(단언 없음, 항상 참인 단언, 실행 결과를 받아 적은 예상값)는 쓰지 않는다.

---

## 3. Git 브랜치 및 PR 전략

### 브랜치: Git Flow

| 브랜치 | 분기 원점 | 병합 대상 | 용도 |
|---|---|---|---|
| `master` | — | — | 릴리스된 코드만. 릴리스마다 태그 `vX.Y.Z` |
| `develop` | `master` | — | 다음 릴리스를 통합하는 기본 개발 브랜치 |
| `feature/<짧은-설명>` | `develop` | `develop` | 이터레이션·기능 (예: `feature/iter-1-lock-ctrl`) |
| `release/X.Y.Z` | `develop` | `master` + `develop` | 릴리스 준비(버그 수정, 문서, 버전 표기만) |
| `hotfix/X.Y.Z` | `master` | `master` + `develop` | 릴리스된 버전의 긴급 수정 |

- `master`와 `develop`에 직접 커밋하거나 push하지 않는다. 모든 변경은 PR로 병합한다.
- 브랜치 이름은 소문자, 숫자, 하이픈만 쓴다. 이터레이션 하나는 `feature/iter-<n>-<설명>` 브랜치 하나로 진행한다. 도구·설정 변경은 `feature/chore-<설명>`을 쓴다.
- `master`, `develop`에 force push하지 않는다. 작업 브랜치의 rebase나 force push는 그 브랜치 작성자만 한다.
- **커밋 메시지**는 한글로 쓴다. 첫 줄은 `<유형>: <요약>` 형식으로 50자 안팎(유형: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`), 빈 줄 다음에 무엇을 왜 바꿨는지 적는다. 관련 요구사항·설계 ID가 있으면 본문에 적는다.

### PR은 CI를 통과해야 완료한다

- PR은 **사용자의 이터레이션 확인 뒤**에 만든다(1절). 설명은 한글로 쓰고 변경 내용, 관련 요구사항 ID, 단계 리뷰 기록 경로, CI 결과를 적는다.
- **PR은 CI(GitHub Actions, `.github/workflows/ci.yml`)를 활용한다. CI를 통과해야만 PR을 완료(병합)한다.** CI가 실패하면 병합하지 않는다. `develop`·`master`의 브랜치 보호에서 `CI / 빌드 및 품질 기준`을 필수 상태 검사로 지정한다(저장소 관리자 작업).
- **CI에서 통과해야 하는 항목은 빌드 성공 외에, 2절의 소스코드 품질 기준 전부**다. 워크플로는 기준마다 단계가 하나씩 있고, 앞 단계가 실패해도 나머지를 끝까지 실행해 결과를 한 번에 보여준다. Jenkinsfile도 같은 구성이다.
  - 포맷, 빌드, 반환 규칙·인터페이스 규칙, HITL 주석, Doxygen, 테스트 주석
  - Cppcheck(예외 없음), Lizard(순환 복잡도·함수 라인수), CLOC(주석률 보고)
  - 단위 테스트(성공률 + MC/DC 100%), 통합 테스트(성공률 + 함수 커버리지 100%), TDD 테스트(성공률, 별도 커버리지)
  - 추적성, 시스템 테스트(요구사항 커버리지 100%)
  - **PR 빌드는 추가로** 모든 함수의 사용자 리뷰 완료(`hitl-reviewed`)
- 병합은 merge commit(`--no-ff`)으로 하고 병합한 `feature/` 브랜치는 삭제한다. `release/`, `hotfix/`는 `master`에 병합한 뒤 태그를 달고 같은 내용을 `develop`에도 병합한다.

---

## 4. 개발 환경

### 툴체인: Docker 이미지 `claudeclangbuild`

개발 빌드와 CI 빌드가 **같은 Docker 이미지**를 쓴다(`docker/Dockerfile`). 이미지 이름은 **ClaudeCLangBuild**이며, Docker가 저장소 이름에 대문자를 허용하지 않아 실제 태그는 소문자 `claudeclangbuild:latest`다. **빌드와 그 이후 작업(테스트, 커버리지, 정적 분석, 매트릭, 포맷, 문서 검사)은 반드시 컨테이너 안에서 수행**한다. 호스트에 설치된 컴파일러나 도구로 결과를 내지 않는다. 호스트에서는 편집, git, `docker compose` 명령만 쓴다.

| 구분 | 도구 | 버전 | 비고 |
|---|---|---|---|
| 개발 언어 | **C (Clang)** | Clang 23.1.6 (18 이상 최신 안정 버전, apt.llvm.org) | C11, 테스트는 C++17 |
| 빌드 도구 | **CMake + Ninja** | CMake 3.28, Ninja | 추천: 아래 설명 |
| 정적 분석 | **Cppcheck** | 2.13 | |
| 테스트 프레임워크 | **GoogleTest / GoogleMock** | 1.14 | |
| 테스트 커버리지 | **LLVM** (`llvm-cov`, `llvm-profdata`) | 23.1.6 | MC/DC까지 측정 |
| 순환 복잡도·함수 라인수 | **Lizard** | 1.17.13 | |
| 주석률 | **CLOC** | 1.98 | |
| 포맷 | clang-format | 23 | 추천 |
| 문서·주석 형식 검사 | Doxygen | 1.9.8 | 추천: Doxygen 주석 형식 오류를 경고(= 실패)로 잡는다 |
| 조합 테스트 | PICT | 3.7.4 | 추천: 페어와이즈 케이스 생성 |
| 시험 명세 검증 | Python 3 + openpyxl | | 추천: SWE.6 요구사항 커버리지 검사 |
| 기반 이미지 | Ubuntu 24.04 | | |

**빌드 도구 추천: CMake + Ninja.** 이유: ① LLVM·Clang과 GoogleTest가 CMake를 1급으로 지원한다. ② `compile_commands.json`을 만들어 Cppcheck·clang-tidy가 빌드와 같은 옵션으로 분석한다. ③ 커버리지 계측 옵션을 타깃 단위로 켜고 끌 수 있어 제품 코드만 계측하기 쉽다. ④ Ninja는 증분 빌드가 빨라 반복(테스트 추가 → 재측정)이 많은 이 프로젝트에 맞다.

### 명령어

```powershell
docker compose build                          # 이미지 빌드 (최초 1회, Dockerfile을 바꾼 뒤)
docker compose run --rm dev tools/run.sh <명령>
docker compose run --rm dev bash              # 컨테이너 셸
```

| 명령 | 내용 |
|---|---|
| `format` / `build` | clang-format 검사 / Debug 빌드(경고 0건) |
| `rules` / `hitl` / `hitl-reviewed` | 반환·인터페이스 규칙 / AI 작성·사용자 리뷰 주석 / 모든 함수 사용자 리뷰 완료 |
| `docs` / `testdocs` | Doxygen 주석 형식 / 테스트 함수 주석·단언·명세 ID 일치 |
| `static` / `complexity` / `cloc` | Cppcheck / Lizard(순환 복잡도 10, 함수 80줄) / 주석률 |
| `unit` / `integration` / `tdd` | 단위(MC/DC) / 통합(함수 커버리지) / TDD(별도 보고) |
| `trace` / `system` | 양방향 추적성 / SWE.6 요구사항 커버리지 |
| `all` | 위 전부(`hitl-reviewed` 제외). PR 전에 실행 |

소스나 해당 산출물이 아직 없으면 그 검사는 "건너뜀"을 출력하고 통과한다(문서만 있는 단계). 보고서는 `build/reports/`(HTML 커버리지 `unit|integration|tdd/html/index.html`, Cppcheck `cppcheck-html/`, Doxygen `doxygen/html/` 등)에 생긴다.

### CMake 빌드 규약

`tools/run.sh`가 동작하려면 `CMakeLists.txt`가 다음을 지켜야 한다. 첫 구현(SWE.3)을 시작할 때 만든다.

- `cmake_minimum_required(VERSION 3.28)`, C11 / C++17, `find_package(GTest REQUIRED)`.
- 제품 코드는 라이브러리 타깃으로 만들고 `-Wall -Wextra -Werror`를 붙인다. **`src/`의 모든 `.c`를 타깃에 넣는다**(빠지면 정적 분석·커버리지가 실패한다).
- `option(ENABLE_COVERAGE ...)`이 ON이면 제품 코드 타깃에만 `-fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc`, 링크에 `-fprofile-instr-generate`를 붙인다. 테스트 코드는 계측하지 않는다.
- 테스트 실행 파일은 `tests/unit/`, `tests/integration/`, `tests/tdd/` 아래 각각의 `CMakeLists.txt`에서 만든다(`tools/run.sh`가 그 빌드 디렉터리의 실행 파일을 모두 실행한다).

### 도구 변경

도구 버전을 바꿀 때는 `docker/Dockerfile`과 이 절의 표를 함께 고치고 별도 `feature/chore-` 브랜치로 PR을 올린다.

---

## 5. CI 환경

- **CI는 GitHub Actions**다(`.github/workflows/ci.yml`). push(`master`, `develop`, `feature/**`, `release/**`, `hotfix/**`), PR(`master`, `develop`), 수동 실행, **매일 02:00 KST 정기 실행(develop)**에 동작한다. 개발과 같은 `docker/Dockerfile`로 이미지를 만들어 `tools/run.sh`를 실행한다. 보고서는 아티팩트 `reports`로, 커버리지 요약은 실행 요약 화면에 남는다.
- **사내 Jenkins**는 같은 검사를 이 PC에서 주기적으로 다시 돌리며(아래), PR 병합 조건은 아니다. `develop`은 매일 02시경 정기 실행하고, 스캔(5분)이 찾은 변경도 빌드한다.

### 사내 Jenkins (안정 버전 LTS, Docker, 포트 8080)

- **Jenkins 안정 버전(LTS 2.580.x)**을 이 PC의 Docker 위에서 실행한다. 포트는 **8080**이다: http://localhost:8080
- 정의: `compose.yaml`의 `jenkins` 서비스, `ci/jenkins/`(`Dockerfile`, `plugins.txt`, `casc.yaml`). 설정은 코드(JCasC)로 관리하며 컨테이너를 다시 만들어도 같은 구성이 된다.
- 빌드는 Jenkins 컨테이너가 아니라 **개발과 같은 `claudeclangbuild` 이미지**에서 실행한다(호스트 Docker 소켓 사용, 워크스페이스는 `--volumes-from`으로 공유). Jenkins 컨테이너는 소켓 접근을 위해 root로 실행하며 이 PC 안에서만 쓰는 로컬 CI다.

```powershell
docker compose up -d --build jenkins          # 기동 (처음에는 ci/jenkins.env 필요)
docker compose restart jenkins                # 첫 기동 뒤 한 번: 작업(ClaudeClang)이 목록에 나타난다
docker compose logs -f jenkins                # 로그
docker compose stop jenkins                   # 중지 (데이터는 볼륨 jenkins_home에 유지)
```

- **계정**: `ci/jenkins.env`(저장소에 커밋하지 않음, `ci/jenkins.env.example` 참고)의 `JENKINS_ADMIN_ID`/`JENKINS_ADMIN_PASSWORD`. 비밀값을 저장소·문서·커밋·PR에 기록하지 않는다.
- **작업**: 멀티브랜치 파이프라인 `ClaudeClang`이 GitHub 저장소(`imunokyh/ClaudeClang`)의 브랜치와 PR(병합 결과)마다 `Jenkinsfile`을 실행한다. 사내 PC라 webhook을 못 쓰므로 5분마다 저장소를 확인한다.
- **GitHub 토큰(권장)**: 익명 GitHub API는 시간당 60회로 제한되어 스캔이 막힌다. `ci/jenkins.env`에 `GITHUB_USER`, `GITHUB_TOKEN`을 설정하면 Jenkins가 인증해서 스캔하고 PR에 빌드 결과(상태 검사)를 남긴다. Jenkins는 병합 조건이 아니라 보조 검사이므로 토큰은 선택이다.

### 설치된 플러그인과 결과 확인

| 플러그인 | 보이는 결과 |
|---|---|
| JUnit | 테스트 결과. 단위(`unit.*`)·통합(`integration.*`)·TDD(`tdd.*`)가 클래스 이름 접두어로 구분된다 |
| Warnings Next Generation | **Cppcheck**, **Clang 경고**, **Lizard**(순환 복잡도·라인수 위반), **Doxygen** 경고를 각각 별도 결과로 표시 |
| Coverage | 단위·통합·TDD 커버리지를 각각 별도 화면으로 표시 (LCOV: 라인 등) |
| HTML Publisher | llvm-cov HTML(**MC/DC**·함수 커버리지 포함), Cppcheck 보고서, Lizard 보고서, Doxygen 문서 |
| Plot | 주석률(CLOC) 추이, 커버리지 요약 추이 |
| 파이프라인(Pipeline, Stage View, Graph View), Git, GitHub Branch Source, Timestamper, Build Timeout, Workspace Cleanup, Configuration as Code, Job DSL | 파이프라인 실행·표시, 저장소 연동, 설정 코드화 |

- 단위 테스트의 **MC/DC**와 통합 테스트의 **함수 커버리지**는 Coverage 플러그인(LCOV)에 표시되지 않으므로, HTML Publisher의 llvm-cov 보고서와 파이프라인 스테이지 결과(판정)로 확인한다.
- 스테이지가 실패해도 나머지 스테이지를 끝까지 실행해 모든 결과를 한 번에 볼 수 있다. 하나라도 실패하면 빌드는 `FAILURE`다.

---

## 코딩 규칙

- **언어**: C11. 테스트 코드는 C++17.
- **반환 규칙**: 모든 함수는 반드시 값을 반환한다. 반환할 값이 없으면 `void` 대신 정상 실행 여부를 판단할 수 있는 값(`status_t`: `STATUS_OK`와 오류 코드)을 반환하고 결과는 출력 매개변수로 낸다. `return`은 함수 마지막에 **한 번만** 호출한다(조기 반환 금지). 반환값을 버리지 않는다(`NODISCARD`). 자세한 내용은 `implementation` 스킬.
- **인터페이스**: 컴포넌트는 인터페이스 헤더(`<이름>_if.h`)로만 서로 통신한다. 상세 설계가 정한 수준 이상의 모듈도 인터페이스를 정의하고 그것으로만 접근한다. 내부 함수와 상태는 `static`, 헤더에서 `extern` 변수를 공개하지 않는다.
- **설계 원칙**: 아키텍처는 높은 응집도·낮은 결합도, 모듈화, 캡슐화, SOLID를 따르고, 상세 설계는 클린 코드 원칙과 단일 책임 원칙을 따른다.
- **한 줄에 한 문장**. 제어문 본문은 항상 중괄호로 감싼다(`.clang-format`의 `InsertBraces`).
- **한 결정(decision)의 조건은 6개 이하를 권장**한다(조건이 N개면 MC/DC 벡터가 최소 N+1개 필요해 검증이 어려워진다). 넘으면 중간 `bool` 변수나 판정 함수로 나누고, 나누지 않으면 설계서에 근거를 쓴다.
- **명명**: 함수·변수는 `lower_snake_case`, 공개 함수는 `<모듈>_` 접두어(`lock_ctrl_decide`), 매크로·상수·열거값은 `UPPER_SNAKE_CASE`, 타입은 `<이름>_t`.
- **고정 폭 정수**(`stdint.h`, `stdbool.h`)를 쓴다. 범위·단위가 있는 신호는 변수 이름이나 주석에 단위를 적는다(`speed_kph`).
- 동적 메모리 할당(`malloc` 등)과 재귀를 쓰지 않는다.

## 작성 언어와 응답 방식

- 이 파일(CLAUDE.md)은 반드시 한글로 작성한다. 내용을 추가하거나 고칠 때도 한글로 쓴다.
- 모든 산출물(설계 문서, 테스트 명세, 주석, 커밋 메시지, PR 설명)은 한글로 작성한다. 코드 식별자, 명령어, 파일 경로는 영어 원문 그대로 둔다.
- 사용자에게는 존댓말(합니다체·해요체)로 응답한다. 반말을 쓰지 않는다.
- 의미 없는 사과를 하지 않는다. 실수가 있으면 무엇이 잘못됐고 어떻게 고쳤는지만 말한다.
