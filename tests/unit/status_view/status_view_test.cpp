// status_view 단위 테스트 (SWE.4). 근거: 상세 설계 FN-037~FN-041의 결정표.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "status_view/status_view_if.h"

namespace
{

lock_ctrl_result_t base_result()
{
    lock_ctrl_result_t result = {};
    result.outputs = {LOCK_RELEASE, LOCK_RELEASE};
    result.state = STATE_NORMAL;
    result.reason = REASON_NONE;
    result.priority_reason = REASON_NONE;
    result.warning = WARNING_NONE;
    result.command_status = COMMAND_NONE;
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        result.input_validity[i] = VALIDITY_VALID;
    }
    return result;
}

std::string format(const lock_ctrl_result_t &result)
{
    char buffer[1024] = {};
    EXPECT_EQ(STATUS_OK, status_view_format(&result, buffer, sizeof(buffer)));
    return std::string(buffer);
}

bool contains(const std::string &text, const char *part)
{
    return text.find(part) != std::string::npos;
}

} // namespace

/**
 * @test UT-100
 * @brief status_view_format은 NULL 결과, NULL 버퍼, 크기 0을 거절한다 (FN-037 R1~R3).
 * @par 입력값
 * (result NULL), (buffer NULL), (size 0)
 * @par 절차
 * 각각 호출
 * @par 예상 출력값
 * STATUS_ERR_PARAM이고 버퍼는 바뀌지 않는다
 */
TEST(StatusViewParams, NullAndZeroSize)
{
    lock_ctrl_result_t result = base_result();
    char buffer[8] = {'x', 0};
    EXPECT_EQ(STATUS_ERR_PARAM, status_view_format(nullptr, buffer, sizeof(buffer)));
    EXPECT_EQ(STATUS_ERR_PARAM, status_view_format(&result, nullptr, sizeof(buffer)));
    EXPECT_EQ(STATUS_ERR_PARAM, status_view_format(&result, buffer, 0));
    EXPECT_EQ('x', buffer[0]);
}

/**
 * @test UT-101
 * @brief 정해진 값의 결과는 5.6절 형식의 정확한 문자열이 된다 (FN-037, FN-039, FN-040).
 * @par 입력값
 * 출력 (LOCK, RELEASE), state DEGRADED, 이유 INPUT_STALE, 경고 SENSOR_FAULT, 명령 ACCEPTED,
 * override 좌 true 우 false, 입력 상태는 항목 0이 INVALID, 항목 1이 STALE, 나머지 VALID
 * @par 절차
 * 직렬화
 * @par 예상 출력값
 * 키 순서와 값이 요구사항 5.6절과 정확히 같은 한 줄 JSON
 */
TEST(StatusViewFormat, ExactString)
{
    lock_ctrl_result_t result = base_result();
    result.outputs = {LOCK_LOCK, LOCK_RELEASE};
    result.state = STATE_DEGRADED;
    result.reason = REASON_INPUT_STALE;
    result.priority_reason = REASON_INPUT_STALE;
    result.warning = WARNING_SENSOR_FAULT;
    result.command_status = COMMAND_ACCEPTED;
    result.override_active[DOOR_LEFT] = true;
    result.input_validity[0] = VALIDITY_INVALID;
    result.input_validity[1] = VALIDITY_STALE;
    const std::string expected =
        "{\"lock_left\":\"LOCK\",\"lock_right\":\"RELEASE\",\"state\":\"DEGRADED\","
        "\"priority_reason\":\"INPUT_STALE\",\"reason_code\":\"INPUT_STALE\","
        "\"warning_code\":\"SENSOR_FAULT\",\"command_status\":\"ACCEPTED\","
        "\"override_left\":true,\"override_right\":false,\"input_validity\":{"
        "\"vehicle_speed\":\"INVALID\",\"gear\":\"STALE\",\"source_timestamp\":\"VALID\","
        "\"crash_status\":\"VALID\",\"rear_left_approach_risk\":\"VALID\","
        "\"rear_right_approach_risk\":\"VALID\",\"fire_detected\":\"VALID\","
        "\"overtemperature_detected\":\"VALID\",\"adult_present\":\"VALID\","
        "\"isofix_left\":\"VALID\",\"isofix_right\":\"VALID\",\"ignition_on\":\"VALID\","
        "\"sensor_fault\":\"VALID\"}}";
    EXPECT_EQ(expected, format(result));
}

/**
 * @test UT-102
 * @brief 모든 열거값은 이름으로, 범위 밖(음수, 개수와 같은 값)은 "UNKNOWN"으로 나온다 (FN-038).
 * @par 입력값
 * 잠금 0, 1, 2(범위 밖), 상태 0~3과 -1, 이유 0~14와 15, 경고 0, 1, 2, 명령 상태 0~2와 3, 입력 상태
 * 0~2와 3
 * @par 절차
 * 값마다 직렬화해 해당 키의 값을 확인
 * @par 예상 출력값
 * 범위 안은 요구사항 5.3절 이름, 범위 밖은 UNKNOWN
 */
