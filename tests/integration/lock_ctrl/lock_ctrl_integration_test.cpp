// 통합 테스트 (SWE.5): lock_ctrl, input_guard, arbiter, common을 실제로 연결해 호출자
// 관점(IF-003)에서 컴포넌트 사이의 인터페이스(IF-001, IF-002, IF-004) 연동을 검증한다. 근거:
// 아키텍처 설계서 7절.
#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <string>

#include "lock_ctrl/lock_ctrl_if.h"
#include "status_view/status_view_if.h"

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/** 호출자(차량 신호 어댑터) 역할의 시험 하네스. 10 ms 주기로 평가한다. */
class Harness
{
  public:
    explicit Harness(lock_cmd_t initial = LOCK_RELEASE)
    {
        lock_ctrl_config_t config = {{initial, initial}};
        EXPECT_EQ(STATUS_OK, lock_ctrl_init(&ctrl_, &config));
        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            snapshot_.item[i] = {true, 0.0};
        }
        snapshot_.item[ITEM_IGNITION_ON] = {true, 1.0}; // 시동 켜짐
    }

    /** 스냅샷의 항목 값을 바꾼다. */
    void set(input_item_t item, double value)
    {
        snapshot_.item[item] = {true, value};
    }

    /** 현재 스냅샷으로 평가한다(시각 = now_ms). */
    status_t step(uint32_t now_ms, bool send_snapshot = true,
                  const raw_command_t *command = nullptr)
    {
        snapshot_.timestamp_ms = {true, now_ms};
        snapshot_.item[ITEM_SOURCE_TIMESTAMP] = {true, static_cast<double>(now_ms)};
        lock_ctrl_input_t input = {now_ms, send_snapshot ? &snapshot_ : nullptr, command};
        return lock_ctrl_evaluate(&ctrl_, &input, &result_);
    }

    const lock_ctrl_result_t &result() const
    {
        return result_;
    }

    lock_ctrl_t *ctrl()
    {
        return &ctrl_;
    }

  private:
    lock_ctrl_t ctrl_ = {};
    raw_snapshot_t snapshot_ = {};
    lock_ctrl_result_t result_ = {};
};

} // namespace

/**
 * @test IT-001
 * @brief 정상 입력이 세 컴포넌트를 거쳐 NORMAL 결과로 나온다 (IF-001, IF-002, IF-003 연동).
 * @par 입력값
 * 모든 항목이 유효한 스냅샷을 10 ms 간격으로 5번
 * @par 절차
 * 초기 출력 LOCK으로 시작해 매 주기 평가
 * @par 예상 출력값
 * 매번 STATUS_OK, 출력 LOCK 유지, state NORMAL, reason NONE, 모든 입력 VALID
 */
TEST(IntegrationFlow, NormalInputKeepsOutputs)
{
    Harness h(LOCK_LOCK);
    for (uint32_t now = 10; now <= 50; now += 10)
    {
        ASSERT_EQ(STATUS_OK, h.step(now));
        EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
        EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
        EXPECT_EQ(STATE_NORMAL, h.result().state);
        EXPECT_EQ(REASON_NONE, h.result().reason);
        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            EXPECT_EQ(VALIDITY_VALID, h.result().input_validity[i]);
        }
    }
}

/**
 * @test IT-002
 * @brief 충돌 CONFIRMED는 같은 평가에서 두 출력을 해제하고 유지된다 (SWR-007, 008, 026; IF-001,
 * IF-002).
 * @par 입력값
 * 초기 출력 LOCK, 정상 3주기 후 충돌 CONFIRMED를 100주기 유지
 * @par 절차
 * 스냅샷 시각 T=40 ms에 CONFIRMED를 넣고 T 이후 매 주기 평가
 * @par 예상 출력값
 * 첫 CONFIRMED 평가부터 100번 모두 두 출력 RELEASE와 reason CRASH_CONFIRMED. T로부터 출력까지 300
 * ms 이내
 */
TEST(IntegrationFlow, CrashConfirmedReleasesAndHolds)
{
    Harness h(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, h.step(10));
    ASSERT_EQ(STATUS_OK, h.step(20));
    ASSERT_EQ(STATUS_OK, h.step(30));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    h.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    const uint32_t crash_time = 40;
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        const uint32_t now = crash_time + 10 * static_cast<uint32_t>(cycle);
        ASSERT_EQ(STATUS_OK, h.step(now));
        EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
        EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
        EXPECT_EQ(REASON_CRASH_CONFIRMED, h.result().reason);
        if (cycle == 0)
        {
            EXPECT_LE(now - crash_time, 300u);
        }
    }
}

/**
 * @test IT-003
 * @brief sensor_fault는 출력을 유지하고 FAULT와 경고를 내며, 충돌 해제가 우선한다 (SWR-021, 023;
 * IF-002).
 * @par 입력값
 * 초기 출력 LOCK, sensor_fault 1 → 충돌 CONFIRMED 추가 → sensor_fault 0
 * @par 절차
 * 각 단계마다 평가
 * @par 예상 출력값
 * 1단계: LOCK 유지, FAULT, 경고. 2단계: RELEASE, FAULT, 경고 유지. 3단계(충돌만): RELEASE, NORMAL,
 * 경고 NONE
 */
