#!/usr/bin/env python3
"""@brief src/ 의 C 함수가 반환 규칙을 지키는지 Clang AST로 검사한다.

검사 규칙:
  RT1  함수는 반드시 값을 반환한다 (반환 타입이 void 이면 위반)
  RT2  return 문은 함수 안에 정확히 한 번만 있다
  RT3  그 return 문은 함수 본문의 마지막 문장이다
  RT4  src/ 에 정의된 함수의 반환값을 버리지 않는다 (호출 결과를 변수·조건·인자로 사용한다.
       `(void)f()` 형태로 버리는 것도 위반)

사용 예 (컨테이너 안):
    python3 check_return_rules.py --src src -- -std=c11 -I src

`--` 뒤의 인자는 clang 에 그대로 전달한다. 종료 코드: 0 = 통과, 1 = 위반, 2 = 입력·실행 오류
"""

import argparse
import json
import os
import re
import subprocess
import sys

VOID_RETURN_RE = re.compile(r"^void\s*\(")


def run_clang_ast(clang, source, clang_args):
    """@brief clang 으로 한 소스 파일의 AST(JSON)를 얻는다.

    @param clang clang 실행 파일 이름
    @param source 소스 파일 경로
    @param clang_args clang 에 전달할 추가 인자 목록
    @return AST 루트 dict
    @raises RuntimeError clang 실행이 실패한 경우
    """
    cmd = [clang, "-fsyntax-only", "-Xclang", "-ast-dump=json", *clang_args, source]
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", check=False)
    if result.returncode != 0 or not result.stdout.strip():
        raise RuntimeError(f"{source}: clang AST 생성 실패\n{result.stderr.strip()}")
    return json.loads(result.stdout)


class LocationTracker:
    """@brief AST JSON 의 위치 정보(파일 이름은 바뀔 때만 출력됨)를 순서대로 따라간다."""

    def __init__(self):
        """@brief 현재 파일·줄을 비워 둔 상태로 만든다."""
        self.file = None
        self.line = 0

    def feed(self, location):
        """@brief loc/range 한 덩어리를 읽어 현재 파일·줄을 갱신한다.

        @param location loc 또는 range 의 begin/end dict
        """
        if not isinstance(location, dict):
            return
        for key in ("spellingLoc", "expansionLoc"):
            if key in location:
                self.feed(location[key])
        if "file" in location:
            self.file = location["file"]
        if "line" in location:
            self.line = location["line"]


def node_children(node):
    """@brief 노드의 자식 목록을 돌려준다.

    @param node AST 노드 dict
    @return 자식 노드 목록 (없으면 빈 목록)
    """
    return node.get("inner", []) if isinstance(node, dict) else []


def collect_returns(node, found):
    """@brief 노드 아래의 ReturnStmt 를 모두 모은다.

    @param node AST 노드
    @param found 결과를 채울 목록
    """
    if node.get("kind") == "ReturnStmt":
        found.append(node)
    for child in node_children(node):
        collect_returns(child, found)


def callee_name(call):
    """@brief CallExpr 가 부르는 함수 이름을 구한다.

    @param call CallExpr 노드
    @return 이름 (직접 호출이 아니면 None)
    """
    stack = list(node_children(call)[:1])
    while stack:
        node = stack.pop()
        decl = node.get("referencedDecl")
        if decl and decl.get("kind") == "FunctionDecl":
            return decl.get("name")
        stack.extend(node_children(node))
    return None


def collect_discarded_calls(node, found):
    """@brief 반환값을 버리는 호출문(문장 위치의 CallExpr, (void) 캐스트 포함)을 모은다.

    @param node AST 노드
    @param found (호출 노드, 이름) 목록을 채울 목록
    """
    if node.get("kind") == "CompoundStmt":
        for child in node_children(node):
            target = child
            if child.get("kind") == "CStyleCastExpr" and child.get("type", {}).get("qualType") == "void":
                inner = node_children(child)
                target = inner[0] if inner else child
            if target.get("kind") == "CallExpr":
                name = callee_name(target)
                if name:
                    found.append((target, name))
    for child in node_children(node):
        collect_discarded_calls(child, found)


