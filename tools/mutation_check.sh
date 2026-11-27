#!/usr/bin/env bash
# @brief 변이 확인: 제품 코드의 조건을 일부러 바꿔도 단위 테스트가 통과하는지 확인한다.
#
# 변이된 코드에서 단위 테스트가 실패해야(= 변이를 잡아야) 의미 있는 테스트다. 통과해 버리면(= 생존)
# 그 조건을 검증하는 단언이 부족하다는 뜻이다.
#
# 사용법 (컨테이너 안, 저장소 루트): bash tools/mutation_check.sh
# 종료 코드: 0 = 모든 변이를 잡음, 1 = 살아남은 변이가 있음
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

# 변이 목록: "파일|원본 텍스트|바꾼 텍스트|설명"
MUTANTS=(
    "src/input_guard/input_guard.c|(time.value >= (int64_t)guard->last_snapshot_ms)|(time.value > (int64_t)guard->last_snapshot_ms)|스냅샷 시각 역행 경계(같은 시각 거절)"
    "src/input_guard/input_guard.c|> STALE_THRESHOLD_MS|>= STALE_THRESHOLD_MS|STALE 경계(200 ms)"
    "src/input_guard/input_guard.c|(raw->value >= lower)|(raw->value > lower)|열거·시각 하한 경계"
    "src/input_guard/input_guard.c|(raw->value <= SPEED_MAX_KPH)|(raw->value < SPEED_MAX_KPH)|차속 상한 경계(300.0)"
    "src/input_guard/input_guard.c|(raw->value == 1.0)|(raw->value == 2.0)|boolean 값 1"
    "src/input_guard/input_guard.c|guard->invalid[index] = !acceptable;|guard->invalid[index] = false;|무효 항목 표시 생략"
    "src/arbiter/arbiter.c|has_item_value(inputs, ITEM_SENSOR_FAULT, SENSOR_FAULT_TRUE))|has_item_value(inputs, ITEM_ISOFIX_LEFT, SENSOR_FAULT_TRUE))|규칙 2의 입력 항목"
    "src/arbiter/arbiter.c|(inputs->validity[item] == VALIDITY_VALID)|(inputs->validity[item] != VALIDITY_VALID)|입력 상태 VALID 조건"
    "src/arbiter/arbiter.c|if (fault_condition)|if (fault_condition && degraded_condition)|state 결정(FAULT 조건 단독)"
    "src/lock_ctrl/lock_ctrl.c|(now_ms < ctrl->last_now_ms)|(now_ms <= ctrl->last_now_ms)|평가 시각 역행 경계(같은 시각 허용)"
    "src/lock_ctrl/lock_ctrl.c|(input->command != NULL)|(input->command == NULL)|명령 검사 호출 조건"
    "src/lock_ctrl/lock_ctrl.c|ctrl->outputs = decision.outputs;|ctrl->outputs = ctrl->outputs;|직전 확정 출력 갱신 생략"
)

killed=0
survived=0
for entry in "${MUTANTS[@]}"; do
    IFS='|' read -r file from to description <<< "${entry}"
    rm -rf "${WORK}/tree" && mkdir -p "${WORK}/tree"
    (cd "${ROOT}" && tar cf - CMakeLists.txt src tests) | tar xf - -C "${WORK}/tree"
    python3 - "${WORK}/tree/${file}" "${from}" "${to}" <<'PYEOF'
import sys
path, old, new = sys.argv[1:4]
text = open(path, encoding="utf-8").read()
if old not in text:
    print(f"오류: 변이 대상 텍스트를 찾지 못함: {old}")
    sys.exit(2)
open(path, "w", encoding="utf-8").write(text.replace(old, new, 1))
PYEOF
    if [ "$?" -ne 0 ]; then
        echo "건너뜀(대상 없음): ${description}"
        survived=$((survived + 1))
        continue
    fi
    cmake -S "${WORK}/tree" -B "${WORK}/build" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ > /dev/null 2>&1
    if ! cmake --build "${WORK}/build" > /dev/null 2>&1; then
        echo "빌드 실패(변이가 컴파일되지 않음): ${description}"
        survived=$((survived + 1))
        continue
    fi
    failed=0
    for exe in "${WORK}"/build/tests/unit/ut_*; do
        "${exe}" > /dev/null 2>&1 || failed=1
    done
    if [ "${failed}" -eq 1 ]; then
        echo "잡힘: ${description}"
        killed=$((killed + 1))
    else
        echo "생존: ${description}  ← 이 조건을 검증하는 단언이 부족함"
        survived=$((survived + 1))
    fi
done
echo "변이 ${#MUTANTS[@]}개: 잡힘 ${killed}, 생존 ${survived}"
if [ "${survived}" -eq 0 ]; then
    echo "판정: 통과 (모든 변이를 잡음)"
else
    echo "판정: 실패 — 살아남은 변이 ${survived}개"
    exit 1
fi
