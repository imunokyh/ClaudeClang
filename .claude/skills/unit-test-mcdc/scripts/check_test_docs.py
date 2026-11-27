#!/usr/bin/env python3
"""@brief GoogleTest 테스트 함수의 Doxygen 주석과 단언 유무를 검사한다 (단위·통합 테스트 공용).

검사 규칙:
  TD1  모든 TEST / TEST_F / TEST_P 바로 위에 Doxygen 블록 주석(/** ... */)이 있다
  TD2  블록에 다음 태그가 모두 있고 내용이 비어 있지 않다
         @test <ID>        테스트 ID (예: UT-001, IT-003)
         @brief            테스트 목적
         @par 입력값
         @par 절차
         @par 예상 출력값
  TD3  테스트 본문에 단언(EXPECT_*, ASSERT_*)이 하나 이상 있다 (검증 없는 테스트 금지)
  TD4  (--spec 지정 시) 테스트의 @test ID 와 명세 문서(표 첫 열)의 ID 가 서로 일치한다

사용 예:
    python3 check_test_docs.py --dir tests/unit --prefix UT --spec docs/swe4/unit-test-spec.md

종료 코드: 0 = 통과, 1 = 위반, 2 = 입력 오류
"""

import argparse
import os
import re
import sys

TEST_RE = re.compile(r"^\s*(TEST|TEST_F|TEST_P)\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)")
ASSERT_RE = re.compile(r"\b(EXPECT|ASSERT)_[A-Z_]+\s*\(|\bFAIL\s*\(")
TAGS = ("@test", "@brief", "@par 입력값", "@par 절차", "@par 예상 출력값")
TAG_SPLIT_RE = re.compile(r"(@test|@brief|@par\s+입력값|@par\s+절차|@par\s+예상\s*출력값|@trace|@par)")


def find_doc_block(lines, index):
    """@brief index 줄(테스트 매크로) 바로 위의 Doxygen 블록 주석을 찾는다.

    @param lines 파일의 줄 목록
    @param index 테스트 매크로가 있는 줄 인덱스
    @return 블록 주석 텍스트 (없으면 None)
    """
    cursor = index - 1
    while cursor >= 0 and not lines[cursor].strip():
        cursor -= 1
    if cursor < 0 or not lines[cursor].rstrip().endswith("*/"):
        return None
    end = cursor
    while cursor >= 0 and "/**" not in lines[cursor]:
        cursor -= 1
    if cursor < 0:
        return None
    return "\n".join(lines[cursor:end + 1])


def tag_contents(block):
    """@brief Doxygen 블록에서 태그별 내용을 뽑는다.

    @param block 블록 주석 텍스트
    @return {정규화한 태그: 내용}
    """
    text = re.sub(r"^\s*/\*\*|\*/\s*$", "", block, flags=re.MULTILINE)
    text = re.sub(r"^\s*\*\s?", "", text, flags=re.MULTILINE)
    parts = TAG_SPLIT_RE.split(text)
    found = {}
    for i in range(1, len(parts) - 1, 2):
        tag = re.sub(r"\s+", " ", parts[i].strip()).replace("예상 출력값", "예상 출력값")
        found[tag] = parts[i + 1].strip()
    return found


def test_body(lines, index):
    """@brief 테스트 매크로 줄부터 중괄호가 닫힐 때까지의 본문을 돌려준다.

    @param lines 파일의 줄 목록
    @param index 테스트 매크로가 있는 줄 인덱스
    @return 본문 문자열
    """
    depth = 0
    started = False
    collected = []
    for line in lines[index:]:
        collected.append(line)
        depth += line.count("{") - line.count("}")
        if "{" in line:
            started = True
        if started and depth <= 0:
            break
    return "\n".join(collected)


