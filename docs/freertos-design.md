# FreeRTOS design

## Why the kernel is called directly

CMSIS-RTOS v2 is a second API in front of the same kernel. This project is here to make the kernel's own rules visible: which calls are legal in an ISR, which objects can be created with static storage, how a notification is different from a queue, and what `configMAX_SYSCALL_INTERRUPT_PRIORITY` actually gates. The HAL is still used to program the peripherals. `USE_RTOS` in `stm32f4xx_hal_conf.h` stays 0. That switch is the old CMSIS-RTOS hook inside the HAL, and turning it on does not mean "this project uses FreeRTOS."

The kernel is FreeRTOS-Kernel tag V11.3.2, commit `640d0c43705e64cd53f46b31d882de6cd8433933`. `tskKERNEL_VERSION_NUMBER` in that tag is still `"V11.1.0+"`. The banner prints the macro. The tag is recorded in `third_party/VERSIONS.md`.

## Why an RTOS is used here, and why it is not required

A 100 Hz PID loop does not need a scheduler. A timer interrupt can run the controller, and the main loop can parse UART bytes and print telemetry whenever the interrupt is not running. That design is smaller, has one stack, and makes the worst-case latency of the control law equal to the ISR itself. Many motor controllers are built that way, and they should be.

This firmware uses a kernel for a narrower reason. The control body must stay short and deterministic, while a person types commands and a 10 Hz log shares the same UART. Putting the logger in the control ISR would stretch the sample whenever the transmitter is busy. Putting it in a polled main loop is honest and would work. The tasks make the three periods (10 ms control, 100 ms log, 100 ms supervision) independent, and they make a blocking write in the logger unable to delay the next PID sample, because the control task is the highest priority and the logger blocks on its own delay.

The cost is real: about 2 KiB of stack per task, priority rules that have to be argued rather than assumed, and a hard split between ISR-safe and task-safe APIs. Those costs are accepted because the project is specifically about that split. They are not accepted because a PID controller is impossible without them.

## Time base

SysTick is the kernel tick at 1 kHz. The HAL does not use SysTick. `HAL_InitTick` is overridden to run TIM6 at 1 kHz. TIM6's priority is 2, which is more urgent than `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5), so the HAL tick is not allowed to call FreeRTOS and does not. After the PLL locks, `HAL_InitTick` runs again so the 1 kHz period matches the 90 MHz timer clock.

TIM7 is the control release. At a 90 MHz timer clock the 100 Hz period is 900000 counts, split across the 16-bit prescaler and auto-reload. The ISR records the DWT stamp, updates the inter-arrival jitter, and calls `vTaskNotifyGiveFromISR`. It does not run the PID.

## Notification instead of a semaphore

The release is a single bit of information from one ISR to one task: the period elapsed. A task notification does that without a queue object. The control task waits with `ulTaskNotifyTake(pdTRUE, 50 ms)`. `pdTRUE` clears the count. The return value is how many gives happened since the last take.

That return value is the miss detector. A count of 1 is a normal sample. A count of N is N-1 missed releases, because the task woke once and will step the plant once. A timeout is one miss. The extra releases are not executed later. Catching up would apply a burst of PID updates with a stale idea of the sample time, and it would also clear the counter that the supervisor is supposed to see.

## Queue for commands, nothing for the snapshot

Commands are small, produced by a lower-priority task, and consumed by the control task at the start of a sample. A queue of 8 fits. The send uses a zero timeout so the command task cannot block on the control task. If the queue is full the command is dropped, the operator gets `error: command queue full`, and the overflow counter moves toward the queue fault.

The snapshot is the other direction: one writer, several readers, newest value only. A queue would either block the control task or require a copy-out plus an overflow policy for data that is obsolete one sample later. The seqlock keeps the writer to a pair of sequence increments and a word copy. Readers retry. On this core a naturally aligned 32-bit store is atomic; the sequence detects a reader that overlapped the copy. The control task is the highest priority, so another task cannot preempt the writer. An ISR can. The ISR does not read the snapshot. The odd sequence still covers that window.

## Mutex, and priority inversion

The TX mutex is the only mutex. Telemetry is priority 2 and the command task is priority 4. If telemetry holds the mutex while the command task needs it, the command task blocks and telemetry inherits priority 4 until it gives the mutex back. The control task never takes the mutex, so the inversion cannot reach the PID. The critical section inside a line is "wait until the ring accepts the next byte," which yields rather than spinning with the scheduler locked.