TEST(IntegrationFlow, SensorFaultAndCrashPriority)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_SENSOR_FAULT, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(STATE_FAULT, h.result().state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, h.result().warning);
    EXPECT_EQ(REASON_SENSOR_FAULT, h.result().reason);
    h.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    ASSERT_EQ(STATUS_OK, h.step(20));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(STATE_FAULT, h.result().state);
    EXPECT_EQ(WARNING_SENSOR_FAULT, h.result().warning);
    h.set(ITEM_SENSOR_FAULT, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(30));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(STATE_NORMAL, h.result().state);
    EXPECT_EQ(WARNING_NONE, h.result().warning);
}

/**
 * @test IT-004
 * @brief 스냅샷이 200 ms를 넘게 오지 않으면 모든 입력이 STALE이 되어 출력을 유지하고, 새 스냅샷에
 * 복귀한다 (SWR-013, 024, 027; IF-001).
 * @par 입력값
 * 초기 출력 LOCK, 100 ms에 스냅샷 후 스냅샷 없이 300, 310 ms, 이후 320 ms에 새 스냅샷
 * @par 절차
 * 순서대로 평가
 * @par 예상 출력값
 * 300 ms: VALID, NORMAL. 310 ms: 모두 STALE, DEGRADED, INPUT_STALE, 출력 LOCK 유지(T+300 ms 이내).
 * 320 ms: 복귀 NORMAL
 */
TEST(IntegrationFlow, StaleAndRecovery)
{
    Harness h(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, h.step(100));
    ASSERT_EQ(STATUS_OK, h.step(300, false));
    EXPECT_EQ(STATE_NORMAL, h.result().state);
    ASSERT_EQ(STATUS_OK, h.step(310, false));
    EXPECT_EQ(STATE_DEGRADED, h.result().state);
    EXPECT_EQ(REASON_INPUT_STALE, h.result().reason);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        EXPECT_EQ(VALIDITY_STALE, h.result().input_validity[i]);
    }
    ASSERT_EQ(STATUS_OK, h.step(320));
    EXPECT_EQ(STATE_NORMAL, h.result().state);
}

/**
 * @test IT-005
 * @brief 범위 밖 입력은 INVALID가 되어 출력을 유지하고, 스냅샷 없이도 유지되며, 유효 값이 오면
 * 풀린다 (SWR-022, 034; IF-001, IF-002).
 * @par 입력값
 * 초기 출력 LOCK, 차속 300.1 → 스냅샷 없음 → 차속 50.0
 * @par 절차
 * 세 번 평가
 * @par 예상 출력값
 * 1, 2번째: 차속 INVALID, DEGRADED, INPUT_INVALID, LOCK 유지. 3번째: NORMAL
 */
TEST(IntegrationFlow, InvalidInputPersistsAndRecovers)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_VEHICLE_SPEED, 300.1);
    ASSERT_EQ(STATUS_OK, h.step(10));
    EXPECT_EQ(VALIDITY_INVALID, h.result().input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(STATE_DEGRADED, h.result().state);
    EXPECT_EQ(REASON_INPUT_INVALID, h.result().reason);
    ASSERT_EQ(STATUS_OK, h.step(20, false));
    EXPECT_EQ(VALIDITY_INVALID, h.result().input_validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    h.set(ITEM_VEHICLE_SPEED, 50.0);
    ASSERT_EQ(STATUS_OK, h.step(30));
    EXPECT_EQ(STATE_NORMAL, h.result().state);
}

/**
 * @test IT-006
 * @brief 스냅샷 시각이 역행·미래이면 모든 입력이 INVALID가 되고 다음 유효 스냅샷에서 복귀한다
 *        (SWR-046; IF-001).
 * @par 입력값
 * 정상 스냅샷(시각 100) 후 시각 90 스냅샷(now 110), 시각이 now보다 큰 스냅샷(시각 200, now 120),
 * 정상(130)
 * @par 절차
 * 순서대로 평가(시각은 스냅샷의 timestamp_ms를 직접 넣는다)
 * @par 예상 출력값
 * 둘째·셋째 평가: 모든 입력 INVALID, DEGRADED. 넷째: NORMAL
 */
TEST(IntegrationFlow, RejectedSnapshotTimeFlow)
{
    Harness h(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, h.step(100));
    raw_snapshot_t snapshot = {};
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        snapshot.item[i] = {true, 0.0};
    }
    snapshot.timestamp_ms = {true, 90};
    lock_ctrl_input_t past = {110, &snapshot, nullptr};
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_evaluate(h.ctrl(), &past, &result));
    EXPECT_EQ(STATE_DEGRADED, result.state);
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        EXPECT_EQ(VALIDITY_INVALID, result.input_validity[i]);
    }
    snapshot.timestamp_ms = {true, 200};
    lock_ctrl_input_t future = {120, &snapshot, nullptr};
    ASSERT_EQ(STATUS_OK, lock_ctrl_evaluate(h.ctrl(), &future, &result));
    EXPECT_EQ(STATE_DEGRADED, result.state);
    ASSERT_EQ(STATUS_OK, h.step(130));
    EXPECT_EQ(STATE_NORMAL, h.result().state);
}

