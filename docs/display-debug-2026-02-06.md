# OV-Watch Display Debug Report (2026-02-06)

## Scope
- 目标问题：板子刷入 firmware 后屏幕不亮。
- 调试仓库：`/Users/wangjialong/Software/OV-Watch`
- 调试方式：OpenOCD + ST-Link 在线寄存器/Flash 验证 + 板级强制 GPIO 测试。

## Baseline Check

### 1) 确认 Boot/App 镜像与启动条件
- 读取启动相关地址：
  - `0x08000000`（boot 向量表）
  - `0x0800C000`（app 向量表）
  - `0x08008000`（`APP FLAG`）
- 核验结果：
  - boot 向量表有效（`SP=0x20020000`, `Reset=0x08002b09`）
  - app 向量表有效（`SP=0x20019870`, `Reset=0x0800c271`）
  - `APP FLAG` 为 `"APP FLAG"`（`0x20505041 0x47414C46`）

### 2) 重新整包烧录，排除镜像不一致
执行：
- `Firmware/BootLoader_F411.hex`
- `Firmware/OV_Watch_V2_4_4.bin` 写入 `0x0800C000`
- 重新写入 `APP FLAG` 到 `0x08008000`

结果：OpenOCD `Programming Finished` + `Verified OK`。

## Runtime Evidence

### 3) 运行态是否在 App
多轮 `reset run -> sleep -> halt` 抓取结果：
- `PC` 多次落在 `0x0801xxxx/0x0803xxxx`
- `VTOR (0xE000ED08)=0x0000C000`

结论：设备已稳定跳转并运行在 App，不是“没烧进去”或“卡在 boot while(1)”问题。

### 4) 背光链路状态
关键寄存器：
- `TIM3_CCR3 (0x4000043C)`
- `GPIOB_MODER (0x40020400)`
- `GPIOB_AFRL (0x40020420)`

观测：
- `TIM3_CCR3` 最终可到 `0x96`（约 50% 占空比）
- `PB0` 已配置为 `AF2(TIM3_CH3)`

说明：软件层有在打开背光 PWM。

### 5) CPU 卡点定位
- 多次 halt 看到 `PC` 出现在 `0x080181f6~0x0801820e`。
- 对应反汇编是 `delay_us()` 的 SysTick 轮询区间（等待 `SysTick->VAL` 变化）。
- 该位置属于运行态行为，不是 HardFault/Panic。

## Hardware Isolation Test

### 6) 背光 GPIO 强制拉高/拉低测试
在 halt 状态直接写寄存器：
- 将 `PB0` 临时改为普通输出并拉高 5 秒
- 再拉低 5 秒
- 恢复 `PB0` 为 `AF2(TIM3_CH3)`

该测试用于区分：
- 若屏幕有明显亮灭变化：背光硬件链路通，问题更偏 LCD 数据链路（SPI/DC/RES/面板初始化）。
- 若完全无变化：优先排查背光硬件链路（FPC、背光供电、背光驱动器件）。

## Final Conclusion
- **软件已成功烧录，App 已运行，VTOR 正确切到 App。**
- **背光 PWM 在软件侧可被设置到有效占空比。**
- 当前“屏幕不亮”更偏向 **硬件链路问题**（背光或 LCD 面板连接/供电），而非“代码未烧录”问题。

## Repro Commands (核心命令)

```bash
# 1) 读关键状态
openocd -f Software/IAP_F411/openocd.cfg \
  -c "init; reset run; sleep 1200; halt; reg pc; mdw 0xE000ED08 1; mdw 0x08008000 2; mdw 0x0800C000 2; mdw 0x4000043C 1; shutdown"

# 2) 整包重刷（boot + app + APP FLAG）
openocd -f Software/IAP_F411/openocd.cfg \
  -c "init; reset halt; program Firmware/BootLoader_F411.hex verify; \
      program Firmware/OV_Watch_V2_4_4.bin 0x0800C000 verify; \
      flash write_image erase /tmp/ov_app_flag.bin 0x08008000 bin; \
      verify_image /tmp/ov_app_flag.bin 0x08008000 bin; reset run; shutdown"
```

