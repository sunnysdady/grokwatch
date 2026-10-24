# Codex 接力：服务已修复，Aora 表情移植进行中

日期：2026-10-25。用户因额度将尽要求立即保存接力文档。本文件是当前状态入口；不要把未完成的图标移植当成已刷入手表。

## 1. 最新任务和边界

用户先要求修好手表 Grok 和待办连接失败，再要求“读取 aora-bot 的项目，把表情包替换现在的 grokbot 的图标”。当前任务停在表情移植的主机预览阶段。

- **不要自动切页、滑动、点击或发送 Bot 指令。** 用户明确反对屏幕自行切页。
- 先前的自动切页来自我们的测试脚本，已停止；后续只读观察也已正常结束并释放串口。
- main 不动；不要写入密钥或内网地址；私有 NVS、整片备份、服务配置和日志不得提交、公开或打包。
- 用户此前授权过 USB 测试固件刷机，但新表情版本目前没有完成固件编译、部署审计或刷机。**当前候选还有肉眼可见缺陷，不可刷。**
- 回复中文，开始实质工作先显示 `AI 指令：`。按 token-saver-coding 做最小改动；刷机/部署前必须 differential-review。
- Reasonix 已确认因缺少 DEEPSEEK_API_KEY 无法使用，允许直接编辑回退；不要再反复尝试或提示。已向用户解释其为 DeepSeek 编码 CLI，与 Codex 登录独立。
- 不要启动子代理；不更新全局 memory。

## 2. 仓库与分支

工作目录：`/Users/mima0000/Documents/ChatGPT/Grokwatch`

- 当前分支：`codex/aora-face`，从 `codex/review-2.6.5` 的 `62f38493018410c130e6d7d456dc47b588e358a9` 建立。
- 接力文件及 Aora WIP 将作为本地检查点提交。接手时用 `git status`、`git log -4 --oneline` 确认实际 SHA。
- `62f3849 fix: restore GrokWatch todo service integration`：服务器待办修复已部署并验证。
- `950d55a`：此前服务恢复审计文档。
- `d1d6f8ed4c739d7d55a26db3df49ae449c56fda7`：当前手表所刷固件的代码候选。
- `6df5ab1`：保留服务发现，仅禁止测试控制台自动安装 OTA。
- `7293d4f`：2.6.5 字体锁与布局修复。
- 审查起点：`handoff/homeview-design@a331b52cc2d5fe1e8d507bc581075b4bbe1b21ec`。main 仍未修改，也没有 push。

## 3. Grok / 待办服务：已经修好

实际服务目录：`/Users/mima0000/xiaozhi-grok/xiaozhi-server`（**没有 Git 元数据**）。目录名来自底层固件服务，实际配置选择 `GrokLLM`，已有 Grok BOT 桥接。此前向用户索要“小智服务器”造成误解，已解释清楚。

Python：`/Users/mima0000/xiaozhi-grok/venv/bin/python`。不要用系统 Python 直接启动，缺少依赖。

根因与修复：
1. 我们早先测试控制台跳过整个 `CheckNewVersion()`，连服务发现也跳过，导致协议配置缺失。已改为保留发现，只禁 OTA 下载/安装。
2. 原手表旧固件嵌入了私有服务地址，原 NVS 无 `wifi/ota_url`。已从验证过的旧二进制恢复地址，只写入设备 NVS，不进代码。
3. 原服务 `/gw/todo` 返回 404：缺少仓库原有的 `server/gw_todo.py` 插件和 HTTP 路由。已安装并重启。

部署目标只有三个文件：
- `plugins_func/functions/gw_todo.py`：仓库插件，额外校验 n 范围 1..50；非法参数在写入前拒绝。
- `core/http_server.py`：增加插件 import 和路由注册，原路由不变。
- `data/.config.yaml`：只向 `Intent.function_call.functions` 添加 `add_todo`；解析后确认其他配置完全相等。

保留 `live_search`、`grokbot_command`、`grokbot_notifications`、`get_weather`、原模型和桥接凭据。未增加依赖、未迁移数据。生产待办原本为空，验证没有创建真实待办。