/**
 * @test IT-007
 * @brief 평가 시각 역행은 STATUS_ERR_TIME으로 거절되고 이전 결과가 유지된다 (SWR-040; IF-003).
 * @par 입력값
 * 100 ms 평가 후 50 ms 평가, 이어서 80 ms 평가(직전에 받아들여진 100 ms보다 작음), 110 ms 평가
 * @par 절차
 * 순서대로 평가하고 결과 조회
 * @par 예상 출력값
 * 50, 80 ms는 STATUS_ERR_TIME, 조회 결과는 100 ms 평가 직후와 같음. 110 ms는 STATUS_OK
 */
TEST(IntegrationFlow, TimeRegressionKeepsResult)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_CRASH_STATUS, CRASH_PENDING);
    ASSERT_EQ(STATUS_OK, h.step(100));
    EXPECT_EQ(STATUS_ERR_TIME, h.step(50));
    EXPECT_EQ(STATUS_ERR_TIME, h.step(80));
    lock_ctrl_result_t kept = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(h.ctrl(), &kept));
    EXPECT_EQ(REASON_CRASH_PENDING, kept.reason);
    EXPECT_EQ(STATE_NORMAL, kept.state);
    EXPECT_EQ(STATUS_OK, h.step(110));
}

/**
 * @test IT-008
 * @brief 유효한 운전자 명령만 적용되고 무효한 명령은 거절되어 출력을 바꾸지 않는다
 *        (SWR-033, SWR-001; IF-001, IF-002).
 * @par 입력값
 * 초기 출력 LOCK, 유효한 명령(all, unlock), 무효한 명령(side 7), 명령 없음
 * @par 절차
 * 각각 평가
 * @par 예상 출력값
 * ACCEPTED(두 출력 RELEASE, reason DRIVER_COMMAND), REJECTED(직전 출력 RELEASE 유지, reason NONE),
 * NONE. state는 모두 NORMAL
 */
TEST(IntegrationFlow, CommandAppliedOnlyWhenValid)
{
    Harness h(LOCK_LOCK);
    raw_command_t good = {{true, 2.0}, {true, 1.0}, {true, 0.0}};
    raw_command_t bad = {{true, 7.0}, {true, 0.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, h.step(10, true, &good));
    EXPECT_EQ(COMMAND_ACCEPTED, h.result().command_status);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(REASON_DRIVER_COMMAND, h.result().reason);
    ASSERT_EQ(STATUS_OK, h.step(20, true, &bad));
    EXPECT_EQ(COMMAND_REJECTED, h.result().command_status);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(REASON_NONE, h.result().reason);
    ASSERT_EQ(STATUS_OK, h.step(30));
    EXPECT_EQ(COMMAND_NONE, h.result().command_status);
    EXPECT_EQ(STATE_NORMAL, h.result().state);
}

/**
 * @test IT-009
 * @brief 충돌 PENDING은 해제하지 않고 이유만 남기며, sensor_fault가 같이 있으면 그 이유가 우선한다
 *        (SWR-036, 023; IF-002).
 * @par 입력값
 * 초기 출력 LOCK, 충돌 PENDING → sensor_fault 추가
 * @par 절차
 * 두 번 평가
 * @par 예상 출력값
 * 1번째: LOCK 유지, CRASH_PENDING. 2번째: LOCK 유지, SENSOR_FAULT, FAULT
 */
TEST(IntegrationFlow, PendingDoesNotRelease)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_CRASH_STATUS, CRASH_PENDING);
    ASSERT_EQ(STATUS_OK, h.step(10));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(REASON_CRASH_PENDING, h.result().reason);
    h.set(ITEM_SENSOR_FAULT, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(20));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
    EXPECT_EQ(REASON_SENSOR_FAULT, h.result().reason);
    EXPECT_EQ(STATE_FAULT, h.result().state);
}

/**
 * @test IT-010
 * @brief 충돌 입력이 STALE이면 마지막 CONFIRMED 값으로 해제하지 않는다. 충돌 입력이 INVALID여도
 * 같다 (SWR-024, 가정 A-04; IF-001, IF-002).
 * @par 입력값
 * 초기 출력 LOCK. (a) 충돌 CONFIRMED 스냅샷 한 번 → 해제 → 이후 STALE 경과, (b) 충돌 값 NaN(무효)
 * @par 절차
 * (a) 정상 → CONFIRMED(RELEASE 확인) 후 스냅샷 없이 200 ms(경과 190)와 211 ms(경과 201) 평가
 * (b) 새 하네스에서 충돌 항목에 NaN
 * @par 예상 출력값
 * (a) STALE 시점에도 출력 RELEASE 유지(직전 확정 출력), reason INPUT_STALE. (b) 출력 LOCK 유지,
 * INPUT_INVALID
 */
