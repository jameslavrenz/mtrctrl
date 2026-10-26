#include "app.hpp"

#include "mtr/control_loop.hpp"
#include "mtr/deadline_injection.hpp"
#include "mtr/fault_policy.hpp"
#include "mtr/simulated_motor.hpp"
#include "mtr/units.hpp"

#include "queue.h"
#include "stm32f4xx_hal.h"

namespace mtr {
namespace {

void apply_command(SpeedControlLoop& loop, SimulatedMotor& motor, FaultPolicy& fault, const CommandMessage& message,
                   DeadlineInjection& injection) {
    switch (message.kind) {
        case CommandKind::Start:
            (void)fault.start();
            break;
        case CommandKind::Stop:
            fault.stop();
            loop.reset_pid();
            injection.cancel();
            break;
        case CommandKind::ResetPid:
            loop.reset_pid();
            break;
        case CommandKind::ResetFaults:
            fault.reset_faults();
            motor.set_feedback_valid(true);
            injection.cancel();
            break;
        case CommandKind::SetSpeed:
            loop.set_setpoint_rpm(message.value);
            break;
        case CommandKind::SetKp:
            (void)loop.set_kp(message.value);
            break;
        case CommandKind::SetKi:
            (void)loop.set_ki(message.value);
            break;
        case CommandKind::SetKd:
            (void)loop.set_kd(message.value);
            break;
        case CommandKind::SetLoad:
            loop.set_load_nm(message.value);
            break;
        case CommandKind::InjectDeadline:
            injection.arm_if_running(fault.state() == RunState::Running);
            break;
        case CommandKind::InjectFeedback:
            motor.set_feedback_valid(false);
            break;
        default:
            break;
    }
}

void burn_cycles(uint32_t cycles) {
    const uint32_t start = dwt_cycles();
    while ((dwt_cycles() - start) < cycles) {
    }
}

}  // namespace

void control_task(void* argument) {
    (void)argument;
    SimulatedMotor motor;
    SpeedControlLoop loop(motor);
    FaultPolicy fault;
    control_clock_start(xTaskGetCurrentTaskHandle());

    uint32_t last_overflows = 0;
    DeadlineInjection injection;
    uint32_t sample_count = 0;
    uint32_t execution_max = 0;
    uint32_t wake_max = 0;
    uint32_t response_max = 0;
    int32_t jitter_abs_max = 0;
    const uint32_t period_cycles = SystemCoreClock / kControlHz;

    while (true) {
        const uint32_t releases = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        const uint32_t started = dwt_cycles();
        const uint32_t isr_stamp = control_clock_last_isr();
        const bool jitter_valid = control_clock_jitter_valid();
        const int32_t jitter_cycles = control_clock_last_jitter();

        if (app().safe_request.exchange(0, std::memory_order_acq_rel) != 0u) {
            fault.on_deadline_misses(FaultPolicy::kDeadlineTrip);
        }
        if (releases == 0u) {
            fault.on_deadline_misses(1);
        } else if (releases > 1u) {
            fault.on_deadline_misses(releases - 1u);
        }

        const uint32_t overflows = app().queue_overflows.load(std::memory_order_relaxed);
        fault.on_queue_overflows(overflows - last_overflows);
        last_overflows = overflows;

        CommandMessage message{};
        while (xQueueReceive(app().commands, &message, 0) == pdTRUE) {
            apply_command(loop, motor, fault, message, injection);
        }

        if (injection.consume()) {
            burn_cycles(25000u * (SystemCoreClock / 1000000u));
        }

        loop.sample(kControlDtS, fault);

        const uint32_t finished = dwt_cycles();
        const uint32_t execution = finished - started;
        const uint32_t wake = releases == 0u ? 0u : (started - isr_stamp);
        const uint32_t response = releases == 0u ? 0u : (finished - isr_stamp);
        fault.on_exec_overrun(execution > period_cycles);
        if (execution > execution_max) {
            execution_max = execution;
        }
        if (releases != 0u && wake > wake_max) {
            wake_max = wake;
        }
        if (releases != 0u && response > response_max) {
            response_max = response;
        }
        if (jitter_valid) {
            const int32_t absolute = jitter_cycles < 0 ? -jitter_cycles : jitter_cycles;
            if (absolute > jitter_abs_max) {
                jitter_abs_max = absolute;
            }
        }

        ControlSnapshot snapshot{};
        snapshot.timestamp_ms = xTaskGetTickCount();
        snapshot.sample_count = ++sample_count;
        snapshot.setpoint_rpm = loop.setpoint_rpm();
        snapshot.actual_rpm = loop.actual_rpm();
        snapshot.error_rpm = loop.error_rpm();
        snapshot.control_output = loop.output();
        snapshot.load_torque_nm = loop.load_nm();
        snapshot.execution_us = cycles_to_us(execution);
        snapshot.execution_max_us = cycles_to_us(execution_max);
        snapshot.wake_latency_us = cycles_to_us(wake);
        snapshot.wake_latency_max_us = cycles_to_us(wake_max);
        snapshot.release_jitter_us = jitter_valid ? cycles_to_us_signed(jitter_cycles) : 0;
        snapshot.release_jitter_abs_max_us = cycles_to_us_signed(jitter_abs_max);
        snapshot.response_us = cycles_to_us(response);
        snapshot.response_max_us = cycles_to_us(response_max);
        snapshot.deadline_misses = fault.deadline_misses();
        snapshot.exec_overruns = fault.exec_overruns();
        snapshot.queue_overflows = fault.queue_overflows();
        snapshot.fault_flags = fault.flags();
        snapshot.state = static_cast<uint32_t>(fault.state());
        snapshot.feedback_valid = loop.feedback_valid() ? 1u : 0u;
        app().published.write(snapshot);
    }
}

}  // namespace mtr
