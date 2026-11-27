#!/usr/bin/env bash
# @brief ClaudeCLangBuild 컨테이너 안에서 빌드·테스트·품질 검사를 실행한다. 개발과 CI(Jenkins)가 같은 명령을 쓴다.
#
# 호스트에서:  docker compose run --rm dev tools/run.sh <명령>
#
# 명령 (소스 품질 기준과 1:1 대응):
#   format        clang-format 차이 없음
#   build         Debug 빌드, 경고 0건 (-Wall -Wextra -Werror)
#   rules         반환 규칙(RT1~4), 인터페이스 경유 접근(I1~5)
#   hitl          모든 함수에 AI 작성·사용자 리뷰 Doxygen 주석 (HC1~3)
#   hitl-reviewed 위에 더해 모든 함수의 사용자 리뷰가 완료됨 (HC4, PR 병합 게이트)
#   docs          Doxygen 주석 형식 검사 (경고 = 실패)
#   testdocs      테스트 함수 Doxygen 주석·단언·명세 ID 일치
#   static        Cppcheck 룰 대상 100% 만족 (예외 처리 없음)
#   complexity    Lizard: 순환 복잡도 10 이하, 함수 라인수 80 이하
#   cloc          CLOC 주석률 측정 (보고)
#   unit          단위 테스트 성공률 100% + MC/DC 100%              (SWE.4)
#   integration   통합 테스트 성공률 100% + 함수 커버리지 100%      (SWE.5)
#   tdd           TDD 테스트 성공률 100%, 커버리지는 별도 측정(보고 전용)
#   trace         요구사항 ↔ 설계 ↔ 코드 ↔ 테스트 양방향 추적성
#   system        SW 검증(SWE.6) 시험 명세 검증 = 요구사항 커버리지 100%
#   all           위 전부 (hitl-reviewed 제외). PR 전에 실행
#
# 소스(src/*.c)나 해당 산출물이 아직 없으면 그 검사는 "건너뜀"을 출력하고 통과한다(문서만 있는 단계).
#
# 빌드 규약 (CMakeLists.txt가 지켜야 할 것):
#   - 옵션 ENABLE_COVERAGE=ON이면 src/ 제품 코드에
#     -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc 를 붙인다.
#   - 테스트 실행 파일은 tests/unit/, tests/integration/, tests/tdd/ 아래 CMakeLists.txt에서 각각 만든다.
#
# 종료 코드: 0 = 통과, 그 외 = 실패
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

SKILLS=".claude/skills"
SKILL_UNIT="${SKILLS}/unit-test-mcdc/scripts"
SKILL_IMPL="${SKILLS}/implementation/scripts"
BUILD_DEBUG="build/debug"
BUILD_COV="build/coverage"
REPORTS="build/reports"
MAX_CCN=10
MAX_FUNC_LINES=80
CLANG_ARGS=(-std=c11 -I src)

mkdir -p "${REPORTS}"

# @brief src/ 에 C 소스가 하나라도 있는지 확인한다.
has_sources() {
    [ -n "$(find src -name '*.c' -print -quit 2>/dev/null)" ]
}

# @brief 소스가 없으면 건너뜀을 알린다. 건너뛰어야 하면 0, 아니면 1을 반환한다.
# @param $1 검사 이름
skip_without_sources() {
    if has_sources; then
        return 1
    fi
    echo "건너뜀: ${1} — src/ 에 C 소스가 없음"
    return 0
}

# @brief CMake 구성과 빌드를 한다.
# @param $1 빌드 디렉터리
# @param $2 ENABLE_COVERAGE 값 (ON/OFF)
cmake_build() {
    cmake -S . -B "$1" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DENABLE_COVERAGE="$2"
    cmake --build "$1" 2>&1 | tee "${REPORTS}/build-$(basename "$1").log"
}

# @brief 계측 빌드 후 테스트를 실행하고 LLVM 도구로 커버리지를 판정한다.
# 실제 수집·판정은 스킬에 들어 있는 스크립트(llvm_cov_run.sh)가 한다.
# @param $1 테스트 종류 (unit | integration | tdd)
# @param $2 판정 지표 (mcdc | functions | lines)
run_with_coverage() {
    cmake_build "${BUILD_COV}" ON
    SRC_DIR=src bash "${SKILL_UNIT}/llvm_cov_run.sh" "$1" "$2" "${BUILD_COV}/tests/$1"
}