TEST(IntegrationFlow, CrashNotAppliedWhenStaleOrInvalid)
{
    Harness a(LOCK_LOCK);
    a.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    ASSERT_EQ(STATUS_OK, a.step(10));
    ASSERT_EQ(LOCK_RELEASE, a.result().outputs.left);
    ASSERT_EQ(STATUS_OK, a.step(200, false));
    EXPECT_EQ(REASON_CRASH_CONFIRMED, a.result().reason);
    ASSERT_EQ(STATUS_OK, a.step(211, false));
    EXPECT_EQ(REASON_INPUT_STALE, a.result().reason);
    EXPECT_EQ(LOCK_RELEASE, a.result().outputs.left);

    Harness b(LOCK_LOCK);
    b.set(ITEM_CRASH_STATUS, kNaN);
    ASSERT_EQ(STATUS_OK, b.step(10));
    EXPECT_EQ(LOCK_LOCK, b.result().outputs.left);
    EXPECT_EQ(REASON_INPUT_INVALID, b.result().reason);
}

/**
 * @test IT-011
 * @brief 초기화 직후의 결과와 첫 평가 전의 get_result가 SWR-025대로이고, 스냅샷 없는 첫 평가는
 * STALE이다 (SWR-025, 045 통합; IF-003).
 * @par 입력값
 * 초기화 직후, 이어서 스냅샷 없이 평가
 * @par 절차
 * get_result 조회 후 evaluate(스냅샷 없음)
 * @par 예상 출력값
 * 초기 결과: RELEASE, NORMAL, 모든 입력 STALE. 첫 평가: DEGRADED, INPUT_STALE, 출력 RELEASE
 */
TEST(IntegrationFlow, InitialAndFirstEvaluation)
{
    Harness h;
    lock_ctrl_result_t initial = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(h.ctrl(), &initial));
    EXPECT_EQ(LOCK_RELEASE, initial.outputs.left);
    EXPECT_EQ(STATE_NORMAL, initial.state);
    EXPECT_EQ(VALIDITY_STALE, initial.input_validity[ITEM_SENSOR_FAULT]);
    ASSERT_EQ(STATUS_OK, h.step(0, false));
    EXPECT_EQ(STATE_DEGRADED, h.result().state);
    EXPECT_EQ(REASON_INPUT_STALE, h.result().reason);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
}

/**
 * @test IT-012
 * @brief 공유 타입(IF-004)의 열거 번호와 상태 코드가 인터페이스 계약과 일치한다 (SWR-038; IF-004).
 * @par 입력값
 * 없음 (타입 상수)
 * @par 절차
 * 열거값과 항목 수를 확인
 * @par 예상 출력값
 * STATUS_OK=0, 충돌 NONE/PENDING/CONFIRMED=0/1/2, 기어 P/N/D/R=0~3, 입력 항목 수 13, LOCK
 * RELEASE/LOCK=0/1
 */
TEST(IntegrationTypes, ContractValues)
{
    EXPECT_EQ(0, STATUS_OK);
    EXPECT_NE(STATUS_OK, STATUS_ERR_PARAM);
    EXPECT_NE(STATUS_ERR_PARAM, STATUS_ERR_TIME);
    EXPECT_EQ(0, CRASH_NONE);
    EXPECT_EQ(1, CRASH_PENDING);
    EXPECT_EQ(2, CRASH_CONFIRMED);
    EXPECT_EQ(0, GEAR_P);
    EXPECT_EQ(3, GEAR_R);
    EXPECT_EQ(13, ITEM_COUNT);
    EXPECT_EQ(0, LOCK_RELEASE);
    EXPECT_EQ(1, LOCK_LOCK);
}

/**
 * @test IT-013
 * @brief 평가 한 번의 실행 시간은 평가 주기(10 ms) 이하이다 (SWR-031; IF-003).
 * @par 입력값
 * 정상, 충돌, sensor_fault, 무효, 명령 입력을 섞어 1,000회 평가
 * @par 절차
 * 평가마다 std::chrono::steady_clock으로 시간을 재고 최댓값을 구함
 * @par 예상 출력값
 * 최댓값이 10 ms 이하
 */
TEST(IntegrationPerformance, EvaluationTimeWithinCycle)
{
    Harness h(LOCK_LOCK);
    raw_command_t command = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    std::chrono::nanoseconds worst{0};
    for (uint32_t i = 0; i < 1000; ++i)
    {
        h.set(ITEM_CRASH_STATUS, (i % 7 == 0) ? CRASH_CONFIRMED : CRASH_NONE);
        h.set(ITEM_SENSOR_FAULT, (i % 5 == 0) ? 1.0 : 0.0);
        h.set(ITEM_VEHICLE_SPEED, (i % 11 == 0) ? 301.0 : 10.0);
        const auto begin = std::chrono::steady_clock::now();
        const status_t status = h.step(10 * (i + 1), true, (i % 3 == 0) ? &command : nullptr);
        const auto elapsed = std::chrono::steady_clock::now() - begin;
        ASSERT_EQ(STATUS_OK, status);
        if (elapsed > worst)
        {
            worst = elapsed;
        }
    }
    EXPECT_LE(worst, std::chrono::milliseconds(10));
}

