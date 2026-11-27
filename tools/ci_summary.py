#!/usr/bin/env python3
"""@brief 단위·통합·TDD 커버리지 요약(summary.json)을 읽어 표로 출력하고 Jenkins Plot용 CSV를 만든다.

입력: build/reports/<unit|integration|tdd>/summary.json (llvm-cov export -summary-only)
출력: build/reports/coverage-summary.csv   열: unit_mcdc, integration_functions, tdd_lines (%)
이 스크립트는 판정하지 않는다 (판정은 tools/run.sh unit|integration 이 한다).

종료 코드: 항상 0
"""

import json
import os
import sys

TARGETS = [
    ("unit", "mcdc", "unit_mcdc"),
    ("integration", "functions", "integration_functions"),
    ("tdd", "lines", "tdd_lines"),
]


def percent(path, metric):
    """@brief summary.json 에서 지표의 전체 커버리지(%)를 구한다.

    @param path summary.json 경로
    @param metric llvm-cov 지표 이름 (mcdc, functions, lines ...)
    @return 백분율 (파일이 없거나 측정 대상이 없으면 None)
    """
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as handle:
        totals = json.load(handle)["data"][0]["totals"]
    entry = totals.get(metric, {})
    count = entry.get("count", 0)
    return None if count == 0 else 100.0 * entry.get("covered", 0) / count


def main(reports="build/reports"):
    """@brief 요약 표와 CSV를 만든다.

    @param reports 보고서 디렉터리
    @return 종료 코드 (0)
    """
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    header, values = [], []
    for kind, metric, column in TARGETS:
        value = percent(os.path.join(reports, kind, "summary.json"), metric)
        shown = "측정 안 됨" if value is None else f"{value:.2f}%"
        print(f"  {kind:12s} {metric:10s} {shown}")
        header.append(column)
        values.append("0" if value is None else f"{value:.2f}")
    os.makedirs(reports, exist_ok=True)
    with open(os.path.join(reports, "coverage-summary.csv"), "w", encoding="utf-8") as handle:
        handle.write(",".join(header) + "\n" + ",".join(values) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:2]))
