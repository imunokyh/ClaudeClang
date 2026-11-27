// input_guard 단위 테스트 (SWE.4). 근거: 상세 설계 FN-001~FN-013의 결정표.
// 비공개(static) 함수는 공개 함수를 통해 시험하며, 예상값은 설계의 결정표에서 정했다.
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "input_guard/input_guard_if.h"

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/** 모든 항목이 유효한 스냅샷(차속 0, 기어 P, 충돌 NONE, 시각 항목은 스냅샷 시각). */
raw_snapshot_t valid_snapshot(int64_t timestamp_ms)
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

/** 초기화된 저장소로 평가 시각 now_ms에 snapshot을 넣어 결과를 얻는다. */
normalized_inputs_t update(input_guard_t *guard, uint32_t now_ms, const raw_snapshot_t *snapshot)
{
    normalized_inputs_t out = {};
    EXPECT_EQ(STATUS_OK, input_guard_update(guard, now_ms, snapshot, &out));
    return out;
}

input_guard_t fresh_guard()
{
    input_guard_t guard = {};
    EXPECT_EQ(STATUS_OK, input_guard_init(&guard));
    return guard;
}

/** 항목 하나에 값을 넣은 스냅샷을 평가하고 그 항목의 입력 상태를 돌려준다. */
validity_t validity_after(input_item_t item, raw_value_t raw)
{
    input_guard_t guard = fresh_guard();
    raw_snapshot_t snapshot = valid_snapshot(100);
    snapshot.item[item] = raw;
    return update(&guard, 100, &snapshot).validity[item];
}

} // namespace

/**
 * @test UT-001
 * @brief input_guard_init은 NULL 저장소를 거절한다 (FN-001, R1).
 * @par 입력값
 * guard = NULL
 * @par 절차
 * input_guard_init(NULL) 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM
 */
TEST(InputGuardInit, NullGuardIsRejected)
{
    EXPECT_EQ(STATUS_ERR_PARAM, input_guard_init(nullptr));
}

/**
 * @test UT-002
 * @brief 초기화 직후 스냅샷 없이 평가하면 모든 입력 항목이 STALE이다 (FN-001, FN-011, FN-012).
 * @par 입력값
 * 초기화한 저장소, snapshot = NULL, now_ms = 0
 * @par 절차
 * input_guard_update 호출 후 입력 상태 확인
 * @par 예상 출력값
 * 13개 항목 모두 VALIDITY_STALE, 보관 값 0
 */
TEST(InputGuardInit, InitialUpdateIsStale)
{
    input_guard_t guard = fresh_guard();
    normalized_inputs_t out = update(&guard, 0, nullptr);
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        EXPECT_EQ(VALIDITY_STALE, out.validity[i]);
        EXPECT_EQ(0, out.value[i]);
    }
}

/**
 * @test UT-003
 * @brief input_guard_update는 guard 또는 out이 NULL이면 거절한다 (FN-002, R1).
 * @par 입력값
 * (guard = NULL, out 유효), (guard 유효, out = NULL)
 * @par 절차
 * 각각 input_guard_update 호출
 * @par 예상 출력값
 * 두 경우 모두 STATUS_ERR_PARAM
 */
TEST(InputGuardUpdate, NullArgumentsAreRejected)
{
    input_guard_t guard = fresh_guard();
    normalized_inputs_t out = {};
    EXPECT_EQ(STATUS_ERR_PARAM, input_guard_update(nullptr, 0, nullptr, &out));
    EXPECT_EQ(STATUS_ERR_PARAM, input_guard_update(&guard, 0, nullptr, nullptr));
}

/**
 * @test UT-004
 * @brief 유효한 스냅샷은 모든 항목을 VALID로 만들고 판정 값을 정규화한다 (FN-002, FN-006~FN-009).
 * @par 입력값
 * 차속 123.4 km/h, 기어 D(2), 충돌 CONFIRMED(2), sensor_fault 1, 나머지 0
 * @par 절차
 * 스냅샷을 평가하고 입력 상태와 판정 값 확인
 * @par 예상 출력값
 * 모두 VALID, 차속 1234(0.1 km/h), 기어 2, 충돌 2, sensor_fault 1
 */