实际部署前备份和清单：
- `work/device-backup/server-before-todo/`：原 HTTP 文件、原 YAML、桥接文件及 SHA256 manifest。
- `work/device-backup/server-launch.json`：原进程启动信息和私有桥接环境。**不要打印或公开。**
- `work/device-backup/server-after-todo.json`：新 PID 和已审计候选哈希。
- `work/device-backup/server-after-todo.log`：私有启动日志。
- `work/server-service-review/`：暂存候选，含私有 YAML，不可提交。

原 PID 93472 已 SIGTERM 优雅退出；新 PID **73375**，保存接力时仍存活。新进程以正确 venv、原 cwd 和保留的 GW_RELAY 环境运行；不要启动第二个实例。PID 仅为本次快照，后续需核实。

验证已通过：
- 临时目录、真实注册接口、aiohttp HTTP 测试 4 项：空列表只读、语音函数新增、完成/恢复、容量、非法参数无副作用、未知 ID 无写入。
- Python 编译、git diff --check。
- 部署前后目标哈希匹配。
- OTA GET 200；服务发现 POST 200 且返回 WebSocket 配置；真实 WebSocket hello 成功、opus 协商。
- 待办 GET 200 且 JSON schema 正确（此前 404）。
- Grok BOT 只读 `/snapshot` 200；**未调用 `/cmd`**。
- 现有 Grok 模型配置的极小请求成功，1.28 秒返回非空响应，无个人内容、无工具调用。
- 重启后复查无 Traceback、认证失败、HTTP 启动失败。

局限：没有自动打开手表待办页；固件只在用户打开该页时请求，所以旧屏幕提示需用户自然打开后刷新。不能声称已观测到手表页面更新。没有验证麦克风→ASR→模型→扬声器整条语音链。待办沿用可信局域网单用户无逐用户鉴权设计，不得暴露公网。

报告：`docs/relay/AUDIT-TODO-SERVICE-2026-10-25.md`。
可重放 HTTP 补丁：`server/http-todo-routes.patch`。
测试：`PYTHONPATH=/Users/mima0000/xiaozhi-grok/xiaozhi-server /Users/mima0000/xiaozhi-grok/venv/bin/python server/test_gw_todo.py -q`。测试日志框架可能在执行 cwd 生成 tmp/server.log，避免把它提交。

## 4. 手表当前状态、备份与串口陷阱

- 端口 `/dev/cu.usbmodem101`；ESP32-S3，16MB flash；安全启动和 flash 加密关闭。
- 当前实际运行 **2.6.5 / ota_1 / VALID**，仍是原来的 Grok 画法，没有新 Aora 表情。
- 代码候选 d1d6f8e；console app SHA256：`e7ed8a834ef378b65129e3a465e9b515f4714ef88b2b004fae51bbd68d369667`。
- app 大小 3,349,136 bytes；slot 0x3f0000。写入 ota_1 0x410000；NVS 0x9000；otadata 0xd000。
- 原 bootloader、分区表、ota_0 的 2.5.3、assets 均保留。
- `work/hardware-test/service-flash-manifest.json`、`flash-service.log` 为本次刷机证据。
- 只读观察完成：uptime **636 秒**，整个观察期间无重启，page 一直是 1；int_free 52631、int_min 33468；ota valid。
- `service-observe-summary.json` completed=true，`service-observe.raw` 为原始私有日志；监控 session 46189 已 exit 0，串口释放。
- `pv_failed=1` 来自先前有意进行的回滚演练，不能当成当前自然崩溃。
- `fps60` 指标有旧统计口径问题，不得作为真实动画 FPS 验收，更不能宣称达到 60fps。

**串口注意：普通 pyserial 的打开/关闭可能改变 DTR/RTS，触发 USB_UART_CHIP_RESET。** 被动观察使用 `work/hardware-test/serial-observe.py` 的 QuietSerial（覆写 DTR/RTS 更新为空操作，清除 HUPCL）。不要普通打开串口或 HardReset 来“看看日志”。不要恢复自动切页脚本。

