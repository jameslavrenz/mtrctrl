# mtrctrl

A FreeRTOS speed controller for a brushless DC motor on the ST NUCLEO-F446RE. Phase 1 runs the PID and a simulated mechanical plant on the STM32F446, so the scheduler, the fault policy, and the console can be tested with only the Nucleo and a USB cable. The motor, the encoder, and the driver are the Phase 2 hardware below. That phase is specified in [docs/phase2-hardware.md](docs/phase2-hardware.md) and is not implemented: this tree has no FOC, no phase PWM, and no encoder driver.

The application calls the FreeRTOS kernel directly (`xTaskCreateStatic`, task notifications, queues, a mutex). It does not use CMSIS-RTOS.

## Quick start

Host checks need CMake, Ninja, Python 3, and a C++20 compiler. The firmware needs an `arm-none-eabi-g++` that can see C++ headers, and a connected board needs OpenOCD.

```sh
make test
make firmware
make flash
python3 tools/hwtest.py
```

`make check-target` flashes and then runs the board script. Neither step runs in CI.

Homebrew's GCC 16 bottle is built without C++ headers. `tools/find-arm-cxx.sh` falls back to the STM32CubeIDE toolchain when that is installed. `cppcheck` is optional (`make cppcheck`). Plotting needs `matplotlib` (`pip install -r tools/requirements.txt`).

## Hardware

Three boards. Only the Nucleo is used by the firmware in this repository.

| Role | Part | What it contributes |
| --- | --- | --- |
| Dev board | ST NUCLEO-F446RE (MB1136) | STM32F446RET6, Cortex-M4F, 180 MHz, 512 KiB flash, 128 KiB SRAM. ST-Link, SWD, and the virtual COM port |
| Motor | DFRobot FIT1035 (DigiKey 1738-FIT1035-ND) | 12 V, 14 W, 0.8 A, 1800 RPM, 7 pole pairs, star winding. The AS5600 absolute encoder is built into the motor |
| Driver | DFRobot DRI0058 SimpleFOCmini (DigiKey 1738-DRI0058-ND) | Three-phase gate driver and inverter. Motor supply 8–30 V, 2.5 A maximum per channel. Logic at 3.3 V or 5 V |

The FIT1035's sensor is an ams OSRAM AS5600: 12-bit absolute angle, I²C, 3.3 V. It is part of the motor, not part of the DRI0058. The DRI0058 does not close a control loop and it does not measure phase current. Its inputs are IN1, IN2, IN3, enable, reset, and sleep, plus an active-low fault output. The phase outputs are M1, M2, and M3. The STM32 has to produce the three PWM waveforms. Published notes and the bring-up order are in [docs/phase2-hardware.md](docs/phase2-hardware.md). Product pages: [FIT1035](https://wiki.dfrobot.com/fit1035/) and [DRI0058](https://wiki.dfrobot.com/dri0058).

On the Nucleo today:

| Item | Value |
| --- | --- |
| Console | ST-Link virtual COM port, USART2 on PA2 (TX) and PA3 (RX), 115200 8N1 |
| Status LED | LD2, PA5 |
| Debug | SWD through CN2, both jumpers fitted |

The console is the ST-Link USB serial port wired to USART2.

Factory solder bridges for that serial port are SB13 and SB14 fitted (0 Ω) and SB62 and SB63 open. That is the configuration that connects the ST-Link to PA2 and PA3. Closing SB62 and SB63 disconnects the ST-Link and routes those pins to the Arduino D1 and D0 headers. Leave SB62 and SB63 open. CN3 is an unpopulated header on the same nets, useful as test points. JP5 at U5V and JP6 fitted are the usual USB-powered setup.

On the board used for bring-up, ST-Link firmware V2J33M25 transmitted the banner and did not deliver host bytes to PA3. Updating that probe to V2J47M34 restored receive, and `help` then got a reply. The updater shipped with STM32CubeIDE is `STLinkUpgrade.jar` (`java -jar STLinkUpgrade.jar -update` against the plugged-in board). OpenOCD on this probe has to connect under reset at 480 kHz. The settings are in `firmware/openocd/nucleo_f446re.cfg`.

