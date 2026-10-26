# Verification and validation

Host tests and the firmware link are results from this workspace. Board observations are only the ones listed under "What the board showed." Nothing in the timing tables was filled in from a model or from a guess.

## Host tests

`make test` builds `mtr_logic` with `-fno-exceptions -fno-rtti` and links it into `mtr_unit_tests` with ASan and UBSan. The test binary does not include the HAL, CMSIS device headers, or FreeRTOS. The runner is a small assertion harness in `tests/`, not GoogleTest, so the tests build without a network fetch.

The last local run reported:

```text
1/2 Test #1: unit .............................   Passed    0.20 sec
2/2 Test #2: telemetry_columns ................   Passed    0.03 sec
100% tests passed out of 2
```

The cases cover:

| Area | What is checked |
| --- | --- |
| PID | P, I, and D against exact binary values; derivative on measurement; first-sample D forced to 0; conditional integration; saturation at the gain cap; reset; rejected gains (negative, non-finite, above the cap) leaving the previous gains |
| Plant | Euler step direction; load torque; overspeed not committed; non-finite input; parameter rejection |
| Closed loop | 1000 RPM step within 20 RPM by 3 s and peak under 1200 RPM; recovery after 0.02 N·m; coast after stop; feedback loss holds speed; zero gains do not track; a tiny inertia trips overspeed |
| Fault policy | start rejected while faulted; stop ignored while faulted; reset returns to stopped; overrun streak clears without clearing the latch; queue threshold |
| Parser | ranges, unknown commands, over-long lines, the help text |
| Text format | integer, lowercase hex, and fixed-point append, including `INT32_MIN` and the fault-flag hex form |
| Deadline injection | an armed spin keeps running after the deadline latch, so the overrun latch still trips; `stop` and `reset faults` cancel the remaining spins |
| Telemetry columns | one fixture row matches the formatter, and the host scripts take column indexes from `src/mtr/telemetry_row.hpp` |

`cppcheck` 2.22, with the suppressions in the Makefile (`shadowFunction`, `functionStatic`, `constParameterPointer`, missing system includes), completed with no findings on `src/`, `firmware/app`, `firmware/platform`, and `firmware/rtos`. Third-party sources are not analyzed. Getter/parameter name clashes are suppressed because renaming them to satisfy cppcheck would make the PID API worse. HAL callback signatures are fixed by the HAL headers.

## Firmware build

`make firmware` cross-compiles with the toolchain selected by `tools/find-arm-cxx.sh`. The last local link:

```text
FLASH: 60552 B of 512 KB (11.55%)
RAM:   18408 B of 128 KB (14.04%)
text 60060, data 480, bss 17928
```

The RAM total includes the 4 KiB main-stack reservation. Symbols present in that image include `main`, `Reset_Handler`, `SVC_Handler`, `PendSV_Handler`, `SysTick_Handler`, `HAL_InitTick`, `TIM7_IRQHandler`, and `USART2_IRQHandler`.

This link is not evidence that the control loop meets a deadline.

## What the board showed

The board on the bench was a NUCLEO-F446RE. OpenOCD 0.12.0 reported DBGMCU IDCODE `0x10006421` (DEV_ID `0x421`), 512 KiB of flash, and a target voltage of 3.248 V on the run below. Connecting without asserting NRST failed at 1800 kHz and at 2000 kHz (`init mode failed`). `reset_config srst_only srst_nogate connect_assert_srst` at an adapter speed of 480 kHz programmed and verified `mtrctrl.elf`.

The on-board ST-Link was updated from V2J33M25 to V2J47M34. On V2J33M25 the MCU transmitted the banner and host bytes never reached PA3: a diagnostic image, not left in the tree, sampled PA3 as a GPIO input for 0.5 s while the host sent 512 zero bytes and recorded `lows 0`. USART status stayed at TXE and TC. The same one-way port also failed an independent USART2 image. After the probe update, `help` returns the command list. The numbers in this section are from `python3 tools/hwtest.py` against that probe, ST-Link V2J47M34, on the image whose link sizes are above. The script resets the board through OpenOCD, waits for the banner, and then drives the command sequence. It passed.

After reset the console produced:

```text
mtrctrl NUCLEO-F446RE
FreeRTOS V11.1.0+
SYSCLK 180000000 Hz
control 100 Hz simulated plant
phase 1: no physical motor
type help
```

That confirms the PLL locked at the frequency `FreeRTOSConfig.h` assumes, the kernel started, and USART2 transmit on PA2 works. `V11.1.0+` is the unmodified kernel version string. `help` was answered with the command list and `ok`.

