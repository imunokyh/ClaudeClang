#!/usr/bin/env python3
"""@brief llvm-cov 의 MC/DC 보고(`llvm-cov show --show-mcdc`)에서 미충족 조건과 보강 방법을 뽑는다.

결정(decision)마다 MC/DC 를 아직 만족하지 못한 조건(Cn)과, 이미 실행된 테스트 벡터,
그리고 Cn 의 독립 영향을 보이기 위해 추가해야 하는 벡터 후보를 출력한다.

사용 예 (컨테이너 안):
    llvm-cov show <실행파일> -instr-profile=<profdata> --show-mcdc > mcdc.txt
    python3 mcdc_gaps.py mcdc.txt

종료 코드: 0 = 미충족 조건 없음, 1 = 미충족 조건 있음, 2 = 입력 오류

벡터 표기: T/F = 조건 값, - = 단락 평가로 평가되지 않음(무관). 후보 벡터는 결과가 달라지는지를
알 수 없으므로 "상세 설계의 결정표로 결과를 확인해 예상값을 정하라"는 표시를 붙인다.
"""

import argparse
import re
import sys

FILE_RE = re.compile(r"^(\S.*):$")
SOURCE_RE = re.compile(r"^\s*(\d+)\|\s*[^|]*\|(.*)$")
REGION_RE = re.compile(r"MC/DC Decision Region \((\d+):(\d+)\) to \((\d+):(\d+)\)")
COND_RE = re.compile(r"Condition C(\d+) --> \((\d+):(\d+)\)")
VECTOR_RE = re.compile(r"^\|\s*(\d+)\s*\{\s*([TF\-,\s]+?)\s*=\s*([TF])\s*\}")
PAIR_RE = re.compile(r"C(\d+)-Pair: (not covered|covered.*)")


def parse(lines, default_file="(파일 미상)"):
    """@brief show --show-mcdc 출력을 결정 목록으로 파싱한다.

    llvm-cov show 는 대상 파일이 하나뿐이면 파일 이름 줄을 출력하지 않는다.
    이 경우 default_file 을 파일 이름으로 쓴다.

    @param lines 출력의 줄 목록
    @param default_file 파일 이름 줄이 없을 때 쓸 이름
    @return 결정 dict 목록 (file, start, end, conditions, vectors, pairs, source)
    """
    decisions = []
    current_file = default_file
    sources = {default_file: {}}
    decision = None
    for raw in lines:
        file_match = FILE_RE.match(raw)
        if file_match and not raw.startswith((" ", "|")):
            current_file = file_match.group(1)
            sources.setdefault(current_file, {})
            continue
        source_match = SOURCE_RE.match(raw)
        if source_match:
            sources[current_file][int(source_match.group(1))] = source_match.group(2)
            continue
        text = raw.strip()
        region = REGION_RE.search(text)
        if region:
            decision = {
                "file": current_file,
                "start": (int(region.group(1)), int(region.group(2))),
                "end": (int(region.group(3)), int(region.group(4))),
                "conditions": {},
                "vectors": [],
                "pairs": {},
            }
            decisions.append(decision)
            continue
        if decision is None:
            continue
        cond = COND_RE.search(text)
        if cond:
            decision["conditions"][int(cond.group(1))] = (int(cond.group(2)), int(cond.group(3)))
            continue
        vector = VECTOR_RE.match(text)
        if vector:
            values = [v.strip() for v in vector.group(2).split(",")]
            decision["vectors"].append((int(vector.group(1)), values, vector.group(3)))
            continue
        pair = PAIR_RE.search(text)
        if pair:
            decision["pairs"][int(pair.group(1))] = pair.group(2).startswith("covered")
    for item in decisions:
        item["source"] = sources.get(item["file"], {})
    return decisions


def condition_text(decision, number):
    """@brief 조건 Cn 의 소스 텍스트를 잘라 낸다 (같은 줄에 있을 때만, 아니면 위치만).

    @param decision parse()의 결정 dict
    @param number 조건 번호
    @return 소스 조각 문자열
    """
    line, col = decision["conditions"][number]
    text = decision["source"].get(line)
    if text is None:
        return f"({line}:{col})"
    ends = [c for (ln, c) in decision["conditions"].values() if ln == line and c > col]
    end = min(ends) - 1 if ends else decision["end"][1] if decision["end"][0] == line else len(text) + 1
    snippet = text[col - 1:end].strip().rstrip("&|( ").strip()
    # 바깥 괄호가 잘려 들어온 경우, 짝이 없는 닫는 괄호를 뗀다.
    while snippet.endswith(")") and snippet.count(")") > snippet.count("("):
        snippet = snippet[:-1].rstrip()
    return snippet


