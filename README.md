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

---
*Created by Jerry Wang*