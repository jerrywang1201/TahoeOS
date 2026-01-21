# Bringup Guide (Core OS Focus)

This document is a working bringup checklist for TahoeOS on STM32F411. It is
intended to make power-on, flashing, and minimal OS validation repeatable.

## Scope

- Board-level bringup: power, reset, SWD, clocks, boot flow
- Core OS validation: HAL init, clock tree, tick source, scheduler start
- Minimal build and flash steps

## Prerequisites

Hardware:
- Target board assembled and powered
- ST-LINK (or compatible SWD probe)
- USB-UART adapter for logs (if available)

Host tools (known-good versions should be recorded here):
- arm-none-eabi-gcc: TODO
- CMake: TODO
- OpenOCD: TODO
- GDB: arm-none-eabi-gdb

Host OS:
- macOS / Linux / Windows: TODO list tested platforms

## Repo Layout (core)

- Bootloader project: `Software/IAP_F411`
- Application project: `Software/OV_Watch`
- Bootloader notes: `Firmware/README.md`
- Core OS entry points:
  - `Software/OV_Watch/Core/Src/main.c`
  - `Software/OV_Watch/Core/Src/system_stm32f4xx.c`
  - `Software/OV_Watch/Core/Src/stm32f4xx_hal_timebase_tim.c`
  - `Software/OV_Watch/Core/Inc/FreeRTOSConfig.h`

## Power and Reset Checklist

- Verify target voltage at MCU VDD (measure at test point or decoupling cap)
- Check reset pin behavior (NRST not held low)
- Confirm SWDIO/SWCLK continuity to MCU pins
- Confirm boot pin strap (if applicable for this board)

## Clock Checklist

- Verify HSE/LSI presence if used by system clock or RTC
- If HSE is missing, confirm firmware falls back to HSI
- Confirm SysTick or TIM-based timebase is configured correctly

## Build (Bootloader)

```
cd Software/IAP_F411
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build --parallel
```

Artifacts are generated in `Software/IAP_F411/build/` and are not tracked by git.

## Flash (Bootloader)

```
openocd -f openocd.cfg -c "program build/IAP_F411.elf verify reset exit"
```

Alternative:
```
tools/flash_debug.sh build-flash
```

## Bootloader Entry Validation

- Hold KEY1, then apply power (KEY2 or programmer power)
- Confirm bootloader menu appears over UART or Bluetooth

## Core OS Validation (Application)

Minimum validation targets:
- `main()` executes and reaches scheduler start
- System clock configured to expected frequency
- Tick source runs (SysTick or TIM)
- Idle task and at least one user task runs

Suggested checks:
- Toggle a GPIO at boot (scope or LED)
- Print a UART banner with version and clock info
- Confirm FreeRTOS heap and stack usage are reasonable

## UART Logging (if available)

TODO: fill in UART instance, pins, and baud rate.

Example (placeholders):
- UART: USARTx
- TX/RX pins: PxY / PxZ
- Baud: 115200 8N1

## Common Failure Modes

- No SWD connection:
  - Check probe driver and SWD pin mapping
  - Lower SWD speed in `Software/IAP_F411/openocd.cfg`
- Immediate reset loop:
  - Check power rail stability and brownout settings
  - Verify vector table and clock init
- No RTOS scheduling:
  - Confirm tick source in `stm32f4xx_hal_timebase_tim.c`
  - Check FreeRTOS config (tick rate, heap, priorities)

## TODOs

- Record known-good toolchain versions
- Add board pinout and power tree references
- Add minimal "blinky + UART" bringup target
