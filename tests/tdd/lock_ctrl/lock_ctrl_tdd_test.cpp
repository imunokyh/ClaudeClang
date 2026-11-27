// TDD 테스트: lock_ctrl 공개 API(IF-003)를 통해 이터레이션 1 요구사항의 핵심 동작을 이끈다.
// 단위 테스트(tests/unit)와 분리되어 있으며 상세 설계의 결정표 전체를 검증하지 않는다.
#include <gtest/gtest.h>

#include "lock_ctrl/lock_ctrl_if.h"

namespace
{

/** 모든 항목이 유효한 스냅샷을 만든다(차속 0, 기어 P, 충돌 NONE, 나머지 FALSE). */
raw_snapshot_t make_valid_snapshot(int64_t timestamp_ms)
{
    raw_snapshot_t snapshot = {};
    snapshot.timestamp_ms = {true, timestamp_ms};
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        snapshot.item[i] = {true, 0.0};
    }
    snapshot.item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(timestamp_ms)};
    return snapshot;
}

/** 초기 출력을 지정해 제어기를 만든다. */
lock_ctrl_t make_ctrl(lock_cmd_t initial)
{
    lock_ctrl_t ctrl = {};
    lock_ctrl_config_t config = {{initial, initial}};
    EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, &config));
    return ctrl;
}

/** 평가를 한 번 한다. */
status_t evaluate(lock_ctrl_t *ctrl, uint32_t now_ms, const raw_snapshot_t *snapshot,
                  lock_ctrl_result_t *result)
{
    lock_ctrl_input_t input = {now_ms, snapshot, nullptr};
    return lock_ctrl_evaluate(ctrl, &input, result);
}

} // namespace

/**
 * @test TDD-001
 * @brief 초기화 직후 출력은 RELEASE이고 모든 입력은 STALE이다 (SWR-025).
 * @par 입력값
 * 설정 없이 초기화
 * @par 절차
 * lock_ctrl_init(NULL) 후 lock_ctrl_get_result 호출
 * @par 예상 출력값
 * RELEASE, RELEASE, NORMAL, 모든 input_validity STALE
 */
TEST(LockCtrlTdd, InitialResult)
{
    lock_ctrl_t ctrl = {};
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, nullptr));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &result));
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(STATE_NORMAL, result.state);
    EXPECT_EQ(VALIDITY_STALE, result.input_validity[ITEM_CRASH_STATUS]);
}

/**
 * @test TDD-002
 * @brief 유효한 충돌 CONFIRMED는 잠금 출력을 해제한다 (SWR-007).
 * @par 입력값
 * 초기 출력 LOCK, 충돌 CONFIRMED 스냅샷
 * @par 절차
 * 평가 한 번
 * @par 예상 출력값
 * 두 출력 RELEASE, reason CRASH_CONFIRMED
 */
TEST(LockCtrlTdd, CrashConfirmedReleases)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = make_valid_snapshot(100);
    snapshot.item[ITEM_CRASH_STATUS].value = CRASH_CONFIRMED;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 100, &snapshot, &result));
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(REASON_CRASH_CONFIRMED, result.reason);
}

/**
 * @test TDD-003
 * @brief sensor_fault는 직전 출력을 유지하고 FAULT를 보고한다 (SWR-021).
 * @par 입력값
 * 초기 출력 LOCK, sensor_fault TRUE
 * @par 절차
 * 평가 한 번
 * @par 예상 출력값
 * 출력 LOCK 유지, state FAULT, warning SENSOR_FAULT
 */
TEST(LockCtrlTdd, SensorFaultHolds)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = make_valid_snapshot(100);
    snapshot.item[ITEM_SENSOR_FAULT].value = 1.0;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 100, &snapshot, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(STATE_FAULT, result.state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, result.warning);
}

/**
 * @test TDD-004
 * @brief 200 ms를 넘게 스냅샷이 없으면 STALE로 DEGRADED가 된다 (SWR-013).
 * @par 입력값
 * 100 ms에 스냅샷, 이후 스냅샷 없음
 * @par 절차
 * 300 ms(경과 200)와 310 ms(경과 210)에 평가
 * @par 예상 출력값
 * 300 ms는 NORMAL, 310 ms는 DEGRADED와 INPUT_STALE
 */
