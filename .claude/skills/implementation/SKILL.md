---
name: implementation
description: A-SPICE SWE.3(SW 구현) 단계에서 상세 설계(docs/swe3/)대로 C 소스 코드를 구현한다. 모든 함수는 반드시 값을 반환하고 return은 함수 마지막에 한 번만 호출하며, 반환할 값이 없으면 void 대신 정상 실행 여부를 나타내는 상태 코드를 반환한다. TDD, 인터페이스 경유 접근, 클린 코드, 품질 게이트(Cppcheck, 순환 복잡도, 포맷)를 함께 적용한다. 코드 구현, 함수 작성, 기능 구현, 버그 수정, 리팩터링, 반환 규칙, 상태 코드 요청이 오면 — "SWE.3 구현"이라고 하지 않더라도 — 반드시 이 스킬을 사용할 것.
---

# 구현 (SWE.3 구현)

승인된 상세 설계(`docs/swe3/`)를 C11 코드로 옮긴다. 설계에 없는 함수·동작을 코드에 추가하지 않는다. 필요해 보이면 **설계서부터** 고치고 리뷰를 다시 받는다.

## 반환 규칙 (필수)

### R1. 모든 함수는 반드시 값을 반환한다 — `void` 금지

반환할 의미 있는 값이 없으면 `void` 대신 **정상 실행 여부를 판단할 수 있는 값**(상태 코드)을 반환한다. 결과값은 출력 매개변수로 내보낸다.

```c
/* 금지 */  void lock_ctrl_reset(lock_ctrl_t *ctrl);
/* 허용 */  status_t lock_ctrl_reset(lock_ctrl_t *ctrl);
```

- 판정·계산 함수처럼 **반환값 자체가 결과**이고 실패할 수 없으면 그 값을 반환한다(`bool is_lock_condition_met(...)`, `uint16_t clamp_speed(...)`). 실패할 수 있으면 상태 코드를 반환하고 결과는 출력 매개변수로 낸다.
- 상태 코드 타입은 `status_t` 하나로 통일한다(`references/status_if.h` 양식, `src/common/status_if.h`). 값은 `STATUS_OK`(0)와 오류 코드들이다. 오류 코드는 상세 설계의 함수 설계에 있는 목록만 쓴다.
- 상태 코드를 반환하는 선언에는 `NODISCARD`를 붙인다. 호출하는 쪽이 값을 버리면 컴파일 오류가 된다.

### R2. `return`은 함수 마지막에 한 번만

함수 본문의 마지막 문장이 유일한 `return`이다. 중간 `return`(조기 반환), 여러 `return`을 쓰지 않는다. 결과는 지역 변수(`status`, `result`)에 모았다가 마지막에 반환한다.

```c
status_t lock_ctrl_step(const lock_in_t *in, lock_out_t *out)
{
    status_t status = STATUS_OK;

    if ((in == NULL) || (out == NULL))
    {
        status = STATUS_ERR_PARAM;
    }
    else
    {
        status = decide_command(in, &out->cmd);
    }

    return status;
}
```

조기 반환을 피하다 보면 중첩이 깊어지기 쉽다. 순서대로 시도한다.

1. **단계 함수로 분리**: 검증 → 판단 → 출력 설정을 각각 `status_t` 함수로 만들고, 앞 단계가 성공했을 때만 다음 단계를 부른다.
   ```c
   status = validate_input(in);
   if (status == STATUS_OK)
   {
       status = evaluate(in, &cmd);
   }
   if (status == STATUS_OK)
   {
       status = publish(cmd, out);
   }
   ```
2. **조건 묶음 추출**: 복합 조건을 의미 있는 이름의 판정 함수로 뺀다.
3. 중첩이 3단계를 넘으면 설계(함수 분할)가 잘못된 것이다. 설계서를 고친다.

`goto`, `longjmp`, 매크로 속에 숨긴 `return`으로 규칙을 우회하지 않는다.

### R3. 반환값을 버리지 않는다