TEST(StatusViewFormat, EnumNamesAndUnknown)
{
    const char *locks[] = {"RELEASE", "LOCK", "UNKNOWN"};
    for (int v = 0; v < 3; ++v)
    {
        lock_ctrl_result_t r = base_result();
        r.outputs.left = static_cast<lock_cmd_t>(v);
        EXPECT_TRUE(
            contains(format(r), (std::string("\"lock_left\":\"") + locks[v] + "\"").c_str()));
    }
    const char *states[] = {"NORMAL", "DEGRADED", "FAULT", "OFF"};
    for (int v = 0; v < 4; ++v)
    {
        lock_ctrl_result_t r = base_result();
        r.state = static_cast<ctrl_state_t>(v);
        EXPECT_TRUE(contains(format(r), (std::string("\"state\":\"") + states[v] + "\"").c_str()));
    }
    lock_ctrl_result_t negative = base_result();
    negative.state = static_cast<ctrl_state_t>(-1);
    EXPECT_TRUE(contains(format(negative), "\"state\":\"UNKNOWN\""));
    const char *reasons[] = {
        "NONE",          "CRASH_CONFIRMED",   "SENSOR_FAULT", "INPUT_INVALID",   "INPUT_STALE",
        "CRASH_PENDING", "IGNITION_OFF",      "FIRE",         "OVERTEMPERATURE", "ADULT_PRESENT",
        "APPROACH_RISK", "APPROACH_OVERRIDE", "ISOFIX",       "AUTO_LOCK",       "DRIVER_COMMAND"};
    for (int v = 0; v < 15; ++v)
    {
        lock_ctrl_result_t r = base_result();
        r.reason = static_cast<reason_t>(v);
        r.priority_reason = static_cast<reason_t>(v);
        const std::string text = format(r);
        EXPECT_TRUE(
            contains(text, (std::string("\"reason_code\":\"") + reasons[v] + "\"").c_str()));
        EXPECT_TRUE(
            contains(text, (std::string("\"priority_reason\":\"") + reasons[v] + "\"").c_str()));
    }
    lock_ctrl_result_t over = base_result();
    over.reason = static_cast<reason_t>(15);
    EXPECT_TRUE(contains(format(over), "\"reason_code\":\"UNKNOWN\""));
    const char *warnings[] = {"NONE", "SENSOR_FAULT", "UNKNOWN"};
    for (int v = 0; v < 3; ++v)
    {
        lock_ctrl_result_t r = base_result();
        r.warning = static_cast<warning_t>(v);
        EXPECT_TRUE(
            contains(format(r), (std::string("\"warning_code\":\"") + warnings[v] + "\"").c_str()));
    }
    const char *commands[] = {"NONE", "ACCEPTED", "REJECTED", "UNKNOWN"};
    for (int v = 0; v < 4; ++v)
    {
        lock_ctrl_result_t r = base_result();
        r.command_status = static_cast<command_status_t>(v);
        EXPECT_TRUE(contains(format(r),
                             (std::string("\"command_status\":\"") + commands[v] + "\"").c_str()));
    }
    const char *validities[] = {"VALID", "INVALID", "STALE", "UNKNOWN"};
    for (int v = 0; v < 4; ++v)
    {
        lock_ctrl_result_t r = base_result();
        r.input_validity[ITEM_SENSOR_FAULT] = static_cast<validity_t>(v);
        EXPECT_TRUE(contains(
            format(r), (std::string("\"sensor_fault\":\"") + validities[v] + "\"}}").c_str()));
    }
}

/**
 * @test UT-103
 * @brief override 상태는 문마다 true 또는 false로 나온다 (FN-039).
 * @par 입력값
 * override (false, false), (true, false), (false, true), (true, true)
 * @par 절차
 * 직렬화
 * @par 예상 출력값
 * "override_left":<값>,"override_right":<값>
 */
TEST(StatusViewFormat, OverrideFlags)
{
    const bool combos[4][2] = {{false, false}, {true, false}, {false, true}, {true, true}};
    for (const auto &c : combos)
    {
        lock_ctrl_result_t r = base_result();
        r.override_active[DOOR_LEFT] = c[0];
        r.override_active[DOOR_RIGHT] = c[1];
        const std::string expected = std::string("\"override_left\":") + (c[0] ? "true" : "false") +
                                     ",\"override_right\":" + (c[1] ? "true" : "false") + ",";
        EXPECT_TRUE(contains(format(r), expected.c_str()));
    }
}

/**
 * @test UT-104
 * @brief 버퍼 크기 경계: 필요 길이(널 포함) 이상은 성공, 1 작으면 STATUS_ERR_BUFFER와 빈 문자열
 * (FN-041).
 * @par 입력값
 * 크기 1부터 필요 길이 + 1까지
 * @par 절차
 * 크기마다 직렬화
 * @par 예상 출력값
 * 필요 길이 미만: STATUS_ERR_BUFFER, buffer[0]은 널 문자. 필요 길이 이상: STATUS_OK와 전체 문자열
 */
TEST(StatusViewFormat, BufferBoundary)
{
    const lock_ctrl_result_t result = base_result();
    const std::string full = format(result);
    const size_t needed = full.size() + 1;
    for (size_t size = 1; size <= needed + 1; ++size)
    {
        char buffer[2048];
        std::memset(buffer, 'x', sizeof(buffer));
        const status_t status = status_view_format(&result, buffer, size);
        if (size < needed)
        {
            EXPECT_EQ(STATUS_ERR_BUFFER, status);
            EXPECT_EQ('\0', buffer[0]);
        }
        else
        {
            EXPECT_EQ(STATUS_OK, status);
            EXPECT_EQ(full, std::string(buffer));
        }
    }
}
