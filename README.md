# OLDROBOT

English | [简体中文](README.zh-CN.md)

STM32F103C8 bare-metal firmware for a three-axis bus-servo mechanism. The firmware runs a fixed power-on motion sequence, then accepts single-byte commands from an HC-05 Bluetooth serial link.

## Current behavior

After reset, the firmware:

1. Initializes HAL, the 72 MHz system clock, GPIO, USART1, and USART2.
2. Starts one-byte interrupt reception on USART2 and prints `System start`.
3. Flashes the PB5 status LED, waits for the power and servos to stabilize, and moves all three servos to the initial pose.
4. Runs the enabled three-step power-on motion sequence and returns to the initial pose.
5. Polls the latest received Bluetooth command and dispatches the corresponding motion.

The application is blocking and uses the STM32 HAL directly; no RTOS is present.

## Hardware and toolchain

| Item | Configuration evidenced by the project |
| --- | --- |
| MCU | STM32F103C8T6, Cortex-M3, LQFP48 |
| Firmware package | STM32Cube FW_F1 V1.8.7 |
| Configuration | `OLDROBOT.ioc` |
| IDE project | Keil MDK-ARM project `MDK-ARM/OLDROBOT.uvprojx` |
| Configured toolchain | MDK-ARM V5.32 with Arm Compiler 6 |
| Compiler in the existing build log | ArmClang 6.24 |
| Runtime | STM32 HAL, bare metal |

### Pin and serial map

| MCU pin | Function | Application role |
| --- | --- | --- |
| PA9 | USART1 TX, 115200 8N1 | Bus-servo controller RX |
| PA10 | USART1 RX, 115200 8N1 | Configured but not consumed by the application |
| PA2 | USART2 TX, 9600 8N1 | HC-05/debug text output |
| PA3 | USART2 RX, 9600 8N1 | HC-05 single-byte command input |
| PB5 | Push-pull output | Status LED |
| PA13 / PA14 | SWDIO / SWCLK | SWD programming and debugging |
| PD0 / PD1 | HSE oscillator | External clock input/output |

Use a power supply suitable for the servos and connect the STM32, Bluetooth module, servo interface, and servo supply grounds together. Electrical voltage levels and the exact servo power arrangement are not documented by the source and must be checked against the hardware datasheets before connection.

## Control interface

USART2 accepts one ASCII byte per command. Uppercase and lowercase forms are accepted.

| Command | Active behavior | Servo targets `(ID1, ID2, ID3)` | Move time |
| --- | --- | --- | --- |
| `A` / `a` | Run action A | `(200, 600, 500)` | 800 ms |
| `B` / `b` | Run action B | `(800, 400, 300)` | 800 ms |
| `C` / `c` | Run action C | `(500, 500, 500)` | 700 ms |
| `R` / `r` | Return to the initial pose | `(610, 480, 490)` | 1000 ms |
| `S` / `s` | Send a broadcast stop command | Not applicable | Immediate command |

Valid motion commands toggle PB5. Status and error text is returned through USART2.

The servo frames use two `0x55` header bytes, an ID, length, command, parameters, and an inverted-sum checksum. Positions are clamped to `0..1000`; move times are clamped to `0..30000` ms.

## Power-on motion

`POWER_ON_ACTION_ENABLE` in `Core/Src/main.c` controls whether the fixed startup sequence runs.

| Step | Servo targets `(ID1, ID2, ID3)` | Move time | Delay before next step |
| --- | --- | --- | --- |
| Initial pose | `(610, 480, 490)` | 1000 ms | 1100 ms inside `Servo_InitPose()`, then 1200 ms in `main()` |
| 1 | `(485, 555, 415)` | 800 ms | 1000 ms |
| 2 | `(795, 405, 565)` | 700 ms | 900 ms |
| 3 | `(485, 555, 415)` | 700 ms | 900 ms |
| Return | `(610, 480, 490)` | 1000 ms | 1100 ms inside `Servo_InitPose()`, then another 1200 ms |

