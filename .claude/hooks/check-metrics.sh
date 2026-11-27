#!/usr/bin/env bash
# @brief Stop 훅: 구현이 끝났을 때(Claude가 응답을 마치려 할 때) 소스 코드 매트릭을 Lizard로 검사한다.
#
# 기준: 순환 복잡도 10 이하, 함수 라인수 80 이하 (CLAUDE.md "소스코드 품질 기준").
# 동작:
#   - src/ 의 C 소스(.c/.h)가 바뀌지 않았으면 아무것도 하지 않는다.
#   - 바뀌었으면 컨테이너(claudeclangbuild)의 lizard 로 src/ 전체를 검사한다.
#   - 위반이 있으면 종료 코드 2 로 Claude 가 멈추지 못하게 하고, 위반 목록을 전달해 고치게 한다.
#   - 훅이 이미 한 번 작동한 뒤(stop_hook_active)에도 위반이 남아 있으면 무한 반복을 막기 위해
#     막지 않고 사용자에게 경고만 보인다. CI(Jenkins)가 최종 게이트다.
#   - Docker 를 쓸 수 없으면 막지 않고 경고만 보인다.
#
# 입력: 표준 입력으로 훅 JSON (stop_hook_active 필드를 읽는다)
set -uo pipefail

MAX_CCN=10
MAX_FUNC_LINES=80

input="$(cat || true)"
stop_active="$(printf '%s' "${input}" | python -c 'import sys, json
try:
    print(str(json.load(sys.stdin).get("stop_hook_active", False)).lower())
except Exception:
    print("false")' 2>/dev/null || echo false)"

cd "${CLAUDE_PROJECT_DIR:-$(pwd)}" || exit 0

# src/ 의 C 소스 변경(수정·추가·삭제 모두)이 없으면 검사하지 않는다.
changed="$(git status --porcelain --untracked-files=all -- src 2>/dev/null | grep -E '\.(c|h)$' || true)"
if [ -z "${changed}" ]; then
    exit 0
fi

# 경고만 보이고 통과시키는 출력 (사용자에게 보이는 systemMessage)
warn() {
    python -c 'import json,sys; print(json.dumps({"systemMessage": sys.argv[1]}))' "$1"
}

if ! docker info > /dev/null 2>&1; then
    warn "코드 매트릭 검사(Lizard)를 건너뜀: Docker 를 사용할 수 없습니다. CI 전에 'docker compose run --rm dev tools/run.sh complexity'를 실행하세요."
    exit 0
fi

result="$(docker compose run --rm -T dev lizard -C "${MAX_CCN}" -L "${MAX_FUNC_LINES}" -w src 2>/dev/null)"
status=$?
if [ "${status}" -eq 0 ]; then
    exit 0
fi

message="소스 코드 매트릭 위반 (순환 복잡도 ${MAX_CCN} 이하, 함수 라인수 ${MAX_FUNC_LINES} 이하):
${result}
함수를 분할해서 고친 뒤 'docker compose run --rm dev tools/run.sh complexity'로 다시 확인하세요. (static-analysis 스킬의 '초과 함수 처리' 참고)"

if [ "${stop_active}" = "true" ]; then
    warn "${message}"
    exit 0
fi

printf '%s\n' "${message}" >&2
exit 2