TEST(LockCtrlTdd, StaleAfter200ms)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = make_valid_snapshot(100);
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 100, &snapshot, &result));
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 300, nullptr, &result));
    EXPECT_EQ(STATE_NORMAL, result.state);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 310, nullptr, &result));
    EXPECT_EQ(STATE_DEGRADED, result.state);
    EXPECT_EQ(REASON_INPUT_STALE, result.reason);
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
}

/**
 * @test TDD-005
 * @brief 범위 밖 입력은 INVALID가 되고 새 유효 값이 올 때까지 유지된다 (SWR-022, SWR-034).
 * @par 입력값
 * 차속 300.1 km/h 스냅샷, 이후 스냅샷 없는 평가, 이후 정상 스냅샷
 * @par 절차
 * 세 번 평가
 * @par 예상 출력값
 * 첫째와 둘째는 차속 INVALID와 INPUT_INVALID, 셋째는 NORMAL
 */
TEST(LockCtrlTdd, InvalidInputPersistsUntilValidUpdate)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t bad = make_valid_snapshot(100);
    bad.item[ITEM_VEHICLE_SPEED].value = 300.1;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 100, &bad, &result));
    EXPECT_EQ(VALIDITY_INVALID, result.input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(REASON_INPUT_INVALID, result.reason);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 110, nullptr, &result));
    EXPECT_EQ(VALIDITY_INVALID, result.input_validity[ITEM_VEHICLE_SPEED]);
    raw_snapshot_t good = make_valid_snapshot(120);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 120, &good, &result));
    EXPECT_EQ(STATE_NORMAL, result.state);
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
}

/**
 * @test TDD-006
 * @brief 평가 시각이 거꾸로 가면 거절하고 상태를 바꾸지 않는다 (SWR-040).
 * @par 입력값
 * 100 ms 평가 후 50 ms 평가
 * @par 절차
 * 두 번 평가하고 결과 조회
 * @par 예상 출력값
 * 두 번째는 STATUS_ERR_TIME, 조회 결과는 첫 평가와 같음
 */
TEST(LockCtrlTdd, TimeRegressionRejected)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = make_valid_snapshot(100);
    lock_ctrl_result_t first = {};
    lock_ctrl_result_t after = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 100, &snapshot, &first));
    EXPECT_EQ(STATUS_ERR_TIME, evaluate(&ctrl, 50, &snapshot, &after));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl, &after));
    EXPECT_EQ(first.state, after.state);
    EXPECT_EQ(first.reason, after.reason);
}

/**
 * @test TDD-007
 * @brief 운전자 명령은 필드가 유효하지 않으면 거절된다 (SWR-033).
 * @par 입력값
 * side 5(범위 밖) 명령, 유효한 명령
 * @par 절차
 * 명령을 담아 평가
 * @par 예상 출력값
 * 첫째 COMMAND_REJECTED, 둘째 COMMAND_ACCEPTED
 */
TEST(LockCtrlTdd, CommandValidation)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_RELEASE);
    raw_snapshot_t snapshot = make_valid_snapshot(100);
    raw_command_t bad = {{true, 5.0}, {true, 0.0}, {true, 0.0}};
    raw_command_t good = {{true, 1.0}, {true, 1.0}, {true, 3.0}};
    lock_ctrl_result_t result = {};
    lock_ctrl_input_t input = {100, &snapshot, &bad};
    ASSERT_EQ(STATUS_OK, lock_ctrl_evaluate(&ctrl, &input, &result));
    EXPECT_EQ(COMMAND_REJECTED, result.command_status);
    input = {110, nullptr, &good};
    ASSERT_EQ(STATUS_OK, lock_ctrl_evaluate(&ctrl, &input, &result));
    EXPECT_EQ(COMMAND_ACCEPTED, result.command_status);
}
