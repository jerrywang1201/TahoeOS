
  <h1 align="center">BootLoader and Application Notes</h1>



## :ledger: Overview

The purpose of the Bluetooth update downloader is to allow Bluetooth IAP upgrades after the watch is assembled in its enclosure, without disassembly.

The Firmware folder contains two files: `BootLoader_F411.hex` and `APP_OV_Watch_V2.4.0.bin`. They correspond to the `IAP_F411` and `OV_Watch_V2.4.0` projects under the `Software` folder, and are the compiled outputs of those projects.

The memory layout of the BootLoader and APP is roughly as shown below:

<p align="center">
	<img border="1px" width="80%" src="../images/storage.jpg">
</p>


## :warning: How to enter BootLoader upgrade mode

After flashing the BootLoader, power on the watch while holding the KEY1 upper button. Sequence matters: hold KEY1 first, then apply power with KEY2 (or in debug mode, hold KEY1 and then plug in the programmer). If you power on without holding KEY1, it will boot directly into the application.



## :black_nib: How to flash the BootLoader

1. Connect the programmer to the watch PCB SWD port.
2. Build the Keil project and click `Download` to program the device.
3. If you do not want to modify the code, drag `BootLoader_F411.hex` into `STM32 ST-LINK Utility` to program it.
4. Alternatively, use GCC + OpenOCD:

```
cd ../Software/IAP_F411
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build --parallel
openocd -f openocd.cfg -c "program build/IAP_F411.elf verify reset exit"
```

One-click build + program:

```
tools/flash_debug.sh build-flash
```

<p align="center">
	<img border="1px" width="80%" src="../images/ST-LINK download.jpg">
</p>
! ! ! Friendly reminder: if you are unsure how to change the Keil project settings, do not modify them.



## :black_nib: How to flash/upgrade the application

1. Enter BootLoader upgrade mode.
2. Pair your computer with the watch over Bluetooth. The device name is typically something like KT6368A-SPP, depending on the hardware. Mine is TD5322A.

<p align="center">
	<img border="1px" width="30%" src="../images/蓝牙配对.jpg">
</p>

3. Open `More Bluetooth settings` and add the paired Bluetooth device to a COM port.

<p align="center">
	<img border="1px" width="80%" src="../images/蓝牙设置.jpg">
</p>

4. After the above setup, you do not need to configure it again. Open `SecureCRT` and connect to the COM port that matches the Bluetooth device (mine is COM14). Then power on and hold KEY1 to enter Boot upgrade mode. You will see the interface below; follow the prompts and enter the numbers.

<p align="center">
	<img border="1px" width="50%" src="../images/SecureCRT.jpg">
</p>

<p align="center">
	<img border="1px" width="50%" src="../images/boot升级界面.jpg">
</p>

5. Once in the interface, enter `1` to start application file transfer. The PC will keep receiving `CCCCCC...`, which means it is waiting for you to send a file via the Ymodem protocol. Choose `send Ymodem` and send `APP_OV_Watch_V2.4.0.bin`. The process is slow; please wait.

<p align="center">
	<img border="1px" width="50%" src="../images/send ymodem.jpg">
</p>

6. After the transfer finishes, enter `3` to run the application and wait for it to boot.
7. After any future application update, you can still upgrade wirelessly via Bluetooth without opening the case.