def candidate_vectors(decision, number):
    """@brief 조건 Cn 의 독립 영향을 보이기 위한 후보 벡터를 만든다.

    Cn 이 이미 평가된 실행 벡터(값이 T/F)에서 Cn 만 뒤집으면 MC/DC 쌍의 한쪽이 된다.
    Cn 을 평가한 벡터가 하나도 없으면(모두 '-') 앞쪽 조건 때문에 Cn 에 도달하지 못한 것이므로,
    도달 벡터를 먼저 추가해야 한다는 뜻으로 빈 목록과 함께 도달 필요 표시를 돌려준다.

    @param decision parse()의 결정 dict
    @param number 조건 번호
    @return (후보 벡터 목록, Cn 을 평가한 벡터가 있는지 여부)
    """
    executed = {tuple(values) for _, values, _ in decision["vectors"]}
    index = number - 1
    found = []
    reached = False
    for _, values, _ in decision["vectors"]:
        if values[index] == "-":
            continue
        reached = True
        candidate = list(values)
        candidate[index] = "F" if values[index] == "T" else "T"
        if tuple(candidate) not in executed and candidate not in found:
            found.append(candidate)
    return found, reached


def report(decisions):
    """@brief 미충족 결정을 사람이 읽을 수 있게 출력한다.

    @param decisions parse()의 결과
    @return 미충족 조건 총 개수
    """
    total_missing = 0
    for decision in decisions:
        missing = sorted(n for n, ok in decision["pairs"].items() if not ok)
        if not missing:
            continue
        total_missing += len(missing)
        line, col = decision["start"]
        print(f"\n{decision['file']}:{line}:{col}  조건 {len(decision['conditions'])}개, 미충족 {len(missing)}개")
        source_line = decision["source"].get(line)
        if source_line is not None:
            print(f"  코드: {source_line.strip()}")
        conds = ", ".join(f"C{n}={condition_text(decision, n)}" for n in sorted(decision["conditions"]))
        print(f"  조건: {conds}")
        header = ", ".join(f"C{n}" for n in sorted(decision["conditions"]))
        print(f"  실행된 벡터 ({header} = 결과):")
        for number, values, result in decision["vectors"]:
            print(f"    {number}: {', '.join(values)} = {result}")
        for n in missing:
            print(f"  [C{n} 미충족] {condition_text(decision, n)}")
            print("    필요: C%d 만 다르고 나머지 조건 값이 같으며(평가된 조건 기준) 결과가 달라지는 벡터 쌍" % n)
            candidates, reached = candidate_vectors(decision, n)
            if not reached:
                print(f"    C{n} 이 한 번도 평가되지 않았다: 앞쪽 조건을 만족시켜 C{n} 에 도달하는 벡터부터 추가한다")
            for candidate in candidates:
                print(f"    후보: {', '.join(candidate)}  → 결정표로 결과를 확인해 예상값을 정할 것")
    return total_missing


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="llvm-cov MC/DC 미충족 조건 분석")
    parser.add_argument("report", help="llvm-cov show --show-mcdc 출력 파일 ('-'은 표준 입력)")
    parser.add_argument("--file", default="(파일 미상)", help="출력에 파일 이름 줄이 없을 때 표시할 파일 이름")
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    try:
        if args.report == "-":
            lines = sys.stdin.read().splitlines()
        else:
            with open(args.report, encoding="utf-8") as handle:
                lines = handle.read().splitlines()
    except OSError as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 2

    decisions = parse(lines, args.file)
    if not decisions:
        print("MC/DC 결정(decision)이 없습니다 (-fcoverage-mcdc 로 빌드했는지, --show-mcdc 를 썼는지 확인)")
        return 0
    missing = report(decisions)
    print(f"\n결정 {len(decisions)}개, MC/DC 미충족 조건 {missing}개")
    if missing:
        print("참고: 후보 벡터는 힌트다. 한 조건을 뒤집으면 단락 평가 때문에 뒤쪽 조건의 평가 여부(-)가")
        print("      달라질 수 있으니, 실제 입력값은 상세 설계 결정표로 정하고 다시 측정해 확인한다.")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
