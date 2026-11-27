// 통합 테스트 (SWE.5): lock_ctrl, input_guard, arbiter, common을 실제로 연결해 호출자
// 관점(IF-003)에서 컴포넌트 사이의 인터페이스(IF-001, IF-002, IF-004) 연동을 검증한다. 근거:
// 아키텍처 설계서 7절.
#include <gtest/gtest.h>

#include <chrono>
#include <limits>

#include "lock_ctrl/lock_ctrl_if.h"

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
 * @brief 운전자 명령은 검사 결과만 command_status로 나오고 출력과 state를 바꾸지 않는다 (SWR-033;
 * IF-001).
 * @par 입력값
 * 초기 출력 LOCK, 유효한 명령, 무효한 명령(side 7), 명령 없음
 * @par 절차
 * 각각 평가
 * @par 예상 출력값
 * ACCEPTED, REJECTED, NONE. 세 경우 모두 출력 LOCK 유지, state NORMAL
 */
TEST(IntegrationFlow, CommandDoesNotChangeOutputs)
{
    Harness h(LOCK_LOCK);
    raw_command_t good = {{true, 2.0}, {true, 1.0}, {true, 0.0}};
    raw_command_t bad = {{true, 7.0}, {true, 0.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, h.step(10, true, &good));
    EXPECT_EQ(COMMAND_ACCEPTED, h.result().command_status);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.left);
    ASSERT_EQ(STATUS_OK, h.step(20, true, &bad));
    EXPECT_EQ(COMMAND_REJECTED, h.result().command_status);
    EXPECT_EQ(LOCK_LOCK, h.result().outputs.right);
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