src/ 함수의 반환값은 변수에 받거나 조건으로 검사한다. `(void)f();`로 버리는 것도 금지한다. 실패해도 계속해야 하는 호출이면 그 사실과 이유를 상세 설계에 쓰고, 받은 상태를 명시적으로 처리한다.

### 검사

```bash
docker compose run --rm dev tools/run.sh rules
```

`tools/run.sh rules`는 `scripts/check_return_rules.py`(Clang AST 기반)로 다음을 검사한다. 위반이 있으면 코드를 고친다.

| 규칙 | 내용 |
|---|---|
| RT1 | `void` 함수 없음 |
| RT2 | 함수당 `return` 정확히 1개 |
| RT3 | 그 `return`이 본문의 마지막 문장 |
| RT4 | src/ 함수의 반환값을 버리는 호출 없음 |

## 구현 절차 (TDD)

1. **설계 확인**: 구현할 함수의 FN ID, 결정표, 오류 처리를 읽는다. 설계에 빠진 것(오류 코드, 경계 동작)이 있으면 설계서를 먼저 보완한다.
2. **`test-driven-development` 스킬을 호출**하고 red-green-refactor 순서를 따른다. 구현 코드보다 **실패하는 테스트가 먼저**다. 테스트 실행은 컨테이너에서 한다.
   - **TDD로 만든 테스트는 단위 테스트(SWE.4)와 분리한다.** `tests/tdd/<모듈>/`에 두고 CMake 타깃도 `tests/tdd/CMakeLists.txt`에서 따로 만든다. 단위 테스트 디렉터리(`tests/unit/`)에 섞지 않는다.
   - 실행·결과·커버리지도 따로 낸다: `docker compose run --rm dev tools/run.sh tdd` → `build/reports/tdd/`(테스트 결과 XML, 커버리지, HTML). 테스트 성공률은 100%여야 하고, 커버리지는 단위·통합과 **별도로 측정해 보고**한다(기준 판정은 하지 않는다).
   - TDD 테스트는 구현을 이끄는 도구이고, 단위 테스트는 상세 설계를 근거로 한 검증이다. TDD 테스트를 `tests/unit/`으로 옮기거나 단위 테스트 결과·MC/DC로 계산하지 않는다. 단위 테스트(SWE.4)는 이 단계 이후 `unit-test-mcdc` 스킬로 설계에서 새로 도출한다.
3. **최소 코드**로 통과시킨다. 반환 규칙 R1~R3은 처음부터 지킨다.
4. **리팩터링** 후에도 테스트가 통과해야 한다.
5. 함수마다 Doxygen 주석에 `@trace FN-nnn`과 아래 "Human in the Loop 주석"을 단다(추적성 T9, `hitl` 게이트).
6. **게이트**를 실행한다. 구현이 끝났을 때 아래를 모두 통과해야 한다.
   ```bash
   docker compose run --rm dev tools/run.sh format
   docker compose run --rm dev tools/run.sh build
   docker compose run --rm dev tools/run.sh rules        # 반환 규칙 + 인터페이스 경유 규칙
   docker compose run --rm dev tools/run.sh hitl         # AI 작성·사용자 리뷰 주석
   docker compose run --rm dev tools/run.sh docs         # Doxygen 주석 형식
   docker compose run --rm dev tools/run.sh static       # Cppcheck: 지적 0건, 예외 없음
   docker compose run --rm dev tools/run.sh complexity   # Lizard: 순환 복잡도 10 이하, 함수 80줄 이하
   docker compose run --rm dev tools/run.sh tdd          # TDD 테스트 (별도 결과·별도 커버리지)
   python3 .claude/skills/requirements-analysis/scripts/trace_check.py --docs docs --src src --stage swe3-impl
   ```
   Claude Code의 Stop 훅(`.claude/hooks/check-metrics.sh`)이 구현을 마칠 때 Lizard 기준을 한 번 더 자동으로 검사한다. 위반이 있으면 멈추지 못하고 고치게 된다. 품질 게이트 정리는 `static-analysis` 스킬을 따른다.
