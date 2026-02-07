```
 _____     _                 ___  ____
|_   _|_ _| |__   ___   ___ / _ \/ ___|
  | |/ _` | '_ \ / _ \ / _ \ | | \___ \
  | | (_| | | | | (_) |  __/ |_| |___) |
  |_|\__,_|_| |_|\___/ \___|\___/|____/
```

# TahoeOS

TahoeOS is a Cortex-M4 smartwatch platform built on STM32F411 + FreeRTOS + LVGL.
This repository contains the full bootloader + application workflow, board bring-up assets, and host-side flashing/debug material.

## At a Glance

- MCU: STM32F411CEU6
- RTOS: FreeRTOS
- UI: LVGL v8.2 (touch)
- Display: 1.69-inch LCD
- Sensors: MPU6050, EM7028, AHT21, SPL06, LSM303
- Upgrade path: UART IAP + YMODEM

## Platform Narrative

The project is organized as one operational loop:

- Bring up board services: power, display, touch, UART, timer, sensors
- Run UI and application tasks: health, tools, connection, low-power workflow
- Maintain reliability: boot/app partitioning, APP flag gate, IAP update path

## Firmware Layout

Flash regions:

- Bootloader: `0x08000000` to `0x08007FFF` (32 KB)
- APP flag: `0x08008000` to `0x0800BFFF` (16 KB)
- Application: `0x0800C000` to `0x0807FFFF`

Key images:

- `Firmware/BootLoader_F411.hex`
- `Firmware/OV_Watch_V2_4_4.bin`
- `Firmware/OV_Watch_V2_4_4_NoBoot.hex`

## Quick Start

### Build Bootloader (GCC + CMake)

```bash
cd Software/IAP_F411
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build --parallel
```

### Flash with OpenOCD

```bash
openocd -f Software/IAP_F411/openocd.cfg \
  -c "program Software/IAP_F411/build/IAP_F411.elf verify reset exit"
```

### Typical Bring-up Check

1. Confirm APP flag at `0x08008000` equals `APP FLAG`.
2. Confirm app vectors at `0x0800C000` are valid.
3. Confirm runtime VTOR is `0x0000C000`.
4. Confirm LCD backlight PWM (`TIM3_CCR3`) is non-zero after app init.

## Core Features

- LVGL touch UI with page system and gesture interactions
- Health and motion data pipeline (heart rate, steps, environment)
- Utility pages (timer, compass, calculator, etc.)
- Low-power behavior (idle dimming + stop mode resume)
- Bootloader menu + YMODEM firmware delivery

## BLE Control Protocol (v1)

Transport:

- UART/BLE transparent serial, line framed with `\r\n`

Request frame:

- `#<LEN>|<SEQ>|<CMD>|<PAYLOAD>|<CRC16>`
- `<LEN>`: byte length of `<SEQ>|<CMD>|<PAYLOAD>`
- `<CRC16>`: CRC16-CCITT (poly `0x1021`, init `0xFFFF`) over `<SEQ>|<CMD>|<PAYLOAD>`

Response frame:

- `#<LEN>|<SEQ>|<ACK>|<CODE>|<PAYLOAD>|<CRC16>`
- `<ACK>`: `ACK` or `NACK`
- `<CODE>`:
  - `0`: OK
  - `1`: format error
  - `2`: length error
  - `3`: CRC error
  - `4`: unknown command
  - `5`: bad payload

Commands:

- `GET_STATUS`
  - payload empty
  - returns runtime status snapshot (version, battery, charge, BLE, wrist/app flags, timeouts, sensor summary)
- `SET_CFG`
  - payload `key=value` list separated by `,` or `;`
  - supported keys:
    - `ble=0|1`
    - `wrist=0|1`
    - `app=0|1`
    - `light=0..100`
    - `ltoff=1..59`
    - `ttoff=2..60`
  - constraint: `ltoff < ttoff`

## Repository Map

- `Software/OV_Watch`: main app firmware
- `Software/IAP_F411`: bootloader and IAP pipeline
- `Firmware`: released binary/hex artifacts
- `docs`: bring-up and debug notes
- `images`: UI/board captures and README assets

## Documentation

- Bring-up notes: `docs/bringup.md`
- Display debug record: `docs/display-debug-2026-02-06.md`