The UART ISR does not take the mutex. It only pops a byte that the task has already published. `TXEIE` is set and cleared with the USART interrupt masked from the task (`taskENTER_CRITICAL` masks priorities at or below the syscall ceiling; USART is priority 7). That closes the window where the ISR's update of `CR1` would clear `RXNEIE`.

## Priorities and inversion elsewhere

No other lock is shared between priorities. The command queue is taken only by the command task (send, timeout 0) and the control task (receive, timeout 0). A zero timeout cannot invert. The snapshot has no lock.

The load task is the experiment that would cause inversion if it shared a lock with a higher task and did not inherit. It shares nothing. It burns cycles and yields. Idle is starved while it runs, which is the point of the CPU-utilization check.

## Deadlines

Two different things are counted, and they are not added together.

| Quantity | Where it is measured | What it means |
| --- | --- | --- |
| Release jitter | TIM7 ISR, DWT delta minus `SystemCoreClock/100` | the period of the interrupt, not the task |
| Wake latency | first instruction of the control body minus the latest ISR stamp | scheduling delay for this wake |
| Execution | DWT across the body, including the snapshot store and a deadline injection | how long the sample took |
| Response | ISR stamp to the end of the body | wake latency plus execution |

If more than one release was pending, the wake latency uses the latest ISR stamp only. The earlier releases are already in the miss counter. Reporting a single latency across several periods would mix the two.

An execution longer than one period increments the overrun streak. Three consecutive overruns latch `kFaultOverrun`. A short sample clears the streak and does not clear the latch. Three accumulated misses latch `kFaultDeadline`.

`fault inject deadline` arms five iterations of a 25 ms DWT spin inside the measured body, and only while running. `DeadlineInjection` in `src/mtr` is what the control task calls, and the host tests cover it. Each of those iterations overruns the 10 ms period and also leaves the next notification pending, so both latches trip. The remaining spins still run after the deadline latch. Stopping them there would make the next sample short, and a short sample clears the overrun streak before `kFaultOverrun` can latch. `stop` and `reset faults` cancel any spins still armed. The injection is inside the timed region on purpose. Putting it outside would show a miss without an execution overrun and would not exercise the measurement.

Sub-microsecond jitter truncates to 0 when converted with integer division by the cycle-per-microsecond count (180 at 180 MHz). A reported jitter of 0 means "below 1 µs," not "the timer did not run."

## CPU utilization

Once a second the supervisor reads `ulTaskGetIdleRunTimeCounter()` and the DWT. Both are configured from the DWT cycle counter (`configGENERATE_RUN_TIME_STATS`). The counter wraps about every 23.8 s at 180 MHz. The window is 1 s, so the subtraction is valid. Utilization is `(total - idle) / total`. Time spent in ISRs while the idle task was the current task is charged to idle, because the run-time counter advances in the idle task context across the interrupt. The number is a load indicator, not a cycle-accurate split of ISR time.

## Stacks

Stacks were sized from the call shape, not from a measured high-water mark:

| Task | Words | Reason |
| --- | --- | --- |
| control | 512 | PID, plant, and one snapshot on the stack. No `printf` |
| command | 768 | a 640-byte `stats` buffer plus the parser |
| telemetry | 512 | a 256-byte row buffer |
| supervisor | 384 | one snapshot copy and the high-water calls |
| load | 256 | a delay loop |
| idle | 256 | `configMINIMAL_STACK_SIZE`, static idle memory hook |

Overflow checking is method 2 (sentinel words and the stack pointer). The hook panics and names the task. High-water marks are stored once a second and printed by `stats`. The free-word counts from the board run are in [verification-validation.md](verification-validation.md). The smallest of those, 220 words on the load task, is inside the 256-word allocation. The telemetry task had 320 words free of 512.

## Errors the kernel is allowed to stop on

`configASSERT` calls `vAssertCalled`, which panics. The usual trip is a FromISR call from a priority more urgent than the syscall ceiling. That is a programming error, not a motor fault, and continuing would corrupt the kernel lists.

Failed `xTaskCreateStatic` refuses to start the scheduler and panics with `tasks`. A clock that does not lock at 180 MHz panics with `clock` before the UART is up; the LED blinks and there is no banner. There is no heap, so there is no malloc-failure hook.

## What was deliberately left out

Software timers are off. The periods that exist are TIM7 and `vTaskDelayUntil`. Event groups, queue sets, and recursive mutexes are off. Nothing in the application needed them. Adding them to have a longer API list would hide which mechanism is actually doing the work.
