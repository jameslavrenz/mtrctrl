# Phase 2 hardware

For the detailed mathematical theory and embedded implementation plan, see the [voltage-mode FOC technical note](foc-voltage-mode-technical-note.pdf) ([LaTeX source](foc-voltage-mode-technical-note.tex)). The note is prospective design guidance; no physical FOC/PWM/encoder integration has been validated in this repository.

This phase is not implemented. There is no FOC, no encoder driver, no phase PWM, and no use of the driver's enable, reset, sleep, or fault pins. The notes below are what the next integration has to decide, written so it does not disturb the speed PID or the task layout without a reason.

## Parts

### Motor, DFRobot FIT1035 (DigiKey 1738-FIT1035-ND)

Published data used here:

| Item | Value |
| --- | --- |
| Rated voltage | 12 V |
| Rated power | 14 W |
| Rated current | 0.8 A |
| Rated speed | 1800 RPM at 12 V |
| Poles | 7 pole pairs |
| Winding | star |
| Encoder | AS5600, 12-bit absolute angle, I²C, 3.3 V |

Rated torque `T = P / ω` is about 0.074 N·m. That is a derived rating, not a stall-torque measurement. Rotor inertia, torque constant, resistance, and inductance are not in the manufacturer's public data. Do not fill them in with plausible numbers and then treat a model as identification.

Wiki: https://wiki.dfrobot.com/fit1035/

### Driver, DFRobot DRI0058 SimpleFOCmini (DigiKey 1738-DRI0058-ND)

The board is a three-phase gate driver and inverter, not a motion controller. The STM32 has to produce the phase waveforms.

Published limits that matter:

| Item | Value |
| --- | --- |
| Motor supply | 8–30 V |
| Current | 2.5 A maximum per channel |
| Logic | 3.3 V or 5 V |
| Inputs | IN1, IN2, IN3, enable, reset, sleep |
| Fault | active-low output |
| Outputs | M1, M2, M3 |

Wiki: https://wiki.dfrobot.com/dri0058

The motor's rated current is 0.8 A, which is under 2.5 A, but the motor can draw more than rated current at start and at stall. Nothing in the published motor data says the stall current is below the driver's per-channel maximum. Phase 2 cannot treat "rated 0.8 A, driver 2.5 A" as proof the combination is safe at any duty cycle or any supply. A current limit, a supply choice, and a check of the driver's fault pin have to be established before the shaft is allowed to run unattended. This repository does not establish them.

## Voltage-mode FOC

The DRI0058 is the gate driver for voltage-mode field-oriented control, the SimpleFOC torque mode called `voltage`. The STM32 reads the FIT1035's AS5600, forms the electrical angle (mechanical angle times 7 pole pairs, plus the alignment offset), and applies a q-axis voltage `Uq` from the speed PID. `Ud` stays 0. Inverse Park and inverse Clarke produce three phase voltages. Each voltage is a PWM duty on IN1, IN2, or IN3, centered at 50% and scaled by the motor-bus voltage.

That is the voltage-control variety of FOC. Current-mode FOC needs phase-current samples. The DRI0058's published inputs are the three PWM pins, enable, reset, sleep, and the fault output. It does not provide those current samples, so `dc_current` and `foc_current` are not the plan for this pair of boards.

The PWM carrier has to be well above the 210 Hz electrical frequency at 1800 RPM. Which timer, which pins, and which carrier frequency are still wiring and bring-up choices. They are not selected in this tree. The 100 Hz FreeRTOS speed task publishes `Uq`. It does not run the Park transform or write the three duties. A faster timer, or the PWM update interrupt, releases that work.

## What has to be built