namespace
{

constexpr raw_command_t kUnlockLeftCmd = {{true, 0.0}, {true, 1.0}, {true, 0.0}};
constexpr raw_command_t kUnlockRightCmd = {{true, 1.0}, {true, 1.0}, {true, 0.0}};
constexpr raw_command_t kUnlockAllCmd = {{true, 2.0}, {true, 1.0}, {true, 0.0}};
constexpr raw_command_t kLockAllCmd = {{true, 2.0}, {true, 0.0}, {true, 0.0}};

} // namespace

/**
 * @test IT-014
 * @brief ignition off는 두 출력을 해제하고 OFF 상태로 하며, 시동이 켜져도 직전 출력을 유지한다
 *        (SWR-020, 035; IF-001, IF-002).
 * @par 입력값
 * 초기 출력 LOCK, ignition_on 0 → 1
 * @par 절차
 * 두 번 평가
 * @par 예상 출력값
 * 1번째: RELEASE, RELEASE, OFF, IGNITION_OFF. 2번째: RELEASE 유지, NORMAL, NONE
 */
TEST(IntegrationDoorRules, IgnitionOffThenOn)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_IGNITION_ON, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(10));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(STATE_OFF, h.result().state);
    EXPECT_EQ(REASON_IGNITION_OFF, h.result().reason);
    h.set(ITEM_IGNITION_ON, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(20));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(STATE_NORMAL, h.result().state);
    EXPECT_EQ(REASON_NONE, h.result().reason);
}

/**
 * @test IT-015
 * @brief 화재, 과온, 성인 탑승은 접근위험 잠금보다 우선해 두 출력을 해제하고, 입력이 사라지면
 * 해제를 유지한다 (SWR-017, 023, 가정 A-18; IF-002).
 * @par 입력값
 * 초기 출력 LOCK, 좌 접근위험 TRUE와 성인 탑승 TRUE, 이후 성인 탑승 FALSE
 * @par 절차
 * 두 번 평가
 * @par 예상 출력값
 * 1번째: RELEASE, RELEASE, ADULT_PRESENT. 2번째: 좌 접근위험 규칙이 다시 적용되어 lock_left
 * LOCK(APPROACH_RISK), lock_right RELEASE 유지
 */
TEST(IntegrationDoorRules, ForcedReleaseBeatsApproachRisk)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_REAR_LEFT_APPROACH_RISK, 1.0);
    h.set(ITEM_ADULT_PRESENT, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(REASON_ADULT_PRESENT, h.result().reason);
    h.set(ITEM_ADULT_PRESENT, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(20));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(REASON_APPROACH_RISK, h.result().reason);
}

/**
 * @test IT-016
 * @brief 접근위험 문은 잠기고 해제 요청이 억제되며 다른 문은 영향받지 않고, 접근위험이 풀리면
 * 해제할 수 있다 (SWR-005, 009, 048; IF-001, IF-002).
 * @par 입력값
 * 초기 RELEASE, 좌 접근위험 TRUE에서 좌 unlock, 우 unlock, 이후 접근위험 FALSE에서 좌 unlock
 * @par 절차
 * 순서대로 평가
 * @par 예상 출력값
 * 좌 unlock: lock_left LOCK 유지(APPROACH_RISK). 우 unlock: lock_right RELEASE. 접근위험 해제 후 좌
 * unlock: lock_left RELEASE
 */
TEST(IntegrationDoorRules, ApproachRiskSuppressesOnlyThatDoor)
{
    Harness h(LOCK_RELEASE);
    h.set(ITEM_REAR_LEFT_APPROACH_RISK, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    ASSERT_EQ(STATUS_OK, h.step(20, true, &kUnlockLeftCmd));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(REASON_APPROACH_RISK, h.result().reason);
    EXPECT_FALSE(h.result().override_active[DOOR_LEFT]);
    ASSERT_EQ(STATUS_OK, h.step(30, true, &kUnlockRightCmd));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    h.set(ITEM_REAR_LEFT_APPROACH_RISK, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(40, true, &kUnlockLeftCmd));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(REASON_DRIVER_COMMAND, h.result().reason);
}

/**
 * @test IT-017
 * @brief 첫 억제 후 10,000 ms 이하에 같은 해제 요청이 다시 오면 override가 성립하고, 초과하면 새 첫
 * 억제이다. override는 접근위험이 풀릴 때까지 이어진다 (SWR-006, 048; IF-001, IF-002, IF-003).
 * @par 입력값
 * 좌 접근위험 TRUE에서 좌 unlock을 10 ms와 10,010 ms에 입력(경과 10,000 ms). 별도 제어기에서 10
 * ms와 10,020 ms (경과 10,010 ms)에 입력
 * @par 절차
 * 10 ms 간격으로 평가하며 명령을 해당 시각에 넣는다
 * @par 예상 출력값
 * 경과 10,000 ms: override_active 좌 true, lock_left RELEASE, APPROACH_OVERRIDE. 이후 접근위험 TRUE
 * 유지에도 RELEASE 유지, 접근위험 FALSE에서 override false. 경과 10,010 ms: LOCK 유지, override
 * false, 억제 기록 갱신
 */
TEST(IntegrationDoorRules, OverrideFlow)
{
    Harness h(LOCK_RELEASE);
    h.set(ITEM_REAR_LEFT_APPROACH_RISK, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10, true, &kUnlockLeftCmd));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    for (uint32_t now = 20; now < 10010; now += 10)
    {
        ASSERT_EQ(STATUS_OK, h.step(now));
        ASSERT_EQ(LOCK_LOCK, h.result().outputs.left);
    }
    ASSERT_EQ(STATUS_OK, h.step(10010, true, &kUnlockLeftCmd));
    EXPECT_TRUE(h.result().override_active[DOOR_LEFT]);
    EXPECT_FALSE(h.result().override_active[DOOR_RIGHT]);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(REASON_APPROACH_OVERRIDE, h.result().reason);
    ASSERT_EQ(STATUS_OK, h.step(10020));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_TRUE(h.result().override_active[DOOR_LEFT]);
    h.set(ITEM_REAR_LEFT_APPROACH_RISK, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(10030));
    EXPECT_FALSE(h.result().override_active[DOOR_LEFT]);

    Harness late(LOCK_RELEASE);
    late.set(ITEM_REAR_LEFT_APPROACH_RISK, 1.0);
    ASSERT_EQ(STATUS_OK, late.step(10, true, &kUnlockLeftCmd));
    for (uint32_t now = 20; now < 10020; now += 10)
    {
        ASSERT_EQ(STATUS_OK, late.step(now));
    }
    ASSERT_EQ(STATUS_OK, late.step(10020, true, &kUnlockLeftCmd));
    EXPECT_FALSE(late.result().override_active[DOOR_LEFT]);
    EXPECT_EQ(LOCK_LOCK, late.result().outputs.left);
    ASSERT_EQ(STATUS_OK, late.step(10030, true, &kUnlockLeftCmd));
    EXPECT_TRUE(late.result().override_active[DOOR_LEFT]);
    EXPECT_EQ(LOCK_RELEASE, late.result().outputs.left);
}

