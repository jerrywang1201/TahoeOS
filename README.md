# TahoeOS (Smart Watch Project)

> A personal custom smart watch system based on STM32F411 & FreeRTOS.

## 📖 Introduction

TahoeOS is a smart watch firmware project running on the **STM32F411CEU6** microcontroller. This project serves as a personal playground for exploring embedded systems development, focusing on real-time operating systems (**FreeRTOS**) and embedded GUIs (**LVGL**).

It currently supports full touch UI interaction, low-power management, and various sensor data processing.

## 🛠️ Tech Stack

* **MCU**: STM32F411CEU6
* **OS**: FreeRTOS
* **GUI**: LVGL v8.2
* **Hardware**: 1.69" LCD (Touch), MPU6050 (IMU), EM7028 (Heart Rate)

## ✨ Key Features

* **User Interface**: Smooth LVGL touch interface with gesture support.
* **Health & Motion**: Step counting, heart rate monitoring, temperature & humidity sensing.
* **Utilities**: Built-in calculator, compass, stopwatch, and altimeter.
* **Power Management**: Wrist-lift wake-up, multi-stage sleep modes (Stop/Shutdown) for extended battery life.
* **OTA**: Supports firmware upgrades via Bluetooth (IAP).

## 📸 Preview

<p align="center">
  <img src="./images/实物图.jpg" width="60%" />
</p>

## 🚀 Development Environment

* **IDE**: Keil / VSCode
* **Compiler**: ARMCC / GCC
* **Simulation**: VSCode (Windows) with LVGL Simulator

## 🧩 Firmware Layout (Bootloader + APP)

Flash layout follows the boot/flag/app split documented in `Firmware/README.md`:

* **Bootloader**: `0x08000000` - `0x08007FFF` (32 KB)
* **Flag**: `0x08008000` - `0x0800BFFF` (16 KB)
* **APP**: `0x0800C000` - `0x0807FFFF`

## 🛠️ GCC + CMake (Bootloader)

Bootloader project: `Software/IAP_F411`

Build:
```bash
cd Software/IAP_F411
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build --parallel
```

Flash via OpenOCD:
```bash
openocd -f openocd.cfg -c "program build/IAP_F411.elf verify reset exit"
```

Debug with GDB:
```bash
openocd -f openocd.cfg
arm-none-eabi-gdb build/IAP_F411.elf
```
In GDB:
```
target extended-remote :3333
monitor reset halt
load
monitor reset
```

Quick script:
```bash
chmod +x tools/flash_debug.sh
tools/flash_debug.sh build-flash
tools/flash_debug.sh flash
```

---
*Created by Jerry Wang*
