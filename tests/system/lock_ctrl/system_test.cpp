// SW 시스템 테스트 (SWE.6, PC/SIL). 시험 명세 docs/swe6/test-cases.json의 기능·비기능 TC를
// 수행한다. 시험 이름은 TC ID를 따른다(Ftc001 = FTC-001). 공개 API(lock_ctrl_if.h)만 사용하는
// 블랙박스 시험이다. NTC-004~NTC-007은 정적 검사(tools/run.sh build, rules)로 수행하며 CI
// 게이트에서 확인한다.
#include <gtest/gtest.h>

#include <chrono>
#include <limits>

#include "lock_ctrl/lock_ctrl_if.h"

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/** 차량 신호 어댑터 역할의 시험 하네스. */
class Rig
{
  public:
    explicit Rig(lock_cmd_t initial = LOCK_RELEASE, bool configure = true)
    {
        lock_ctrl_config_t config = {{initial, initial}};
        EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl_, configure ? &config : nullptr));
        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            snapshot_.item[i] = {true, 0.0};
        }
        snapshot_.item[ITEM_IGNITION_ON] = {true, 1.0}; // 시동 켜짐
    }

    void set(input_item_t item, double value)
    {
        snapshot_.item[item] = {true, value};
    }

    void drop(input_item_t item)
    {
        snapshot_.item[item] = {false, 0.0};
    }

    /** 스냅샷 시각을 now_ms와 같게 하여 평가한다. */
    status_t step(uint32_t now_ms, bool snapshot = true, const raw_command_t *command = nullptr)
    {
        snapshot_.timestamp_ms = {true, now_ms};
        snapshot_.item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(now_ms)};
        return eval(now_ms, snapshot ? &snapshot_ : nullptr, command);
    }

    /** 스냅샷을 직접 지정해 평가한다. */
    status_t eval(uint32_t now_ms, const raw_snapshot_t *snapshot, const raw_command_t *command)
    {
        lock_ctrl_input_t input = {now_ms, snapshot, command};
        return lock_ctrl_evaluate(&ctrl_, &input, &result_);
    }

    raw_snapshot_t &snapshot()
    {
        return snapshot_;
    }

    const lock_ctrl_result_t &r() const
    {
        return result_;
    }

    lock_ctrl_result_t current() const
    {
        lock_ctrl_result_t out = {};
        EXPECT_EQ(STATUS_OK, lock_ctrl_get_result(&ctrl_, &out));
        return out;
    }

  private:
    lock_ctrl_t ctrl_ = {};
    raw_snapshot_t snapshot_ = {};
    lock_ctrl_result_t result_ = {};
};

bool all_validity(const lock_ctrl_result_t &r, validity_t expected)
{
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        if (r.input_validity[i] != expected)
        {
            return false;
        }
    }
    return true;
}

} // namespace

// FTC-001
TEST(SystemFunctional, Ftc001CrashConfirmedReleases)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(0));
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
    rig.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.right);
    EXPECT_EQ(REASON_CRASH_CONFIRMED, rig.r().reason);
}

// FTC-002
TEST(SystemFunctional, Ftc002CrashConfirmedHolds)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    for (uint32_t i = 0; i < 100; ++i)
    {
        ASSERT_EQ(STATUS_OK, rig.step(10 + 10 * i));
        EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
        EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.right);
        EXPECT_EQ(REASON_CRASH_CONFIRMED, rig.r().reason);
    }
}

// FTC-003
TEST(SystemFunctional, Ftc003CrashPending)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_CRASH_STATUS, CRASH_PENDING);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.right);
    EXPECT_EQ(REASON_CRASH_PENDING, rig.r().reason);
    EXPECT_EQ(STATE_NORMAL, rig.r().state);
}

// FTC-004
TEST(SystemFunctional, Ftc004SensorFault)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_SENSOR_FAULT, 1.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.right);
    EXPECT_EQ(STATE_FAULT, rig.r().state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, rig.r().warning);
    EXPECT_EQ(REASON_SENSOR_FAULT, rig.r().reason);
}

// FTC-005
TEST(SystemFunctional, Ftc005CrashAndSensorFault)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    rig.set(ITEM_SENSOR_FAULT, 1.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.right);
    EXPECT_EQ(REASON_CRASH_CONFIRMED, rig.r().reason);
    EXPECT_EQ(STATE_FAULT, rig.r().state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, rig.r().warning);
}

