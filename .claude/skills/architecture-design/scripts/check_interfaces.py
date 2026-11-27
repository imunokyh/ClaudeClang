#!/usr/bin/env python3
"""@brief src/ 의 컴포넌트·모듈이 인터페이스 헤더를 통해서만 서로 접근하는지 검사한다.

규약: 디렉터리가 컴포넌트(src/<컴포넌트>/)와 모듈(그 아래 하위 디렉터리, 없으면 컴포넌트 자신)의
경계다. 인터페이스 헤더는 이름이 `_if.h`로 끝난다.

검사 규칙:
  I1  다른 컴포넌트의 파일은 `*_if.h` 만 #include 할 수 있다 (공유 디렉터리는 --shared 로 명시).
      같은 컴포넌트 안에서는 대상 모듈(디렉터리)에 `*_if.h` 가 있을 때(= 인터페이스가 정의된
      모듈) `*_if.h` 만 허용하고, 인터페이스가 없는 하위 모듈은 직접 접근을 허용한다.
  I2  `.c` 파일을 #include 하지 않는다
  I3  헤더에서 전역 변수를 `extern` 으로 공개하지 않는다 (캡슐화)
  I4  컴포넌트 간 의존에 순환이 없다
  I5  모듈 간 의존에 순환이 없다

사용 예:
    python3 check_interfaces.py --src src [--shared common]

종료 코드: 0 = 통과, 1 = 위반, 2 = 입력 오류
"""

import argparse
import os
import re
import sys

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
EXTERN_VAR_RE = re.compile(r"^\s*extern\s+(?!\"C\")[^()]*;")
SOURCE_EXTENSIONS = (".c", ".h")


def list_sources(src_dir):
    """@brief src/ 아래의 .c/.h 파일을 모은다.

    @param src_dir 소스 루트
    @return src_dir 기준 상대 경로(슬래시 구분) 목록
    """
    found = []
    for root, _, names in os.walk(src_dir):
        for name in sorted(names):
            if name.endswith(SOURCE_EXTENSIONS):
                full = os.path.join(root, name)
                found.append(os.path.relpath(full, src_dir).replace(os.sep, "/"))
    return sorted(found)


def resolve_include(src_dir, includer, target):
    """@brief #include "x" 가 가리키는 src/ 내부 파일을 찾는다.

    includer 파일의 디렉터리를 먼저, 그다음 src 루트를 기준으로 찾는다.

    @param src_dir 소스 루트
    @param includer include 하는 파일 (src 기준 상대 경로)
    @param target include 문에 쓴 경로
    @return src 기준 상대 경로, 찾지 못하면 None
    """
    includer_dir = os.path.dirname(includer)
    for base in (includer_dir, ""):
        candidate = os.path.normpath(os.path.join(base, target)).replace(os.sep, "/")
        if os.path.isfile(os.path.join(src_dir, candidate)):
            return candidate
    return None


def component_of(path):
    """@brief 파일이 속한 컴포넌트(최상위 디렉터리)를 구한다.

    @param path src 기준 상대 경로
    @return 컴포넌트 이름 (src 바로 아래 파일이면 ".")
    """
    parts = path.split("/")
    return parts[0] if len(parts) > 1 else "."


def find_cycle(graph):
    """@brief 방향 그래프에서 순환 하나를 찾는다.

    @param graph {노드: 이웃 집합}
    @return 순환을 이루는 노드 목록 (없으면 None)
    """
    state = {}

    def visit(node, stack):
        state[node] = 1
        stack.append(node)
        for nxt in sorted(graph.get(node, ())):
            if state.get(nxt) == 1:
                return stack[stack.index(nxt):] + [nxt]
            if state.get(nxt) is None:
                cycle = visit(nxt, stack)
                if cycle:
                    return cycle
        stack.pop()
        state[node] = 2
        return None

    for node in sorted(graph):
        if state.get(node) is None:
            cycle = visit(node, [])
            if cycle:
                return cycle
    return None


def check(src_dir, shared):
    """@brief 규칙 I1~I5 를 검사한다.

    @param src_dir 소스 루트
    @param shared 인터페이스 규칙에서 제외할 공유 디렉터리 이름 목록
    @return 위반 메시지 목록
    """
    errors = []
    comp_graph = {}
    mod_graph = {}
    sources = list_sources(src_dir)
    # 인터페이스 헤더(*_if.h)를 가진 디렉터리 = 인터페이스가 정의된 모듈
    interface_dirs = {os.path.dirname(p) for p in sources if p.endswith("_if.h")}
    for path in sources:
        with open(os.path.join(src_dir, path), encoding="utf-8", errors="replace") as handle:
            lines = handle.read().splitlines()
        for number, line in enumerate(lines, start=1):
            if path.endswith(".h") and EXTERN_VAR_RE.match(line):
                errors.append(f"I3 {path}:{number}: 헤더에서 전역 변수를 extern 으로 공개함")
            match = INCLUDE_RE.match(line)
            if not match:
                continue
            target = resolve_include(src_dir, path, match.group(1))
            if target is None:
                continue
            if target.endswith(".c"):
                errors.append(f"I2 {path}:{number}: .c 파일을 include 함: {target}")
                continue
            src_dir_of = os.path.dirname(path)
            dst_dir_of = os.path.dirname(target)
            if src_dir_of == dst_dir_of or component_of(target) in shared:
                continue
            same_component = component_of(path) == component_of(target)
            if same_component and dst_dir_of not in interface_dirs:
                # 같은 컴포넌트 안에서 인터페이스가 없는 하위 모듈은 직접 접근을 허용한다.
                mod_graph.setdefault(src_dir_of, set()).add(dst_dir_of)
                continue
            if not os.path.basename(target).endswith("_if.h"):
                errors.append(
                    f"I1 {path}:{number}: 다른 디렉터리의 비공개 헤더를 include 함: {target}"
                    " (인터페이스 헤더 *_if.h 만 허용)"
                )
            mod_graph.setdefault(src_dir_of, set()).add(dst_dir_of)
            if component_of(path) != component_of(target):
                comp_graph.setdefault(component_of(path), set()).add(component_of(target))
    cycle = find_cycle(comp_graph)
    if cycle:
        errors.append("I4 컴포넌트 의존 순환: " + " -> ".join(cycle))
    cycle = find_cycle(mod_graph)
    if cycle:
        errors.append("I5 모듈 의존 순환: " + " -> ".join(cycle))
    return errors


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="인터페이스 경유 접근 규칙 검사")
    parser.add_argument("--src", default="src", help="소스 루트 (기본 src)")
    parser.add_argument(
        "--shared",
        action="append",
        default=[],
        help="인터페이스 규칙에서 제외할 공유 디렉터리 (아키텍처에 명시한 것만, 여러 번 지정 가능)",
    )
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    if not os.path.isdir(args.src):
        print(f"오류: 소스 디렉터리가 없습니다: {args.src}", file=sys.stderr)
        return 2
    files = list_sources(args.src)
    errors = check(args.src, set(args.shared))
    print(f"인터페이스 규칙 검사: 파일 {len(files)}개")
    for message in errors:
        print(f"  {message}")
    if errors:
        print(f"판정: 실패 — 위반 {len(errors)}건")
        return 1
    print("판정: 통과 (인터페이스 경유 접근, 순환 의존 없음)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