cmd_format() {
    mapfile -t files < <(find src tests -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) 2>/dev/null)
    if [ "${#files[@]}" -eq 0 ]; then
        echo "검사할 파일이 없습니다"
        return 0
    fi
    clang-format --dry-run -Werror "${files[@]}"
    echo "판정: 통과 (포맷 차이 없음)"
}

cmd_build() {
    skip_without_sources "build" && return 0
    cmake_build "${BUILD_DEBUG}" OFF
}

# @brief 공유 디렉터리 옵션을 만든다. src/common 이 있으면 아키텍처의 공유 디렉터리로 본다.
shared_dir_args() {
    if [ -d src/common ]; then
        echo "--shared common"
    fi
}

cmd_rules() {
    skip_without_sources "rules" && return 0
    local status=0
    python3 "${SKILL_IMPL}/check_return_rules.py" --src src -- "${CLANG_ARGS[@]}" || status=1
    # shellcheck disable=SC2046
    python3 "${SKILLS}/architecture-design/scripts/check_interfaces.py" --src src $(shared_dir_args) || status=1
    return "${status}"
}

cmd_hitl() {
    skip_without_sources "hitl" && return 0
    python3 "${SKILL_IMPL}/check_hitl_comments.py" --src src -- "${CLANG_ARGS[@]}"
}

cmd_hitl_reviewed() {
    skip_without_sources "hitl-reviewed" && return 0
    python3 "${SKILL_IMPL}/check_hitl_comments.py" --src src --require-reviewed -- "${CLANG_ARGS[@]}"
}

cmd_docs() {
    skip_without_sources "docs" && return 0
    rm -rf "${REPORTS}/doxygen" && mkdir -p "${REPORTS}/doxygen"
    doxygen tools/Doxyfile > "${REPORTS}/doxygen/doxygen.log" 2>&1 || {
        cat "${REPORTS}/doxygen/doxygen.log"
        echo "판정: 실패 — Doxygen 경고가 있습니다 (주석 형식 오류, 문서화되지 않은 항목)"
        return 1
    }
    echo "판정: 통과 (Doxygen 경고 0건) — ${REPORTS}/doxygen/html/index.html"
}

cmd_testdocs() {
    local status=0 kind prefix dir doc
    for kind in unit integration; do
        prefix="UT"; dir="tests/unit"; doc="docs/swe4/unit-test-spec.md"
        if [ "${kind}" = "integration" ]; then
            prefix="IT"; dir="tests/integration"; doc="docs/swe5/integration-test-spec.md"
        fi
        if [ ! -d "${dir}" ]; then
            echo "건너뜀: testdocs(${kind}) — ${dir} 없음"
            continue
        fi
        local spec=()
        if [ -f "${doc}" ]; then
            spec=(--spec "${doc}")
        fi
        python3 "${SKILL_UNIT}/check_test_docs.py" --dir "${dir}" --prefix "${prefix}" "${spec[@]}" || status=1
    done
    return "${status}"
}

cmd_static() {
    skip_without_sources "static" && return 0
    # 예외 처리 없음: 억제 주석·억제 파일이 있으면 그 자체가 실패다.
    local suppressed
    suppressed="$(grep -rnE 'cppcheck-suppress|cppcheck:[[:space:]]*suppress' src tests 2>/dev/null || true)"
    if [ -n "${suppressed}" ] || [ -n "$(find . -path ./build -prune -o \( -name 'suppressions*.txt' -o -name '.cppcheck*' \) -print -quit)" ]; then
        echo "${suppressed}"
        echo "판정: 실패 — Cppcheck 억제(suppress)는 허용되지 않습니다"
        return 1
    fi
    cmake_build "${BUILD_DEBUG}" OFF
    # 분석 누락 방지: src/ 의 모든 .c 가 빌드(compile_commands.json)에 들어 있어야 Cppcheck가 분석한다.
    python3 - "${BUILD_DEBUG}/compile_commands.json" <<'PYEOF' || return 1
import json
import os
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    built = {os.path.realpath(entry["file"]) for entry in json.load(handle)}
missing = sorted(
    os.path.join(root, name)
    for root, _, names in os.walk("src")
    for name in names
    if name.endswith(".c") and os.path.realpath(os.path.join(root, name)) not in built
)
for path in missing:
    print(f"{path}: 빌드(CMake)에 포함되지 않아 정적 분석 대상에서 빠짐")
if missing:
    print(f"판정: 실패 — 정적 분석에서 빠진 소스 {len(missing)}개 (CMakeLists.txt에 추가할 것)")
    sys.exit(1)
PYEOF
    # 룰 대상: warning, style, performance, portability (information·unusedFunction 제외). 억제 옵션은 쓰지 않는다.
    cppcheck --project="${BUILD_DEBUG}/compile_commands.json" \
        --file-filter="src/*" \
        --enable=warning,style,performance,portability \
        --inconclusive --check-level=exhaustive --std=c11 \
        --xml --xml-version=2 2> "${REPORTS}/cppcheck.xml" || true
    cppcheck-htmlreport --file="${REPORTS}/cppcheck.xml" --report-dir="${REPORTS}/cppcheck-html" \
        --source-dir=. --title=ClaudeClang > /dev/null 2>&1 || true
    python3 - "${REPORTS}/cppcheck.xml" <<'PYEOF'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
errors = root.findall("./errors/error")
for error in errors:
    location = error.find("location")
    where = f"{location.get('file')}:{location.get('line')}" if location is not None else "-"
    print(f"{where}: [{error.get('severity')}/{error.get('id')}] {error.get('msg')}")
print(f"Cppcheck 지적 {len(errors)}건")
print("판정: " + ("통과 (지적 0건)" if not errors else f"실패 — 지적 {len(errors)}건"))
sys.exit(1 if errors else 0)
PYEOF
}