TEST(InputGuardUpdate, ValidSnapshotIsNormalized)
{
    input_guard_t guard = fresh_guard();
    raw_snapshot_t snapshot = valid_snapshot(100);
    snapshot.item[ITEM_VEHICLE_SPEED].value = 123.4;
    snapshot.item[ITEM_GEAR].value = GEAR_D;
    snapshot.item[ITEM_CRASH_STATUS].value = CRASH_CONFIRMED;
    snapshot.item[ITEM_SENSOR_FAULT].value = 1.0;
    normalized_inputs_t out = update(&guard, 100, &snapshot);
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        EXPECT_EQ(VALIDITY_VALID, out.validity[i]);
    }
    EXPECT_EQ(1234, out.value[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(2, out.value[ITEM_GEAR]);
    EXPECT_EQ(2, out.value[ITEM_CRASH_STATUS]);
    EXPECT_EQ(1, out.value[ITEM_SENSOR_FAULT]);
    EXPECT_EQ(100, out.value[ITEM_SOURCE_TIMESTAMP]);
}

/**
 * @test UT-005
 * @brief 차속의 경계값 분석: 0.0과 300.0은 유효, -0.1, 300.1, NaN, 누락은 무효 (FN-009).
 * @par 입력값
 * 차속 0.0, 300.0, -0.1, 300.1, NaN, 누락(present=false)
 * @par 절차
 * 각각 스냅샷에 넣어 평가
 * @par 예상 출력값
 * 0.0, 300.0 → VALID(0, 3000), 나머지 INVALID
 */
TEST(InputGuardUpdate, SpeedBoundaryValues)
{
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_VEHICLE_SPEED, {true, 0.0}));
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_VEHICLE_SPEED, {true, 300.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_VEHICLE_SPEED, {true, -0.1}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_VEHICLE_SPEED, {true, 300.1}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_VEHICLE_SPEED, {true, kNaN}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_VEHICLE_SPEED, {false, 50.0}));
    input_guard_t guard = fresh_guard();
    raw_snapshot_t snapshot = valid_snapshot(100);
    snapshot.item[ITEM_VEHICLE_SPEED].value = 300.0;
    EXPECT_EQ(3000, update(&guard, 100, &snapshot).value[ITEM_VEHICLE_SPEED]);
}

/**
 * @test UT-006
 * @brief boolean 항목: 0과 1은 유효, 2, 0.5, NaN, 누락은 무효 (FN-007, 동치 클래스).
 * @par 입력값
 * ISOFIX 좌 항목에 0, 1, 2, 0.5, NaN, 누락(값은 0)
 * @par 절차
 * 각각 스냅샷에 넣어 평가
 * @par 예상 출력값
 * 0, 1 → VALID. 나머지 INVALID. 누락은 값이 0이어도 INVALID
 */
TEST(InputGuardUpdate, BooleanItemRules)
{
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_ISOFIX_LEFT, {true, 0.0}));
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_ISOFIX_LEFT, {true, 1.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_ISOFIX_LEFT, {true, 2.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_ISOFIX_LEFT, {true, 0.5}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_ISOFIX_LEFT, {true, kNaN}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_ISOFIX_LEFT, {false, 0.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_ISOFIX_LEFT, {false, 1.0}));
}

/**
 * @test UT-007
 * @brief 열거 항목(기어 0~3, 충돌 0~2)의 경계값과 정수가 아닌 값 (FN-008).
 * @par 입력값
 * 기어 0, 3, -1, 4, 1.5, 누락 / 충돌 0, 2, 3
 * @par 절차
 * 각각 스냅샷에 넣어 평가
 * @par 예상 출력값
 * 범위 안 정수는 VALID, 그 밖(범위 밖, 소수, 누락)은 INVALID
 */
TEST(InputGuardUpdate, EnumItemRules)
{
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_GEAR, {true, 0.0}));
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_GEAR, {true, 3.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_GEAR, {true, -1.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_GEAR, {true, 4.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_GEAR, {true, 1.5}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_GEAR, {false, 1.0}));
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_CRASH_STATUS, {true, 0.0}));
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_CRASH_STATUS, {true, 2.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_CRASH_STATUS, {true, 3.0}));
}

/**
 * @test UT-008
 * @brief 시각 항목(source_timestamp)은 0 이상이고 스냅샷 시각 이하의 정수만 유효하다 (FN-006,
 * FN-010).
 * @par 입력값
 * 스냅샷 시각 100에서 source_timestamp 100, 0, 101, -1, 99.5
 * @par 절차
 * 각각 스냅샷에 넣어 평가
 * @par 예상 출력값
 * 100, 0 → VALID. 101(상한 초과), -1(하한 미만), 99.5(소수)는 INVALID
 */