1. AS5600 on I²C at 3.3 V. The F446 has I²C peripherals; which pins, and whether the encoder's default address conflicts with anything else on the bus, is a wiring decision.
2. A position sample and a speed estimate. The outer loop should not block on I²C. A dedicated sample, or DMA, has to finish well inside the speed period.
3. Electrical angle: mechanical angle times 7 pole pairs, plus the offset found at alignment.
4. An alignment move at low current before closed-loop commutation. That current is part of the limit discussion above.
5. Three PWM channels for IN1–IN3, plus GPIO for enable, reset, and sleep, and an input for the fault pin.
6. The voltage-mode FOC update. Inverse Park and inverse Clarke from `Uq`, `Ud = 0`, and the electrical angle, then three PWM duties for IN1–IN3. The DRI0058 does not do that modulation.
7. The existing speed PID, with its output interpreted as the q-axis voltage, still clamped. The clamp is a voltage limit. It is not an ampere limit.
8. Two rates. The speed loop can stay near 100 Hz. The electrical frequency at 1800 RPM and 7 pole pairs is 210 Hz, so the inner loop has to be much faster than both. TIM7's 100 Hz notification is the wrong release for the inner loop. A faster timer, or a PWM update interrupt, should release that work. It should not run inside the speed task.
9. Enable sequencing: sleep and reset released, fault checked, alignment, then enable. The reverse on stop and on fault.
10. A supply for the motor power input, separate from the Nucleo's USB 5 V. The motor is a 12 V, 14 W part. USB cannot feed it.
11. A written current, voltage, and thermal limit, and a test that the fault pin actually stops the PWM.

## What should stay

| Keep | Replace or extend |
| --- | --- |
| `PidController` and the gain, saturation, and anti-windup rules | The meaning of ±1, from "normalized torque in the model" to "q-axis voltage `Uq`" |
| Control, command, telemetry, and supervisor tasks | A faster inner-loop release, not a fifth copy of the speed task |
| Command queue and the snapshot seqlock | Feedback: `MotorPlant::speed_rad_s` and `feedback_valid` filled from the encoder estimate |
| Fault policy and `reset faults` | Driver fault and overcurrent as additional trip bits, and a GPIO action that disables the bridge |
| 100 Hz outer sample order (read, compute, apply) | Apply becomes "publish `Uq`" rather than "Euler step" |

`SimulatedMotor` can remain in the tree behind a compile-time choice so the console and the supervisor still run with no inverter on the bench. That choice should be a single translation unit, not a class hierarchy.

## Bring-up order

Do these in order. Skipping to "spin the motor" skips the checks that tell you the encoder and the bridge are the devices you think they are.

1. I²C read of the AS5600 angle with the shaft turned by hand. Confirm 12-bit wrap and the 3.3 V supply current.
2. Enable, reset, sleep, and fault wiring with the motor supply off. Confirm the fault input reads the idle level the data sheet specifies.
3. Motor supply on, PWM off, fault still clear. Measure supply current at idle.
4. A low-duty alignment pulse on one phase, current limited, shaft free, and confirm the encoder moves to a repeatable offset. Record the offset.
5. Open-loop rotation at low speed and low current. The angle should advance by 7 electrical cycles per mechanical revolution.
6. Only then close the speed PID around the encoder speed estimate, with a small setpoint and the same current clamp.
7. Compare a step response with the Phase 1 log format so the telemetry columns do not change meaning. `actual_rpm` becomes measured speed. The timing columns stay measurements of the STM32.

## Safety

Phase 1's faulted state zeros a variable in a differential equation. It does not open a contactor and it does not stop a PWM. Calling that a safe state for the FIT1035 would be false. The Phase 2 fault path has to drive the driver's enable or sleep into the state that turns the bridge off, and it has to do that from the inner-loop context, not from the 1 Hz-class supervisor. The supervisor can log the fault. It is too slow to be the only thing that removes gate drive.

The Nucleo ground, the encoder ground, and the driver logic ground have to be one ground. The motor supply return follows the driver's power layout, not a breadboard jumper rated as an afterthought. None of that wiring is in this repository, and none of it has been checked.