7. **리뷰**: `aspice-stage-review` 스킬로 `stage-reviewer`에게 구현 코드 리뷰를 맡긴다.

## 코딩 규칙 요약

- 인터페이스 경유: 다른 컴포넌트·인터페이스 있는 모듈은 `*_if.h`만 include한다. 내부 함수·상태는 `static`. 헤더에서 `extern` 변수를 공개하지 않는다.
- 이름은 의도를 드러낸다. 한 함수는 한 가지 일만 하고 30줄 안팎, 매개변수 3개 이하를 목표로 한다. **함수 라인수는 80줄 이하, 순환 복잡도는 10 이하가 기준**이다(Lizard).
- 한 줄에 한 문장, 제어문 본문은 항상 중괄호, 결정당 조건 6개 이하.
- 고정 폭 정수와 단위 있는 이름(`speed_kph`). 매직 넘버 금지(설계의 상수 이름 사용).
- 동적 메모리 할당·재귀 금지.
- 주석은 **Doxygen 형식**, 한글. 모든 함수(static 포함)에 `@brief`, `@param`, `@return`(가능한 상태 코드 나열), `@trace`를 단다. 형식 오류와 문서화 누락은 `tools/run.sh docs`(Doxygen 경고 = 실패)가 잡는다.
- 주석을 코드가 말하는 "무엇"의 반복으로 채우지 않는다. 이유와 전제, 단위를 쓴다.

## Human in the Loop 주석 (필수)

AI가 쓴 코드가 **누가 요청했고 사람이 검토했는지** 추적되도록, **모든 함수 정의 바로 위 Doxygen 블록**에 다음을 포함한다. 이것도 Doxygen 형식(`@par` 구역과 `- 키: 값` 목록)이다.

- **AI 작성**: 어느 AI 모델이(`모델`), 언제(`일시`, YYYY-MM-DD), 누가 요청해서(`요청자`) 작성했는가
- **사용자 리뷰**: 사용자가 리뷰를 끝낸 코드인가(`상태`: `미완료` | `완료`), 완료했다면 언제(`일시`), 누가(`리뷰어`) 했는가

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
 * - 요청자: 홍길동
 * @par 사용자 리뷰
 * - 상태: 미완료
 */
NODISCARD status_t lock_decision_evaluate(uint16_t speed_kph, bool door_closed, lock_cmd_t *cmd)
{
    /* ... */
}
```

사용자 리뷰가 완료되면(사용자가 명시적으로 알린 경우에만) 같은 구역을 이렇게 바꾼다.

```c
 * @par 사용자 리뷰
 * - 상태: 완료
 * - 리뷰어: 홍길동
 * - 일시: 2026-11-28
```

규칙:
- `모델`은 코드를 쓰는 에이전트 자신의 모델 ID(예: `claude-opus-5-5`)를 쓴다. 추측하지 않는다.
- `일시`는 작성한 날짜다. `요청자`는 CLAUDE.md "사람 개입(HITL) 설정"의 이름이다. 모르면 사용자에게 묻고, 지어내지 않는다.
- **`상태: 완료`는 사용자가 그 코드를 리뷰했다고 직접 말했을 때만** 쓴다. 에이전트가 스스로 완료로 바꾸지 않는다. 리뷰어와 일시는 사용자가 알려준 값이다.
- **리뷰가 끝난 함수를 AI가 수정하면** `AI 작성`(모델, 일시)을 이번 수정 기준으로 갱신하고 `사용자 리뷰`를 `미완료`로 되돌린다. 검토받지 않은 변경이 검토 완료로 보이면 안 된다.
- 검사: `tools/run.sh hitl`(형식), `tools/run.sh hitl-reviewed`(모든 함수가 리뷰 완료 — PR 병합 전 게이트).

## 보고

호출한 쪽에 구현한 함수(FN ID), 각 게이트의 판정 줄(출력 그대로), 설계서와 달라서 고친 설계 항목, 남은 문제를 돌려준다. 게이트를 실행하지 못했으면 통과했다고 쓰지 않는다.