TEST(InputGuardUpdate, SourceTimestampRules)
{
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_SOURCE_TIMESTAMP, {true, 100.0}));
    EXPECT_EQ(VALIDITY_VALID, validity_after(ITEM_SOURCE_TIMESTAMP, {true, 0.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_SOURCE_TIMESTAMP, {true, 101.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_SOURCE_TIMESTAMP, {true, -1.0}));
    EXPECT_EQ(VALIDITY_INVALID, validity_after(ITEM_SOURCE_TIMESTAMP, {true, 99.5}));
}

/**
 * @test UT-009
 * @brief 스냅샷 시각이 누락, 역행, 미래이면 스냅샷 전체를 거절한다 (FN-004, FN-005).
 * @par 입력값
 * 100 ms에 유효 스냅샷을 받은 뒤, (시각 누락), (시각 99), (시각 now_ms 초과 = 151, now 150)
 * @par 절차
 * 각 비정상 스냅샷을 평가하고 모든 항목의 입력 상태, 보관 값, 마지막 수신 시각의 영향 확인
 * @par 예상 출력값
 * 세 경우 모두 모든 항목 INVALID. 이후 새 유효 스냅샷(시각 100 이상)이 오면 풀린다
 */
TEST(InputGuardUpdate, RejectedSnapshotMarksAllInvalid)
{
    raw_snapshot_t missing = valid_snapshot(120);
    missing.timestamp_ms.present = false;
    raw_snapshot_t regressed = valid_snapshot(99);
    raw_snapshot_t future = valid_snapshot(151);
    const raw_snapshot_t *bad[] = {&missing, &regressed, &future};
    for (const raw_snapshot_t *snapshot : bad)
    {
        input_guard_t guard = fresh_guard();
        raw_snapshot_t first = valid_snapshot(100);
        first.item[ITEM_VEHICLE_SPEED].value = 50.0;
        ASSERT_EQ(VALIDITY_VALID, update(&guard, 100, &first).validity[ITEM_VEHICLE_SPEED]);
        normalized_inputs_t out = update(&guard, 150, snapshot);
        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            EXPECT_EQ(VALIDITY_INVALID, out.validity[i]);
        }
        EXPECT_EQ(500, out.value[ITEM_VEHICLE_SPEED]); // 보관 값 불변
        raw_snapshot_t good = valid_snapshot(150);
        EXPECT_EQ(VALIDITY_VALID, update(&guard, 150, &good).validity[ITEM_VEHICLE_SPEED]);
    }
}

/**
 * @test UT-010
 * @brief 마지막 수신 후 200 ms까지는 VALID, 초과하면 STALE이다 (FN-011, FN-012).
 * @par 입력값
 * 100 ms에 유효 스냅샷, 이후 스냅샷 없이 now 300, 301, 310
 * @par 절차
 * 각 시각에 input_guard_update 호출
 * @par 예상 출력값
 * 300 → VALID(경과 200), 301 → STALE(경과 201), 310 → STALE
 */
TEST(InputGuardUpdate, StaleBoundary)
{
    input_guard_t guard = fresh_guard();
    raw_snapshot_t snapshot = valid_snapshot(100);
    ASSERT_EQ(VALIDITY_VALID, update(&guard, 100, &snapshot).validity[ITEM_CRASH_STATUS]);
    EXPECT_EQ(VALIDITY_VALID, update(&guard, 300, nullptr).validity[ITEM_CRASH_STATUS]);
    EXPECT_EQ(VALIDITY_STALE, update(&guard, 301, nullptr).validity[ITEM_CRASH_STATUS]);
    EXPECT_EQ(VALIDITY_STALE, update(&guard, 310, nullptr).validity[ITEM_GEAR]);
}

/**
 * @test UT-011
 * @brief INVALID는 스냅샷이 없어도 유지되고, 그 항목이 유효하게 갱신되면 풀리며 보관 값이 바뀐다
 *        (FN-002, FN-011).
 * @par 입력값
 * 차속 50.0 유효 → 차속 301.0 무효 → 스냅샷 없음 → 차속 60.0 유효
 * @par 절차
 * 네 번 평가
 * @par 예상 출력값
 * 둘째·셋째에서 차속 INVALID(보관 값 500 유지), 넷째에서 VALID와 값 600. 다른 항목은 계속 VALID
 */
