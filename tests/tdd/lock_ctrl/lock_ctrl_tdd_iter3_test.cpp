// TDD 테스트(이터레이션 3): 상태 직렬화, 제어 결정 이력, 일반 해제 허용 조건을 공개 API로 이끈다.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "lock_ctrl/lock_ctrl_if.h"
#include "status_view/status_view_if.h"

namespace
{

raw_snapshot_t valid_snapshot(int64_t timestamp_ms)
{
    raw_snapshot_t snapshot = {};
    snapshot.timestamp_ms = {true, timestamp_ms};
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        snapshot.item[i] = {true, 0.0};
    }
    snapshot.item[ITEM_IGNITION_ON] = {true, 1.0};
    snapshot.item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(timestamp_ms)};
    return snapshot;
}

status_t evaluate(lock_ctrl_t *ctrl, uint32_t now_ms, raw_snapshot_t *snapshot,
                  const raw_command_t *command, lock_ctrl_result_t *result)
{
    snapshot->timestamp_ms = {true, now_ms};
    snapshot->item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(now_ms)};
    lock_ctrl_input_t input = {now_ms, snapshot, command};
    return lock_ctrl_evaluate(ctrl, &input, result);
}

lock_ctrl_t make_ctrl(lock_cmd_t initial)
{
    lock_ctrl_t ctrl = {};
    lock_ctrl_config_t config = {{initial, initial}};
    EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, &config));
    return ctrl;
}

} // namespace

/**
 * @test TDD-030
 * @brief 초기 결과를 직렬화하면 정해진 JSON 문자열이 된다 (SWR-015).
 * @par 입력값
 * 초기화 직후의 조회 결과, 충분한 버퍼
 * @par 절차
 * lock_ctrl_get_result 후 status_view_format 호출
 * @par 예상 출력값
 * STATUS_OK와 요구사항 5.6절 형식의 문자열(입력 상태는 모두 STALE)
 */
TEST(StatusViewTdd, InitialResultJson)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_RELEASE);
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    char buffer[1024] = {};
    ASSERT_EQ(STATUS_OK, status_view_format(&result, buffer, sizeof(buffer)));
    const char *names[] = {"vehicle_speed",
                           "gear",
                           "source_timestamp",
                           "crash_status",
                           "rear_left_approach_risk",
                           "rear_right_approach_risk",
                           "fire_detected",
                           "overtemperature_detected",
                           "adult_present",
                           "isofix_left",
                           "isofix_right",
                           "ignition_on",
                           "sensor_fault"};
    std::string expected =
        "{\"lock_left\":\"RELEASE\",\"lock_right\":\"RELEASE\",\"state\":\"NORMAL\","
        "\"priority_reason\":\"NONE\",\"reason_code\":\"NONE\",\"warning_code\":\"NONE\","
        "\"command_status\":\"NONE\",\"override_left\":false,\"override_right\":false,"
        "\"input_validity\":{";
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        expected += std::string(i == 0 ? "" : ",") + "\"" + names[i] + "\":\"STALE\"";
    }
    expected += "}}";
    EXPECT_EQ(expected, std::string(buffer));
}

/**
 * @test TDD-031
 * @brief 버퍼가 부족하면 STATUS_ERR_BUFFER와 빈 문자열이다 (SWR-015).
 * @par 입력값
 * 필요 길이보다 1 작은 버퍼, 필요 길이와 같은 버퍼
 * @par 절차
 * 필요 길이를 구해 각각 호출
 * @par 예상 출력값
 * 작은 버퍼: STATUS_ERR_BUFFER, 빈 문자열. 같은 크기(널 포함): STATUS_OK
 */
TEST(StatusViewTdd, SmallBuffer)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    char big[1024] = {};
    ASSERT_EQ(STATUS_OK, status_view_format(&result, big, sizeof(big)));
    const size_t needed = std::strlen(big) + 1;
    char exact[1024] = {};
    EXPECT_EQ(STATUS_OK, status_view_format(&result, exact, needed));
    char small[1024];
    std::memset(small, 'x', sizeof(small));
    EXPECT_EQ(STATUS_ERR_BUFFER, status_view_format(&result, small, needed - 1));
    EXPECT_EQ('\0', small[0]);
    EXPECT_EQ(STATUS_ERR_PARAM, status_view_format(&result, small, 0));
}

/**
 * @test TDD-032
 * @brief 제어 결정이 바뀔 때만 이력에 추가되고 이벤트 ID가 1부터 증가한다 (SWR-010).
 * @par 입력값
 * 정상 평가 3번(변화 없음), sensor_fault TRUE, 다시 FALSE
 * @par 절차
 * 평가하며 이력 건수와 항목 확인
 * @par 예상 출력값
 * 정상 평가는 이력 0건(결과가 초기 결과와 같지 않아 첫 평가만 1건: NORMAL 유지 시 0건),
 * sensor_fault 평가에서 이벤트 ID 순서대로 추가
 */
