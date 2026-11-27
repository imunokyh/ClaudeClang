// lock_ctrl 단위 테스트용 하위 컴포넌트 스텁 (IF-001 input_guard, IF-002 arbiter).
#include "stub_control.h"

#include "arbiter/arbiter_if.h"
#include "input_guard/input_guard_if.h"

StubControl g_stub;

void stub_reset()
{
    g_stub = StubControl();
    g_stub.guard_init_status = STATUS_OK;
    g_stub.update_status = STATUS_OK;
    g_stub.check_status = STATUS_OK;
    g_stub.decide_status = STATUS_OK;
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        g_stub.normalized.validity[i] = VALIDITY_VALID;
        g_stub.normalized.value[i] = 0;
    }
    g_stub.checked = {COMMAND_NONE, 0, 0};
    g_stub.decision.outputs = {LOCK_RELEASE, LOCK_RELEASE};
    g_stub.decision.state = STATE_NORMAL;
    g_stub.decision.reason = REASON_NONE;
    g_stub.decision.warning = WARNING_NONE;
    g_stub.decision.approach = {};
}

extern "C"
{

status_t input_guard_init(input_guard_t *guard)
{
    (void)guard;
    return g_stub.guard_init_status;
}

status_t input_guard_update(input_guard_t *guard, uint32_t now_ms, const raw_snapshot_t *snapshot,
                            normalized_inputs_t *out)
{
    (void)guard;
    (void)snapshot;
    g_stub.update_calls++;
    g_stub.last_update_now_ms = now_ms;
    if (g_stub.update_status == STATUS_OK)
    {
        *out = g_stub.normalized;
    }
    return g_stub.update_status;
}

status_t input_guard_check_command(const raw_command_t *command, checked_command_t *out)
{
    (void)command;
    g_stub.check_calls++;
    if (g_stub.check_status == STATUS_OK)
    {
        *out = g_stub.checked;
    }
    return g_stub.check_status;
}

status_t arbiter_decide(const arbiter_input_t *input, decision_t *out)
{
    g_stub.decide_calls++;
    g_stub.last_previous = input->previous;
    g_stub.last_command = input->command;
    g_stub.last_decide_now_ms = input->now_ms;
    g_stub.last_approach_in = input->approach;
    if (g_stub.decide_status == STATUS_OK)
    {
        *out = g_stub.decision;
    }
    return g_stub.decide_status;
}

} // extern "C"
