#!/usr/bin/env python3
"""@brief trace_check.py 의 ID 인식 정규식 회귀 시험."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import trace_check  # noqa: E402


def ids(text):
    """@brief 문자열에서 인식된 ID 목록을 돌려준다."""
    return [m.group(0) for m in trace_check.ID_RE.finditer(text)]


class IdRegexTest(unittest.TestCase):
    """@brief ID 정규식 시험 모음."""

    def test_swr_is_recognized(self):
        """@brief SWR-nnn 을 인식한다."""
        self.assertEqual(ids("| SWR-007 |"), ["SWR-007"])

    def test_upstream_prefixed_id_is_ignored(self):
        """@brief OEM-IF-002 같은 상위 문서 ID 는 IF-002 로 오인하지 않는다."""
        self.assertEqual(ids("OEM-IF-002, OEM-SR-001"), [])

    def test_korean_particle_after_id(self):
        """@brief ID 바로 뒤에 한글 조사가 붙어도 인식한다."""
        self.assertEqual(ids("SWR-099를 참조"), ["SWR-099"])

    def test_four_digit_number_is_not_id(self):
        """@brief 네 자리 번호는 ID 가 아니다."""
        self.assertEqual(ids("SWR-0071"), [])

    def test_range_notation_is_detected(self):
        """@brief 범위 표기는 T13 오류로 검출한다."""
        self.assertIsNotNone(trace_check.RANGE_RE.search("SWR-021~025"))
        self.assertIsNone(trace_check.RANGE_RE.search("OEM-FR-001~006"))
        self.assertIsNone(trace_check.RANGE_RE.search("SWR-021, SWR-025"))


if __name__ == "__main__":
    unittest.main()
