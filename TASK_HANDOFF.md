# GrokWatch 本地修复与图片裁切接续

更新时间：2026-10-26（Asia/Shanghai）
基线：main / 07c8aa5edf754eb916adcda488a72969b6284a1e。
授权：正式修复待机唤醒计时，补齐回归并接入常规入口；改善背景图片画质，加入圆形裁切预览。最新授权：用户已连接手表，允许准备就绪后刷机；仍未授权发布。未提交或推送。刷机前仍需完整固件构建及回滚条件。

## 已完成

- firmware/board/gw_sbsm.c：恢复唤醒时 last_change_ms / last_input_ms 赋值，分别独立成行。
- firmware/board/sim/test_sbsm.c：修复漏发动画完成事件的旧测试；覆盖普通待机 9 类唤醒、深度待机 8 类允许唤醒、完整 20 秒闲置边界、第二次唤醒、250 ms 红键防抖边界。保留深度待机忽略触摸的原策略。
- firmware/board/test/run.sh：接入状态机回归；在临时目录编译，避免覆盖仓库内已有二进制；编译/测试失败立即返回非零。
- Android WatchBgPixels.java / WatchBg.kt：取消导入时强制压暗 40%，解码目标设为 sRGB，RGB565 转换加入 4x4 有序抖动，透明像素合成到黑底；协议和 466x466 输出不变。固件夜间调暗策略未改。
- Android WatchBgCrop.java / WatchBgCropScreen.kt / MainActivity.kt：选图只进入圆形裁切预览；拖动、双指缩放、滑杆 1–4 倍缩放、重置、重选、取消；确认且已连接后才开始传输。预览为最长边 1600 的解码图，最终按同一归一化选区从原图输出，不使用预览图再编码。小于 466 像素的选区提示画质不足；读取失败可重选，传输失败保留当前选区。
- 图片传输沿用既有 BLE 队列协议，无设备完成 ACK，因此提示为“已提交传输，请在手表确认效果”，不声称落盘已完成。
- android/test/run.sh：无 SDK 的 Java 图片编码与裁切几何回归入口。README 已记录入口与新图片流程。

## 验证结果

- 新增状态机回归修复前：110/183 通过，73 项失败，常规入口返回 1。
- 修复后常规入口：返回 0；模型 32 项、健康检查 25 项、输入 16 项、方向测试 OK、待机 183/183。
- 状态机额外以 -Wall -Wextra -Werror + AddressSanitizer/UndefinedBehaviorSanitizer 编译执行：183/183，无检测异常。
- CC=false 故障注入：常规入口返回 1。
- 图片编码 361 项通过；裁切几何 1807 项通过。覆盖端点/纯色/字节序、全部灰阶平均亮度、透明像素、单像素棋盘格、输出大小、横竖图、边界、缩放及预览/源图选区一致性。
- sh -n、git diff --check 通过；已人工审阅改动。
- 日志在 work/review-2.6.37/{regression-before,regression-after,sanitized,android-tests}.log。

## 未完成和阻塞

- 完整 APK 尚未编译：本机起初仅有 JDK 26，无 Android SDK。尝试补工具链时，Android command-line tools 下载 180 秒超时（54,738,935 / 143,250,852 字节）；JDK 21 的未完成下载随后主动停止，没有后台安装任务遗留。
- 这是工具链获取失败，不是已验证的源码编译失败。不要将纯 Java 测试当作 Kotlin/Compose 编译通过。
- 下载日志：work/review-2.6.37/sdk-download.log、jdk-setup.log。SDK 部分下载：work/android-tools/commandlinetools.zip（不完整，不能解压使用）。
- 未执行 Android 图片解码/EXIF/大图运行测试、裁切页真机或模拟器交互验收，也未获取用户原图和实拍对比；抖动对真实照片的主观观感仍需验收。圆形预览只显示背景选区，不模拟实际表盘指针和夜间亮度。
- 固件完整上游工程不在当前仓库，未编译固件、连接设备、读取设备版本、测量功耗或刷机。
- 之前发现的省电硬件调用不检查返回值问题不在此次唤醒计时修复范围，仍未修改。

## 下一步（不扩大范围）

1. 在具备兼容 JDK 21 和 Android SDK 36 的本地环境执行 android/gradlew assembleDebug，解决实际编译问题；不安装到设备或发布，除非取得对应授权。
2. 在可用本地模拟器验证：选图不发送；圆形拖动缩放无空白边；取消不发送；确认只发送一次；断连、读取失败、旋转屏幕和重试；EXIF 旋转图及高分辨率/透明/低分辨率图片。
3. 用用户原图核对手机裁切与设备结果。已有背景是旧算法预先压暗后保存的，更新 App 后需重新导入；仅换固件不能恢复丢失色阶。
4. 本轮新增文件及 work/ 都未提交；work/ 含测试程序、日志和未完成工具下载，不应纳入发布提交。

