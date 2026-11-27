// TDD 테스트(이터레이션 2): 문 단위 규칙과 전역 해제 규칙을 lock_ctrl 공개 API(IF-003)로 이끈다.
// 단위 테스트(tests/unit)와 분리되어 있으며 상세 설계의 결정표 전체를 검증하지 않는다.
#include <gtest/gtest.h>

#include "lock_ctrl/lock_ctrl_if.h"

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
    snapshot.item[ITEM_IGNITION_ON] = {true, 1.0}; // 시동 켜짐
    snapshot.item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(timestamp_ms)};
    return snapshot;
}

lock_ctrl_t make_ctrl(lock_cmd_t initial)
{
    lock_ctrl_t ctrl = {};
    lock_ctrl_config_t config = {{initial, initial}};
    EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl, &config));
    return ctrl;
}

status_t evaluate(lock_ctrl_t *ctrl, uint32_t now_ms, raw_snapshot_t *snapshot,
                  const raw_command_t *command, lock_ctrl_result_t *result)
{
    snapshot->timestamp_ms = {true, now_ms};
    snapshot->item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(now_ms)};
    lock_ctrl_input_t input = {now_ms, snapshot, command};
    return lock_ctrl_evaluate(ctrl, &input, result);
}

constexpr raw_command_t kUnlockLeft = {{true, 0.0}, {true, 1.0}, {true, 0.0}};
constexpr raw_command_t kUnlockAll = {{true, 2.0}, {true, 1.0}, {true, 0.0}};
constexpr raw_command_t kLockLeft = {{true, 0.0}, {true, 0.0}, {true, 1.0}};

} // namespace

/**
 * @test TDD-020
 * @brief ignition off는 두 출력을 해제하고 OFF 상태로 한다 (SWR-020).
 * @par 입력값
 * 초기 출력 LOCK, ignition_on = 0
 * @par 절차
 * 평가 한 번
 * @par 예상 출력값
 * RELEASE, RELEASE, state OFF, reason IGNITION_OFF
 */
TEST(LockCtrlTddIter2, IgnitionOffReleasesAndOff)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = valid_snapshot(10);
    snapshot.item[ITEM_IGNITION_ON].value = 0.0;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, nullptr, &result));
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(STATE_OFF, result.state);
    EXPECT_EQ(REASON_IGNITION_OFF, result.reason);
}

/**
 * @test TDD-021
 * @brief 화재, 과온, 성인 탑승은 각각 단독으로 두 출력을 해제하고 이유를 남긴다 (SWR-017).
 * @par 입력값
 * 초기 출력 LOCK, 각 입력을 단독으로 TRUE
 * @par 절차
 * 입력마다 새 제어기로 평가
 * @par 예상 출력값
 * 두 출력 RELEASE, 이유 FIRE, OVERTEMPERATURE, ADULT_PRESENT
 */
TEST(LockCtrlTddIter2, ForcedReleaseInputs)
{
    const input_item_t items[] = {ITEM_FIRE_DETECTED, ITEM_OVERTEMPERATURE_DETECTED,
                                  ITEM_ADULT_PRESENT};
    const reason_t reasons[] = {REASON_FIRE, REASON_OVERTEMPERATURE, REASON_ADULT_PRESENT};
    for (int i = 0; i < 3; ++i)
    {
        lock_ctrl_t ctrl = make_ctrl(LOCK_LOCK);
        raw_snapshot_t snapshot = valid_snapshot(10);
        snapshot.item[items[i]].value = 1.0;
        lock_ctrl_result_t result = {};
        ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, nullptr, &result));
        EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
        EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
        EXPECT_EQ(reasons[i], result.reason);
    }
}

/**
 * @test TDD-022
 * @brief 접근위험은 그 문만 잠그고 그 문의 해제 요청을 억제한다 (SWR-005, SWR-009).
 * @par 입력값
 * 초기 출력 RELEASE, 좌 접근위험 TRUE, 이후 좌 unlock 명령
 * @par 절차
 * 접근위험 평가, 명령 평가
 * @par 예상 출력값
 * lock_left LOCK(APPROACH_RISK), lock_right RELEASE. 명령 뒤에도 lock_left LOCK 유지
 */
TEST(LockCtrlTddIter2, ApproachRiskLocksAndSuppresses)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_RELEASE);
    raw_snapshot_t snapshot = valid_snapshot(10);
    snapshot.item[ITEM_REAR_LEFT_APPROACH_RISK].value = 1.0;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(REASON_APPROACH_RISK, result.reason);
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, &snapshot, &kUnlockLeft, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_FALSE(result.override_active[DOOR_LEFT]);
}