/**
 * @test IT-018
 * @brief 문 규칙 우선순위(ISOFIX > 자동 잠금 > 명령)와 잠금 유지가 평가에 걸쳐 이어진다
 *        (SWR-003, 018, 001; IF-002).
 * @par 입력값
 * 초기 RELEASE, isofix_left TRUE에서 (all, unlock), 차속 10 km/h에서 (all, unlock), 정지 후 (all,
 * unlock)
 * @par 절차
 * 순서대로 평가
 * @par 예상 출력값
 * 1: lock_left LOCK(ISOFIX), lock_right RELEASE. 2: 두 문 LOCK(좌는 ISOFIX, 우는 AUTO_LOCK이며
 * 순위가 높은 ISOFIX가 이유). 정지 후 isofix FALSE: 직전 출력 LOCK 유지, (all, unlock)으로 두 문
 * RELEASE(DRIVER_COMMAND)
 */
TEST(IntegrationDoorRules, IsofixAutoLockAndCommandPriority)
{
    Harness h(LOCK_RELEASE);
    h.set(ITEM_ISOFIX_LEFT, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10, true, &kUnlockAllCmd));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(REASON_ISOFIX, h.result().reason);
    h.set(ITEM_VEHICLE_SPEED, 10.0);
    ASSERT_EQ(STATUS_OK, h.step(20, true, &kUnlockAllCmd));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
    EXPECT_EQ(REASON_ISOFIX, h.result().reason);
    h.set(ITEM_VEHICLE_SPEED, 0.0);
    h.set(ITEM_ISOFIX_LEFT, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(30));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
    ASSERT_EQ(STATUS_OK, h.step(40, true, &kUnlockAllCmd));
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.left);
    EXPECT_EQ(LOCK_RELEASE, h.result().outputs.right);
    EXPECT_EQ(REASON_DRIVER_COMMAND, h.result().reason);
}

/**
 * @test IT-019
 * @brief 전역 규칙이 성립한 평가의 운전자 명령은 버려지고 다음 평가로 이어지지 않는다
 *        (SWR-001, 가정 A-16; IF-001, IF-002).
 * @par 입력값
 * 초기 LOCK, sensor_fault TRUE에서 (all, unlock), 이어서 sensor_fault FALSE이고 명령 없음
 * @par 절차
 * 두 번 평가
 * @par 예상 출력값
 * 1: LOCK 유지, SENSOR_FAULT, command_status ACCEPTED. 2: LOCK 유지(명령은 이어지지 않음), reason
 * NONE
 */
TEST(IntegrationDoorRules, CommandDroppedUnderGlobalRule)
{
    Harness h(LOCK_LOCK);
    h.set(ITEM_SENSOR_FAULT, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10, true, &kUnlockAllCmd));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(REASON_SENSOR_FAULT, h.result().reason);
    EXPECT_EQ(COMMAND_ACCEPTED, h.result().command_status);
    h.set(ITEM_SENSOR_FAULT, 0.0);
    ASSERT_EQ(STATUS_OK, h.step(20));
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
    EXPECT_EQ(REASON_NONE, h.result().reason);
}