Android 裁切 API 依据：https://developer.android.com/reference/android/graphics/ImageDecoder （setTargetSize 后的 setCrop 使用缩放后坐标）。

## 2026-10-26 本轮进度

- JDK 21 已通过 Homebrew 安装并验证（21.0.13.0.2）。JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home。
- command-line tools 已完整下载解压，SDK 位于 work/android-tools/sdk。安装 SDK 36/build-tools36/platform-tools、Gradle8.14.3、ARM64 API35 ATD 模拟器中。日志位于 work/android-acceptance/。上文下载失败属于历史状态。
- 曾遇 macOS dataless 文件读取等待，已恢复；未修改云同步设置。
- MainActivity 首页将表盘提前；连接权限、骑行设置及开发者选项默认收起；调试、更新源、日志在开发者组。离线也可进入选图裁切，确认传输仍要求连接。
- 新增 androidTest 页面/解码用例：WatchBgCropScreenTest、HomeScreenTest，尚未运行，不得声称验收通过。截图预期输出至模拟器 app external files 的 crop-preview.png、home-collapsed.png。
- 新增 android/test/verify.sh：Java 回归 + APK + AndroidTest 编译 + lint；--emulator 仅接受 ANDROID_SERIAL=emulator-* 后运行连接测试。
- 本轮已再次通过图片编码361、裁切几何1807、待机183及其他固件常规回归；git diff --check 通过。
- USB /dev/cu.usbmodem21101 可见，未打开串口，未刷机；完整固件上游工程仍缺失。
- GUI 验收批处理清单：work/android-acceptance/acceptance-manifest.md。

## 本轮工具预算检查点（未完成，不可标记交付）

- JDK21、Gradle8.14.3、SDK36、Build Tools35/36、platform-tools和模拟器程序已安装。API35 ARM64 ATD系统镜像仍下载中。
- 主 APK/lint 构建正在运行，tool session 13020，日志 work/android-acceptance/build.log；最后观察到 processDebugNavigationResources，尚无 APK 编译通过结论。
- 页面测试编译 session83001 已失败：与主构建重叠时 Build Tools35 未安装完成；主构建随后已完成该组件安装。下一轮等主构建结束后串行执行 sh android/test/verify.sh，避免并行 SDK 安装。
- 模拟器下载 session57678；日志 work/android-acceptance/emulator-install.log。仅下载/本地构建留在运行，不会自动刷机或发布。须核查进程、日志和剩余磁盘后继续；最后磁盘约7GB可用。
- 下一步：编译及lint问题修复；创建本地ATD模拟器；ANDROID_SERIAL指定模拟器运行页面测试；拉取crop-preview.png/home-collapsed.png审阅。未运行页面测试、未完成视觉验收、未刷机，禁止宣称这些已完成。
- 本轮遵守单代理及60次调用检查点限制；后续从现有缓存继续，不重复下载或重做已通过回归。

## 最新增量（2026-10-26，电量/充电与手机无线调试）
- 用户新增：电量不清楚、没有充电界面/动画；已实现常驻百分比/低电量色、充电开始8秒可点击关闭界面、1Hz闪电动画，导航/骑行/语音/模态忙碌时不遮挡，深度待机充电唤醒。新增gw_power.h策略及13检查；SBSM充电唤醒扩为199/199通过。此固件UI尚未完整编译/视觉验收，不可刷入。
- 修改gw_faces.h、gw_ultra.c、nav_ui.cc、gw_sbsm.h/c、sim/test_sbsm.c、test/run.sh，新增test/test_gw_power.c。现有充电引脚不能识别满电仍插电，因此不虚报已充满。
- Android新增真正ImageDecoder中间色亮度测试，防止60%压暗回归。修复BleLink在Android12调用API33接口的2个lint错误，使用SDK版本分支兼容。
- 主APK+测试APK+lint在本地输出根构建全部成功，日志verify-local.log。debug applicationIdSuffix=.qa，显示GrokWatch QA；手机已有原App签名与本机不同，禁止卸载或覆盖原App。一次旧APK安装因签名不一致被Android拒绝，未更改原App。
- SDK已移至 /Users/mac/.cache/grokwatch/android-sdk，原work/android-tools/sdk为符号链接；云端回收中间文件导致AAPT读取magic失败，改用GW_ANDROID_BUILD_ROOT=/Users/mac/.cache/grokwatch/android-build和--project-cache-dir /Users/mac/.cache/grokwatch/gradle-project后构建成功。
- APK: /Users/mac/.cache/grokwatch/android-build/app/outputs/apk/debug/app-debug.apk；测试APK在同目录树outputs/apk/androidTest/debug。
- 手机ADB已配对（不保存配对码）；地址192.168.0.169:38249曾掉线，自动重连后serial为adb-51ce23e6-pxzmWK._adb-tls-connect._tcp。无线端口/serial需当场核对。QA安装与instrumentation session15372，日志work/android-acceptance/phone-tests.log，需读最终结果。
- 模拟器现在也启动成功，emulator-5554；日志emulator-3.log。早先磁盘不足失败，macOS后来释放空间，未删除用户数据。
- LVGL语法验证依赖下载失败（curl18 partial），未把固件UI当成已编译。临时下载在/Users/mac/.cache/grokwatch/lvgl-check。