TEST(DecisionLogTdd, RecordsOnlyChanges)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = valid_snapshot(10);
    lock_ctrl_result_t result = {};
    uint32_t count = 99;
    for (uint32_t now = 10; now <= 30; now += 10)
    {
        ASSERT_EQ(STATUS_OK, evaluate(&ctrl, now, &snapshot, nullptr, &result));
    }
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(&ctrl, &count));
    EXPECT_EQ(0u, count);
    snapshot.item[ITEM_SENSOR_FAULT].value = 1.0;
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 40, &snapshot, nullptr, &result));
    snapshot.item[ITEM_SENSOR_FAULT].value = 0.0;
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 50, &snapshot, nullptr, &result));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(&ctrl, &count));
    ASSERT_EQ(2u, count);
    decision_entry_t first = {};
    decision_entry_t second = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(&ctrl, 0, &first));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(&ctrl, 1, &second));
    EXPECT_EQ(1u, first.event_id);
    EXPECT_EQ(40u, first.now_ms);
    EXPECT_EQ(STATE_FAULT, first.state);
    EXPECT_EQ(REASON_SENSOR_FAULT, first.reason);
    EXPECT_EQ(2u, second.event_id);
    EXPECT_EQ(STATE_NORMAL, second.state);
}

/**
 * @test TDD-033
 * @brief 101건을 만들면 최신 100건만 오래된 순서로 남고, 초기화하면 비워진다 (SWR-010, SWR-012).
 * @par 입력값
 * sensor_fault를 매 평가마다 바꿔 101번 결정 변화, 이후 초기화
 * @par 절차
 * 평가 후 이력 조회, lock_ctrl_init 후 다시 조회
 * @par 예상 출력값
 * 건수 100, 첫 항목 이벤트 ID 2, 마지막 101, 인덱스 100은 STATUS_ERR_PARAM. 초기화 후 건수 0
 */
TEST(DecisionLogTdd, KeepsLatestHundred)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = valid_snapshot(10);
    lock_ctrl_result_t result = {};
    for (uint32_t i = 0; i < 101; ++i)
    {
        snapshot.item[ITEM_SENSOR_FAULT].value = (i % 2 == 0) ? 1.0 : 0.0;
        ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10 * (i + 1), &snapshot, nullptr, &result));
    }
    uint32_t count = 0;
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(&ctrl, &count));
    EXPECT_EQ(100u, count);
    decision_entry_t entry = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(&ctrl, 0, &entry));
    EXPECT_EQ(2u, entry.event_id);
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(&ctrl, 99, &entry));
    EXPECT_EQ(101u, entry.event_id);
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_log_entry(&ctrl, 100, &entry));
    ASSERT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, nullptr));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(&ctrl, &count));
    EXPECT_EQ(0u, count);
}

/**
 * @test TDD-034
 * @brief 일반 해제는 접근위험·ISOFIX·자동 잠금·전역 규칙이 없을 때만 허용된다 (SWR-004).
 * @par 입력값
 * 초기 LOCK, (all, unlock) 명령 + (좌 접근위험 / 우 ISOFIX / 차속 10 km/h / sensor_fault / 조건
 * 없음)
 * @par 절차
 * 조건마다 새 제어기로 평가
 * @par 예상 출력값
 * 좌 접근위험: (LOCK, RELEASE). 우 ISOFIX: (RELEASE, LOCK). 주행, sensor_fault: (LOCK, LOCK). 조건
 * 없음: (RELEASE, RELEASE)
 */
TEST(GeneralReleaseTdd, AllowedOnlyWithoutBlockers)
{
    const raw_command_t unlock_all = {{true, 2.0}, {true, 1.0}, {true, 0.0}};
    struct Case
    {
        input_item_t item;
        double value;
        lock_cmd_t left;
        lock_cmd_t right;
    };
    const Case cases[] = {{ITEM_REAR_LEFT_APPROACH_RISK, 1.0, LOCK_LOCK, LOCK_RELEASE},
                          {ITEM_ISOFIX_RIGHT, 1.0, LOCK_RELEASE, LOCK_LOCK},
                          {ITEM_VEHICLE_SPEED, 10.0, LOCK_LOCK, LOCK_LOCK},
                          {ITEM_SENSOR_FAULT, 1.0, LOCK_LOCK, LOCK_LOCK},
                          {ITEM_GEAR, 0.0, LOCK_RELEASE, LOCK_RELEASE}};
    for (const Case &c : cases)
    {
        lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
        raw_snapshot_t snapshot = valid_snapshot(10);
        snapshot.item[c.item].value = c.value;
        lock_ctrl_result_t result = {};
        ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, &unlock_all, &result));
        EXPECT_EQ(c.left, result.outputs.left);
        EXPECT_EQ(c.right, result.outputs.right);
    }
}
