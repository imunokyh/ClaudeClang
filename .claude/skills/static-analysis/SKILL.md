---
name: static-analysis
description: C 제품 코드(src/)의 정적 분석을 Cppcheck로(룰 대상 100% 만족, 예외 처리 없음), 순환 복잡도(10 이하)와 함수 라인수(80 이하)를 Lizard로, 포맷을 clang-format으로, 주석률을 CLOC으로 검사하고 지적이 0건이 될 때까지 코드를 고치는 절차. 정적 분석, Cppcheck, 코드 품질 검사, 순환 복잡도, 함수 라인수, 복잡도·길이 초과 함수 분할, 코딩 규칙 위반 확인, 구현 완료 후 품질 게이트 확인 요청이 오면 반드시 이 스킬을 사용할 것.
---

# 정적 분석 · 소스코드 매트릭

## 완료 조건

컨테이너에서 아래가 모두 `판정: 통과`, 종료 코드 0이어야 한다. 구현을 마치면 Stop 훅(`.claude/hooks/check-metrics.sh`)도 Lizard 기준을 자동 검사하고, CI(Jenkins)가 최종 게이트다.

```bash
docker compose run --rm dev tools/run.sh format       # clang-format 차이 없음
docker compose run --rm dev tools/run.sh rules        # 반환 규칙(RT1~4), 인터페이스 경유 접근(I1~5)
docker compose run --rm dev tools/run.sh static       # Cppcheck: 룰 대상 100% 만족, 예외 없음
docker compose run --rm dev tools/run.sh complexity   # Lizard: 순환 복잡도 10 이하, 함수 라인수 80 이하
docker compose run --rm dev tools/run.sh cloc         # CLOC 주석률 (보고)
```

결과 파일: `build/reports/cppcheck.xml`, `cppcheck-html/`, `lizard-warnings.txt`, `lizard.html`, `lizard.xml`, `cloc-files.csv`

`rules`는 `implementation`·`architecture-design` 스킬이 정한 규칙의 검사이며, 위반은 코드를 고쳐서 해결한다(규칙 완화 금지).

## Cppcheck — 룰 대상 100% 만족, 예외 처리 없음

- 설정은 `tools/run.sh`의 `cmd_static`에 있다: `compile_commands.json` 기반, `src/`만, `warning,style,performance,portability`, `--inconclusive`, `--check-level=exhaustive`, `--std=c11`. 설정을 완화하지 않는다.
- 지적 출력 형식: `파일:줄: [심각도/ID] 메시지`. 결과는 `build/reports/cppcheck.xml`(Jenkins Warnings 플러그인이 읽는다)과 HTML 보고서로도 남는다.
- **예외(억제)는 없다.** `cppcheck-suppress` 주석, 억제 파일(`suppressions*.txt`), `--suppress`/`--inline-suppr` 옵션은 어떤 경우에도 쓰지 않는다. `tools/run.sh static`은 억제 흔적이 있으면 그 자체로 실패한다. 오탐처럼 보여도 코드를 분석기가 문제없다고 증명할 수 있는 형태로 고친다.
- **분석 누락도 실패다.** `src/`의 모든 `.c`는 CMake 빌드에 들어 있어야 한다. 빌드에 없는 파일은 Cppcheck가 분석하지 못하므로 `static`이 실패한다(`CMakeLists.txt`에 추가할 것).

### 지적 처리

1. ID로 의미를 확인한다(`cppcheck --errorlist`에서 ID 검색, 또는 메시지).
2. **진짜 결함**(NULL 역참조, 경계 초과, 초기화 안 된 변수, 누수, 부호 문제 등): 결함을 재현하는 단위 테스트를 먼저 쓰고(`test-driven-development` 스킬) 코드를 고친다.
3. **분석기가 의심하지만 도달할 수 없다고 생각되는 지적**: 억제하지 않는다. 방어 코드가 정말 필요한지 설계에서 판단하고, 필요하면 분석기가 이해할 수 있게 코드를 단순화하고(검사를 한곳으로 모으고, 범위를 타입으로 표현), 필요 없으면 지운다.
4. **스타일 지적**(`constParameter`, `variableScope`, `knownConditionTrueFalse` 등): 고친다. `knownConditionTrueFalse`는 죽은 분기의 신호이므로 커버리지 문제와 함께 본다.

## 소스코드 매트릭 (Lizard)

| 기준 | 값 | 설정 위치 |
|---|---|---|
| 순환 복잡도 | 함수마다 **10 이하** | `tools/run.sh`의 `MAX_CCN` (`lizard -C`) |
| 함수 라인수 | 함수마다 **80줄 이하** | `tools/run.sh`의 `MAX_FUNC_LINES` (`lizard -L`) |

- Lizard의 라인수는 함수의 물리적 줄 수(빈 줄·주석 포함)다. 함수 위의 Doxygen 블록은 함수 밖이라 세지 않는다.
- Lizard는 `if`, `for`, `while`, `case`, `&&`, `||`, `?:`를 하나씩 센다. 그래서 조건이 많은 결정 하나만으로도 기준을 넘을 수 있다.
- 위반 출력: `파일:줄: warning: <함수> has N NLOC, N CCN, ... N length`
- 구현이 끝났을 때 Claude Code Stop 훅이 같은 기준으로 자동 검사한다. 위반이 있으면 멈추지 못하고 이 절차로 고치게 된다.

### 초과 함수 처리 (순환 복잡도 또는 라인수)

순서대로 시도한다. 동작은 바꾸지 않으며 기존 테스트(단위, TDD)가 계속 통과해야 한다(TDD의 refactor 단계).

1. **조건 묶음 추출**: 복합 조건을 의미 있는 이름의 `static` 판정 함수로 뺀다(`is_lock_condition_met()`).
2. **테이블화**: `switch`/`else if` 사슬이 값 매핑이면 `const` 테이블 조회로 바꾼다.
3. **단계 분리**: 입력 검증 → 판단 → 출력 설정처럼 단계별 `status_t` 함수로 나눈다. 라인수 초과도 같은 방법으로 해결한다.

**조기 반환(early return)으로 중첩을 줄이지 않는다.** 이 저장소는 함수마다 `return`을 마지막에 한 번만 두도록 정했다(`implementation` 스킬 R2). 단계 분리로 해결한다.

함수를 나누면 상세 설계(`docs/swe3/`)의 함수 목록과 `@trace`도 함께 고치고, 새 함수에도 단위 테스트가 MC/DC 100%로 따라와야 한다. 기준(10, 80)을 올리지 않는다.

## 주석률 (CLOC)

- `tools/run.sh cloc`은 `cloc --by-file`로 파일별 주석 줄·코드 줄을 세어 주석률(주석 ÷ (주석 + 코드))을 보고한다. 기본은 보고 전용이며, 환경 변수 `COMMENT_RATIO_MIN=<백분율>`을 주면 파일마다 그 값 이상을 요구한다. Jenkins에서는 추이 그래프로 보인다.
- 주석률을 올리려고 의미 없는 주석을 쓰지 않는다.

## 컴파일 경고

`tools/run.sh build`는 `-Wall -Wextra -Werror`로 빌드한다(CMakeLists.txt 규약). 경고 억제 pragma나 플래그 완화는 하지 않는다.

## 보고

호출한 쪽에 각 명령의 판정 줄(그대로 인용), 고친 지적 수(Cppcheck ID별), 분할한 함수 목록을 돌려준다. 억제는 허용되지 않으므로 "억제한 지적"은 항상 없다.
