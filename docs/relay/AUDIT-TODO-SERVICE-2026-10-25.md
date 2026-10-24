# Grok 与待办服务部署审计 — 2026-10-25

结论：APPROVE，仅适用于现有可信局域网单用户服务；未授权扩大网络暴露。
Critical 0 / High 0 / Medium 1（局域网信任边界） / Low 0（非法 n 已修复）。

## 基线与候选
实际服务进程 93472，现有服务目录没有 Git 元数据，以部署文件 SHA256 为基线。原文件已备份至本机私有目录，配置和运行环境不进入仓库。仓库来源 950d55a；待办模块最初来自 86df4b7。

| 基线文件 | SHA256 |
|---|---|
| core/http_server.py | `d136a04d70562ddcf64be3fc97d2575f312e1af1f6831947b3066d39869ff1d4` |
| data/.config.yaml | `6d5155cc408426c5e891137d3f9bc6c1f6634ac367f469c7c5bc9f6d7264e3ed` |
| plugins_func/functions/grokbot_bridge.py | `b2e91fe74b843925d032d8741907ffc04c686eac96c4eac73342ce1c21d8c13a` |

| 候选文件 | SHA256 |
|---|---|
| http_server.py | `0bfb2903f1c5fe0eaa8781e62ada0d5cf89162795177efaf60aefc95f8183b1d` |
| config.yaml | `2a33d8bfa724e34faa4b2d7dc20d238800e588d2d7c0dd2d15754d4bb6c90ee9` |
| gw_todo.py | `2baf87f8f38229a9cfb70ddc365307fbe7280cd85e8a1dedf47e5f871cc01dab` |

## 改动与调用影响
- 新增 plugins_func/functions/gw_todo.py：仓库原有模块，增加 n=1..50 校验，拒绝非法参数时先验证再写数据。
- core/http_server.py：一处 import、一处路由注册。仅增加 GET /gw/todo、POST /gw/todo/done；原 OTA、视觉和 WebSocket 路由保持不变。
- data/.config.yaml：只向 Intent.function_call.functions 添加 add_todo，解析后逐项对比确认其余配置完全一致。
- 保留 GrokLLM、grokbot_command、grokbot_notifications 和原桥接凭据；无新依赖、无数据迁移、无固件刷新。
- 影响范围：2 个 HTTP handler 和 1 个语音工具；生产存储为空时 GET 不创建数据文件。

## 风险审查
模块原设计是可信局域网单用户存储，不含逐用户鉴权。已确认实际服务地址属于本机局域网、现有监听为全接口；不添加公网路由、不改变端口。局域网其他客户端可以读取和标记待办，此边界仍存在，不能将这些接口直接代理到公网。没有外部网络可达性证明，外部 NAT/代理配置不在本次覆盖内。
不执行 Grok BOT /cmd，不发送消息；只读 /snapshot 成功返回 HTTP 200。
文件路径由环境配置，不受 HTTP 参数控制；写操作有线程锁、原子替换；输入文本限制 120 字，最多保留 50 条未完成、20 条已完成。

## 验证
- 真实服务虚拟环境、真实插件注册接口、临时数据目录和 aiohttp HTTP 测试：4 项通过。
- 覆盖空列表只读、语音创建、HTTP 完成/恢复、容量上限、未知 ID 无写入、非法 n 返回 400 且无副作用。
- Python 编译和 git diff --check 通过。
- 部署前原服务 OTA HTTP 200、待办 HTTP 404、Grok BOT snapshot HTTP 200。
- 候选与备份哈希部署前再次比对；部署后验证路由、发现配置、WebSocket hello、桥接只读健康。

## 回退
恢复私有备份中的 HTTP 文件和 YAML，删除本次新增插件，以原虚拟环境、工作目录、已备份环境重启。不覆盖任何新产生的待办数据。恢复前比对部署清单，避免覆盖他人后续改动。

## 覆盖限制
此审计证明服务连接和待办接口，不证明麦克风、语音识别、模型推理和扬声器整条语音链。手表不自动切页，不注入点击、语音或 Bot 指令。

## 部署后验证结果
- 已按清单逐个比对基线和候选哈希，再原子替换三个目标文件；原服务优雅退出，新进程启动。
- 生效文件再次比对：三项全部匹配审计清单。
- 原 OTA 接口 HTTP 200；服务发现 POST 返回有效 WebSocket 配置；实际 WebSocket hello 成功，协商音频为 opus。
- 待办 GET 从 404 恢复为 200，返回合法空列表；没有创建或改动真实待办。
- Grok BOT 的只读 snapshot HTTP 200；未调用命令转发接口。
- 现有 GrokLLM 配置的最小模型请求成功，1.28 秒返回非空响应；未包含个人内容或工具调用。
- 服务启动日志未发现 Traceback、认证失败或 HTTP 启动失败。
- 没有刷新固件、切换页面、注入触摸或重启手表。
- 待办需要用户自然打开页面才会再次拉取；本次未擅自打开，不能把服务器验证写成已观察到手表界面刷新。