def check_file(path, prefix):
    """@brief 테스트 파일 하나를 검사한다.

    @param path 파일 경로
    @param prefix 테스트 ID 접두어 (UT 또는 IT)
    @return (위반 목록, 찾은 테스트 ID 집합, 테스트 개수)
    """
    errors = []
    ids = set()
    count = 0
    with open(path, encoding="utf-8") as handle:
        lines = handle.read().splitlines()
    id_re = re.compile(rf"^{prefix}-\d{{3}}$")
    for index, line in enumerate(lines):
        match = TEST_RE.match(line)
        if not match:
            continue
        count += 1
        where = f"{path}:{index + 1}: {match.group(2)}.{match.group(3)}"
        block = find_doc_block(lines, index)
        if block is None:
            errors.append(f"TD1 {where}: Doxygen 블록 주석(/** ... */)이 없음")
        else:
            contents = tag_contents(block)
            for tag in TAGS:
                if not contents.get(tag):
                    errors.append(f"TD2 {where}: 태그 {tag} 가 없거나 내용이 비어 있음")
            test_id = contents.get("@test", "").split()[0] if contents.get("@test") else ""
            if test_id and not id_re.match(test_id):
                errors.append(f"TD2 {where}: @test ID 형식 오류: {test_id} ({prefix}-nnn 이어야 함)")
            elif test_id:
                ids.add(test_id)
        if not ASSERT_RE.search(test_body(lines, index)):
            errors.append(f"TD3 {where}: 단언(EXPECT_*/ASSERT_*)이 없음 — 검증 없는 테스트")
    return errors, ids, count


def spec_ids(spec_path, prefix):
    """@brief 명세 문서의 표 첫 열에서 테스트 ID 를 모은다.

    @param spec_path 명세 마크다운 경로
    @param prefix 테스트 ID 접두어
    @return ID 집합
    """
    ids = set()
    with open(spec_path, encoding="utf-8") as handle:
        for line in handle:
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if line.lstrip().startswith("|") and cells and re.fullmatch(rf"{prefix}-\d{{3}}", cells[0]):
                ids.add(cells[0])
    return ids


def main(argv=None):
    """@brief 명령행 진입점.

    @param argv 인자 목록 (None이면 sys.argv)
    @return 종료 코드
    """
    parser = argparse.ArgumentParser(description="GoogleTest 테스트 주석·단언 검사")
    parser.add_argument("--dir", required=True, help="테스트 코드 디렉터리 (예: tests/unit)")
    parser.add_argument("--prefix", required=True, choices=["UT", "IT"], help="테스트 ID 접두어")
    parser.add_argument("--spec", help="테스트 명세 마크다운 (지정하면 ID 일치를 검사)")
    args = parser.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    if not os.path.isdir(args.dir):
        print(f"오류: 디렉터리가 없습니다: {args.dir}", file=sys.stderr)
        return 2
    errors = []
    code_ids = set()
    total = 0
    for root, _, names in os.walk(args.dir):
        for name in sorted(names):
            if name.endswith((".cpp", ".cc", ".cxx")):
                file_errors, ids, count = check_file(os.path.join(root, name), args.prefix)
                errors += file_errors
                code_ids |= ids
                total += count
    if args.spec:
        if not os.path.isfile(args.spec):
            print(f"오류: 명세 파일이 없습니다: {args.spec}", file=sys.stderr)
            return 2
        documented = spec_ids(args.spec, args.prefix)
        for test_id in sorted(documented - code_ids):
            errors.append(f"TD4 {args.spec}: {test_id} 는 명세에 있으나 테스트 코드에 없음")
        for test_id in sorted(code_ids - documented):
            errors.append(f"TD4 {args.dir}: {test_id} 는 테스트 코드에 있으나 명세에 없음")
    print(f"테스트 주석 검사: 테스트 {total}개")
    for message in errors:
        print(f"  {message}")
    if total == 0:
        print("오류: 테스트 함수를 찾지 못했습니다", file=sys.stderr)
        return 2
    if errors:
        print(f"판정: 실패 — 위반 {len(errors)}건")
        return 1
    print("판정: 통과 (모든 테스트에 목적·입력값·절차·예상 출력값 주석과 단언이 있음)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
