# GrokWatch 硬件能力与上限（M5Stack StopWatch）

机器：StopWatch **v1.0.1**（包装盒标签 V1.01）。这一版背部扩展口 14 脚是 **BAT（电池针脚）**，不是 5V IN；v1.0 在这一脚上是 5V IN。来源：[官方说明书](https://docs.m5stack.com/en/core/StopWatch) Version Change / Sticker 表。

标注：【规格】引自规格书/官方资料，【代码】来自本仓库或 M5GFX 源码，【实测】来自手表串口，【推断】计算或推断。

## 1. 主要器件
| 项 | 值 | 来源 |
|---|---|---|
| SoC | ESP32-S3R8，Xtensa LX7 双核 240 MHz，16 MB Flash，8 MB 八线 PSRAM（qio_opi） | 【规格】说明书；【代码】sdkconfig: CPU 240、SPIRAM OCT 80 MHz |
| 屏幕 | 1.75" AMOLED 466×466，驱动 CO5300，QSPI | 【规格】说明书 |
| 屏幕引脚 | CS G39、SCK G40、TE G38、D0–D3 = G41/G42/G46/G45；复位 M5IOE1 PYG5，供电 M5IOE1 PYG8 | 【规格】说明书 |
| 触摸 | CST820B，I2C G47/G48，INT G13，复位 M5IOE1 PYG4 | 【规格】说明书 |
| 按键 | 黄键 G2（BUTTON1_GPIO）、蓝键 G1（BUTTON2_GPIO），GPIO 输入 | 【规格】说明书；【代码】config.h |
| 电源 | M5PM1，450 mAh；红键 = PMIC 按键（单击报 IRQ 0x02/0x03 或 BTN_FLAG，见 gw_redkey.c） | 【规格】【实测】 |
| 硬件 2D 加速 | ESP32-S3 **没有 PPA**（PPA 是 ESP32-P4 的外设）；可用的是 GDMA 和 PIE SIMD 指令 | 【规格】ESP32-S3 技术参考手册【推断：需再核对一次 TRM 目录】 |

## 2. 屏幕（CO5300）
| 项 | 值 | 来源 |
|---|---|---|
| 刷新率 | 规格书 TE 时序按 **60 Hz** 帧率给出 | 【规格】[CO5300 Datasheet](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/CO5300_Datasheet.pdf) 5.3 Tearing Effect |
| TE | 35h 开 TE，44h 设扫描线（2–1280）；TE 引脚 G38 | 【规格】同上；【规格】说明书 |
| QSPI 最高时钟 | 规格书摘录里**没有写明**；M5 官方 M5GFX 用 **80 MHz** 写 | 【代码】M5GFX `bus_cfg.freq_write = 80000000` |
| 像素格式 | RGB565（3Ah=55h）；也支持 RGB666/888 | 【代码】【规格】 |
| 窗口对齐 | 列/行起点和终点按偶数对齐（我们的 rounder 已做） | 【代码】RoundLcdDisplay::rounder_event_cb；【推断】对应规格书窗口要求 |

## 3. 我们的固件 vs 官方（M5GFX）
| 项 | GrokWatch 2.6.23 | M5GFX 官方 | 差距 |
|---|---|---|---|
| QSPI 时钟 | 40 MHz | 80 MHz | 传输慢一倍 |
| TE | 发了 35h 但**没接 G38**，不做同步 | 35h 80h + 44h 扫描线 466 | 无防撕裂 |
| 缓冲 | LVGL 局部刷新，466×16 行 ×2（双缓冲），内部 RAM，DMA | PSRAM 整帧帧缓冲 + 自动推送 | — |
| 编译优化 | `-Os`（体积优先），LVGL 无 S3 SIMD 汇编，1 个绘制单元 | — | 渲染慢 |
| 唤醒词 | assets 里的 WakeNet9 常驻，AFE 常驻占 CPU | — | 2.6.24 去掉 |

## 4. 三个上限
整帧数据量 466×466×2 B = **434,312 B**。

| 层 | 当前 | 可达 | 来源 |
|---|---|---|---|
| 屏幕 | 60 Hz | 60 Hz | 【规格】 |
| 传输（整屏） | 40 MHz×4 线 = 20 MB/s → **21.7 ms/帧，≤46 fps** | 80 MHz → **10.9 ms/帧，≤92 fps** | 【推断】按时钟计算；单次整屏实测待 2.6.24 打点 |
| 渲染（切页，整屏重绘） | 切页实测 **6 fps**，约 160 ms/帧 | 待测 | 【实测】串口 `NavUI: slides 6 fps < 30 twice: switching pages directly`（2.6.17、2.6.19） |
| 渲染（局部，秒针） | 闲时 5–6 fps 是因为每秒只有少量重绘，不代表上限；最慢帧 45–300 ms | — | 【实测】GW-PERF |

**结论**：天花板 = 屏幕 60 fps。现在卡在**渲染层**：160 ms/帧里传输只占约 22 ms（【推断】），其余是 CPU 绘制。因为切页太慢，固件里的帧率守护会在两次低于 30 fps 后把切页动画关掉、改成直接跳页，Sunny 感觉"一点也不丝滑"就是这个原因。

## 5. 顺格优化（每项先实测）
1. 打点：每帧拆开 渲染 / 等待 DMA / 传输，整屏刷新耗时实测。
2. 去掉常驻唤醒和 AFE，空出 CPU（2.6.24）。
3. 切页改为"快照平移"：切页开始时把两页各渲染一次成 RGB565 图（PSRAM），动画期间只平移两张不透明图，不重画控件（2.6.24 目标）。
4. LVGL 编译改 `-O2`，开 ESP32-S3 SIMD 混合（esp_lvgl_port），绘制缓冲加大到 466×32~64 行。
5. QSPI 提到 80 MHz（按 M5GFX），实测有无花屏。
6. 接 TE（G38，44h 扫描线）做同步，防撕裂。
7. 绑核：LVGL 独占一个核，Wi‑Fi/BLE/音频在另一个核。