The motor supply is separate from the Nucleo's USB 5 V. The FIT1035 is a 12 V, 14 W motor. USB does not feed it. No motor supply, current limit, or driver-fault check has been established in this repository.

## FreeRTOS

FreeRTOS-Kernel V11.3.2 is vendored under `third_party/` and called directly. `USE_RTOS` in the HAL stays 0. The banner prints the kernel's own version macro, which in that tag is still `V11.1.0+`.

The kernel is here so the 100 Hz speed sample, the command parser, and the 10 Hz log can block independently on one UART. A PID loop does not require a scheduler. A timer interrupt plus a main loop would also be a valid motor controller, and [docs/freertos-design.md](docs/freertos-design.md) says why this project did not do that. Priorities, the TIM7 notification, the command queue, the TX mutex, and the deadline counters are in that note. The task table below is the Phase 1 map.

The future voltage-mode FOC update does not belong in the 100 Hz task. That task stays the outer speed loop. The PWM carrier, or a faster timer, releases the electrical update. The design note for the two rates is in [docs/phase2-hardware.md](docs/phase2-hardware.md).

## Voltage-mode FOC and PWM

Phase 1 commands a normalized torque inside the simulated plant. It does not configure a timer for motor PWM, and it does not transform a rotor angle into phase voltages.

The hardware path is voltage-mode field-oriented control, the same idea as SimpleFOC torque mode `voltage`. It is the variety that matches the DRI0058: three PWM inputs and no phase-current measurement. Current-mode FOC (`dc_current` or `foc_current`) needs phase-current samples this driver does not provide, so it is not the plan.

One electrical update, once Phase 2 exists, is:

1. Read the AS5600 mechanical angle over I²C. Estimate speed from that angle for the outer loop. Do not block the speed task on the bus transfer.
2. Electrical angle is mechanical angle times 7 pole pairs, plus the offset found during alignment.
3. The existing speed PID runs near 100 Hz. Its output is the q-axis voltage `Uq`. The d-axis voltage `Ud` stays 0. This scheme has no field weakening.
4. Inverse Park and inverse Clarke turn `Ud`, `Uq`, and the electrical angle into three phase voltages.
5. Each phase voltage becomes a duty cycle on a center-aligned PWM channel, around a 50% zero, scaled by the motor-bus voltage and clipped. Those three duties drive IN1, IN2, and IN3.
6. Enable, reset, and sleep are GPIO. The fault pin stops the PWM. The carrier frequency has to sit well above the electrical frequency, which is 210 Hz at 1800 RPM with 7 pole pairs. The carrier rate itself is still a Phase 2 choice. It is not a number in this tree.

The 100 Hz PID output is therefore a voltage command, not a duty written straight to the phase pins and not a current in amperes. The ±1 clamp in Phase 1 is the normalized torque of the simulated plant. In voltage mode that same clamp is where the q-axis voltage limit will sit. A real current limit still has to be chosen, because voltage mode does not measure current and the motor can exceed its 0.8 A rating at stall.

## What the firmware does

Four tasks, plus a low-priority CPU-load task used only for a stress command:

| Task | Priority | Role |
| --- | --- | --- |
| control | 5 | 100 Hz PID and plant step, released by TIM7 |
| command | 4 | USART2 command line |
| supervisor | 3 | Fault and stall watch, stack and CPU figures |
| telemetry | 2 | 10 Hz CSV on the same console |
| load | 1 | Optional busy loop, below everything else |
| idle | 0 | FreeRTOS idle task |

Higher numbers are higher priority. Time slicing is off. TIM7 notifies the control task from its ISR. TIM6 is the HAL tick and does not call FreeRTOS. SysTick belongs to the kernel.

The plant is

```text
J dw/dt = Tm - b w - Tl
```

integrated with forward Euler at 10 ms. `J` is an illustrative inertia. `Tm` and `b` are derived from the FIT1035's published 14 W and 1800 RPM, not from a measurement of this motor. Default gains are `Kp = 0.012`, `Ki = 0.06`, `Kd = 0.0005`, `Tf = 0.03` s. Details are in [docs/control-design.md](docs/control-design.md).