// FTC-006
TEST(SystemFunctional, Ftc006StaleBoundary)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(100));
    ASSERT_EQ(STATUS_OK, rig.step(300, false));
    EXPECT_TRUE(all_validity(rig.r(), VALIDITY_VALID));
    EXPECT_EQ(STATE_NORMAL, rig.r().state);
    ASSERT_EQ(STATUS_OK, rig.step(301, false));
    EXPECT_TRUE(all_validity(rig.r(), VALIDITY_STALE));
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
}

// FTC-007
TEST(SystemFunctional, Ftc007StaleHolds)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(100));
    ASSERT_EQ(STATUS_OK, rig.step(310, false));
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.right);
    EXPECT_EQ(REASON_INPUT_STALE, rig.r().reason);
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
}

// FTC-008
TEST(SystemFunctional, Ftc008CrashStaleNotReapplied)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    ASSERT_EQ(LOCK_RELEASE, rig.r().outputs.left);
    ASSERT_EQ(STATUS_OK, rig.step(200, false));
    EXPECT_EQ(REASON_CRASH_CONFIRMED, rig.r().reason);
    ASSERT_EQ(STATUS_OK, rig.step(211, false));
    EXPECT_EQ(REASON_INPUT_STALE, rig.r().reason);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
}

// FTC-009
TEST(SystemFunctional, Ftc009SpeedBoundary)
{
    for (double speed : {0.0, 300.0})
    {
        Rig rig;
        rig.set(ITEM_VEHICLE_SPEED, speed);
        ASSERT_EQ(STATUS_OK, rig.step(10));
        EXPECT_EQ(VALIDITY_VALID, rig.r().input_validity[ITEM_VEHICLE_SPEED]);
    }
    Rig over;
    over.set(ITEM_VEHICLE_SPEED, 300.1);
    ASSERT_EQ(STATUS_OK, over.step(10));
    EXPECT_EQ(VALIDITY_INVALID, over.r().input_validity[ITEM_VEHICLE_SPEED]);
}

// FTC-010
TEST(SystemFunctional, Ftc010GearInvalid)
{
    Rig rig;
    rig.set(ITEM_GEAR, 4.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_GEAR]);
    EXPECT_EQ(VALIDITY_VALID, rig.r().input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
}

// FTC-011
TEST(SystemFunctional, Ftc011BooleanInvalid)
{
    Rig rig;
    rig.set(ITEM_ISOFIX_LEFT, 2.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_ISOFIX_LEFT]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
}

// FTC-012
TEST(SystemFunctional, Ftc012MissingInvalid)
{
    Rig rig;
    rig.drop(ITEM_CRASH_STATUS);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_CRASH_STATUS]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
}

// FTC-013
TEST(SystemFunctional, Ftc013NanInvalid)
{
    Rig rig;
    rig.set(ITEM_VEHICLE_SPEED, kNaN);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
}

// FTC-014
TEST(SystemFunctional, Ftc014InvalidPersistsThenClears)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_VEHICLE_SPEED, 301.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
    ASSERT_EQ(STATUS_OK, rig.step(20, false));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
    rig.set(ITEM_VEHICLE_SPEED, 50.0);
    ASSERT_EQ(STATUS_OK, rig.step(30));
    EXPECT_EQ(STATE_NORMAL, rig.r().state);
    EXPECT_EQ(VALIDITY_VALID, rig.r().input_validity[ITEM_VEHICLE_SPEED]);
}

// FTC-015
TEST(SystemFunctional, Ftc015StateMatrix)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_SENSOR_FAULT, 1.0);
    rig.set(ITEM_GEAR, 4.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(STATE_FAULT, rig.r().state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, rig.r().warning);
    rig.set(ITEM_GEAR, 0.0);
    rig.set(ITEM_SENSOR_FAULT, 5.0);
    ASSERT_EQ(STATUS_OK, rig.step(20));
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
    EXPECT_EQ(WARNING_NONE, rig.r().warning);
    rig.set(ITEM_SENSOR_FAULT, 0.0);
    ASSERT_EQ(STATUS_OK, rig.step(30));
    EXPECT_EQ(STATE_NORMAL, rig.r().state);
    EXPECT_EQ(WARNING_NONE, rig.r().warning);
}