## 手机验收结果补充
- GrokWatch QA与QA测试包已安装到用户手机，原io.grokwatch.app仍保留。主APK和测试包/lint完整通过verify-local.log；将Espresso升级3.7.0、runner/rules1.7.0后verify-test-upgrade.log再次通过。
- 手机API37：最初Espresso3.5.0不兼容InputManager反射，7项全受阻；升级后phone-tests-2.log记录7项运行、6项通过、1项HomeScreenTest被OEM拒绝GRANT_RUNTIME_PERMISSIONS（不是页面断言失败）。亮度ImageDecoder、原图像素、裁切交互、离线、忙碌、损坏图6项已真机通过。
- 圆形裁切截图已拉取并视觉检查：work/android-acceptance/crop-preview.png（测试主题为紫色，不是正式主Activity的橙色主题）。
- 首页改在emulator-5554运行HomeScreenTest，session97991，日志emulator-home-test.log；检查最终结果和home-collapsed.png。
- firmware UI完整构建/视觉验收/实机充电检测仍未完成，不得宣称全部交付或刷机成功。没有刷机或发布。

## 本轮收束结果
- emulator-home-test.log 最终 OK (1 test)。home-collapsed.png 已视觉检查：首页表盘入口、连接权限、骑行设置与开发者分组布局正常；低频开关默认折叠。全部7项Android页面/解码测试分布为手机6项、模拟器1项通过，不等同于手机7项全部通过。
- 手机原App仍保留，QA独立安装。手机权限限制没有绕过；QA连接手表前可能需要用户在系统界面允许附近设备/定位权限。
- 本轮达到工具调用检查点，后续限定完成固件完整构建、充电界面视觉/设备验证及已有图片实机传输验收，不扩展范围。需恢复上游构建依赖；先检查项目构建说明，不重复无效LVGL部分下载。用户已授权准备充分后刷机，但当前固件未过完整编译，尚不具备刷机条件；未发布。
- 用户旧照片需从原图重新导入，已压暗文件不能自动恢复；手机像素回归通过不等于手表屏幕最终观感已验证。

## 2026-10-26 固件恢复与真实 LVGL 回归检查点
- 用户明确授权：固件验证无问题后刷手表，不发布；新增待机翻页黑线带入主表盘数秒。用户随后确认新App照片偏暗是夜间模式，关闭后正常，因此保留原有夜间照片遮罩，不再改变该效果。
- 已按固定提交恢复上游：/Users/mac/.cache/grokwatch/firmware-build（39d23ae）；应用upstream-outside-board.patch，再手动补2.6.6 OTA循环保护和2.6.24 audio/O2补丁。候选版本2.6.38-qa，sdkconfig.ble开启PM_ENABLE。没有secrets_gen.h，不公开任何产物。
- ESP-IDF /Users/mac/.cache/grokwatch/esp-idf，9a97f6c5，所有子模块核对unmatched=0。Xtensa15.2.0安装好，Python3.12环境在/Users/mac/.espressif/python_env/idf6.1_py3.12_env；cmake/ninja pip安装完成后已启动构建。必须 export ESP_IDF_VERSION=6.1.0（手动环境缺此变量会导致component manager TypeError）。
- 构建脚本work/build-firmware-local.sh；正在执行session29368，日志work/firmware-build.log，最后在组件获取/配置阶段。其余IDF辅助工具安装session24049仍运行，日志idf-install.log；未完成，不要重复启动安装。早先submodule父shell70406已停止，续跑session54557完成子模块；无设备操作。
- LVGL源码 /Users/mac/.cache/grokwatch/lvgl v9.5.0 (85aa60d)；模拟器库 /Users/mac/.cache/grokwatch/power-sim，截图 /Users/mac/.cache/grokwatch/power-preview；SIM64现在不依赖32位cbin字体，使用内置中文字体，LVGL_DIR可指定，md5sum改shasum兼容Mac。
- 已修复电量UI的lab未定义、闪电字体缺字（改矢量线）、100%数字挤压。真实渲染10场景通过，缺字0。主机回归总入口支持GW_TEST_UI=1，README已给命令；日志work/firmware-full-regression.log。
- 待机新修复：gw_ultra.c 黑色铰链g.bg_opa跟随opa；退出取消sb_fp翻页动画/归并旧字；隐藏完成invalidate主屏。新增sim/sim.c断言：退出只剩淡出动画，退出300ms/2s framebuffer与稳定主表盘相同。测试曾因正常页码圆点1600+400ms淡出产生344像素差异，已先等待2500ms稳定，不是忽略像素；新断言全部通过。
- 最新修改已复制到本地缓存构建的board目录，尤其gw_ultra.c/sim/test。主机截图见work/firmware-validation/power-contact-sheet.png；结果见RESULTS.md。未宣称真机残影已经解决。
- 当前完整固件编译仍未通过/未取得bin，不可刷机。待构建完成：处理真实编译错误，审查配置差异，核对app/slot大小；恢复安全备份/回读/rollback刷机流程（旧work/hardware-test脚本本机不存在）；开串口会复位，须保留启动健康检查与等待60s原则，避免擦除NVS/assets。串口/dev/cu.usbmodem21101此前存在，尚未打开。不要发布。