/**
 * @test TDD-023
 * @brief 10초 이내 같은 해제 명령을 다시 입력하면 override가 성립하고 문이 해제된다 (SWR-006,
 * SWR-048).
 * @par 입력값
 * 좌 접근위험 TRUE에서 좌 unlock 명령을 10 ms, 10,010 ms(경과 10,000 ms)에 입력
 * @par 절차
 * 두 번 평가한 뒤 접근위험이 풀린 평가
 * @par 예상 출력값
 * 두 번째에 override_left TRUE, lock_left RELEASE, reason APPROACH_OVERRIDE. 접근위험이 풀리면
 * override FALSE
 */
TEST(LockCtrlTddIter2, OverrideWithinTenSeconds)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_RELEASE);
    raw_snapshot_t snapshot = valid_snapshot(10);
    snapshot.item[ITEM_REAR_LEFT_APPROACH_RISK].value = 1.0;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, &kUnlockLeft, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    for (uint32_t now = 20; now < 10010; now += 10)
    {
        ASSERT_EQ(STATUS_OK, evaluate(&ctrl, now, &snapshot, nullptr, &result));
    }
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10010, &snapshot, &kUnlockLeft, &result));
    EXPECT_TRUE(result.override_active[DOOR_LEFT]);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    EXPECT_EQ(REASON_APPROACH_OVERRIDE, result.reason);
    snapshot.item[ITEM_REAR_LEFT_APPROACH_RISK].value = 0.0;
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10020, &snapshot, nullptr, &result));
    EXPECT_FALSE(result.override_active[DOOR_LEFT]);
}

/**
 * @test TDD-024
 * @brief ISOFIX는 해당 문만 잠근다 (SWR-018).
 * @par 입력값
 * 초기 RELEASE, isofix_left TRUE
 * @par 절차
 * 평가 한 번
 * @par 예상 출력값
 * lock_left LOCK(ISOFIX), lock_right RELEASE
 */
TEST(LockCtrlTddIter2, IsofixLocksOneDoor)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_RELEASE);
    raw_snapshot_t snapshot = valid_snapshot(10);
    snapshot.item[ITEM_ISOFIX_LEFT].value = 1.0;
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(REASON_ISOFIX, result.reason);
}

/**
 * @test TDD-025
 * @brief 차속 3.0 km/h 이상이면 두 출력이 잠기고, 2.9 km/h는 잠기지 않는다 (SWR-003).
 * @par 입력값
 * 초기 RELEASE, 차속 2.9 → 3.0 km/h
 * @par 절차
 * 두 번 평가
 * @par 예상 출력값
 * 2.9: RELEASE 유지. 3.0: 두 출력 LOCK, reason AUTO_LOCK
 */
TEST(LockCtrlTddIter2, AutoLockAtThreeKph)
{
    lock_ctrl_t ctrl = make_ctrl(LOCK_RELEASE);
    raw_snapshot_t snapshot = valid_snapshot(10);
    lock_ctrl_result_t result = {};
    snapshot.item[ITEM_VEHICLE_SPEED].value = 2.9;
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 10, &snapshot, nullptr, &result));
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    snapshot.item[ITEM_VEHICLE_SPEED].value = 3.0;
    ASSERT_EQ(STATUS_OK, evaluate(&ctrl, 20, &snapshot, nullptr, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(LOCK_LOCK, result.outputs.right);
    EXPECT_EQ(REASON_AUTO_LOCK, result.reason);
}

/**
 * @test TDD-026
 * @brief 운전자 명령은 선택한 문에만 적용되고 자동 잠금 중의 해제 명령은 적용되지 않는다 (SWR-001,
 * SWR-002).
 * @par 입력값
 * 초기 LOCK, (all, unlock) 명령 / 같은 명령에 차속 10 km/h / (left, lock) 명령
 * @par 절차
 * 각각 평가
 * @par 예상 출력값
 * 정지: 두 출력 RELEASE(DRIVER_COMMAND). 주행: LOCK 유지(AUTO_LOCK). 좌 lock: 좌만 LOCK
 */
TEST(LockCtrlTddIter2, DriverCommands)
{
    lock_ctrl_result_t result = {};
    lock_ctrl_t still = make_ctrl(LOCK_LOCK);
    raw_snapshot_t snapshot = valid_snapshot(10);
    ASSERT_EQ(STATUS_OK, evaluate(&still, 10, &snapshot, &kUnlockAll, &result));
    EXPECT_EQ(LOCK_RELEASE, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
    EXPECT_EQ(REASON_DRIVER_COMMAND, result.reason);

    lock_ctrl_t moving = make_ctrl(LOCK_LOCK);
    snapshot.item[ITEM_VEHICLE_SPEED].value = 10.0;
    ASSERT_EQ(STATUS_OK, evaluate(&moving, 10, &snapshot, &kUnlockAll, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(REASON_AUTO_LOCK, result.reason);

    lock_ctrl_t one = make_ctrl(LOCK_RELEASE);
    snapshot.item[ITEM_VEHICLE_SPEED].value = 0.0;
    ASSERT_EQ(STATUS_OK, evaluate(&one, 10, &snapshot, &kLockLeft, &result));
    EXPECT_EQ(LOCK_LOCK, result.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, result.outputs.right);
}
