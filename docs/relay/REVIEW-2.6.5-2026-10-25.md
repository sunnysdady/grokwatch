# 2.6.5 字体修复审查

基线：handoff/homeview-design@a331b52cc2d5fe1e8d507bc581075b4bbe1b21ec。
审查分支：codex/review-2.6.5。结论：**原提交需要改，不建议原样作为已验证固件刷入；下述补强已实现。**

## 1. 根因及其他字体指针

根因成立。上游 LvglBuiltInFont 的 font_ 是对象内的结构体副本；shared_ptr 管理的包装对象释放后，指向该成员的裸指针失效。nav_ui.cc 初始化把主题 font() 借给 gw_ultra.c 的 f20_rt/f21_rt.fallback，SetTextFont 释放 previous_light_font / previous_dark_font 后就可能触发 UAF。不是 gz_20 静态字体本身被释放。

已运行提交中的 repro_font_uaf.c：NOPOISON=1，bug 模式仍由 ASan 报 heap-use-after-free，fix 模式通过。PC=0xfffffffd 与失效函数指针相符，但该 PC 单独不能唯一证明根因。

本次检索板级 F20/F21、gw_grok、标签样式，以及实际使用的 LcdDisplay、LvglFont 和 DynamicGlyphCache：未发现第二个已经确认的同类旧主题字体缓存。屏幕样式的主题字体由 SetTheme 更新，其他文字标签继承屏幕或使用静态 F20/F21；图标字体属于独立主题所有者，本次 SetTextFont 不释放它们。此结论限于当前 StopWatch 调用链，不宣称上游全部板型都已审计。

DynamicGlyphCache::EnsureFont 返回显示对象成员 font_ 的地址，Rebuild 重建其数组并重新绑定 dsc，不会释放该 font_ 对象；正常生命周期和显示锁内更新可保持安全。原 ClearTextGlyphs 忽略取锁失败，存在数组释放与渲染并行的条件性风险，本次一并改为取锁失败即返回。不要直接释放/深拷贝未知 fallback 子链，也不要将 F20/F21 设为自己的祖先；当前实际资源字体入口没有构造这种环。

## 2. 回调时机和显示锁

原回调位于 SetTheme 之后、previous_*_font.reset() 之前，正常取得锁时顺序正确。旧 shared_ptr 本地变量让 SetTheme 内的同步布局仍可读旧 fallback。ESP LVGL port 使用递归互斥锁，SetTheme 嵌套取锁不会因同线程重入而自锁。

发现并修复：

- **HIGH / 条件性竞态**：SetTextFont 创建 DisplayLockGuard 后没有检查结果。锁等待 30 秒超时仍修改主题、调用回调并释放旧字体，与“始终在锁内”的保证不符。现在失败返回 false。nav_ui 的初始化、调度、BLE UI 更新及待办完成回调也在取锁失败时返回，确保唯一的回调注册调用点受锁保护。
- **MEDIUM / 可复现布局错误**：on_text_font_changed 仅 lv_obj_invalidate，F20/F21 地址没变，LVGL 可保留旧标签尺寸。直接执行原生产 SetTextFont 和原生产回调的测试，在替换后 `label width == new glyph width * 2` 断言失败。改用 lv_obj_report_style_change(nullptr)，通知布局重新计算，覆盖当前屏及隐藏页面。
- 注册接口仍为单回调、静态存储，适合本板单显示实例。已明确注册须持显示锁、借用字体仅有效至下一次替换；若未来支持多显示器，应改成每显示实例的生命周期管理。

测试没有仅模拟一个“类似”的回调：从补丁提取实际 SetTextFont，从 nav_ui.cc 提取实际 on_text_font_changed，使用真实 LVGL、真实 LvglBuiltInFont 及生产 F20/F21。仅用主机替身替换 ESP 锁与主题容器；未模拟 FreeRTOS 的真实并发调度。

## 3. 静态 fallback 方案

静态字体从生命周期上更简单，但必须直接指向真正的静态常量；再次套 LvglBuiltInFont 再借其 font() 仍可能指向堆对象成员。

不建议直接把 fallback 换成 gz_20：F20/F21 本来就是 gz_20 的副本，缺失字符仍然缺失。现有 basic 内置字体也不能自动视为完整 CJK；换静态字体会绕过资源字体和动态补字，需要重新确认覆盖字符、字号、排版和 Flash 预算。当前优先保留经补强的回调，保持原有动态字形功能。不能通过永不释放所有旧堆字体来掩盖 UAF。

## 4. OTA 循环

字体 UAF 可以解释崩溃。只有在 PENDING_VERIFY 期间发生复位才会触发启动程序回滚，服务器继续提供同一坏版本才构成升级循环。

a331b52 的健康检查最早 30 秒即标 VALID；若实际已在 30 秒确认，之后 1–2 分钟崩溃不会因这个机制自动回滚。因此不能只凭相同崩溃栈认定全部历史循环同因，应结合 pending/valid 状态、复位原因和服务端下发记录。延长确认窗口并加同设备/版本的隔离是独立防线；本字体审查不等于服务器防循环已经上线。

## 验证与范围

- 原生产回调的布局负对照失败，证据保存于 work/review-2.6.5/test-original-callback.log。
- 补强后真实 LVGL ASan/UBSan 测试：200 次字体替换、F20/F21 中文字宽及标签布局、旧所有者确实释放、回调持锁且旧所有者仍存活、取锁失败保持原状态、空字体/解除绑定均通过。
- 原始独立复现：不手动污染内存仍触发 ASan UAF；静态字体 fix 通过。
- 现有主机检查 63/63 通过；ESP32-S3 固定工具链对改动的 lvgl_display.cc、nav_ui.cc 两个编译单元编译通过；补丁重新应用与源码逐字节一致，diff 空白检查通过。
- 本轮尚未完整链接/刷入本审查分支，不用先前另一份同名 2.6.5 的构建或设备运行冒充该提交验证。完整固件构建、联网资源替换、动态补字、真机运行至少 10 分钟后，才可决定发布。
- 私有控制台 prof 的任务快照、计数及跨任务读取并不因加大栈而全部解决；它不是字体回调的证明，不应作为当前分支已可靠测得 FPS 的依据。

改动同步到 firmware-patch/2.6.5-font-fallback.diff 和 upstream-outside-board.patch；无 main 改动，无 push，无服务器部署，无凭据或内网地址。
