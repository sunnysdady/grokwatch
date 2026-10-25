# "你好小智" sources (2026-10-25) and 2.6.13 fix

## Firmware (2.6.12 d6e8933)
- **Shown on the GrokBot page (most likely what Sunny saw):** upstream `Application::ShowStartupWakeWordHint()` builds
  `Lang::Strings::WAKE_WORD_HINT` = "说「%s」就能唤醒我。" with the WakeNet model name and calls
  `SetChatMessage("assistant", …)` for 5 s after boot when idle and wake word running. Our override forwards assistant
  text to gw_grok -> subtitle "说「你好小智」就能唤醒我。". (WakeNet9 nihaoxiaozhi_tts is loaded: boot log "wakenet9l_tts1h8").
- Board strings: none contain 小智 (status hints: 长按说话 / 正在聆听… / 思考中… / 离线).
- zh-CN language.json: only WAKE_WORD_HINT (template) and HELLO_MY_FRIEND "你好，我的朋友！" (not shown by our UI). OGG prompts
  (welcome/activation/upgrade/wificonfig/digits/err_*) are stock voice files; content not audited by ear.
- 2.6.13: `IsWakeWordHint()` drops exactly that template (prefix "说「" + suffix "」就能唤醒我。"), logs W "startup wake-word hint suppressed".
  Host test: hint variants -> dropped; "我是 Grok，不是小智" etc. -> shown.

## Wake word change feasibility
- Model is selected by sdkconfig (CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS) and stored in the *model/assets partition*, not in the app.
  Options available in esp-sr: WN9 "Hi ESP" (SR_WN_WN9_HIESP), "Hi M Five", "Jarvis", "Computer", etc.; or MultiNet custom
  command words (e.g. pinyin "hai ge luo ke") via CONFIG_USE_CUSTOM_WAKE_WORD (more false triggers, more PSRAM/CPU).
- Either way requires rewriting the model partition -> outside the current "ota_1 + otadata only" rule; needs separate approval
  and a backup of that partition first. The watch's main path is long-press, so disabling wake word entirely is also an option.

## Server (~/xiaozhi-grok/xiaozhi-server) — proposal only, NOT applied
data/.config.yaml overrides only `prompt` (already "你是 Grok…你不是小智"). Inherited from config.yaml:
- `system_error_response: "主人，小智现在有点忙，我们稍后再试吧。"`  -> spoken on errors.
- `wakeup_words: ["你好小智", …]` + `enable_wakeup_words_response_cache: true` -> cached wake reply
  config/assets/wakeup_words.wav whose reference_text is "哈啰啊，我是小智啦…".
- `enable_greeting: true`, end_prompt (no 小智 text).
Recent log: no wake-word events; ASR texts were user questions ("你是Grok还是小只人？"); replies already say "不是小智".

Proposed additions to data/.config.yaml:
```yaml
system_error_response: "Grok 现在有点忙，稍后再试吧。"
enable_wakeup_words_response_cache: false
wakeup_words: []
```
Backup / apply / rollback:
```sh
B=~/grokwatch-backups/server-$(date +%Y%m%d-%H%M%S)-noxiaozhi; mkdir -p $B
cp ~/xiaozhi-grok/xiaozhi-server/data/.config.yaml $B/
# edit data/.config.yaml (append the 3 keys), then:
launchctl kickstart -k gui/$(id -u)/com.sunny.xiaozhi-grok
# rollback:
cp $B/.config.yaml ~/xiaozhi-grok/xiaozhi-server/data/.config.yaml && launchctl kickstart -k gui/$(id -u)/com.sunny.xiaozhi-grok
```
