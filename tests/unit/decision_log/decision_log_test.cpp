// decision_log 단위 테스트 (SWE.4). 근거: 상세 설계 FN-042~FN-045의 결정표.
#include <gtest/gtest.h>

#include "decision_log/decision_log_if.h"

namespace
{

decision_entry_t make_entry(uint32_t now_ms)
{
    return {0,
            now_ms,
            {LOCK_LOCK, LOCK_RELEASE},
            STATE_FAULT,
            REASON_SENSOR_FAULT,
            WARNING_SENSOR_FAULT};
}

decision_log_t fresh_log()
{
    decision_log_t log = {};
    EXPECT_EQ(STATUS_OK, decision_log_init(&log));
    return log;
}

} // namespace

/**
 * @test UT-090
 * @brief decision_log_init은 NULL을 거절하고, 초기화하면 이력을 비우고 ID를 1로 되돌린다 (FN-042).
 * @par 입력값
 * log = NULL / 건수와 ID가 채워진 저장소
 * @par 절차
 * 각각 decision_log_init 호출 후 건수와 다음 추가 항목의 ID 확인
 * @par 예상 출력값
 * NULL: STATUS_ERR_PARAM. 초기화: 건수 0, 다음 추가 항목의 event_id 1
 */
TEST(DecisionLogInit, NullAndReset)
{
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_init(nullptr));
    decision_log_t log = fresh_log();
    const decision_entry_t entry = make_entry(10);
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(STATUS_OK, decision_log_append(&log, &entry));
    }
    ASSERT_EQ(STATUS_OK, decision_log_init(&log));
    uint32_t count = 99;
    ASSERT_EQ(STATUS_OK, decision_log_count(&log, &count));
    EXPECT_EQ(0u, count);
    ASSERT_EQ(STATUS_OK, decision_log_append(&log, &entry));
    decision_entry_t out = {};
    ASSERT_EQ(STATUS_OK, decision_log_get(&log, 0, &out));
    EXPECT_EQ(1u, out.event_id);
}

/**
 * @test UT-091
 * @brief decision_log_append는 NULL 인자를 거절하고 저장소를 바꾸지 않는다 (FN-043 R1, R2).
 * @par 입력값
 * (log NULL), (entry NULL)
 * @par 절차
 * 각각 호출 후 건수 확인
 * @par 예상 출력값
 * STATUS_ERR_PARAM, 건수 0
 */
TEST(DecisionLogAppend, NullArgumentsAreRejected)
{
    decision_log_t log = fresh_log();
    const decision_entry_t entry = make_entry(10);
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_append(nullptr, &entry));
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_append(&log, nullptr));
    uint32_t count = 99;
    ASSERT_EQ(STATUS_OK, decision_log_count(&log, &count));
    EXPECT_EQ(0u, count);
}

/**
 * @test UT-092
 * @brief 추가한 항목은 이벤트 ID가 1부터 부여되고(항목의 event_id는 무시) 오래된 순서로 조회된다
 *        (FN-043 R3, FN-045 R1).
 * @par 입력값
 * event_id 777인 항목 3개(now_ms 10, 20, 30)
 * @par 절차
 * 추가 후 건수와 항목 조회
 * @par 예상 출력값
 * 건수 3, 항목 0~2의 event_id 1, 2, 3, now_ms 10, 20, 30, 나머지 필드는 입력과 같다
 */
TEST(DecisionLogAppend, AssignsIdsInOrder)
{
    decision_log_t log = fresh_log();
    for (uint32_t i = 1; i <= 3; ++i)
    {
        decision_entry_t entry = make_entry(10 * i);
        entry.event_id = 777;
        ASSERT_EQ(STATUS_OK, decision_log_append(&log, &entry));
    }
    uint32_t count = 0;
    ASSERT_EQ(STATUS_OK, decision_log_count(&log, &count));
    EXPECT_EQ(3u, count);
    for (uint32_t i = 0; i < 3; ++i)
    {
        decision_entry_t out = {};
        ASSERT_EQ(STATUS_OK, decision_log_get(&log, i, &out));
        EXPECT_EQ(i + 1, out.event_id);
        EXPECT_EQ(10 * (i + 1), out.now_ms);
        EXPECT_EQ(LOCK_LOCK, out.outputs.left);
        EXPECT_EQ(LOCK_RELEASE, out.outputs.right);
        EXPECT_EQ(STATE_FAULT, out.state);
        EXPECT_EQ(REASON_SENSOR_FAULT, out.reason);
        EXPECT_EQ(WARNING_SENSOR_FAULT, out.warning);
    }
}

/**
 * @test UT-093
 * @brief 100건을 넘으면 가장 오래된 항목을 덮어쓰고 최신 100건만 오래된 순서로 남는다
 *        (FN-043 R4, 경계값 100/101, SWR-010).
 * @par 입력값
 * 100건, 101건, 250건 추가
 * @par 절차
 * 건수와 첫·마지막 항목의 이벤트 ID, 범위 밖 index 조회
 * @par 예상 출력값
 * 100건: 건수 100, ID 1~100. 101건: 건수 100, ID 2~101. 250건: 건수 100, ID 151~250.
 * index 100은 STATUS_ERR_PARAM이고 출력 불변
 */
TEST(DecisionLogAppend, KeepsLatestHundred)
{
    const uint32_t totals[] = {100, 101, 250};
    for (uint32_t total : totals)
    {
        decision_log_t log = fresh_log();
        for (uint32_t i = 1; i <= total; ++i)
        {
            const decision_entry_t entry = make_entry(i);
            ASSERT_EQ(STATUS_OK, decision_log_append(&log, &entry));
        }
        uint32_t count = 0;
        ASSERT_EQ(STATUS_OK, decision_log_count(&log, &count));
        EXPECT_EQ(100u, count);
        decision_entry_t first = {};
        decision_entry_t last = {};
        ASSERT_EQ(STATUS_OK, decision_log_get(&log, 0, &first));
        ASSERT_EQ(STATUS_OK, decision_log_get(&log, 99, &last));
        EXPECT_EQ(total - 99, first.event_id);
        EXPECT_EQ(total, last.event_id);
        decision_entry_t untouched = make_entry(5);
        EXPECT_EQ(STATUS_ERR_PARAM, decision_log_get(&log, 100, &untouched));
        EXPECT_EQ(5u, untouched.now_ms);
    }
}

/**
 * @test UT-094
 * @brief decision_log_count와 decision_log_get은 NULL 인자를 거절한다 (FN-044 R2, R3, FN-045 R2,
 * R3).
 * @par 입력값
 * (log NULL), (count NULL), (out NULL)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM
 */
TEST(DecisionLogQuery, NullArgumentsAreRejected)
{
    decision_log_t log = fresh_log();
    const decision_entry_t entry = make_entry(10);
    ASSERT_EQ(STATUS_OK, decision_log_append(&log, &entry));
    uint32_t count = 0;
    decision_entry_t out = {};
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_count(nullptr, &count));
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_count(&log, nullptr));
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_get(nullptr, 0, &out));
    EXPECT_EQ(STATUS_ERR_PARAM, decision_log_get(&log, 0, nullptr));
}