## 2026-10-26 本轮刷写现场事实（覆盖上方过期状态）
- 完整构建已通过两次；work/firmware-build-final.log。固件2.6.38-qa，3452592字节，SHA256 4384ce0905aee5e2370e8d62a524e542b2c263149978c611c6731df6ecde0f9b。路径 /Users/mac/.cache/grokwatch/firmware-build/build-console/xiaozhi.bin。
- 首次只读备份发现历史原厂2.5.3记录不适用当前设备，安全停止，无写入；实际 ota_0=2.6.33 (9907545882d618baa756a46df1c0d44b2fa3129df8c587666e7caae6489fbb62)，ota_1=2.6.37 (309e5d5ec810cd98796cf2dbac3bd780a78b4443d3d61eadc6683a6e4d27d0aa)，两项VALID。现场STAT证实运行ota_1 2.6.37 valid。
- 只读备份 /Users/mac/.cache/grokwatch/device-backups/pre-2638/full-flash.bin，包含秘密，不复制到公开报告。旧脚本RTC复位后USB重枚举，保持90秒后状态写失败；随后安全窗口外重连确认2.6.37正常。修复脚本支持90秒后重连并再等待90秒。
- 门禁 work/firmware-validation/FLASH_GATE.md 已通过。work/hardware-test/flash_verified.py 锁定当前两镜像SHA和VALID选择器，改写inactive ota_0及seq3 NEW，保留seq2 VALID的ota_1 2.6.37回滚；写后回读全部保护区。自测通过。
- 用户已授权刷机、不发布。刷写命令正在session98046执行，日志work/hardware-test/flash-2638.log；新备份和结果 /Users/mac/.cache/grokwatch/device-backups/flash-2638。不要另开串口或中断，等待其结果和boot_validated；启动健康验证后再做实机视觉验收。

- 用户在2.6.38-qa刷后验证中追加：取消卡片切换之间的震动或减弱。选择取消卡片切换震动、保留其他提示震动。当前二进制不含此调整；本轮已首次压缩，按用户上下文预算规则不新增实现范围，下一有界阶段执行并回归，随后沿已授权流程刷机。不发布。

- session98046退出0，实际刷写和回读通过，boot_validated=true；2.6.38-qa运行ota_0 VALID，STAT 177/229秒启动计数均71、内部空闲约58KB。结果已脱敏复制work/firmware-validation/FLASH_RESULT.json和DEVICE_STATUS.txt；私有原始日志仍在缓存备份目录。没有发布。
- bat控制台无有效输出，stat正常；驱动usb_serial_jtag_write_bytes一次xRingbufferSend整个字符串，旧bat历史可能超过环容量。此诊断需下阶段最小修正/验证，当前不声称已核对硬件电量读数。
- 已通过async问题请求用户实物检查电量、拔插充电动画、翻页唤醒黑线。待反馈；设备软件健康验证与实物观感验收分开。
- 下一阶段取消卡片切换震动，并验证该变更；再次刷写前必须重新读取当前分区/选择器！现有flash_verified.py严格锁定刷前2.6.33/2.6.37状态，不能直接重复运行，也不能盲目改SHA放行。现在ota_0是新版本、ota_1保留2.6.37。

## 用户实机反馈：充电验收失败（最新）
用户确认电量31%可以看到，但插拔后没有充电状态、闪电动画或充电界面。不能将本次交付标为完成。黑线实物结果尚未反馈。
下一有界阶段优先修充电检测，再处理取消切卡震动。现有策略只在充电引脚 false->true 时显示8秒覆盖；先取得原始充电引脚/电压与UI忙碌状态，区分硬件输入、触发及遮挡，禁止靠假定USB连接就是正在充电绕过。
已确认诊断阻碍：gw_console.cc out()一次发送整个字符串，ESP-IDF usb_serial_jtag默认TX缓冲256字节，底层xRingbufferSend；bat历史最长4096字节，因此整条无法发送。先修为分块发送或添加短状态命令并做边界测试，以便真机读取充电状态。当前固件2.6.38-qa已运行valid，暂未包含该诊断修复或取消切卡震动；没有发布。
本轮达到工具调用预算检查点，已首次压缩；保留当前可运行固件和完整备份，下阶段按新分区事实继续，不能复用旧刷写脚本锁定值。

