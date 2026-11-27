#!/usr/bin/env python3
"""@brief src/ 의 모든 함수 정의 위에 Human in the Loop(HITL) Doxygen 주석이 있는지 검사한다.

각 함수 정의 바로 위 Doxygen 블록에 다음이 있어야 한다.

    @par AI 작성
    - 모델: claude-opus-5-5          (어느 AI 모델이 작성했는가)
    - 일시: 2026-11-27               (언제 작성했는가, YYYY-MM-DD)
    - 요청자: 이름                   (누가 요청했는가)
    @par 사용자 리뷰
    - 상태: 미완료 | 완료            (사용자 리뷰가 끝난 코드인가)
    - 리뷰어: 이름                   (상태가 완료일 때 필수)
    - 일시: 2026-11-28               (상태가 완료일 때 필수, AI 작성 일시 이후)

검사 규칙:
  HC1  함수 정의 위에 Doxygen 블록(/** ... */)이 있다
  HC2  `@par AI 작성`에 모델, 일시(YYYY-MM-DD), 요청자가 모두 있다
  HC3  `@par 사용자 리뷰`에 상태(미완료|완료)가 있고, 완료이면 리뷰어와 일시(AI 작성 일시 이후)가 있다
  HC4  (--require-reviewed) 모든 함수의 사용자 리뷰 상태가 완료다 — PR 병합 전 게이트

AI 에이전트는 사용자가 리뷰 완료를 명시적으로 말하기 전에는 상태를 `완료`로 쓰지 않는다.

사용 예 (컨테이너 안):
    python3 check_hitl_comments.py --src src [--require-reviewed] -- -std=c11 -I src

종료 코드: 0 = 통과, 1 = 위반, 2 = 입력·실행 오류
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_return_rules import LocationTracker, run_clang_ast, walk  # noqa: E402

DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
FIELD_RE = re.compile(r"^\s*\*?\s*-\s*([^:：]+?)\s*[:：]\s*(.*?)\s*$")


def find_block(lines, begin_line):
    """@brief 함수 시작 줄 바로 위의 Doxygen 블록 주석 줄들을 찾는다.

    @param lines 파일의 줄 목록
    @param begin_line 함수 정의가 시작하는 줄 번호 (1부터)
    @return 블록 주석의 줄 목록 (없으면 None)
    """
    cursor = begin_line - 2
    while cursor >= 0 and not lines[cursor].strip():
        cursor -= 1
    if cursor < 0 or not lines[cursor].rstrip().endswith("*/"):
        return None
    end = cursor
    while cursor >= 0 and "/**" not in lines[cursor]:
        cursor -= 1
    return lines[cursor:end + 1] if cursor >= 0 else None


def parse_sections(block):
    """@brief 블록에서 `@par <제목>` 구역별 `- 키: 값` 항목을 뽑는다.

    @param block 블록 주석 줄 목록
    @return {구역 제목: {키: 값}}
    """
    sections = {}
    current = None
    for line in block:
        header = re.match(r"^\s*\*?\s*@par\s+(.+?)\s*$", line)
        if header:
            current = header.group(1)
            sections[current] = {}
            continue
        if re.match(r"^\s*\*?\s*@\w+", line):
            current = None
            continue
        field = FIELD_RE.match(line)
        if field and current is not None:
            sections[current][field.group(1)] = field.group(2)
    return sections


def check_block(where, block, require_reviewed):
    """@brief 함수 하나의 HITL 주석을 검사한다.

    @param where 메시지에 붙일 위치 문자열
    @param block 블록 주석 줄 목록 (None이면 주석 없음)
    @param require_reviewed 사용자 리뷰 완료를 요구할지 여부
    @return 위반 메시지 목록
    """
    if block is None:
        return [f"HC1 {where}: Doxygen 블록 주석(/** ... */)이 없음"]
    errors = []
    sections = parse_sections(block)
    author = sections.get("AI 작성")
    if author is None:
        errors.append(f"HC2 {where}: `@par AI 작성` 구역이 없음")
    else:
        for key in ("모델", "일시", "요청자"):
            if not author.get(key):
                errors.append(f"HC2 {where}: AI 작성의 `{key}`가 없거나 비어 있음")
        if author.get("일시") and not DATE_RE.match(author["일시"]):
            errors.append(f"HC2 {where}: AI 작성 일시 형식 오류: {author['일시']} (YYYY-MM-DD)")
    review = sections.get("사용자 리뷰")
    if review is None:
        errors.append(f"HC3 {where}: `@par 사용자 리뷰` 구역이 없음")
        return errors
    status = review.get("상태", "")
    if status not in ("미완료", "완료"):
        errors.append(f"HC3 {where}: 사용자 리뷰 상태는 `미완료` 또는 `완료`여야 함: '{status}'")
    elif status == "완료":
        if not review.get("리뷰어"):
            errors.append(f"HC3 {where}: 리뷰 완료인데 `리뷰어`가 없음")
        date = review.get("일시", "")
        if not DATE_RE.match(date):
            errors.append(f"HC3 {where}: 리뷰 완료인데 `일시`가 없거나 형식 오류: '{date}'")
        elif author and DATE_RE.match(author.get("일시", "")) and date < author["일시"]:
            errors.append(f"HC3 {where}: 리뷰 일시({date})가 AI 작성 일시({author['일시']})보다 앞섬")
    if require_reviewed and status != "완료":
        errors.append(f"HC4 {where}: 사용자 리뷰가 완료되지 않음 (상태: {status or '없음'})")
    return errors


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    argv = list(sys.argv[1:] if argv is None else argv)
    clang_args = []
    if "--" in argv:
        split = argv.index("--")
        argv, clang_args = argv[:split], argv[split + 1:]
    parser = argparse.ArgumentParser(description="HITL Doxygen 주석 검사 (Clang AST)")
    parser.add_argument("--src", default="src", help="소스 루트 (기본 src)")
    parser.add_argument("--clang", default="clang", help="clang 실행 파일 (기본 clang)")
    parser.add_argument("--require-reviewed", action="store_true", help="사용자 리뷰 완료를 요구 (HC4)")
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    if not os.path.isdir(args.src):
        print(f"오류: 소스 디렉터리가 없습니다: {args.src}", file=sys.stderr)
        return 2
    src_root = os.path.abspath(args.src)
    sources = sorted(
        os.path.join(root, name)
        for root, _, names in os.walk(args.src)
        for name in names
        if name.endswith(".c")
    )
    found = {}
    try:
        for source in sources:
            functions = {}
            walk(run_clang_ast(args.clang, source, clang_args), LocationTracker(), functions, src_root)
            for key, (_, has_body, line) in functions.items():
                if has_body:
                    found[key] = line
    except (RuntimeError, OSError, ValueError) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 2

    errors = []
    cache = {}
    for (path, _, name), line in sorted(found.items()):
        if path not in cache:
            with open(path, encoding="utf-8") as handle:
                cache[path] = handle.read().splitlines()
        where = f"{os.path.relpath(path)}:{line}: {name}"
        errors += check_block(where, find_block(cache[path], line), args.require_reviewed)
    print(f"HITL 주석 검사: 함수 {len(found)}개")
    for message in sorted(set(errors)):
        print(f"  {message}")
    if errors:
        print(f"판정: 실패 — 위반 {len(set(errors))}건")
        return 1
    suffix = ", 모든 함수 사용자 리뷰 완료" if args.require_reviewed else ""
    print(f"판정: 통과 (모든 함수에 AI 작성·사용자 리뷰 주석이 있음{suffix})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