cmd_complexity() {
    skip_without_sources "complexity" && return 0
    lizard -C "${MAX_CCN}" -L "${MAX_FUNC_LINES}" --xml src > "${REPORTS}/lizard.xml" || true
    lizard -C "${MAX_CCN}" -L "${MAX_FUNC_LINES}" -H src > "${REPORTS}/lizard.html" || true
    if lizard -C "${MAX_CCN}" -L "${MAX_FUNC_LINES}" -w src > "${REPORTS}/lizard-warnings.txt"; then
        echo "판정: 통과 (모든 함수 순환 복잡도 ${MAX_CCN} 이하, 라인수 ${MAX_FUNC_LINES} 이하)"
    else
        cat "${REPORTS}/lizard-warnings.txt"
        echo "판정: 실패 — 순환 복잡도 ${MAX_CCN} 또는 함수 라인수 ${MAX_FUNC_LINES} 초과 함수가 있습니다" >&2
        return 1
    fi
}

cmd_cloc() {
    skip_without_sources "cloc" && return 0
    cloc --by-file --csv --quiet src > "${REPORTS}/cloc.csv"
    python3 tools/cloc_report.py "${REPORTS}/cloc.csv" --out-dir "${REPORTS}" --min-ratio "${COMMENT_RATIO_MIN:-0}"
}

cmd_unit() {
    skip_without_sources "unit" && return 0
    run_with_coverage unit mcdc
}

cmd_integration() {
    skip_without_sources "integration" && return 0
    run_with_coverage integration functions
}

cmd_tdd() {
    skip_without_sources "tdd" && return 0
    if [ ! -d tests/tdd ]; then
        echo "건너뜀: tdd — tests/tdd 없음"
        return 0
    fi
    # TDD 테스트는 단위 테스트와 분리해 별도 결과·별도 커버리지로 낸다. 커버리지는 보고만 한다.
    GATE=0 run_with_coverage tdd lines
}

cmd_trace() {
    if [ ! -d docs/swe1 ]; then
        echo "건너뜀: trace — docs/swe1 없음"
        return 0
    fi
    python3 "${SKILLS}/requirements-analysis/scripts/trace_check.py" --docs docs --src src --stage all
}

cmd_system() {
    local spec="docs/swe6/test-cases.json"
    if [ ! -f "${spec}" ]; then
        echo "건너뜀: system — ${spec} 없음"
        return 0
    fi
    mkdir -p "${REPORTS}/system"
    # build_workbook.py 는 요구사항(테스트 베이시스) 커버리지·양방향 추적성이 100%가 아니면 오류로 실패한다.
    python3 "${SKILLS}/system-test-case-generator/scripts/build_workbook.py" "${spec}" \
        --out "${REPORTS}/system/test-spec.xlsx"
}

cmd_all() {
    cmd_format && cmd_build && cmd_rules && cmd_hitl && cmd_docs && cmd_testdocs \
        && cmd_static && cmd_complexity && cmd_cloc \
        && cmd_unit && cmd_integration && cmd_tdd && cmd_trace && cmd_system
}

case "${1:-}" in
    format | build | rules | hitl | docs | testdocs | static | complexity | cloc | unit | integration | tdd | trace | system | all)
        "cmd_$1"
        ;;
    hitl-reviewed) cmd_hitl_reviewed ;;
    *)
        sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
