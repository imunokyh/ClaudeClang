#!/usr/bin/env python3
"""@brief A-SPICE 산출물(docs/swe1~swe5, src/)의 양방향 추적성을 검사한다.

규약은 references/traceability.md에 있다. 마크다운 표의 한 행이 첫 열의 ID를 정의하고,
같은 행의 다른 ID가 연결이다. 코드 블록(```) 안은 무시한다.

사용 예:
    python3 trace_check.py --docs docs --src src --stage all

종료 코드: 0 = 통과, 1 = 추적성 오류, 2 = 입력 오류
"""

import argparse
import os
import re
import sys

ID_RE = re.compile(r"\b(REQ-[FQLC]|SAF|CMP|IF|MOD|MIF|FN|UT|IT)-(\d{3})\b")
TRACE_TAG_RE = re.compile(r"@trace\s+((?:FN-\d{3}[\s,]*)+)")
SAFETY_DOC = "safety-requirements.md"

## 단계 순서. --stage로 지정한 단계까지의 규칙만 검사한다.
STAGES = ["swe1", "swe2", "swe3", "swe3-impl", "swe4", "swe5", "all"]

## ID 종류 묶음
REQ = ("REQ-F", "REQ-Q", "REQ-L", "REQ-C", "SAF")
ARCH = ("CMP", "IF")
DESIGN = ("MOD", "MIF", "FN")

## (규칙 번호, 필요 단계, 방향, 대상 접두어, 연결 대상 접두어, 설명)
## 정방향: 대상 ID마다 연결 대상 접두어를 가진 행이 하나 이상 있어야 한다.
## 역방향: 대상 접두어를 정의하는 행마다 연결 대상 접두어 ID를 하나 이상 가져야 한다.
RULES = [
    ("T1", "swe2", "forward", REQ, ARCH, "요구사항이 아키텍처(CMP/IF)에 연결되지 않음"),
    ("T2", "swe2", "backward", ARCH, REQ, "아키텍처 항목에 상위 요구사항이 없음"),
    ("T3", "swe3", "forward", ARCH, DESIGN, "아키텍처 항목이 상세 설계(MOD/MIF/FN)에 연결되지 않음"),
    ("T4", "swe3", "backward", DESIGN, ARCH, "상세 설계 항목에 상위 아키텍처(CMP/IF)가 없음"),
    ("T5", "swe4", "forward", ("FN",), ("UT",), "함수(FN)에 단위 테스트(UT)가 없음"),
    ("T6", "swe4", "backward", ("UT",), ("FN",), "단위 테스트에 대상 함수(FN)가 없음"),
    ("T7", "swe5", "forward", ("IF",), ("IT",), "인터페이스(IF)에 통합 테스트(IT)가 없음"),
    ("T8", "swe5", "backward", ("IT",), ("IF",), "통합 테스트에 대상 인터페이스(IF)가 없음"),
]


def prefix_of(token):
    """@brief ID 문자열에서 접두어를 뽑는다.

    @param token 예: "REQ-F-001"
    @return 접두어 (예: "REQ-F")
    """
    return token.rsplit("-", 1)[0]


def read_table_rows(path):
    """@brief 마크다운 파일에서 코드 블록 밖의 표 행을 읽는다.

    @param path 마크다운 파일 경로
    @return (줄 번호, 셀 목록) 목록. 구분선 행(---)은 제외한다.
    """
    rows = []
    in_code = False
    with open(path, encoding="utf-8") as handle:
        for number, line in enumerate(handle, start=1):
            stripped = line.strip()
            if stripped.startswith("```"):
                in_code = not in_code
                continue
            if in_code or not stripped.startswith("|"):
                continue
            cells = [cell.strip() for cell in stripped.strip("|").split("|")]
            if all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells if cell):
                continue
            rows.append((number, cells))
    return rows


def collect_definitions(docs_dir):
    """@brief docs/ 아래 모든 마크다운에서 ID 정의(첫 열)와 연결(같은 행)을 모은다.

    @param docs_dir docs 디렉터리
    @return (정의 dict {ID: (파일, 줄, 연결 ID 집합)}, 오류 목록)
    """
    definitions = {}
    errors = []
    for root, _, names in os.walk(docs_dir):
        if os.sep + "reviews" in root + os.sep:
            continue
        for name in sorted(names):
            if not name.endswith(".md"):
                continue
            path = os.path.join(root, name)
            for number, cells in read_table_rows(path):
                first = ID_RE.fullmatch(cells[0]) if cells else None
                if not first:
                    continue
                token = cells[0]
                links = {m.group(0) for cell in cells[1:] for m in ID_RE.finditer(cell)}
                if token in definitions:
                    prev_file, prev_line, _ = definitions[token]
                    errors.append(
                        f"T11 {path}:{number}: {token}을(를) 중복 정의함 (이전: {prev_file}:{prev_line})"
                    )
                    continue
                definitions[token] = (path, number, links)
    return definitions, errors