## Build

```sh
make test       # host unit tests, ASan and UBSan, no HAL and no FreeRTOS
make firmware   # build/fw/firmware/mtrctrl.elf
make cppcheck   # src/, firmware/app, firmware/platform, firmware/rtos
make flash      # rebuild and program the connected board
make hwtest     # board procedure; the board must already be running this image
make clean
```

`make test` builds `build/host`. The last local link reported 60552 bytes of flash (11.55% of 512 KiB) and 18408 bytes of SRAM (14.04% of 128 KiB), including a 4 KiB main-stack reservation.

`MTR_PORT` selects the serial device for `tools/hwtest.py` (default: the first `/dev/cu.usbmodem*` or `/dev/ttyACM*`). `MTR_LOG_DIR` selects where the log and CSV are written (default: `build/fw`). `OPENOCD` overrides the OpenOCD binary.

## Serial console

115200 8N1, no flow control. On macOS the device is `/dev/cu.usbmodem*`. Opening the port often resets the Nucleo. The banner looks like this:

```text
mtrctrl NUCLEO-F446RE
FreeRTOS V11.1.0+
SYSCLK 180000000 Hz
control 100 Hz simulated plant
phase 1: no physical motor
type help
```

`V11.1.0+` is the string in the FreeRTOS V11.3.2 sources. The tag and that string disagree; the tree does not patch it.

A line is at most 80 characters and ends with CR or LF. Replies are `ok` or `error: ...`.

| Command | Effect |
| --- | --- |
| `help` | Print the command list |
| `status` | One snapshot: state, speed, torque command, fault mask, misses |
| `stats` | `status` plus execution, wake, jitter, response, CPU, and stack free words |
| `start` | Run the 100 Hz loop. Rejected while faulted (`error: faulted`) |
| `stop` | Coast at zero torque and reset the PID. Ignored while faulted |
| `set speed <rpm>` | Setpoint, −1800..1800. Out of range: `error: speed out of range` |
| `set kp <gain>` | 0..5 |
| `set ki <gain>` | 0..20 |
| `set kd <gain>` | 0..1 |
| `set load <N.m>` | Simulated load torque, −0.05..0.05 |
| `reset pid` | Clear integrator and derivative state |
| `reset faults` | Clear latches and return to stopped. Does not start the loop |
| `telemetry on` / `telemetry off` | 10 Hz CSV |
| `load on` / `load off` | Busy-loop the priority-1 task |
| `load priority <1-4>` | Change that task's priority. It yields, so it cannot lock out a peer |
| `fault inject deadline` | While running, five 25 ms spins inside the measured control body. Both the deadline and overrun latches trip. Rejected otherwise (`error: not running`) |
| `fault inject feedback` | Mark plant feedback invalid. Immediate feedback fault |

Other failures: `error: unknown command`, `error: invalid number`, `error: command too long`, `error: too many arguments`, `error: command queue full`. A rejected `set` leaves the previous value.

`state` in telemetry is 0 stopped, 1 running, 2 faulted. `status` prints the fault mask in hex. The CSV prints the same mask as a decimal integer. The bits are in [docs/architecture.md](docs/architecture.md).

Telemetry is CSV at 10 Hz after a comment header. One row:

```text
tel,<timestamp_ms>,<setpoint_rpm>,<actual_rpm>,<error_rpm>,<control_output>,<load_torque_nm>,<execution_us>,<wake_latency_us>,<release_jitter_us>,<response_us>,<deadline_misses>,<exec_overruns>,<queue_overflows>,<fault_flags>,<state>,<feedback_valid>,<cpu_percent>
```

`setpoint_rpm`, `actual_rpm`, `error_rpm`, `control_output`, and `load_torque_nm` come from the simulated plant. The timing columns, `timestamp_ms`, the miss and overflow counters, and `cpu_percent` are measurements on the STM32.

Plot a captured log on the computer, not on the MCU:

