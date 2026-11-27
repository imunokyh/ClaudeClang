#!/usr/bin/env python3
"""@brief llvm-cov 커버리지 요약(JSON)으로 커버리지 기준 충족 여부를 판정한다.

단위 테스트(SWE.4)는 MC/DC, 통합 테스트(SWE.5)는 함수(functions) 커버리지를
검사한다. 기준에 못 미치는 파일과 미충족 항목 수를 출력하고 종료 코드로 결과를 알린다.

사용 예 (컨테이너 안):
    llvm-cov export <실행파일> -instr-profile=<profdata> -summary-only > cov.json
    python3 tools/coverage_gate.py cov.json --metric mcdc --include src/

종료 코드: 0 = 통과, 1 = 기준 미달, 2 = 입력 오류
"""

import argparse
import json
import os
import sys

## 측정 누락 검사 대상 확장자
SOURCE_EXTENSIONS = {".c", ".cc", ".cpp"}

## llvm-cov export JSON의 summary 키와 보고용 이름
METRICS = {
    "mcdc": "MC/DC",
    "lines": "Line",
    "branches": "Branch",
    "regions": "Region",
    "functions": "Function",
}


def load_files(path):
    """@brief llvm-cov export JSON에서 파일별 summary 목록을 읽는다.

    @param path JSON 파일 경로 ("-"이면 표준 입력)
    @return (파일 이름, summary dict) 튜플 목록
    @raises ValueError JSON 구조가 llvm-cov export 형식이 아닌 경우
    """
    if path == "-":
        data = json.load(sys.stdin)
    else:
        with open(path, encoding="utf-8") as handle:
            data = json.load(handle)
    try:
        files = data["data"][0]["files"]
    except (KeyError, IndexError, TypeError) as exc:
        raise ValueError("llvm-cov export 형식이 아닙니다") from exc
    return [(item["filename"], item["summary"]) for item in files]


def check(files, metric, threshold, includes):
    """@brief 파일별 커버리지를 기준과 비교한다.

    측정 대상 항목이 0개인 파일(예: 조건문이 없는 파일의 MC/DC)은 통과로 본다.

    @param files load_files()의 결과
    @param metric METRICS의 키
    @param threshold 기준 백분율 (예: 100.0)
    @param includes 경로에 이 문자열 중 하나가 들어간 파일만 검사 (빈 목록이면 전체)
    @return (검사한 파일 수, 미달 목록[(파일, 충족 수, 전체 수, 백분율)], 전체 충족 수, 전체 수)
    """
    failures = []
    checked = 0
    total_covered = 0
    total_count = 0
    for name, summary in files:
        normalized = name.replace("\\", "/")
        if includes and not any(inc in normalized for inc in includes):
            continue
        checked += 1
        entry = summary.get(metric, {})
        count = entry.get("count", 0)
        covered = entry.get("covered", 0)
        total_count += count
        total_covered += covered
        if count == 0:
            continue
        percent = 100.0 * covered / count
        if percent < threshold:
            failures.append((normalized, covered, count, percent))
    return checked, failures, total_covered, total_count


def find_missing(files, require_dir):
    """@brief 디렉터리의 소스 파일 중 커버리지 보고서에 없는 파일을 찾는다.

    테스트 실행 파일에 링크되지 않은 소스는 보고서에 아예 나타나지 않아
    커버리지 100%로 잘못 판정될 수 있으므로 따로 검출한다.

    @param files load_files()의 결과
    @param require_dir 검사할 디렉터리 (None이면 검사하지 않음)
    @return 누락된 파일의 절대 경로 목록 (정렬됨)
    """
    if not require_dir:
        return []
    reported = {os.path.realpath(name) for name, _ in files}
    missing = []
    for root, _, names in os.walk(require_dir):
        for name in names:
            if os.path.splitext(name)[1] in SOURCE_EXTENSIONS:
                path = os.path.realpath(os.path.join(root, name))
                if path not in reported:
                    missing.append(path)
    return sorted(missing)


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="llvm-cov 커버리지 기준 판정")
    parser.add_argument("json", help="llvm-cov export -summary-only 출력 파일 ('-'은 표준 입력)")
    parser.add_argument("--metric", choices=sorted(METRICS), required=True)
    parser.add_argument("--threshold", type=float, default=100.0, help="기준 백분율 (기본 100)")
    parser.add_argument(
        "--include",
        action="append",
        default=[],
        help="검사할 경로 조각 (여러 번 지정 가능, 예: src/)",
    )
    parser.add_argument(
        "--require-dir",
        help="이 디렉터리 아래의 모든 .c/.cpp 파일이 보고서에 있어야 한다 (측정 누락 검출)",
    )
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    try:
        files = load_files(args.json)
    except (OSError, ValueError) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 2

    checked, failures, covered, count = check(files, args.metric, args.threshold, args.include)
    label = METRICS[args.metric]
    if checked == 0:
        print(f"오류: 검사 대상 파일이 없습니다 (include={args.include})", file=sys.stderr)
        return 2

    missing = find_missing(files, args.require_dir)
    for path in missing:
        print(f"  누락 {path}: 테스트 실행 파일에 링크되지 않아 측정되지 않음")

    total = 100.0 if count == 0 else 100.0 * covered / count
    print(f"{label} 커버리지: {covered}/{count} = {total:.2f}% (파일 {checked}개, 기준 {args.threshold:g}%)")
    for name, f_cov, f_cnt, pct in failures:
        print(f"  미달 {name}: {f_cov}/{f_cnt} = {pct:.2f}% (미충족 {f_cnt - f_cov}건)")
    if failures or missing:
        print(f"판정: 실패 — 미달 파일 {len(failures)}개, 누락 파일 {len(missing)}개")
        return 1
    print("판정: 통과")
    return 0


if __name__ == "__main__":
    sys.exit(main())
