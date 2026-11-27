#!/usr/bin/env python3
"""@brief 제품 코드(src/)의 금지 호출을 검사한다.

규칙:
  B1 동적 메모리 할당 함수(malloc, calloc, realloc, free, strdup 등)를 호출하지 않는다.
  B2 재귀 호출을 하지 않는다(직접 재귀, 정의된 함수 사이의 순환 호출).
  B3 시스템 시계·난수 함수(time, clock, clock_gettime, gettimeofday, rand, srand 등)를 호출하지 않는다.

한계: 정규식 기반이다. 함수 포인터를 통한 호출은 추적하지 못한다(제품 코드는 함수 포인터 호출 금지를 권장).
주석과 문자열은 지우고 검사한다.

종료 코드: 0 = 통과, 1 = 위반, 2 = 입력 오류
"""

import argparse
import os
import re
import sys

## B1 금지 함수
ALLOC = {"malloc", "calloc", "realloc", "free", "strdup", "strndup", "aligned_alloc", "posix_memalign", "alloca"}
## B3 금지 함수
CLOCK = {"time", "clock", "clock_gettime", "gettimeofday", "timespec_get", "rand", "srand", "random", "srandom",
         "localtime", "gmtime", "mktime"}
## 호출처럼 보이지만 호출이 아닌 예약어
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "defined", "_Alignof", "_Static_assert", "do", "else"}

CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
DEF_RE = re.compile(r"^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)


def strip_noise(text):
    """@brief 주석과 문자열·문자 리터럴을 같은 길이의 공백으로 바꾼다(줄 번호 유지).

    @param text C 소스 텍스트
    @return 처리된 텍스트
    """
    pattern = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\.|[^"\\n])*"|\'(?:\.|[^\'\\n])*\'', re.S)
    return pattern.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def function_bodies(text):
    """@brief 함수 정의 이름과 본문(중괄호 짝 맞춤) 위치를 찾는다.

    @param text strip_noise() 처리된 텍스트
    @return [(함수 이름, 본문 시작 오프셋, 본문 끝 오프셋)]
    """
    result = []
    for match in DEF_RE.finditer(text):
        start = match.end() - 1
        depth = 0
        index = start
        while index < len(text):
            if text[index] == "{":
                depth += 1
            elif text[index] == "}":
                depth -= 1
                if depth == 0:
                    break
            index += 1
        result.append((match.group(1), start, index))
    return result


def analyze(files):
    """@brief 파일 목록을 검사해 위반을 모은다.

    @param files {경로: 텍스트}
    @return 위반 메시지 목록
    """
    errors = []
    defined = {}
    bodies = {}
    for path, raw in files.items():
        text = strip_noise(raw)
        for name, start, end in function_bodies(text):
            defined[name] = path
            bodies[name] = (path, text, start, end)
    graph = {}
    for name, (path, text, start, end) in bodies.items():
        callees = set()
        for call in CALL_RE.finditer(text, start + 1, end):
            callee = call.group(1)
            line = text.count("\n", 0, call.start()) + 1
            if callee in KEYWORDS:
                continue
            if callee in ALLOC:
                errors.append(f"B1 {path}:{line}: 동적 메모리 함수 {callee}() 호출 금지")
            elif callee in CLOCK:
                errors.append(f"B3 {path}:{line}: 시계·난수 함수 {callee}() 호출 금지")
            if callee in defined:
                callees.add(callee)
        graph[name] = callees
    errors.extend(find_cycles(graph, bodies))
    return errors


def find_cycles(graph, bodies):
    """@brief 호출 그래프에서 순환(재귀)을 찾는다.

    @param graph {함수: 호출하는 정의된 함수 집합}
    @param bodies {함수: (경로, 텍스트, 시작, 끝)}
    @return B2 위반 메시지 목록
    """
    errors = []
    state = {}

    def visit(node, chain):
        state[node] = 1
        for callee in sorted(graph.get(node, ())):
            if state.get(callee) == 1:
                cycle = chain[chain.index(callee):] + [callee] if callee in chain else [node, callee]
                path, text, start, _ = bodies[callee]
                line = text.count("\n", 0, start) + 1
                errors.append(f"B2 {path}:{line}: 재귀 호출 금지: {' -> '.join(cycle)}")
            elif state.get(callee) is None:
                visit(callee, chain + [callee])
        state[node] = 2

    for node in sorted(graph):
        if state.get(node) is None:
            visit(node, [node])
    return errors


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="금지 호출 검사(B1 동적 메모리, B2 재귀, B3 시계·난수)")
    parser.add_argument("--src", default="src", help="소스 디렉터리")
    args = parser.parse_args(argv)
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    files = {}
    for root, _, names in os.walk(args.src):
        for name in sorted(names):
            if name.endswith((".c", ".h")):
                path = os.path.join(root, name)
                try:
                    with open(path, encoding="utf-8") as handle:
                        files[path.replace("\\", "/")] = handle.read()
                except OSError as exc:
                    print(f"오류: {exc}", file=sys.stderr)
                    return 2
    errors = analyze(files)
    for message in errors:
        print(message)
    print(f"금지 호출 검사: 파일 {len(files)}개, 위반 {len(errors)}건")
    print("판정: 통과" if not errors else "판정: 실패")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