```sh
pip install -r tools/requirements.txt
python3 tools/plot_telemetry.py build/fw/hwtest-telemetry.csv -o build/fw/hwtest-telemetry.png
```

The last board run's numbers, including stack free words and the fault-injection masks, are in [docs/verification-validation.md](docs/verification-validation.md).

## Tests

| Command | Needs the board |
| --- | --- |
| `make test` | no |
| `make firmware` | no |
| `make cppcheck` | no |
| `make hwtest` | yes |

Host tests cover the PID, the plant, fault transitions, the deadline-injection policy, the telemetry column order, the command parser, and text formatting. They link none of the HAL or FreeRTOS. ASan and UBSan are on for that binary. The runner is the harness in `tests/`, not GoogleTest.

The GitHub Actions workflow in `.github/workflows/ci.yml` runs those three host jobs on every push: unit tests, the Arm cross-compile, and cppcheck. It does not open a serial port or start OpenOCD.

## Layout

```text
src/mtr/            PID, plant, faults, parser, snapshot (host-testable)
firmware/app/       FreeRTOS tasks
firmware/platform/  clock, UART, timers, hooks into the HAL
firmware/rtos/      FreeRTOSConfig.h and kernel hooks
firmware/openocd/   NUCLEO-F446RE adapter settings
firmware/linker/    STM32F446RE memory map
tests/              host unit tests
tools/              board check and plotting
docs/               design and the Phase 2 notes
third_party/        pinned FreeRTOS, CMSIS, and a subset of the HAL
```

## Documents

Read in this order if you are new to the tree:

1. This file, for how to build, flash, and talk to the board.
2. [Architecture](docs/architecture.md), for tasks, interrupts, the sample order, and the fault bits.
3. [FreeRTOS design](docs/freertos-design.md), for why the kernel is called directly, and why a PID loop does not require one.
4. [Control design](docs/control-design.md), for the PID equations and which plant numbers are derived versus chosen.
5. [Verification](docs/verification-validation.md), for host results and the numbers measured on this Nucleo.
6. [Phase 2 hardware](docs/phase2-hardware.md), for the FIT1035, the AS5600, the DRI0058, and the voltage-mode FOC and PWM plan.
7. [Voltage-mode FOC technical note](docs/foc-voltage-mode-technical-note.pdf), for the full motor-model derivation, Clarke/Park transforms, SPWM and SVPWM, encoder alignment, STM32 PWM design, and from-scratch C++ implementation. The [LaTeX source](docs/foc-voltage-mode-technical-note.tex) is provided for reproducible editing.
8. [Vendored versions](third_party/VERSIONS.md), for the pinned FreeRTOS, CMSIS, and HAL commits.

## Roadmap

Phase 1 is the tree as it stands: native FreeRTOS, a 100 Hz speed PID, an illustrative mechanical plant on the MCU, a serial command and telemetry console, and a host test suite. The board procedure in `tools/hwtest.py` has been run on one NUCLEO-F446RE. The record of that run is the verification note.

Phase 2 replaces the simulated plant with the FIT1035, its AS5600, and the DRI0058. The speed PID stays the outer loop. Voltage-mode FOC and the three PWM channels are the inner electrical path, and they are not started. The bring-up order and the current and supply limits that still have to be established are in the Phase 2 note.

## Limitations

- The plant is not an identification of the FIT1035. Inertia is chosen, not measured.
- Phase 1 has no current limit, no PWM, and no path that would stop a real driver. A safe state here only zeros the simulated torque command.
- Board timing, stack high-water, and fault-injection results are in [docs/verification-validation.md](docs/verification-validation.md). They come from one `tools/hwtest.py` run on this NUCLEO-F446RE after the ST-Link was updated to V2J47M34. They are measurements of the simulated plant on that board, not a proof about a physical motor.
- GitHub Actions runs the host tests, the cross-compile, and cppcheck. It does not flash the board or run `tools/hwtest.py`.

## License of vendored code

FreeRTOS is MIT. CMSIS Core is Apache-2.0. The ST device headers and HAL are BSD-3-Clause. See `third_party/VERSIONS.md`. Application sources in this repository are the project author's.