/**
 * @test IT-020
 * @brief 운전자 명령의 source 네 값은 같은 결과를 낸다 (SWR-002; IF-001, IF-002).
 * @par 입력값
 * (all, lock) 명령에 source 0~3, 초기 RELEASE
 * @par 절차
 * source마다 새 제어기로 평가
 * @par 예상 출력값
 * 네 경우 모두 두 출력 LOCK, reason DRIVER_COMMAND, command_status ACCEPTED
 */
TEST(IntegrationDoorRules, SourceDoesNotMatter)
{
    for (int source = 0; source < 4; ++source)
    {
        Harness h(LOCK_RELEASE);
        raw_command_t command = kLockAllCmd;
        command.source = {true, static_cast<double>(source)};
        ASSERT_EQ(STATUS_OK, h.step(10, true, &command));
        EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
        EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
        EXPECT_EQ(REASON_DRIVER_COMMAND, h.result().reason);
        EXPECT_EQ(COMMAND_ACCEPTED, h.result().command_status);
    }
}

namespace
{

/** 조회 결과를 직렬화한 문자열. */
std::string serialize(Harness &h)
{
    lock_ctrl_result_t result = {};
    EXPECT_EQ(STATUS_OK, lock_ctrl_get_result(h.ctrl(), &result));
    char buffer[2048] = {};
    EXPECT_EQ(STATUS_OK, status_view_format(&result, buffer, sizeof(buffer)));
    return std::string(buffer);
}

} // namespace

/**
 * @test IT-021
 * @brief 평가 결과를 조회해 직렬화하면 그 평가의 출력, 이유, 입력 상태, override가 문자열에 그대로
 * 나온다 (SWR-014, 015; IF-003, IF-005).
 * @par 입력값
 * 초기 RELEASE, 좌 접근위험 TRUE에서 좌 unlock 두 번(10,000 ms 이내), 이후 STALE
 * @par 절차
 * 각 단계 뒤 lock_ctrl_get_result와 status_view_format 호출
 * @par 예상 출력값
 * 접근위험: "lock_left":"LOCK", "reason_code":"APPROACH_RISK". override 성립: "override_left":true,
 * "lock_left":"RELEASE", "APPROACH_OVERRIDE". STALE 후: "state":"DEGRADED", "INPUT_STALE", 입력
 * 상태 "STALE"
 */
TEST(IntegrationStatusView, ReflectsEvaluationResult)
{
    Harness h(LOCK_RELEASE);
    h.set(ITEM_REAR_LEFT_APPROACH_RISK, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(10));
    std::string text = serialize(h);
    EXPECT_NE(std::string::npos, text.find("\"lock_left\":\"LOCK\""));
    EXPECT_NE(std::string::npos, text.find("\"reason_code\":\"APPROACH_RISK\""));
    ASSERT_EQ(STATUS_OK, h.step(20, true, &kUnlockLeftCmd));
    for (uint32_t now = 30; now <= 10000; now += 10)
    {
        ASSERT_EQ(STATUS_OK, h.step(now));
    }
    ASSERT_EQ(STATUS_OK, h.step(10010, true, &kUnlockLeftCmd));
    text = serialize(h);
    EXPECT_NE(std::string::npos, text.find("\"override_left\":true"));
    EXPECT_NE(std::string::npos, text.find("\"lock_left\":\"RELEASE\""));
    EXPECT_NE(std::string::npos, text.find("\"reason_code\":\"APPROACH_OVERRIDE\""));
    ASSERT_EQ(STATUS_OK, h.step(10300, false));
    ASSERT_EQ(STATUS_OK, h.step(10310, false));
    text = serialize(h);
    EXPECT_NE(std::string::npos, text.find("\"state\":\"DEGRADED\""));
    EXPECT_NE(std::string::npos, text.find("\"reason_code\":\"INPUT_STALE\""));
    EXPECT_NE(std::string::npos, text.find("\"vehicle_speed\":\"STALE\""));
}

/**
 * @test IT-022
 * @brief 직렬화 버퍼가 부족하면 STATUS_ERR_BUFFER와 빈 문자열이다 (SWR-015; IF-005).
 * @par 입력값
 * 초기 결과, 버퍼 크기 10
 * @par 절차
 * status_view_format 호출
 * @par 예상 출력값
 * STATUS_ERR_BUFFER, 빈 문자열
 */
TEST(IntegrationStatusView, SmallBufferFails)
{
    Harness h(LOCK_LOCK);
    lock_ctrl_result_t result = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_result(h.ctrl(), &result));
    char buffer[10] = {'x'};
    EXPECT_EQ(STATUS_ERR_BUFFER, status_view_format(&result, buffer, sizeof(buffer)));
    EXPECT_EQ('\0', buffer[0]);
}

/**
 * @test IT-023
 * @brief 결정이 바뀔 때만 이력에 남고 이벤트 ID, 시각, 이유가 평가 결과와 일치한다
 *        (SWR-010, 011; IF-003, IF-006).
 * @par 입력값
 * 초기 LOCK, 정상 → sensor_fault TRUE → 같은 상태 반복 → 충돌 CONFIRMED → 정상(충돌 해제 유지)
 * @par 절차
 * 평가하며 이력 건수와 항목 조회
 * @par 예상 출력값
 * 이력: (ID 1, 시각 20, FAULT, SENSOR_FAULT), (ID 2, 시각 40, RELEASE, CRASH_CONFIRMED), (ID 3,
 * 시각 50, NORMAL, NONE). 반복 평가(30)는 이력에 없음
 */