## 2026-10-26 充电与切卡修复本轮
- 用户已确认黑线残影消失。新目标充电界面/动画、取消切卡震动，验证后刷手表、不发布。
- 对照厂商M5StopWatch-UserDemo/main/hal/hal_pmic.cpp：VIN>4000mV检测外部电源，CHG_STAT低电平才是实际充电。当前代码加入VIN联合判断：0电池/1正在充电/2接电未充电/-1未知；界面改由接电边沿弹出，标题与徽标如实区分。PMIC禁用I2C空闲休眠、检查充电使能/电流引脚设置结果。
- 取消nav_ui.cc go()和drag_release()两处切卡18ms震动，保留其他提示。gw_console_write.h以128字节分块解决256字节USB TX环无法发送bat长历史；新增短power命令含VIN/CHG_STAT/配置/PROG信号。
- 常规测试通过（新增power21、USB23）；LVGL11场景通过、缺字0，contact_sheet已目视验证。日志work/charge-regression-final.log、charge-ui-regression.log；构建work/firmware-build-2639-final.log成功，源码与缓存构建副本核对一致。
- 候选2.6.39-qa，3454000bytes，SHA787fd84dcfaaf9e34c29e6dd1bed8525a682f1c7e0b92a336dbbe98275a881a0。门禁work/firmware-validation/GATE_2639.md。
- session23408运行刷写，日志work/hardware-test/flash-2639.log，私有备份/Users/mac/.cache/grokwatch/device-backups/flash-2639。flash_2639.py严格锁定ota_0 2.6.38-qa seq3 VALID和ota_1 2.6.37 seq2 VALID；只改ota_1+seq4 NEW，保护2.6.38回滚；写后全保护区回读。等待boot_validated与POWER读数，不另开串口。

- 2.6.39-qa已成功刷入ota_1并VALID，up177s、boots72；全部回读通过。原始备份/boot日志含私密信息留缓存，脱敏证据FLASH_RESULT_2639.json、DEVICE_STATUS_2639.txt。
- 真机POWER得到VIN=0mV，charge_pin=1、state=0，cfg=31/err0、PROG=0；充电使能/电流设置已正确，但没有检测到外部输入，不能宣称真实充电验收成功。bat长日志现在正常返回15条，诊断修复已实机验证。
- 已询问用户实际充电底座/接口、重新插拔并观察界面/取消震动。session34526正在运行只读observe_power.py，最多180秒，每5秒POWER，连续3次VIN>4000则结束；输出work/firmware-validation/POWER_OBSERVATION_2639.json。在此期间不要另开串口。黑线此前用户确认修复。

## 本轮最终状态：2.6.40-qa + 正式App0.6.3
- 用户确认2.6.39充电动画出现，但报告5%两分钟跳50%；按瞬时电压换算导致显示跳变。新增gw_battery.h及test_battery.c、每秒滤波/30秒最多1%/不补跳/断电不增，临界区保护共享状态；UI明确~和估算电量。此为稳定性修复，不是精确SOC校准；首次估值仍受电压影响。
- 2.6.40-qa编译和全部回归/LVGL11场景通过，已刷ota_0 VALID；session1171退出0。SHA72e03cbd46745b7b34393debc9cd5abd7973ef6c4c581a20b4f21a9e98466ec3；保留ota_1 2.6.39回滚，私有备份flash-2640。不要复用旧flash脚本选择器锁，当前seq5/seq4 VALID。
- 用户要求正式App，0.6.3/version63正式包名io.grokwatch.app Release签名/lint通过，手机PKH110已安装启动、首屏检查。APK work/android-release/GrokWatch-0.6.3-release.apk；QA保留，部分权限待用户在连接与权限开启，未验证正式包BLE/照片传输。
- 新持久签名 ~/.grokwatch-release，私钥权限600且未输出/上传，未来升级复用，不能丢失、不能提交。原开发者旧签名不同，用户已卸载其旧App。截图最初有347字节系统warning前缀，已提取PNG正文并检查。
- 原始电源只读观察因用户换充电连接断开电脑而未确认外部VIN；用户动画反馈是实机证据。2.6.40调试连接最终VIN0、pin1、level24、vbat3578mV，正确显示未充电。
- 完整结果work/firmware-validation/DELIVERY_2640.md。黑线已用户确认；切卡震动代码移除但暂无用户确认。真实SOC准确性/2.6.40两分钟充电对照仍待受控测量，不能宣称校准完成。没有发布。