def collect_code_traces(src_dir):
    """@brief src/ 의 Doxygen @trace FN-nnn 태그를 모은다.

    @param src_dir 소스 디렉터리
    @return {FN ID: [(파일, 줄)]}
    """
    traces = {}
    for root, _, names in os.walk(src_dir):
        for name in sorted(names):
            if os.path.splitext(name)[1] not in (".c", ".h"):
                continue
            path = os.path.join(root, name)
            with open(path, encoding="utf-8") as handle:
                for number, line in enumerate(handle, start=1):
                    match = TRACE_TAG_RE.search(line)
                    if match:
                        for token in re.findall(r"FN-\d{3}", match.group(1)):
                            traces.setdefault(token, []).append((path, number))
    return traces


def check_definitions(definitions, stage_index):
    """@brief T11(존재하지 않는 ID 연결), T12(안전 요구사항 문서 분리)를 검사한다.

    @param definitions collect_definitions()의 정의 dict
    @param stage_index 검사할 최대 단계 번호
    @return 오류 목록
    """
    errors = []
    for token, (path, number, links) in sorted(definitions.items()):
        if prefix_of(token) == "SAF" and os.path.basename(path) != SAFETY_DOC:
            errors.append(f"T12 {path}:{number}: {token}은(는) {SAFETY_DOC}에서만 정의해야 함")
        for link in sorted(links):
            if link not in definitions:
                errors.append(f"T11 {path}:{number}: {token}이(가) 존재하지 않는 {link}을(를) 연결함")
    return errors


def check_rules(definitions, stage_index):
    """@brief T1~T8 정방향·역방향 규칙을 검사한다.

    @param definitions collect_definitions()의 정의 dict
    @param stage_index 검사할 최대 단계 번호
    @return 오류 목록
    """
    errors = []
    for rule, stage, direction, targets, others, message in RULES:
        if STAGES.index(stage) > stage_index:
            continue
        if direction == "forward":
            linked = set()
            for token, (_, _, links) in definitions.items():
                if prefix_of(token) in others:
                    linked |= links
            for token in sorted(definitions):
                if prefix_of(token) in targets and token not in linked:
                    path, number, _ = definitions[token]
                    errors.append(f"{rule} {path}:{number}: {token}: {message}")
        else:
            for token, (path, number, links) in sorted(definitions.items()):
                if prefix_of(token) in targets and not any(prefix_of(x) in others for x in links):
                    errors.append(f"{rule} {path}:{number}: {token}: {message}")
    return errors


def check_code(definitions, traces):
    """@brief T9(FN이 코드 @trace에 있음), T10(@trace가 가리키는 FN이 설계에 있음)을 검사한다.

    @param definitions collect_definitions()의 정의 dict
    @param traces collect_code_traces()의 결과
    @return 오류 목록
    """
    errors = []
    for token in sorted(definitions):
        if prefix_of(token) == "FN" and token not in traces:
            path, number, _ = definitions[token]
            errors.append(f"T9 {path}:{number}: {token}: src/에 @trace {token} 가 없음 (미구현 또는 태그 누락)")
    for token, places in sorted(traces.items()):
        if token not in definitions:
            for path, number in places:
                errors.append(f"T10 {path}:{number}: @trace {token}: 상세 설계에 없는 FN")
    return errors


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="A-SPICE 양방향 추적성 검사")
    parser.add_argument("--docs", default="docs", help="산출물 디렉터리 (기본 docs)")
    parser.add_argument("--src", default="src", help="소스 디렉터리 (기본 src)")
    parser.add_argument("--stage", choices=STAGES, default="all", help="이 단계까지 검사 (기본 all)")
    args = parser.parse_args(argv)

    # Windows 콘솔(cp949)에서도 한글 출력이 깨지거나 예외가 나지 않게 UTF-8로 고정한다.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    if not os.path.isdir(args.docs):
        print(f"오류: 산출물 디렉터리가 없습니다: {args.docs}", file=sys.stderr)
        return 2

    stage_index = STAGES.index(args.stage)
    definitions, errors = collect_definitions(args.docs)
    errors += check_definitions(definitions, stage_index)
    errors += check_rules(definitions, stage_index)
    if stage_index >= STAGES.index("swe3-impl") and os.path.isdir(args.src):
        errors += check_code(definitions, collect_code_traces(args.src))

    counts = {}
    for token in definitions:
        counts[prefix_of(token)] = counts.get(prefix_of(token), 0) + 1
    summary = ", ".join(f"{key} {counts[key]}" for key in sorted(counts)) or "정의된 ID 없음"
    print(f"추적성 검사 (단계 {args.stage}): {summary}")
    if not definitions:
        print("오류: ID를 정의한 표가 하나도 없습니다 (docs/ 경로와 표 형식을 확인)", file=sys.stderr)
        return 2
    for message in errors:
        print(f"  {message}")
    if errors:
        print(f"판정: 실패 — 오류 {len(errors)}건")
        return 1
    print("판정: 통과 (양방향 추적성 100%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
