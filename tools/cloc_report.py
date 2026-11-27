#!/usr/bin/env python3
"""@brief CLOC CSV(--by-file --csv) 결과로 주석률을 계산해 출력하고 CI용 파일로 저장한다.

주석률 = 주석 줄 ÷ (주석 줄 + 코드 줄), 빈 줄 제외.
기준(--min-ratio, 백분율)을 주면 그 미만인 파일이 있을 때 실패한다. 기본은 0(보고만).

출력 파일 (--out-dir):
  cloc-comment-ratio.csv   Jenkins Plot 플러그인용 한 줄 CSV (비율 %)
  cloc-files.csv           파일별 코드·주석·빈 줄·주석률

종료 코드: 0 = 통과(또는 보고 전용), 1 = 기준 미달, 2 = 입력 오류
"""

import argparse
import csv
import os
import sys


def read_rows(path):
    """@brief CLOC CSV에서 파일별 줄 수를 읽는다 (SUM 줄 제외).

    @param path CSV 경로
    @return [(파일, 빈 줄, 주석, 코드)] 목록
    @raises ValueError 형식이 CLOC --by-file --csv 가 아닌 경우
    """
    rows = []
    with open(path, encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        needed = {"filename", "blank", "comment", "code"}
        if reader.fieldnames is None or not needed <= set(reader.fieldnames):
            raise ValueError("CLOC --by-file --csv 형식이 아닙니다 (열: language,filename,blank,comment,code)")
        for record in reader:
            if record["filename"] == "SUM":
                continue
            rows.append((record["filename"], int(record["blank"]), int(record["comment"]), int(record["code"])))
    return rows


def ratio(comment, code):
    """@brief 주석률(%)을 구한다.

    @param comment 주석 줄 수
    @param code 코드 줄 수
    @return 주석률 (주석+코드가 0이면 100.0)
    """
    total = comment + code
    return 100.0 if total == 0 else 100.0 * comment / total


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="CLOC 주석률 보고")
    parser.add_argument("csv", help="cloc --by-file --csv 출력")
    parser.add_argument("--out-dir", default=".", help="CI용 파일을 쓸 디렉터리")
    parser.add_argument("--min-ratio", type=float, default=0.0, help="파일마다 요구하는 최소 주석률(%%), 0이면 보고만")
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    try:
        rows = read_rows(args.csv)
    except (OSError, ValueError) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 2

    os.makedirs(args.out_dir, exist_ok=True)
    total_comment = sum(r[2] for r in rows)
    total_code = sum(r[3] for r in rows)
    failures = []
    with open(os.path.join(args.out_dir, "cloc-files.csv"), "w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["file", "code", "comment", "blank", "comment_ratio_percent"])
        for name, blank, comment, code in sorted(rows):
            value = ratio(comment, code)
            writer.writerow([name, code, comment, blank, f"{value:.2f}"])
            print(f"  {name}: 코드 {code}, 주석 {comment}, 주석률 {value:.1f}%")
            if value < args.min_ratio:
                failures.append((name, value))
    overall = ratio(total_comment, total_code)
    with open(os.path.join(args.out_dir, "cloc-comment-ratio.csv"), "w", encoding="utf-8", newline="") as handle:
        handle.write("comment_ratio_percent\n")
        handle.write(f"{overall:.2f}\n")
    print(f"주석률 전체: {overall:.1f}% (주석 {total_comment}줄, 코드 {total_code}줄, 파일 {len(rows)}개)")
    if args.min_ratio > 0:
        for name, value in failures:
            print(f"  미달 {name}: {value:.1f}% < {args.min_ratio:g}%")
        if failures:
            print(f"판정: 실패 — 주석률 {args.min_ratio:g}% 미만 파일 {len(failures)}개")
            return 1
        print(f"판정: 통과 (모든 파일 주석률 {args.min_ratio:g}% 이상)")
    else:
        print("판정: 보고 전용 (주석률 기준 없음)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