The action groups first queue positions with `SERVO_MOVE_TIME_WAIT_WRITE` and then issue a start command. The active A/B/C handlers use broadcast ID `0xFE` for the start operation.

## Software structure

| Path | Responsibility |
| --- | --- |
| `Core/Src/main.c` | Initialization, startup sequence, USART2 command reception, active action groups, and main loop |
| `Hardware/servo_bus.c` | Bus-servo frame encoding, range limiting, transmission, stop/start operations, and initial pose |
| `Core/Src/usart.c` | USART1 and USART2 HAL configuration |
| `Core/Src/gpio.c` | PB5 status LED configuration |
| `Drivers/` | STM32F1 HAL and CMSIS sources supplied with the project |
| `MDK-ARM/` | Keil project, startup code, settings, and generated build artifacts |

Representative command flow:

`USART2 IRQ -> HAL_UART_RxCpltCallback() -> bt_cmd/bt_cmd_flag -> main loop -> Process_BT_Command() -> servo_bus -> USART1 frame`

## Build

The repository contains a Keil project but no command-line build script.

1. Install Keil MDK with Arm Compiler 6 support and the STM32F1 device pack.
2. Open `MDK-ARM/OLDROBOT.uvprojx` in µVision.
3. Select the `OLDROBOT` target.
4. Build the target with **Project > Build Target** (`F7`).
5. Expected outputs are `MDK-ARM/OLDROBOT/OLDROBOT.axf` and `MDK-ARM/OLDROBOT/OLDROBOT.hex`.

These GUI steps are inferred from the checked project file. No verified CLI command is supplied because µVision was not available on PATH or at the default local installation path during this documentation pass.

### Existing build evidence

The checked-in/generated build log dated 2026-04-24 records:

- ArmClang 6.24
- `Code=7908`, `RO-data=344`, `RW-data=12`, `ZI-data=1788`
- AXF and HEX generation
- 0 errors and 0 warnings

This is an existing build record, not a rebuild performed while creating this README.

## Known limitations

1. **CubeMX clock conflict:** `OLDROBOT.ioc` records an AHB divide-by-2 configuration and 36 MHz HCLK, while the active `SystemClock_Config()` uses an AHB divide-by-1 configuration and therefore runs HCLK at 72 MHz from the 8 MHz HSE and PLL x9. Resolve this before regenerating code with CubeMX.
2. **Latest-command buffering only:** the USART2 callback stores one current command and one flag. Commands received during a blocking action can overwrite an earlier unprocessed command.
3. **Blocking motion path:** startup motions, pose changes, and action helpers use `HAL_Delay()`, so the main loop cannot schedule other work during those delays.
4. **Duplicate action definitions:** `Hardware/servo_bus.c` exports `Action_Group1/2/3`, but the active Bluetooth path uses the separate `Action_Group_A/B/C` functions in `main.c`. The exported numbered groups are not connected to the current application flow.
5. **Error LED does not continuously blink:** `Error_Handler()` disables interrupts and then calls `HAL_Delay()`. Because the HAL tick no longer advances, execution can stall on the first delay instead of repeatedly toggling PB5.
6. **Transmit-only servo behavior:** the active application sends UART frames but does not parse servo responses or confirm position. The driver also notes that half-duplex direction control may be required by some interface boards but is not implemented.
7. **No automated tests:** no host-side or target-side test suite is included.

## Validation status

| Acceptance layer | Status |
| --- | --- |
| Source and project configuration inspected | Completed during README creation |
| Firmware artifact generated | Existing 2026-04-24 AXF/HEX and zero-error build log found; not rebuilt in this pass |
| Programmer/debugger detected | Not verified |
| Firmware flashed to the MCU | Not verified |
| Bluetooth or servo communication observed | Not verified |
| Physical servo motion and system safety accepted | Not verified |

Review the pin map, servo IDs, motion limits, mechanical clearances, power delivery, and emergency-stop behavior on the real assembly before enabling motion.