## 2026-10-26 实际充放电校准接续
- 用户要求真实校准，不接受只放慢数字；禁止发布。当前算法仍线性电压映射加限速，准确性未验证。
- 本轮没有手表USB串口（仅debug-console和Bluetooth）；已async请求连接，尚未收到回复。没有刷机、没有实机新采样、没有后台采集进程。
- 新增work/hardware-test/capture_battery.py及test_capture_battery.py；现有stat/power/bat只读5秒采集，原始JSONL、非原子标注、无效/冲突剔除、断连留证并停止。4项解析测试通过，不能当实机验收。
- 官方原理图确认PROG低对应425mA设置，450mAh标称下2分钟仅约3.15个百分点理论量，不能当实际电池净电流。未找到库仑计API，不凭空制造SOC曲线。
- 详见work/firmware-validation/BATTERY_CALIBRATION_STATUS.md，含完整实验判据、参考SOC要求及断开调试时需设备端记录的约束。接上后优先读取版本和十分钟基线，确认能否同步充电与调试；保留既有刷机授权，但重新读取分区和门禁后才刷。

## 2026-10-26 已完成真实短时充放电试验（接续优先读此段）
- 用户已接电脑，随后按要求同时接充电、拔充电、重插调试线。当前固件2.6.40-qa ota0 VALID boots73全程一致，无重启。没有刷机或发布。
- 已获得108组有效实测：未充电6组，充电36组/175s，拔线前仍充电6组，恢复调试后放电60组/295s。未充电VIN0/pin1，充电VIN约4.9V/pin0。
- 未充电约3700mV显示36；充电3818→3854mV显示37→43，末段3860mV显示44；拔线后重连3720→3708mV显示41→37。限速仍让充电电压误差滞后回调，准确性未通过。
- 插拔充电均使USB短暂断开；首次重枚举自动恢复，第二次用户重插调试线恢复。断点分段保留，不插值。实际完整充放电周期、真实SOC/库仑积分仍没有，不能声称容量校准完成。无后台采样进程。
- 数据和报告：work/firmware-validation/BATTERY_REAL_TEST_20261026.md，BATTERY_MEASUREMENTS_20261026.json，battery-measured-20261026.csv，以及battery-*-20261026-0[1-4].jsonl。报告明确负载/USB采样影响与参考缺失。
- 本阶段不基于一次短测硬编码电压补偿曲线。下一阶段须完成完整充电终止及固定负载放电曲线，明确独立容量参考；如无电流积分，只能校准该负载剩余运行时间而非宣称通用精确SOC。旧BATTERY_CALIBRATION_STATUS的未连接状态已被此段和新报告替代。

## 2026-10-26 完整充放电长测正在进行（最新，禁止抢占串口）
- 用户明确继续完整校准、修算法/补回归、插拔线时通知，不发布。新一轮实验已启动，并已确认用户同时接好调试线与充电线。
- 独立后台采样进程 PID21664，firmware/tools/battery_cycle.py，实验目录 work/firmware-validation/full-cycle-20261026-01。status.json 为原子更新的简短状态，samples.jsonl 每5秒刷盘（流式读取，不全文输出）。上限12小时；caffeinate -i -w PID 维持采样时电脑不因空闲休眠，合盖仍会受影响。禁止并发打开串口、重启或刷机。
- 当前已实测 charging、VIN约5.04V、VBAT约3.816V，ver2.6.40-qa ota0 valid boots73未变。插拔短暂断开会单独标记并重连；超过120秒无串口或无有效数据则失败，重启/固件变化也失败。
- 充电终止候选要求先检测到至少60秒实际充电，再持续300秒 VIN在、CHG解除、VBAT>=4100；此为候选，不能把CHG解除直接认成100%。到await_unplug提醒只拔充电线、保留调试。放电阶段意外接电则停止；VBAT<=3400持续60秒记录usable_low_endpoint并停止。3400仅原产品可用低端点，不是物理电池空，不绕过保护。
- 新增firmware/tools/battery_capture.py和battery_cycle.py（旧work工具保留）；新增7项状态/缺失数据/重启/终止判据测试firmware/board/test/test_battery_cycle.py，接入常规run.sh；完整常规测试通过，work/firmware-validation/battery-cycle-tests.log。尚未修改电量固件算法，不能把采集测试当算法准确性通过。
- 已建立本线程 heartbeat automation id=grokwatch，每5分钟仅检查状态，阶段变化/故障/需插拔才通知；完成或失败后停用。第一次工具调用缺destination未创建，第二次destination=thread成功（不重复创建）。需维护实验目录monitor-state.json避免重复通知。用户已授权需要插拔时通知。全实验12小时到期则报告检查点，不无限轮询。

## 用户追加核对小智充电方案：已找到官方可复用估算框架
- 当前上游StopWatch源码依然3400–4200mV线性映射，无这块板的通用精确SOC方案：https://github.com/78/xiaozhi-esp32/blob/main/main/boards/m5stack/stopwatch/m5stack_stopwatch.cc
- M5官方UserDemo为3300–4200线性映射+1秒采样7:1滤波，不是实测容量曲线：https://github.com/m5stack/M5StopWatch-UserDemo/blob/main/main/hal/hal_pmic.cpp
- 小智通用AdcBatteryMonitor使用乐鑫adc_battery_estimation，缓存已有0.2.x依赖；该版本计算OCV曲线插值+滤波+充电不减/放电不增，仍不能消除充电端电压影响。不要把这个包装当作解决问题。
- 官方0.3.0增加充满待机状态回调，官方明确充电电流抬升端电压、需硬件CHRG/STDBY信号，并允许自定义OCV-SOC曲线：https://components.espressif.com/components/espressif/adc_battery_estimation/versions/0.3.0/readme
- 采用思路：优先复用官方曲线/状态框架，再针对M5PM1 readVbat采样路径和本板是否有独立DONE可读信号做适配，结合长测数据验证，不能直接接ESP ADC接口也不能把VIN存在+CHG解除当无条件充满。当前未升级依赖、未刷机、不发布。