// FTC-016
TEST(SystemFunctional, Ftc016InvalidBeatsStale)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_GEAR, 4.0);
    ASSERT_EQ(STATUS_OK, rig.step(100));
    ASSERT_EQ(STATUS_OK, rig.step(310, false));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_GEAR]);
    EXPECT_EQ(VALIDITY_STALE, rig.r().input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
}

// FTC-017
TEST(SystemFunctional, Ftc017InvalidReflectedInSameEvaluation)
{
    Rig rig(LOCK_LOCK);
    rig.set(ITEM_GEAR, 9.0);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(VALIDITY_INVALID, rig.r().input_validity[ITEM_GEAR]);
    EXPECT_EQ(REASON_INPUT_INVALID, rig.r().reason);
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
}

// FTC-018
TEST(SystemFunctional, Ftc018InitialState)
{
    Rig rig(LOCK_RELEASE, false);
    lock_ctrl_result_t r = rig.current();
    EXPECT_EQ(LOCK_RELEASE, r.outputs.left);
    EXPECT_EQ(LOCK_RELEASE, r.outputs.right);
    EXPECT_EQ(STATE_NORMAL, r.state);
    EXPECT_TRUE(all_validity(r, VALIDITY_STALE));
    EXPECT_EQ(REASON_NONE, r.reason);
    EXPECT_EQ(REASON_NONE, r.priority_reason);
    EXPECT_EQ(WARNING_NONE, r.warning);
    EXPECT_EQ(COMMAND_NONE, r.command_status);
}

// FTC-019
TEST(SystemFunctional, Ftc019InitialOutputsConfigured)
{
    Rig rig(LOCK_LOCK);
    lock_ctrl_result_t r = rig.current();
    EXPECT_EQ(LOCK_LOCK, r.outputs.left);
    EXPECT_EQ(LOCK_LOCK, r.outputs.right);
}

// FTC-020
TEST(SystemFunctional, Ftc020FirstEvaluationStale)
{
    Rig rig(LOCK_RELEASE, false);
    ASSERT_EQ(STATUS_OK, rig.step(0, false));
    EXPECT_TRUE(all_validity(rig.r(), VALIDITY_STALE));
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
    EXPECT_EQ(REASON_INPUT_STALE, rig.r().reason);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
}

// FTC-021
TEST(SystemFunctional, Ftc021EvaluationTimeBoundary)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(100));
    const lock_ctrl_result_t before = rig.current();
    EXPECT_EQ(STATUS_ERR_TIME, rig.step(99));
    const lock_ctrl_result_t after = rig.current();
    EXPECT_EQ(before.state, after.state);
    EXPECT_EQ(before.reason, after.reason);
    EXPECT_EQ(STATUS_OK, rig.step(100));
    EXPECT_EQ(STATUS_OK, rig.step(101));
}

// FTC-022
TEST(SystemFunctional, Ftc022SnapshotRegression)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(100));
    raw_snapshot_t past = rig.snapshot();
    past.timestamp_ms = {true, 99};
    ASSERT_EQ(STATUS_OK, rig.eval(110, &past, nullptr));
    EXPECT_TRUE(all_validity(rig.r(), VALIDITY_INVALID));
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
    ASSERT_EQ(STATUS_OK, rig.step(120));
    EXPECT_EQ(STATE_NORMAL, rig.r().state);
}

// FTC-023
TEST(SystemFunctional, Ftc023SnapshotFuture)
{
    Rig rig(LOCK_LOCK);
    raw_snapshot_t future = rig.snapshot();
    future.timestamp_ms = {true, 200};
    ASSERT_EQ(STATUS_OK, rig.eval(120, &future, nullptr));
    EXPECT_TRUE(all_validity(rig.r(), VALIDITY_INVALID));
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
}

// FTC-024
TEST(SystemFunctional, Ftc024SnapshotTimeMissing)
{
    Rig rig(LOCK_LOCK);
    raw_snapshot_t missing = rig.snapshot();
    missing.timestamp_ms = {false, 10};
    ASSERT_EQ(STATUS_OK, rig.eval(10, &missing, nullptr));
    EXPECT_TRUE(all_validity(rig.r(), VALIDITY_INVALID));
    EXPECT_EQ(STATE_DEGRADED, rig.r().state);
}

