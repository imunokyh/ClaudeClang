#!/usr/bin/env python3
"""@brief check_banned.py 시험."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_banned  # noqa: E402


def run(source):
    """@brief 소스 하나를 검사해 위반 메시지 목록을 돌려준다."""
    return check_banned.analyze({"a.c": source})


class BannedTest(unittest.TestCase):
    """@brief 금지 호출 검사 시험 모음."""

    def test_clean_code_passes(self):
        """@brief 위반 없는 코드는 통과한다."""
        self.assertEqual(run("int f(int x)\n{\n    return x + 1;\n}\n"), [])

    def test_malloc_detected(self):
        """@brief malloc 호출을 검출한다."""
        errors = run("int f(void)\n{\n    char *p = malloc(4);\n    return p == 0;\n}\n")
        self.assertTrue(any(e.startswith("B1") for e in errors))

    def test_clock_detected(self):
        """@brief time 호출을 검출한다."""
        errors = run("int f(void)\n{\n    return (int)time(0);\n}\n")
        self.assertTrue(any(e.startswith("B3") for e in errors))

    def test_file_io_detected(self):
        """@brief 파일 입출력 호출을 B4로 검출한다."""
        errors = run("int f(void)
{
    return fopen(0, 0) == 0;
}
")
        self.assertTrue(any(e.startswith("B4") for e in errors))

    def test_direct_recursion_detected(self):
        """@brief 직접 재귀를 검출한다."""
        errors = run("int f(int n)\n{\n    return f(n - 1);\n}\n")
        self.assertTrue(any(e.startswith("B2") for e in errors))

    def test_mutual_recursion_detected(self):
        """@brief 상호 재귀를 검출한다."""
        src = "int g(int n);\nint f(int n)\n{\n    return g(n);\n}\nint g(int n)\n{\n    return f(n);\n}\n"
        errors = run(src)
        self.assertTrue(any(e.startswith("B2") for e in errors))

    def test_comment_and_string_ignored(self):
        """@brief 주석과 문자열 안의 이름은 무시한다."""
        src = 'int f(void)\n{\n    /* malloc(1) */\n    const char *s = "free(x)";\n    return s != 0;\n}\n'
        self.assertEqual(run(src), [])

    def test_call_to_other_function_is_not_recursion(self):
        """@brief 서로 다른 함수 호출은 재귀가 아니다."""
        src = "int g(int n)\n{\n    return n;\n}\nint f(int n)\n{\n    return g(n);\n}\n"
        self.assertEqual(run(src), [])


if __name__ == "__main__":
    unittest.main()