## 2026-10-26 用户确定按官方机制适配（完整采样继续）
- 新增firmware/board/gw_battery_curve.h：不绑定ESP ADC、可接M5PM1毫伏的自定义曲线插值与显式充满依据状态适配。没有内置/伪造本机曲线，没有30秒1%限速。它目前未接生产路径，旧gw_battery.h仍在设备运行。
- 新增test_battery_curve.c并接常规run.sh；非线性插值/2001点遍历/坏曲线/满电依据边界通过，完整常规测试退出0，日志battery-curve-tests.log；未刷机，不能把基础层测试当最终算法准确性通过。
- 详见work/firmware-validation/OFFICIAL_BATTERY_ADAPTATION.md：官方0.3.0直接ADC依赖不可直接替换M5PM1；其滤波+单调性也不解决充电电压偏置，后续必须单独处理并独立验证。禁止充电期间直接把OCV曲线当真实SOC。
- 长测仍由PID21664记录，最近phase charging、84/84有效、启动计数73未变。不要抢占串口或为了候选编译刷机打断基线；heartbeat grokwatch 继续通知需要插拔线的节点。

## 2026-10-26 充电/接电频繁切换新调查（完整采样继续）
- 用户要求查清根因连同电量回落一起修，不仅改显示，不发布。详情full-cycle-20261026-01/observations.md与charge-switching-analysis.json。
- 918次配置CHG_EN保持开启；cfg_ok=0为成功。源码无周期充电开关，UI用VIN上升沿而非CHG引脚唤醒，排除直接的CHG→唤醒循环。芯片厂商确认终止/再充机制，但无法仅凭现有采样确认其为全部原因。旧线性电压算法确有充电偏置回落问题。
- 不擅自改300秒终止判据，不把状态反复切换解释为精确充满，不更改采集进程或抢串口。等待真实放电段；必要后续鉴别实验须和基线分开。算法最终修复及准确性回归仍未完成。

## 2026-10-26 机身绿灯默认关闭（待下次固件验证安装）
- 用户照片确认USB旁机身绿灯。官方原理图第2页LED1 GREEN由PM_STATUS_LED经R19驱动，连接M5PM1 LED_EN_PP，非心率灯、非充电DONE灯。setLedEnLevel(false)仅修改PWR_CFG_LED_CTRL bit4，不改变CHG_EN。
- 已在InitializeI2c添加关闭绿灯并检查返回值；驱动接口已核对。尚未编译/刷入/实机验收，当前灯未实际关闭，避免重启打断PID21664充放电基线。下次固件统一编译验证，确认cfg由31变15且充电使能保留；LED改变负载后需独立验收电量表现。
- 额外原理图证据：LGS4056H的DONE pin6画有未连接标记；CHRG pin7接PMG2。当前板无该DONE直连软件输入，不能凭CHG解除宣称精确满电。

## 2026-10-26 用户补充线缆连接方式，拔线提示须先核对
- 用户称现在是充电和连接都有功能的线连接。已询问实际一条还是两条物理线，尚待回答；不能从线材能力推断板上接口供电路径。已告诉用户暂不拔线。
- 若只剩同一根充电/数据线，不能继续提示只拔充电线保留调试；先核对拓扑，必要时另立设备端记录放电方案，不把拔线后无法采样当校准完成。现有采集器不变，禁止抢串口。
- 用户再次明确三项一起验证后授权刷手表，不发布；完整算法修复及硬件原因验证仍待完成。

- 用户已确认：只有一条线同时充电和连接电脑。已更新grokwatch heartbeat，禁止再按两根线提示拔线。当前数据只能确认USB连接期间的充电记录；完整放电段尚无可行的连续USB采样路径。须核对独立调试路径或设计设备端持久记录，不能以停止充电使能却仍USB供电冒充真实放电。

## 2026-10-26 14:50 单线放电记录实施检查点
- 用户指出等待过久，已承认单纯轮询没有解决单线放电采样阻塞。现有BatTick每15分钟NVS环缺VIN/有效性/单调时间/启动标记，且未检查写入返回值，不能冒充完整校准记录。
- 已新增work/firmware-validation/OFFLINE_DISCHARGE_PLAN.md，明确存储容量预检、原始读数/负载/重启分段、持久化失败、分页导出和回归判据。尚未实现记录器，不得汇报功能完成。下一阶段应离线实现/测试/编译，不能继续仅轮询当作实质进展。
- 采集PID21664仍运行且新鲜；禁止竞争串口或刷写。当前充电基线与新记录固件必须分实验，不能拼成同一完整周期。