def walk(node, tracker, functions, src_root):
    """@brief AST 를 문서 순서대로 돌며 src/ 의 함수 정의와 선언을 모은다.

    @param node AST 노드
    @param tracker 위치 추적기
    @param functions {(파일, 줄, 이름): (노드, 정의 여부)} 를 채울 dict
    @param src_root src 절대 경로
    """
    tracker.feed(node.get("loc"))
    if isinstance(node.get("range"), dict):
        tracker.feed(node["range"].get("begin"))
    if node.get("kind") == "FunctionDecl" and not node.get("isImplicit"):
        path = os.path.abspath(tracker.file) if tracker.file else ""
        if path.startswith(src_root + os.sep):
            has_body = any(c.get("kind") == "CompoundStmt" for c in node_children(node))
            functions[(path, tracker.line, node.get("name"))] = (node, has_body, tracker.line)
    for child in node_children(node):
        walk(child, tracker, functions, src_root)
    if isinstance(node.get("range"), dict):
        tracker.feed(node["range"].get("end"))


def check_function(path, line, name, node):
    """@brief 함수 정의 하나에 RT1~RT3 을 적용한다.

    @param path 파일 경로
    @param line 줄 번호
    @param name 함수 이름
    @param node FunctionDecl 노드
    @return 위반 메시지 목록
    """
    errors = []
    where = f"{os.path.relpath(path)}:{line}"
    if VOID_RETURN_RE.match(node.get("type", {}).get("qualType", "")):
        errors.append(f"RT1 {where}: {name}: void 함수 — 상태 코드를 반환해야 함")
    body = next(c for c in node_children(node) if c.get("kind") == "CompoundStmt")
    returns = []
    collect_returns(body, returns)
    statements = node_children(body)
    if len(returns) > 1:
        errors.append(f"RT2 {where}: {name}: return 문이 {len(returns)}개 — 마지막에 한 번만 호출해야 함")
    if len(returns) == 1 and (not statements or statements[-1].get("kind") != "ReturnStmt"):
        errors.append(f"RT3 {where}: {name}: return 문이 함수 본문의 마지막 문장이 아님")
    if not returns and not VOID_RETURN_RE.match(node.get("type", {}).get("qualType", "")):
        errors.append(f"RT2 {where}: {name}: return 문이 없음")
    return errors


def check_file(clang, source, clang_args, src_root):
    """@brief 소스 파일 하나를 검사한다.

    @param clang clang 실행 파일 이름
    @param source 소스 파일 경로
    @param clang_args clang 추가 인자
    @param src_root src 절대 경로
    @return (위반 메시지 목록, src/ 안의 함수 이름 집합)
    """
    root = run_clang_ast(clang, source, clang_args)
    functions = {}
    walk(root, LocationTracker(), functions, src_root)
    errors = []
    names = {key[2] for key in functions}
    for (path, line, name), (node, has_body, _) in sorted(functions.items()):
        if has_body:
            errors.extend(check_function(path, line, name, node))
    return errors, names, root


def check_discarded(root, src_names, source):
    """@brief RT4: src 함수의 반환값을 버리는 호출을 찾는다.

    @param root AST 루트
    @param src_names src/ 에 선언된 함수 이름 집합
    @param source 검사 중인 소스 파일 (메시지용)
    @return 위반 메시지 목록
    """
    errors = []
    tracker = LocationTracker()
    functions = {}
    walk(root, tracker, functions, os.path.abspath("src"))
    for (path, line, name), (node, has_body, _) in functions.items():
        if not has_body:
            continue
        body = next(c for c in node_children(node) if c.get("kind") == "CompoundStmt")
        calls = []
        collect_discarded_calls(body, calls)
        counts = {}
        for _, callee in calls:
            if callee in src_names:
                counts[callee] = counts.get(callee, 0) + 1
        for callee, count in sorted(counts.items()):
            errors.append(
                f"RT4 {os.path.relpath(path)}:{line}: {name}: {callee}()의 반환값을 버림 ({count}곳)"
            )
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
    parser = argparse.ArgumentParser(description="C 함수 반환 규칙 검사 (Clang AST)")
    parser.add_argument("--src", default="src", help="소스 루트 (기본 src)")
    parser.add_argument("--clang", default="clang", help="clang 실행 파일 (기본 clang)")
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
    errors = []
    all_names = set()
    roots = []
    try:
        for source in sources:
            file_errors, names, root = check_file(args.clang, source, clang_args, src_root)
            errors.extend(file_errors)
            all_names |= names
            roots.append((source, root))
    except (RuntimeError, OSError, json.JSONDecodeError) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 2
    for source, root in roots:
        errors.extend(check_discarded(root, all_names, source))
    errors = sorted(set(errors))
    print(f"반환 규칙 검사: 소스 {len(sources)}개")
    for message in errors:
        print(f"  {message}")
    if errors:
        print(f"판정: 실패 — 위반 {len(errors)}건")
        return 1
    print("판정: 통과 (모든 함수가 마지막에 한 번만 값을 반환)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
