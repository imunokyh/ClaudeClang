#!/usr/bin/env bash
# @brief 계측 빌드된 테스트 실행 파일을 모두 돌리고, LLVM 도구로 커버리지를 수집·판정한다.
#
# 사용법 (컨테이너 안, 저장소 루트에서):
#   llvm_cov_run.sh <kind> <metric> [bindir]
#     kind    unit | integration | tdd   보고서 디렉터리 이름 (build/reports/<kind>/)
#     metric  mcdc | lines | branches | regions | functions   판정할 지표
#     bindir  테스트 실행 파일이 있는 디렉터리 (기본 build/coverage/tests/<kind>)
#
# 하는 일:
#   1) 실행 파일마다 LLVM_PROFILE_FILE 로 프로파일을 만들며 실행 (GoogleTest XML 결과 포함)
#   2) llvm-profdata 로 병합, llvm-cov 로 summary.json / html / mcdc.txt 생성
#   3) coverage_gate.py 로 기준(기본 100%) 판정 — src/ 의 모든 소스 파일이 측정되어야 함
#   4) MC/DC 판정에서 미달이면 mcdc_gaps.py 로 미충족 조건과 보강 후보를 출력
#
# 환경 변수:
#   SRC_DIR    소스 디렉터리 (기본 src)
#   THRESHOLD  기준 백분율 (기본 100)
#   GATE       0 이면 판정하지 않고 측정·보고만 한다 (TDD 테스트처럼 별도 측정만 하는 경우). 기본 1
# 종료 코드: 0 = 통과, 1 = 테스트 실패 또는 기준 미달, 2 = 실행 파일 없음
set -euo pipefail

KIND="${1:?kind(unit|integration)가 필요합니다}"
METRIC="${2:?metric(mcdc|lines|...)이 필요합니다}"
BINDIR="${3:-build/coverage/tests/${KIND}}"
SRC_DIR="${SRC_DIR:-src}"
THRESHOLD="${THRESHOLD:-100}"
GATE="${GATE:-1}"
SCRIPTS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="build/reports/${KIND}"
FILTER='(/usr/|/tests/|/build/)'

rm -rf "${OUT}" && mkdir -p "${OUT}/profraw"

mapfile -t exes < <(find "${BINDIR}" -path '*/CMakeFiles' -prune -o -type f -executable -print 2>/dev/null | sort)
if [ "${#exes[@]}" -eq 0 ]; then
    echo "오류: ${BINDIR} 에 테스트 실행 파일이 없습니다" >&2
    exit 2
fi

failed=0
for exe in "${exes[@]}"; do
    echo "== 실행: ${exe}"
    LLVM_PROFILE_FILE="${OUT}/profraw/$(basename "${exe}")-%p.profraw" \
        "${exe}" --gtest_output="xml:${OUT}/$(basename "${exe}").xml" || failed=1
done
# Jenkins 테스트 결과 화면에서 단위·통합·TDD 결과를 구분하도록 클래스 이름에 종류 접두어를 붙인다.
# (실패해도 접두어는 붙인다: 아래 실패 판정 전에 처리)
for xml in "${OUT}"/*.xml; do
    [ -f "${xml}" ] && sed -i "s/classname=\"/classname=\"${KIND}./g" "${xml}"
done
if [ "${failed}" -ne 0 ]; then
    echo "판정: 실패 — 테스트 실패가 있습니다 (커버리지 판정 전에 먼저 고칠 것)" >&2
    exit 1
fi

llvm-profdata merge -sparse -o "${OUT}/merged.profdata" "${OUT}"/profraw/*.profraw
objects=("${exes[0]}")
for exe in "${exes[@]:1}"; do
    objects+=(-object "${exe}")
done

llvm-cov export "${objects[@]}" -instr-profile="${OUT}/merged.profdata" \
    -summary-only -ignore-filename-regex="${FILTER}" > "${OUT}/summary.json"
# Jenkins 커버리지 플러그인용: lcov, llvm-cov JSON(전체)
llvm-cov export "${objects[@]}" -instr-profile="${OUT}/merged.profdata" \
    -format=lcov -ignore-filename-regex="${FILTER}" > "${OUT}/coverage.lcov"
llvm-cov export "${objects[@]}" -instr-profile="${OUT}/merged.profdata" \
    -ignore-filename-regex="${FILTER}" > "${OUT}/coverage.json"
llvm-cov show "${objects[@]}" -instr-profile="${OUT}/merged.profdata" \
    -show-mcdc -show-branches=count -ignore-filename-regex="${FILTER}" > "${OUT}/mcdc.txt"
llvm-cov show "${objects[@]}" -instr-profile="${OUT}/merged.profdata" \
    -show-mcdc -show-branches=count -format=html -output-dir="${OUT}/html" \
    -ignore-filename-regex="${FILTER}"
echo "== 상세 보고서: ${OUT}/html/index.html, ${OUT}/mcdc.txt"

if [ "${GATE}" = "0" ]; then
    # 보고만 한다: 기준 0%, 측정 누락 파일 검사 없음. 수치는 출력하되 종료 코드에 영향을 주지 않는다.
    python3 "${SCRIPTS}/coverage_gate.py" "${OUT}/summary.json" --metric "${METRIC}" \
        --threshold 0 --include "${SRC_DIR}/" || true
    echo "== 보고 전용(GATE=0): 커버리지 기준 판정을 하지 않음"
    exit 0
fi

status=0
python3 "${SCRIPTS}/coverage_gate.py" "${OUT}/summary.json" --metric "${METRIC}" \
    --threshold "${THRESHOLD}" --include "${SRC_DIR}/" --require-dir "${SRC_DIR}" || status=$?

if [ "${status}" -ne 0 ] && [ "${METRIC}" = "mcdc" ]; then
    echo "== MC/DC 미충족 조건 분석"
    python3 "${SCRIPTS}/mcdc_gaps.py" "${OUT}/mcdc.txt" || true
fi
exit "${status}"