## 2026-10-26 15:00 离线记录模型已实现，尚未接设备
- 新增firmware/board/gw_battery_trace.h及test/test_battery_trace.c：原始VBAT/VIN/引脚/负载/有效性/启动标识/单调时间，分页只读导出，容量满停止保留旧数据、重启和时间倒退错误保持。无百分比推SOC，无充满推断。RAM结构明确不是持久化格式。
- 测试先因缺头文件失败，再实现；已接常规test/run.sh并全量通过，work/firmware-validation/battery-trace-tests.log。此为记录基础层验证，未接NVS/板级采样/控制台，未完成固件编译或实机验证。后续必须继续存储容量预检、版本化持久记录/失败检查和设备集成；不能将本层测试宣称完整离线记录已完成。
- 现有PID21664充电采样继续，未竞争串口、未刷机。绿灯代码仍待统一编译刷入。

## 2026-10-26 15:01 备用仅数据线：优先验证，无需先扩展离线记录
- 用户补充另有一条只能连接、不能充电的线。目前尚未替换或实测。已告知暂不换，到切换阶段再用它替换当前充电/数据共用线。
- 已更新heartbeat：await_unplug候选后提示换线，随后读现有logger核对USB恢复、VIN无外部供电、boot不变，换线缺口保留。VIN有供电或断USB不算有效放电。禁止竞争串口、重启或降低候选判据。
- 此信息改变之前“必须先做设备端记录”的判断：优先验证线缆；暂停扩展尚未接设备的离线记录模块，避免不必要刷机打断基线。现有模块测试通过仍不等于设备可用。

## 2026-10-26 16:27 用户指出长时间空转：明确未完成及推进编译
- 只轮询状态未推进算法是执行问题，不能用采样健康代表充电正常或修复进展。流式检查点 checkpoint-1627.json：约4小时263次状态切换，最长终止候选190.2秒（目标300秒）；无参考SOC、无有效放电段，未校准。未降低判据、未重启或竞争串口。
- 已核对构建缓存与项目板文件差异仅关闭LED三行，复制并执行实际ESP-IDF构建成功，日志work/firmware-validation/green-led-build.log。未刷入、未声称绿灯已熄灭；硬件切换原因及电量算法仍待完成。
- 后续不能继续把仅增加样本数当任务推进。当前基线有12小时硬上限；必须保留失败结论，不能无限延长。需要实测备用仅数据线的步骤仍遵守既有候选阶段条件。

## 2026-10-26 新聊天已接管：旧基线终止，部分放电就绪
- 用户明确选择“结束旧基线，转入放电诊断”。已核对 PID21664 后 SIGINT 正常封存：finished=true/error=operator_interrupted，旧 samples.jsonl 保留，未满足完整充电终止候选，不是校准成功。旧 heartbeat 已删除。
- 新实验 work/firmware-validation/partial-discharge-20261026-01，PID86165，6小时硬上限，当前 await_disconnect；2.6.40-qa/ota_0/boots73 身份确认未变。用户待换备用仅数据线，只有实测 VIN 接近0且 USB 恢复、无重启才确认有效放电；缺口保留。不能竞争串口或刷入固件。新 heartbeat id=grokwatch 绑定当前聊天，仅阶段/故障/需操作通知，完成或失败删除。
- 第一次新采集启动因系统 Python 无 pyserial 在打开串口前失败；切换已验证含 serial3.5 的 ESP-IDF Python 后启动成功，记录保留于launcher.log/json。
- 已修 PowerSample 无效 VIN/CHG 仍更新估计、跨次诊断错配：前后校验供电分类及CHG，限100ms窗口；POWER携带同窗口VBAT。仍非硬件原子采样。常规测试、9项解析测试与固件构建通过；后新增部分放电状态机后11项Python回归通过。候选含绿灯关闭但未刷入，现有设备仍旧算法。详情 work/firmware-validation/CONTINUATION_20261026.md。
- 完整电量算法、充电反复切换的硬件原因及绿灯实机验收仍未完成；不允许写成项目完成。后续看新实验原始放电曲线，不能从显示百分比推SOC。

## 2026-10-26 17:07 部分放电实验失败并封存
- 09:05:26 UTC USB断开（Device not configured），超过120秒未恢复。采集器finished=true，connection_lost_over_120s，进程退出；有效样本337，最后仍VIN5202mV，未进入有效放电段，不能称备用线已验证或校准成功。
- 原始数据和缺口保留于partial-discharge-20261026-01。删除本次heartbeat，不自动重启采样；下一步用户恢复原充电/数据线，确认设备连接后另立实验。未刷固件。
