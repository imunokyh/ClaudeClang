#!/usr/bin/env python3
"""@brief PICT 모델 파일에서 페어와이즈 조합을 만들어 테스트 코드에 붙일 표로 출력한다.

PICT(`pict`, 컨테이너에 설치됨)를 실행해 조합을 만들고, 행마다 번호를 붙여
GoogleTest 의 값 파라미터 테스트(INSTANTIATE_TEST_SUITE_P)에 바로 붙일 수 있는 C++ 초기화 목록으로 낸다.
예상 출력값은 **만들지 않는다** — 행마다 상세 설계(결정표)로 직접 정하고 `/* 예상 */` 자리에 채운다.

모델 작성 규칙은 system-test-case-generator/references/techniques.md "페어와이즈" 절과 같다
(값은 동치 클래스 대표값·경계값에서, 불가능한 조합은 제약으로 제거, 이름은 영문·숫자).

사용 예:
    python3 pict_cases.py model.pict.txt --id PICT-LOCK-01 [--order 2] [--out cases.json]

종료 코드: 0 = 성공, 2 = 입력·실행 오류
"""

import argparse
import json
import shutil
import subprocess
import sys


def run_pict(model, order):
    """@brief pict 를 실행해 조합 표를 얻는다.

    @param model 모델 파일 경로
    @param order 조합 차수 (2 = 페어와이즈)
    @return (파라미터 이름 목록, 행 목록(문자열 리스트))
    @raises RuntimeError pict 실행이 실패한 경우
    """
    exe = shutil.which("pict")
    if exe is None:
        raise RuntimeError("pict 를 찾을 수 없습니다 (컨테이너 안에서 실행하세요)")
    result = subprocess.run(
        [exe, model, f"/o:{order}"], capture_output=True, text=True, encoding="utf-8", check=False
    )
    if result.returncode != 0:
        raise RuntimeError(f"pict 실행 실패\n{result.stderr.strip() or result.stdout.strip()}")
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    header = lines[0].split("\t")
    rows = [line.split("\t") for line in lines[1:]]
    return header, rows


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="PICT 조합 → GoogleTest 파라미터 표")
    parser.add_argument("model", help="PICT 모델 파일")
    parser.add_argument("--id", required=True, help="모델 ID (테스트 주석에 쓰는 추적용 이름)")
    parser.add_argument("--order", type=int, default=2, help="조합 차수 (기본 2)")
    parser.add_argument("--out", help="행을 JSON 으로도 저장할 경로")
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    try:
        header, rows = run_pict(args.model, args.order)
    except (RuntimeError, OSError) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 2

    print(f"// {args.id}: {args.order}-way, 파라미터 {len(header)}개, 행 {len(rows)}개")
    print(f"// 열: {', '.join(header)}")
    print("// 예상 출력값은 상세 설계 결정표로 행마다 정해 /* 예상 */ 자리에 채운다.")
    print("{")
    for number, row in enumerate(rows, start=1):
        print(f'    {{ "{args.id}#{number}", {", ".join(row)}, /* 예상 */ }},')
    print("};")
    if args.out:
        with open(args.out, "w", encoding="utf-8") as handle:
            json.dump(
                {"id": args.id, "order": args.order, "columns": header, "rows": rows},
                handle,
                ensure_ascii=False,
                indent=2,
            )
        print(f"// JSON 저장: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