TEST(IntegrationDecisionLog, RecordsDecisionChanges)
{
    Harness h(LOCK_LOCK);
    ASSERT_EQ(STATUS_OK, h.step(10));
    h.set(ITEM_SENSOR_FAULT, 1.0);
    ASSERT_EQ(STATUS_OK, h.step(20));
    ASSERT_EQ(STATUS_OK, h.step(30));
    h.set(ITEM_CRASH_STATUS, CRASH_CONFIRMED);
    ASSERT_EQ(STATUS_OK, h.step(40));
    h.set(ITEM_SENSOR_FAULT, 0.0);
    h.set(ITEM_CRASH_STATUS, CRASH_NONE);
    ASSERT_EQ(STATUS_OK, h.step(50));
    uint32_t count = 0;
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(h.ctrl(), &count));
    ASSERT_EQ(3u, count);
    decision_entry_t entry = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(h.ctrl(), 0, &entry));
    EXPECT_EQ(1u, entry.event_id);
    EXPECT_EQ(20u, entry.now_ms);
    EXPECT_EQ(STATE_FAULT, entry.state);
    EXPECT_EQ(REASON_SENSOR_FAULT, entry.reason);
    EXPECT_EQ(WARNING_SENSOR_FAULT, entry.warning);
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(h.ctrl(), 1, &entry));
    EXPECT_EQ(2u, entry.event_id);
    EXPECT_EQ(40u, entry.now_ms);
    EXPECT_EQ(LOCK_RELEASE, entry.outputs.left);
    EXPECT_EQ(REASON_CRASH_CONFIRMED, entry.reason);
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(h.ctrl(), 2, &entry));
    EXPECT_EQ(3u, entry.event_id);
    EXPECT_EQ(50u, entry.now_ms);
    EXPECT_EQ(STATE_NORMAL, entry.state);
    EXPECT_EQ(REASON_NONE, entry.reason);
}

/**
 * @test IT-024
 * @brief 결정이 101번 바뀌면 최신 100건만 오래된 순서로 남고, 초기화(재기동)하면 비워진다
 *        (SWR-010, 012; IF-003, IF-006).
 * @par 입력값
 * sensor_fault를 매 평가마다 바꿔 101번 결정 변화, 이후 lock_ctrl_init
 * @par 절차
 * 평가 후 첫·마지막 항목 조회, 범위 밖 index 조회, 초기화 후 건수와 새 ID
 * @par 예상 출력값
 * 건수 100, 첫 ID 2, 마지막 ID 101, index 100은 STATUS_ERR_PARAM. 초기화 후 건수 0, 새 항목은 ID 1
 */
TEST(IntegrationDecisionLog, KeepsLatestHundredAndResets)
{
    Harness h(LOCK_LOCK);
    for (uint32_t i = 0; i < 101; ++i)
    {
        h.set(ITEM_SENSOR_FAULT, (i % 2 == 0) ? 1.0 : 0.0);
        ASSERT_EQ(STATUS_OK, h.step(10 * (i + 1)));
    }
    uint32_t count = 0;
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(h.ctrl(), &count));
    EXPECT_EQ(100u, count);
    decision_entry_t entry = {};
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(h.ctrl(), 0, &entry));
    EXPECT_EQ(2u, entry.event_id);
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_entry(h.ctrl(), 99, &entry));
    EXPECT_EQ(101u, entry.event_id);
    EXPECT_EQ(STATUS_ERR_PARAM, lock_ctrl_get_log_entry(h.ctrl(), 100, &entry));
    ASSERT_EQ(STATUS_OK, lock_ctrl_init(h.ctrl(), nullptr));
    ASSERT_EQ(STATUS_OK, lock_ctrl_get_log_count(h.ctrl(), &count));
    EXPECT_EQ(0u, count);
}

/**
 * @test IT-025
 * @brief 일반 해제 허용 조건: 접근위험·ISOFIX·자동 잠금·전역 규칙이 없을 때만 해제가 적용된다
 *        (SWR-004, 001; IF-001, IF-002).
 * @par 입력값
 * 초기 LOCK, (all, unlock) 명령 + (좌 접근위험 / 우 ISOFIX / 차속 10 km/h / sensor_fault / 조건
 * 없음)
 * @par 절차
 * 조건마다 새 제어기로 평가
 * @par 예상 출력값
 * (LOCK, RELEASE), (RELEASE, LOCK), (LOCK, LOCK), (LOCK, LOCK), (RELEASE, RELEASE)
 */
TEST(IntegrationDoorRules, GeneralReleaseAllowedOnlyWithoutBlockers)
{
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
        Harness h(LOCK_LOCK);
        h.set(c.item, c.value);
        ASSERT_EQ(STATUS_OK, h.step(10, true, &kUnlockAllCmd));
        EXPECT_EQ(c.left, h.result().outputs.left);
        EXPECT_EQ(c.right, h.result().outputs.right);
    }
}