恢复材料在权限受限的 `work/device-backup/`：
- `full-flash.bin` 原始 16MB 完整验证备份，SHA256 `d7a01c21f3b574f2f574ba650438022bcfe753db283da3dacb4b525957b97766`。
- 原 ota_0 2.5.3 SHA256 `41be2146cebdcb578975bd94f631e40f48f40d13c829d9183d42ba3983439a00`。
- `RECOVERY.md`、原 otadata、原/当前 NVS、`service-settings.json`、`service-nvs.csv/bin`。
- NVS 重建时已逐项语义比对，只添加 wifi/ota_url；不要无故覆盖整片或 NVS，避免撤销服务恢复。
- 旧候选回滚实测：待验证镜像 10 秒主动 reboot，回到 ota_0 2.5.3；otadata seq1 VALID、seq2 ABORTED。证据 `rollback-proven.json`、`after-rollback-otadata.bin`、`rollback-serial.raw`。

## 5. Aora 表情移植：当前 WIP 和确定的缺陷

源项目已克隆至 `work/aora-bot`，上游 **sam70361/aora-bot@e3b6148c818da4a8e1966f2bc89cdb3cee473b73**，GitHub 仓库已核实。
本机 `/Users/mima0000/groki-bot` 也有先前移植，可参考 `main/aora_face.hpp`、`main/aora_ring_data.hpp`、`tools/aora_rings/convert.mjs`，本次没有修改它。

本次选择 emotion-ball“球球”，并已告知用户：浅色球体、深色眼睛；5 状态映射：
- idle → 02 待机放空
- listening → 35 等待输入
- thinking → 30 思考中
- speaking → 10 开心
- offline → 12 失落

版权：Emotion Ball Community License，非商业免费；球形角色视觉形象禁止商业用途。已复制完整 LICENSE / NOTICE 到 `firmware/board/third_party/aora/`，保留数据署名。用户目前是个人手表项目；不要宣称素材可商用。

改动文件：
- `firmware/board/gw_grok.c`：替换原橙色圆环/几何眼睛，绘制浅色渐变圆球和上游 48 点眼睛；按时间插值和眨眼，5 状态、聆听音量条、思考彩点、字幕；隐藏时暂停计时器，只有 avatar 220×220 区域主动 invalidation；无网络/持久化改动。
- `firmware/board/gw_aora_data.h`：14 对轮廓、5 状态，静态 int16 坐标，不保留外部分配指针。
- `firmware/board/sim/convert_aora.mjs`：用隔离 vm 加载上游 rings.js/emotions.js，生成坐标/状态数据。输入路径用 execFileSync 传 Git，无 shell 拼接。
- `firmware/board/sim/aora_preview.c`：真实 LVGL 的脱机渲染、隐藏生命周期、状态循环检查；无串口访问。
- `firmware/board/sim/run_aora_preview.py`：编译 LVGL+真实 UI，ASan/UBSan 运行。
- 许可文件两份。

已经通过：
- `node --check firmware/board/sim/convert_aora.mjs`
- gw_grok.c C11 `-Wall -Wextra -Werror` 语法检查。
- 主机 ASan/UBSan：五种状态、250 次 hide/show、每个轮廓池和形变、非法状态、隐藏无重绘；heap delta=-8 bytes，无内存错误。

**尚未通过视觉验收，不能发布：**
刚查看 `work/aora-preview/states.png`，**眼睛内部有明显斜纹/细缝**，很可能是当前 46 个独立三角形条带填充的抗锯齿拼缝；浅色球体还有 RGB565 渐变色带。修复应先集中在眼睛填充：整体轮廓一次 rasterize/遮罩或其他 LVGL 支持的无拼缝画法，不能仅因 sanitizer 通过就认为完成。需检查真实 LVGL 9.5 API 和当前配置，不要猜可用接口。主机图不是手表截图。