TEST(InputGuardUpdate, InvalidPersistsUntilValidUpdate)
{
    input_guard_t guard = fresh_guard();
    raw_snapshot_t s1 = valid_snapshot(100);
    s1.item[ITEM_VEHICLE_SPEED].value = 50.0;
    ASSERT_EQ(500, update(&guard, 100, &s1).value[ITEM_VEHICLE_SPEED]);
    raw_snapshot_t s2 = valid_snapshot(110);
    s2.item[ITEM_VEHICLE_SPEED].value = 301.0;
    normalized_inputs_t second = update(&guard, 110, &s2);
    EXPECT_EQ(VALIDITY_INVALID, second.validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(VALIDITY_VALID, second.validity[ITEM_GEAR]);
    EXPECT_EQ(500, second.value[ITEM_VEHICLE_SPEED]);
    normalized_inputs_t third = update(&guard, 120, nullptr);
    EXPECT_EQ(VALIDITY_INVALID, third.validity[ITEM_VEHICLE_SPEED]);
    raw_snapshot_t s4 = valid_snapshot(130);
    s4.item[ITEM_VEHICLE_SPEED].value = 60.0;
    normalized_inputs_t fourth = update(&guard, 130, &s4);
    EXPECT_EQ(VALIDITY_VALID, fourth.validity[ITEM_VEHICLE_SPEED]);
    EXPECT_EQ(600, fourth.value[ITEM_VEHICLE_SPEED]);
}

/**
 * @test UT-012
 * @brief 같은 시각의 스냅샷은 받아들이고(역행 아님), 스냅샷이 없는 평가는 저장 상태를 바꾸지 않는다
 *        (FN-004 경계, FN-002 R2).
 * @par 입력값
 * 시각 100 스냅샷을 두 번(같은 시각), 이후 스냅샷 없음
 * @par 절차
 * 세 번 평가
 * @par 예상 출력값
 * 모두 VALID
 */
TEST(InputGuardUpdate, EqualTimestampIsAccepted)
{
    input_guard_t guard = fresh_guard();
    raw_snapshot_t snapshot = valid_snapshot(100);
    EXPECT_EQ(VALIDITY_VALID, update(&guard, 100, &snapshot).validity[ITEM_GEAR]);
    EXPECT_EQ(VALIDITY_VALID, update(&guard, 100, &snapshot).validity[ITEM_GEAR]);
    EXPECT_EQ(VALIDITY_VALID, update(&guard, 105, nullptr).validity[ITEM_GEAR]);
}

/**
 * @test UT-013
 * @brief 운전자 명령 검사: 모든 필드가 유효하면 ACCEPTED, 하나라도 무효·누락이면 REJECTED (FN-003).
 * @par 입력값
 * (side, action, source) = (2,1,3) 유효, side 3, side 누락, action 2, action 누락, source 4,
 * source 누락, side 0.5
 * @par 절차
 * 각각 input_guard_check_command 호출
 * @par 예상 출력값
 * 유효 조합만 COMMAND_ACCEPTED, 나머지는 COMMAND_REJECTED. 반환은 모두 STATUS_OK
 */
TEST(InputGuardCommand, FieldRules)
{
    command_status_t status = COMMAND_NONE;
    raw_command_t cmd = {{true, 2.0}, {true, 1.0}, {true, 3.0}};
    ASSERT_EQ(STATUS_OK, input_guard_check_command(&cmd, &status));
    EXPECT_EQ(COMMAND_ACCEPTED, status);

    raw_command_t cases[] = {
        {{true, 3.0}, {true, 1.0}, {true, 3.0}}, {{false, 1.0}, {true, 1.0}, {true, 3.0}},
        {{true, 2.0}, {true, 2.0}, {true, 3.0}}, {{true, 2.0}, {false, 1.0}, {true, 3.0}},
        {{true, 2.0}, {true, 1.0}, {true, 4.0}}, {{true, 2.0}, {true, 1.0}, {false, 3.0}},
        {{true, 0.5}, {true, 1.0}, {true, 3.0}},
    };
    for (const raw_command_t &c : cases)
    {
        status = COMMAND_NONE;
        ASSERT_EQ(STATUS_OK, input_guard_check_command(&c, &status));
        EXPECT_EQ(COMMAND_REJECTED, status);
    }
    raw_command_t zero = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    ASSERT_EQ(STATUS_OK, input_guard_check_command(&zero, &status));
    EXPECT_EQ(COMMAND_ACCEPTED, status);
}

/**
 * @test UT-014
 * @brief input_guard_check_command는 NULL 인자를 거절하고 출력을 바꾸지 않는다 (FN-003, R0).
 * @par 입력값
 * (command = NULL), (out = NULL)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM, out은 호출 전 값(COMMAND_NONE) 그대로
 */
TEST(InputGuardCommand, NullArgumentsAreRejected)
{
    command_status_t status = COMMAND_NONE;
    raw_command_t cmd = {{true, 0.0}, {true, 0.0}, {true, 0.0}};
    EXPECT_EQ(STATUS_ERR_PARAM, input_guard_check_command(nullptr, &status));
    EXPECT_EQ(COMMAND_NONE, status);
    EXPECT_EQ(STATUS_ERR_PARAM, input_guard_check_command(&cmd, nullptr));
}
