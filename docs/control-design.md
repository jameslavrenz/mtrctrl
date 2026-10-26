# Control design

## Plant

The Phase 1 plant is the rigid-body mechanical equation

```text
J dw/dt = Tm - b w - Tl
```

with `w` in rad/s, torque in N·m, `J` in kg·m², and `b` in N·m·s/rad. The normalized command `u` is in [-1, 1]:

```text
Tm = u * T_scale
```

Integration is forward Euler at `dt = 0.01` s, the same period as the hardware timer:

```text
w[k+1] = w[k] + dt * (Tm - b w[k] - Tl) / J
```

`dt` in the controller is the float `0.01f`, which is not a binary 10 ms. The timer period is exact: the APB1 timer clock is 90 MHz and 90e6/100 = 900000. The float step and the timer are specified separately so a later change of sample rate does not silently change only one of them.

A step that is non-finite, or whose absolute speed would exceed 2500 RPM, is not written back. The previous speed remains the next measurement, and the fault policy latches overspeed or numerical.

### What is published data, and what is not

The FIT1035 data sheet and the DFRobot wiki give 12 V, 14 W, 0.8 A, 1800 RPM at 12 V, 7 pole pairs, and a star connection. They do not give rotor inertia or viscous friction.

From the published power and speed, at the rated point:

```text
w_rated = 1800 * 2π / 60 rad/s
T_rated = 14 W / w_rated
```

`T_scale` is `T_rated`, so `u = 1` is that torque. `b` is `T_rated / w_rated`, so with `Tl = 0` the full-scale torque balances at 1800 RPM. Both numbers are derived from the published rating. They are not a measured torque constant or a measured drag.

`J = 8.0e-5` kg·m² is illustrative. It is large enough that a 100 Hz loop shows a visible acceleration, and it includes a notional load. It is not the rotor inertia of the FIT1035. The simulation checks the software, not the motor.

Electrical dynamics, commutation, and the 7 pole pairs are not in this model. Pole pairs matter for Phase 2 electrical angle, not for a speed-only mechanical plant.

Overspeed is 2500 RPM, above the 1800 RPM setpoint limit, so a modest overshoot is not immediately a fault. Setpoint commands outside ±1800 RPM are rejected and leave the previous setpoint. Load commands outside ±0.05 N·m are rejected the same way.

## PID

The controller is parallel-form, discrete, and independent of FreeRTOS. Units are rad/s in, normalized torque out.

```text
e[k]  = setpoint - measurement
P     = Kp * e[k]
I[k]  = I[k-1] + Ki * e[k] * dt          if not saturated against e[k]
d[k]  = (measurement[k] - measurement[k-1]) / dt
d_f   = d_f + dt/(Tf + dt) * (d[k] - d_f)    when Tf > 0
D     = -Kd * d_f
u     = clamp(P + I + D, u_min, u_max)
```

Derivative acts on the measurement, with a minus sign, so a step in the setpoint does not feed a spike through `Kd`. The first sample after `reset()` forces `D` to 0 because there is no previous measurement.

Anti-windup is conditional integration. The integrator is updated only when the unsaturated sum is inside the limits, or is outside them in the direction the error would bring it back. A saturated output therefore stops accumulating, and a reversal of the error still unwinds it.

`set_gains` rejects non-finite values, negatives, and gains above the caps (`Kp ≤ 5`, `Ki ≤ 20`, `Kd ≤ 1`, `Tf ≤ 1` s). A rejected set leaves the previous gains. `update` returns false, and does not write the output, if the controller has no gains or if `dt` or the signals are not usable. The caller treats that as a numerical fault.

Default gains, used by the firmware and by the host closed-loop test:

```text
Kp = 0.012
Ki = 0.06
Kd = 0.0005
Tf = 0.03 s
```

They were chosen so a 1000 RPM step on the illustrative plant settles near the setpoint with a limited overshoot, and so a 0.02 N·m load is rejected. The host test checks that behavior. The gains are not claimed to be optimal, and they are not claimed to suit the physical motor. `Tf` is on the order of a few samples (30 ms at a 10 ms period), which is enough to keep the difference from amplifying one noisy sample once a real encoder rate exists.

Output limits in the firmware are ±1, matching the normalized torque command.

## Sample order

The control task reads speed, runs the PID, then steps the plant. The snapshot reports that same pre-step speed. Reporting the post-step speed would make `error_rpm` disagree with the error that produced `control_output` by one sample. Either convention is usable if it is fixed. This one is fixed so the telemetry error is the PID error.

Stopped mode still steps the plant with zero torque and a reset integrator, so a non-zero speed coasts down under friction and load instead of freezing. Faulted mode does not step. A fault is allowed to hold the last accepted speed so an overspeed rejection cannot be integrated again on the next sample.

Zero setpoint is a normal command. The PID regulates toward zero; it is not a special case that bypasses the integrator reset except through the usual saturation rules.

## What Phase 2 changes

The outer speed PID can stay at a rate near 100 Hz. Rotor-angle commutation or FOC cannot. The FIT1035's electrical frequency at 1800 RPM with 7 pole pairs is 210 Hz, and a current loop has to run well above that. The Phase 1 rate is the speed loop only. [phase2-hardware.md](phase2-hardware.md) separates the two rates.

The PID will then regulate speed estimated from the AS5600, and its output will be a torque or current reference for an inner loop, not a duty cycle written straight to the three phase pins. The normalized ±1 limit is the place that current limit will be applied. It is not a current limit today.