预览文件：
- `work/aora-preview/states.png`：五态并排，已查看，存在上述缺陷。
- `work/aora-preview/{idle,listening,thinking,speaking,offline}.png`
- `work/aora-preview/idle.gif`：48 帧动画。
- `work/aora-preview-build.log`：测试成功输出。

重跑：
```
node firmware/board/sim/convert_aora.mjs work/aora-bot firmware/board/gw_aora_data.h
python3 firmware/board/sim/run_aora_preview.py --lvgl work/firmware-build/managed_components/lvgl__lvgl --output work/aora-preview
```
runner 的 obj 缓存仅按索引存在性复用 LVGL，当前固定 LVGL checkout 可用；若改 lv_conf、编译选项、LVGL 版本/文件集，需要清空该专用 obj 缓存再编译。UI 源始终重编。PPM 用本机 Pillow 转 PNG/GIF，已有 Pillow，不必安装依赖。

## 6. ESP-IDF 编译环境和后续顺序

- `work/activate-idf.sh` 设置工具路径并 source IDF export。
- IDF commit `9a97f6c54ec638111ce55cd36581b3c192f15207`，GCC esp15.2.0_20251204。
- `work/firmware-build` 为上游 `39d23aee5b8682f77bbdf9cdc66799456b5ad408` 加板目录和 consolidated patch。
- **该 build tree 仍是之前 d1d6f8e 版本，尚未同步本次 Aora 源码。** 不要直接拿旧 bin 声称新表情已编译。
- 板目录实际位置先核实 `work/firmware-build/main/boards/m5stack/stopwatch`。只同步当前相关源码和新 header/许可，不覆盖 sdkconfig、NVS、服务器或其他内容。
- compile 命令必须 cwd=`work/firmware-build`：
```
bash -c '. ../activate-idf.sh >/dev/null 2>&1 && idf.py -B build-console -D SDKCONFIG=sdkconfig.console -D GW_TEST_CONSOLE=1 build > ../hardware-test/build-aora.log 2>&1'
```
- public 使用 build-public / sdkconfig.ble / GW_TEST_CONSOLE=0。
- 原配置是公共默认 OTA 地址，私有地址仅来自设备 NVS；没有 secrets_gen.h。

建议接手顺序：
1. 读本接力和 git 状态，明确 Aora 仍是 WIP，服务已修好。
2. 修复眼睛拼缝并查看五态图、动画、字幕；保持隐藏零计时器重绘，不引入每帧大内存分配。
3. 重跑 targeted preview/ASan；随后同步构建树，console/public 编译和分区体积检查。
4. 写针对当前已刷固件 d1d6f8e 到精确新候选的 differential-review，覆盖本次数据/渲染/计时器和资源增长；有缺陷先修。
5. 仅在候选和审计一致、恢复路径确认后再考虑刷机。不要重新自动切页测试；不要为了展示新图标擅自跳到 Grok 页。
6. 报告区分：主机验证 / 真机已刷 / 真机实际观察；不要把模拟器时间推成设备 FPS。

## 7. 之前字体与 OTA 审查结论（保持）

完整报告 `docs/relay/REVIEW-2.6.5-2026-10-25.md` 和 `AUDIT-SERVICE-2026-10-25.md`。

- F20/F21 fallback 捕获 LvglBuiltInFont 对象内的 lv_font_t 裸指针，旧 shared owner 释放后确实 UAF；ASan 在不手动 poison 的复现中确认。
- 上游回调时机位于新主题设置之后、旧 owner reset 之前，锁成功时正确；但是 DisplayLockGuard 可失败，SetTextFont 原来未检查。已修。
- 原回调只 invalidate 未更新 label metrics；真实 LVGL + 生产 SetTextFont/回调的负例断言失败。改为 style change 通知，并补相关显示锁检查。
- 200 次字体替换、生命周期与锁失败验证通过；原 host 63 检查通过。
- 静态 fallback 生命周期简单，但不能无条件替代动态中文覆盖；没有引入永久泄漏旧字体。
- 当前 a331 健康验证仍为 30 秒；1–2 分钟后崩溃未必会触发已 valid 固件回滚。不能把字体问题说成所有 OTA 循环的唯一解释。
- **服务器 OTA 反复下发循环防护尚未部署，P1 真机 FPS 目标尚未完成验收。** 当前服务连接修复和图标替换不代表这些旧目标完成。