### Nominal 1000 RPM window

`start`, then `set speed 1000`. Status while running, before telemetry:

```text
state running
setpoint_rpm 1000.00
actual_rpm 1000.04
fault_flags 0x0
deadline_misses 0
```

The script then kept 12 complete telemetry rows. Every row had simulated actual speed `1000.00` RPM, control output `0.556`, load torque `0.0000`, and these measured columns:

| Column | Min | Max |
| --- | --- | --- |
| execution_us | 11 | 11 |
| wake_latency_us | 6 | 6 |
| release_jitter_us | 0 | 0 |
| response_us | 18 | 18 |
| deadline_misses | 0 | 0 |
| exec_overruns | 0 | 0 |
| queue_overflows | 0 | 0 |
| cpu_percent | 0 | 0 |

A jitter of 0 µs is the integer truncation of a sub-microsecond timer error at 180 MHz. Response is wake plus execution, not a third independent latency. In this window the deadline was met: the miss counter stayed 0 and no sample overran the 10 ms period.

`set load 0.02` left the state running. The following status reported `actual_rpm 1000.21`, `fault_flags 0x0`, and `deadline_misses 0`. The 0.02 N·m load was not cleared afterward, so later speed readings include that disturbance. `stop` then reported `state stopped` and `actual_rpm -419.89`.

### Fault injection

`status` prints `fault_flags` in hex. The telemetry CSV prints that same mask as a decimal integer, so `0x21` in status is `33` in a `tel` row.

`fault inject feedback` reported `state faulted` and `fault_flags 0x2` (`kFaultFeedback`). The next `start` was answered `error: faulted`. `reset faults` returned `state stopped` and `fault_flags 0x0`.

`fault inject deadline`, while running, reported `state faulted`, `fault_flags 0x21` (`kFaultDeadline` together with `kFaultOverrun`), and `deadline_misses 7`. `reset faults` cleared the latches. The `stats` snapshot after that reset is the high-water view, so the miss and overrun counters in it are 0. The maxima still include the 25 ms spins:

```text
execution_us 8
execution_max_us 25015
wake_latency_us 6
wake_latency_max_us 5129
release_jitter_us 0
release_jitter_abs_max_us 0
response_us 15
response_max_us 30092
exec_overruns 0
queue_overflows 0
cpu_percent 12
stack_min_free_words control 359 command 518 supervisor 291 telemetry 320 load 220
```

`execution_max_us 25015` is one injected spin, not the nominal body. `response_max_us 30092` and `wake_latency_max_us 5129` are from that same injected stretch. The nominal window above is the steady-state measurement.

### CPU load

`load on` was issued while the controller was already stopped. Telemetry during that busy loop reported `cpu_percent` 57 on the first complete row and 100 on the rest. The script's scored window was 100 to 100. Those rows stayed at `deadline_misses 0`, `execution_us 8`, and `state` stopped. The load task is what filled the idle time. It is not a measurement of the running speed loop.

## How to repeat it

```sh
make flash
python3 tools/hwtest.py
python3 tools/plot_telemetry.py build/fw/hwtest-telemetry.csv -o build/fw/hwtest-telemetry.png
```

The script writes `build/fw/hwtest.log` and `build/fw/hwtest-telemetry.csv`. The plot from the nominal CSV of this run is `build/fw/hwtest-telemetry.png`. Read the columns as defined in [freertos-design.md](freertos-design.md).

| Run | Commands | What this run showed |
| --- | --- | --- |
| Nominal | `start`, `set speed 1000`, telemetry on | 12 samples, misses 0, execution 11 µs |
| Load | `set load 0.02` while running | stayed running, actual 1000.21 RPM |
| Console load | `load on`, then telemetry | cpu_percent reached 100 while stopped |
| Deadline | `fault inject deadline` while running | flags `0x21`, 7 misses |
| Feedback | `fault inject feedback` | flags `0x2`, `start` rejected, reset returns to stopped |

## Continuous integration

`.github/workflows/ci.yml` has three jobs: host tests, Arm cross-compile, and cppcheck. None of them open a serial port or start OpenOCD. The workflow runs on every push to GitHub. Local `make test`, `make firmware`, and `make cppcheck` are the runs that happened on this machine. The last local `make cppcheck` completed with no findings.

## Limits of what Phase 1 can claim

The host closed-loop test and the board run both exercise the discrete controller and the illustrative plant. Neither is an identification of the FIT1035, and neither shows that a physical motor would track 1000 RPM. The timing numbers above are measurements on this Nucleo with the simulated plant. They are not a worst-case execution-time proof, and they are not a claim about PWM, current, or a real encoder.