// FTC-025
TEST(SystemFunctional, Ftc025CommandAccepted)
{
    Rig rig(LOCK_LOCK);
    raw_command_t command = {{true, 2.0}, {true, 1.0}, {true, 2.0}};
    ASSERT_EQ(STATUS_OK, rig.step(10, true, &command));
    EXPECT_EQ(COMMAND_ACCEPTED, rig.r().command_status);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.right);
    EXPECT_EQ(REASON_DRIVER_COMMAND, rig.r().reason);
    EXPECT_EQ(STATE_NORMAL, rig.r().state);
}

// FTC-026
TEST(SystemFunctional, Ftc026CommandSideOutOfSet)
{
    Rig rig(LOCK_LOCK);
    raw_command_t command = {{true, 3.0}, {true, 0.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, rig.step(10, true, &command));
    EXPECT_EQ(COMMAND_REJECTED, rig.r().command_status);
    EXPECT_EQ(LOCK_LOCK, rig.r().outputs.left);
}

// FTC-027
TEST(SystemFunctional, Ftc027CommandActionMissing)
{
    Rig rig(LOCK_LOCK);
    raw_command_t command = {{true, 0.0}, {false, 0.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, rig.step(10, true, &command));
    EXPECT_EQ(COMMAND_REJECTED, rig.r().command_status);
}

// FTC-028
TEST(SystemFunctional, Ftc028NoCommand)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(10));
    EXPECT_EQ(COMMAND_NONE, rig.r().command_status);
}

// NTC-001
TEST(SystemNonFunctional, Ntc001CrashReleaseLatency)
{
    Rig rig(LOCK_LOCK);
    for (uint32_t now = 0; now < 100; now += 10)
    {
        ASSERT_EQ(STATUS_OK, rig.step(now));
    }
    const uint32_t crash_time = 100;
    rig.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    raw_snapshot_t crash = rig.snapshot();
    crash.timestamp_ms = {true, crash_time};
    crash.item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(crash_time)};
    const uint32_t first_call =
        crash_time + 10; // 스냅샷 시각 이후 처음 호출되는 평가(호출 주기 10 ms)
    ASSERT_EQ(STATUS_OK, rig.eval(first_call, &crash, nullptr));
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, rig.r().outputs.right);
    EXPECT_LE(first_call - crash_time, 300u);
}

// NTC-002
TEST(SystemNonFunctional, Ntc002StaleDetectionTime)
{
    Rig rig(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, rig.step(100));
    uint32_t first_degraded = 0;
    for (uint32_t now = 110; now <= 400 && first_degraded == 0; now += 10)
    {
        ASSERT_EQ(STATUS_OK, rig.step(now, false));
        if (rig.r().state == STATE_DEGRADED)
        {
            first_degraded = now;
        }
    }
    EXPECT_EQ(310u, first_degraded);
    EXPECT_LE(first_degraded - 100u, 300u);
}

// NTC-003
TEST(SystemNonFunctional, Ntc003EvaluationTime)
{
    Rig rig(LOCK_LOCK);
    raw_command_t command = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    std::chrono::nanoseconds worst{0};
    for (uint32_t i = 0; i < 1000; ++i)
    {
        rig.set(ITEM_CRASH_STATUS, (i % 7 == 0) ? CRASH_CONFIRMED : CRASH_NONE);
        rig.set(ITEM_SENSOR_FAULT, (i % 5 == 0) ? 1.0 : 0.0);
        rig.set(ITEM_VEHICLE_SPEED, (i % 11 == 0) ? 301.0 : 10.0);
        const auto begin = std::chrono::steady_clock::now();
        const status_t status = rig.step(10 * (i + 1), true, (i % 3 == 0) ? &command : nullptr);
        const auto elapsed = std::chrono::steady_clock::now() - begin;
        ASSERT_EQ(STATUS_OK, status);
        if (elapsed > worst)
        {
            worst = elapsed;
        }
    }
    EXPECT_LE(worst, std::chrono::milliseconds(10));
}