## 8. 交付目录

公开交付文件放在：
`/Users/mima0000/Documents/Codex/2026-10-25/https-github-com-sunnysdady-grokwatch-https/outputs`

现有服务修复报告和 `grokwatch-todo-service-62f3849.patch` 已导出，导出时检查没有私有地址/桥接 token。接力结束将复制本文件和 WIP 补丁至同目录。**不要把整个 work 目录打包**。

## 9. 用户最新确认：按“完整手表系统”收尾交付

用户随后询问“现在能不能收尾交付一个完整的手表系统”，并要求将以下全部写入接力文件，因为当前额度不足。**本轮只保存交接，不继续开发或刷机。下次接手以完整系统收尾为目标，锁定现有功能，不再扩功能。**

当前判断：可以继续收尾，但现在不能宣称完整交付。服务恢复、字体修复验证和约 10 分钟无重启，仅覆盖部分条件，不等于全功能验收。

### 必须完成的四项

1. **表情界面**：修复 Aora 眼睛拼缝，检查五种状态、动画、字幕及隐藏时开销；完成目标固件编译、分区体积检查。当前 WIP 不能直接刷入。
2. **稳定性与升级恢复**：补齐固件端和服务器端 OTA 失败后重复下发的循环防护；审查已有实现，避免重复修改。验证新候选启动、联网加载资源、长时间运行和恢复路径。既有旧候选回滚演练不能代替新候选全部验收。
3. **真实流畅度**：修正或核实统计口径，测量真机滑动帧率，目标 60fps、最低 30fps。不得引用现有 fps60 字段或模拟器数据直接宣称达标。若未达标，记录实测条件、瓶颈和差距，按收益排序优化现有功能。
4. **功能全链路**：真机验证语音对话（麦克风→识别→Grok→播放）、待办新增/显示/完成、手机连接及导航。现有 HTTP/模型探针不能代替这些链路。涉及手表切页、滑动、点击或语音输入时由用户手动操作，Codex 只观察与记录；不得恢复自动切页。

### 收尾执行规则

- 先读取本文件及现有审计，确认实际分支、设备版本、备份和运行服务，再继续。
- 原服务部署已完成，避免无故重新配置模型、替换服务器或新增并行服务。
- 保持 main 不动；使用当前工作分支或新的明确收尾分支，提交可追溯改动。
- 每次部署/刷机必须以实际运行基线对精确候选进行 differential-review；有严重问题或必要验证失败时先修复，不得将未完成审计标记通过。
- 用户此前的刷测试固件授权与“不自动切页”约束一并保留；不要把授权扩大为发送 Grok BOT 消息或任意业务指令。
- 需要用户操作才能完成的验收应明确列成短步骤和预期结果，不得编造测试完成。暂时无法验证的功能标记待验收。
- 完成后统一汇报；不增加与完整现有系统交付无关的新功能、架构重构或优化。

### 最终交付清单与完成判据

- **可用固件**：准确版本、候选 commit、二进制 SHA256、大小及目标分区；明确正式版与测试控制台版，明确实际刷入哪一版。
- **对应源码**：可追溯提交/补丁、板目录外的上游补丁、构建环境与复现命令；保留第三方许可。不得携带密钥、私有地址或私人数据。
- **恢复材料和步骤**：保留已验证原始备份，并说明回滚/恢复适用范围；原始整片/NVS 等敏感材料只保存在私有目录，不放公共交付包。
- **验收报告**：逐项写通过、失败或待验证，附测试版本、条件、时长、日志/截图位置。覆盖上述四项，区分主机模拟、服务端探针、真机验证和用户手动验证。
- 未通过项目不能写“已完成”；未满足完整系统验收时只能交付明确标注限制的候选版本，不能宣称完整系统已交付。

本节源于用户最新收尾讨论；截至保存时没有新增开发、编译、刷机或功能验收结果。
