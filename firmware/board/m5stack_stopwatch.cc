#include "wifi_board.h"
#include "backlight.h"
#include "display/lcd_display.h"
#include "esp_lcd_co5300.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "M5IOE1.h"
#include "M5PM1.h"
#include "config.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <wifi_manager.h>
#include "gw_sbsm.h"
#include "gw_redkey.h"
#include "gw_power.h"
#include "gw_battery.h"
#include <functional>
#include <esp_lcd_touch_cst816s.h>
#include <esp_lvgl_port.h>
#include <cmath>
#include "bmi270_api.h"
#include "gw_orient.h"
#include <esp_pm.h>
#include <nvs.h>
#include <ctime>

extern "C" void gw_ble_start(void);
extern "C" void gw_bg_load(void);
void NavUiToggle(); void NavUiNext(); void NavUiHome(); void NavUiShowChat(); void NavUiInit();
void NavUiSetHaptic(void (*f)(int)); void NavUiSetBacklight(void (*f)(int)); void NavUiKey(int ev, std::function<void()> normal); bool NavUiIsStandby(); void NavUiSetChatTap(void (*f)()); bool NavUiPowerPending(); void NavUiStandby(bool on); void NavUiPowerMenu(bool show);
void NavUiSetTouchIndev(lv_indev_t* i); extern "C" void gw_console_start(void); bool NavUiBusy();
void NavUiSetDeep(void (*f)(bool)); int NavUiPowerLevel();   // 2.6.37 deep standby
#include <esp_timer.h>
#include "gw_health.h"
#include <ssid_manager.h>
// Optional local Wi-Fi presets (git-ignored). Without it the watch uses normal Wi-Fi provisioning.
#if __has_include("secrets_gen.h")
#include "secrets_gen.h"
#endif
#ifndef WIFI_HOME_SSID
#define WIFI_HOME_SSID ""
#define WIFI_HOME_PASS ""
#define WIFI_HOT_SSID ""
#define WIFI_HOT_PASS ""
#endif
#include "settings.h"
extern "C" { void gw_grok_state(int st); void gw_grok_text(const char* role, const char* text); }
#include <cstring>
#define TAG "M5StackStopwatch"
#define LCD_OPCODE_WRITE_CMD (0x02ULL)

// CO5300 AMOLED: initialize at full brightness, then restore the saved setting.
static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
    // {cmd, { data }, data_size, delay_ms}
    {0xFE, (uint8_t []){0x00}, 0, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 0, 10}, // RGB565
    {0x35, (uint8_t []){0x00}, 0, 10},
    {0x53, (uint8_t []){0x20}, 1, 10},
    {0x51, (uint8_t []){0xFF}, 1, 10},
    {0x63, (uint8_t []){0xFF}, 1, 10},
    {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Column address: 0-465
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Row address: 0-465
    {0x11, (uint8_t []){0x00}, 0, 120}, // Exit sleep
    {0x29, (uint8_t []){0x00}, 0, 20},  // Display on
};

class RoundLcdDisplay : public SpiLcdDisplay {
public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(e));
        area->x1 = (area->x1 >> 1) << 1;
        area->y1 = (area->y1 >> 1) << 1;
        area->x2 = ((area->x2 >> 1) << 1) + 1;
        area->y2 = ((area->y2 >> 1) << 1) + 1;
    }

    RoundLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    // GrokWatch: the stock emoji box is hidden for good (gw_grok owns the chat page), so never decode emoji images
    // (this was the boot-time "Emoji not found: robot_2" lookup).
    bool SetEmotion(const char* emotion) override { (void)emotion; return true; }
    // GrokWatch: the chat page is gw_grok (mint character). Map xiaozhi device states onto its five states.
    void SetStatus(const char* status) override {
        SpiLcdDisplay::SetStatus(status);
        if (!status) return;
        int st = -1;
        if (!strcmp(status, Lang::Strings::LISTENING)) st = 1;
        else if (!strcmp(status, Lang::Strings::CONNECTING)) st = 2;
        else if (!strcmp(status, Lang::Strings::SPEAKING)) st = 3;
        else if (!strcmp(status, Lang::Strings::STANDBY)) st = WifiManager::GetInstance().IsConnected() ? 0 : 4;
        if (st < 0) return;
        DisplayLockGuard lock(this);
        gw_grok_state(st);
    }
    static bool IsWakeWordHint(const char* c) {
        const char* t = Lang::Strings::WAKE_WORD_HINT; const char* ph = strstr(t, "%s"); if (!ph) return false;
        size_t pre = ph - t; const char* suf = ph + 2; size_t n = strlen(c), ns = strlen(suf);
        return n >= pre + ns && !strncmp(c, t, pre) && !strcmp(c + n - ns, suf);
    }
    void SetChatMessage(const char* role, const char* content) override {
        // Never call the stock SetChatMessage: it re-shows bottom_bar_ (the dark bar with a second, larger copy of the
        // text seen on 2.6.7) and allocates chat bubbles nobody sees. gw_grok is the only subtitle renderer.
        if (!role || !content || (strcmp(role, "user") && strcmp(role, "assistant"))) return;
        // 2.6.13: drop the stock startup hint "说「你好小智」就能唤醒我。" (Lang::Strings::WAKE_WORD_HINT). GrokBot never calls
        // itself 小智; the idle status line already says 长按说话. Matches the template text around %s, not the wake word.
        if (!strcmp(role, "assistant") && IsWakeWordHint(content)) { ESP_LOGW("gw", "startup wake-word hint suppressed"); return; }
        DisplayLockGuard lock(this);
        gw_grok_text(role, content);
        if (!strcmp(role, "user") && content[0]) gw_grok_state(2);   // question heard -> thinking until speech starts
        if (!strcmp(role, "assistant") && content[0]) NavUiKey(SB_EV_VOICE, nullptr);   // 2.6.19: a spoken reply wakes standby to the GrokBot page
    }
    void ClearChatMessages() override {
        DisplayLockGuard lock(this);
        gw_grok_text("user", "");
    }
    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);
        // Old stock chat UI (robot emoji, orange status, subtitle bar) is never shown: gw_grok owns this page.
        hide_subtitle_ = true;   // belt and braces: stock code paths that re-show bottom_bar_ check this flag
        { lv_obj_t* old[] = {emoji_box_, status_bar_, bottom_bar_}; for (auto* o : old) if (o) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); }

        // Horizontal inset so labels stay inside the round mask
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.2, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.2, 0);

        // Status bar: upper half, below top icon bar (not at screen bottom)
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, DISPLAY_STATUS_BAR_TOP_OFF);
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.6);
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.6);
        lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);

        // Chat/subtitle text: a bit higher, away from bottom arc
        if (bottom_bar_ != nullptr) {
            lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -DISPLAY_CHAT_BAR_BOTTOM_OFF);
            lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.75);
        }

        // emoji_box_: keep default LV_ALIGN_CENTER from SpiLcdDisplay::SetupUI()

        // Top icons: move the top bar down inside the round display safe area
        if (top_bar_ != nullptr) {
            lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, DISPLAY_ROUND_EDGE_INSET / 2);
            lv_obj_set_style_pad_top(top_bar_, DISPLAY_ROUND_EDGE_INSET / 4, 0);
        }

        // RC5b Ultra chat page: black, orange status, big centred emotion, subtitle inside the round safe area
        if (top_bar_) lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);   // time/indicators are drawn by gw_ultra
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
        if (container_) lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        lv_obj_align(status_bar_, LV_ALIGN_CENTER, 0, -118);
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFF6A13), 0);
        if (bottom_bar_) { lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0); lv_obj_align(bottom_bar_, LV_ALIGN_CENTER, 0, 118);
            lv_obj_set_width(chat_message_label_, 300); lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0); }
        lv_obj_t* nc[] = {container_, emoji_box_, top_bar_, status_bar_, bottom_bar_};
        for (auto* o : nc) if (o) lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);  // taps reach the screen handler
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

class StopwatchBacklight : public Backlight {
public:
    StopwatchBacklight(esp_lcd_panel_io_handle_t panel_io, Display* display)
        : panel_io_(panel_io), display_(display) {}

protected:
    void SetBrightnessImpl(uint8_t brightness) override {
        DisplayLockGuard lock(display_);
        uint8_t data[] = {static_cast<uint8_t>((255 * brightness) / 100)};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;

        esp_err_t err = esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, data, sizeof(data));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set display brightness: %s", esp_err_to_name(err));
        }
    }

private:
    esp_lcd_panel_io_handle_t panel_io_;
    Display* display_;
};

class M5StackStopwatchBoard;
class M5StackStopwatchBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    M5PM1 pmic_;
    M5IOE1 ioe_;
    Button button1_;
    Button button2_;
    RoundLcdDisplay* display_;
    StopwatchBacklight* backlight_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_lcd_touch_handle_t tp_ = nullptr;
public:
    bool pa_on_ = true;
private:

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {.enable_internal_pullup = 1},
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        if (ioe_.begin(i2c_bus_, M5IOE1_I2C_ADDR, M5IOE1_I2C_FREQ_100K, M5IOE1_INT_MODE_POLLING) != M5IOE1_OK) {
            ESP_LOGE(TAG, "M5IOE1 begin failed");
            return;
        }

        if (pmic_.begin(i2c_bus_, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK) {
            ESP_LOGE(TAG, "M5PM1 begin failed");
            return;
        }

        // Match vendor PMIC setup: keep register transactions awake and check writes.
        int sleep_err=pmic_.setI2cSleepTime(0);
        int charge_err=pmic_.setChargeEnable(true);
        if (sleep_err != M5PM1_OK || charge_err != M5PM1_OK)
            ESP_LOGE(TAG, "PMIC charging configuration failed");
        pmic_.setBoostEnable(true);
        pmic_.pinMode(PMIC_PIN_CHARGE_STATE, INPUT);
        if (pmic_.gpioSet(PMIC_PIN_CHARGE_PROG, M5PM1_GPIO_MODE_OUTPUT, 0,
                         M5PM1_GPIO_PULL_NONE, M5PM1_GPIO_DRIVE_PUSHPULL) != M5PM1_OK)
            ESP_LOGE(TAG, "PMIC charge-current pin configuration failed");

        ioe_.pinMode(IOE_PIN_LCD_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_POWER, HIGH);

        ioe_.pinMode(IOE_PIN_LCD_RST, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_RST, HIGH);

        ioe_.pinMode(IOE_PIN_CODEC_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_CODEC_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_CODEC_POWER, HIGH);

        ioe_.pinMode(IOE_PIN_PA_EN, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_PA_EN, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_PA_EN, HIGH);

        ioe_.pinMode(IOE_PIN_MOTOR, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_MOTOR, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = DISPLAY_QSPI_SCK;
        buscfg.data0_io_num = DISPLAY_QSPI_D0;
        buscfg.data1_io_num = DISPLAY_QSPI_D1;
        buscfg.data2_io_num = DISPLAY_QSPI_D2;
        buscfg.data3_io_num = DISPLAY_QSPI_D3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        ioe_.digitalWrite(IOE_PIN_LCD_RST, LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        ioe_.digitalWrite(IOE_PIN_LCD_RST, HIGH);
        vTaskDelay(pdMS_TO_TICKS(120));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_QSPI_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 80 * 1000 * 1000;   // 2.6.34: full frame 21.7 -> 10.9 ms (CO5300 QSPI max 80 MHz)
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_QSPI_HOST, &io_config, &panel_io));

        co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {.use_qspi_interface = 1},
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));

        esp_lcd_panel_set_gap(panel, 0x06, 0);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        { Settings th("display", true); th.SetString("theme", "dark"); }
        display_ = new RoundLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                       DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                       DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        panel_io_ = panel_io; panel_ = panel;
        backlight_ = new StopwatchBacklight(panel_io, display_);
        backlight_->RestoreBrightness();
    }

    static M5StackStopwatchBoard* self_;
    static void Haptic(int ms) {
        if (!self_) return;
        static esp_timer_handle_t t = nullptr;
        if (!t) { esp_timer_create_args_t a = {}; a.callback = [](void*) { self_->ioe_.digitalWrite(IOE_PIN_MOTOR, LOW); }; a.name = "haptic"; esp_timer_create(&a, &t); }
        self_->ioe_.digitalWrite(IOE_PIN_MOTOR, HIGH); esp_timer_stop(t); esp_timer_start_once(t, ms * 1000);
    }
    // 2.6.19: no WakeScreen / screen_off_ / PowerSaveTimer standby any more. Every key goes through NavUiKey(): the
    // standby state machine (gw_sbsm.c, docs/relay/STANDBY-SPEC.md) decides "wake only" vs "wake + normal action".
    static void SetBacklightPct(int pct) { if (!self_ || !self_->backlight_) return; if (pct < 0) self_->backlight_->RestoreBrightness(); else self_->backlight_->SetBrightness(pct); }
    // 2.6.37: deep standby (10 min into the standby clock, gw_sbsm SB_A_DEEP). LVGL context, display lock held.
    // Raw SLPIN/SLPOUT keeps MADCTL/TE/gap; esp_lcd_panel_disp_sleep's deep-standby path resets and re-inits the panel.
    bool wifi_was_on_ = false;
    static void PanelCmd(uint8_t c) { esp_lcd_panel_io_tx_param(self_->panel_io_, (LCD_OPCODE_WRITE_CMD << 24) | (c << 8), nullptr, 0); }
    static void SetDeep(bool on) {
        auto* b = self_; if (!b || !b->panel_io_) return;
        if (on) { PanelCmd(0x28); PanelCmd(0x10); } else { PanelCmd(0x11); vTaskDelay(pdMS_TO_TICKS(120)); PanelCmd(0x29); }
#if CONFIG_PM_ENABLE
        esp_pm_config_t c = {}; c.max_freq_mhz = c.min_freq_mhz = on ? 80 : 240; c.light_sleep_enable = false; esp_pm_configure(&c);
#endif
        Application::GetInstance().Schedule([b, on]() {   // Wi-Fi off/on outside the UI thread; BLE stays up for nav
            auto& w = WifiManager::GetInstance();
            if (on) { if (w.IsInitialized() && !w.IsConfigMode()) { b->wifi_was_on_ = true; w.StopStation(); } }
            else if (b->wifi_was_on_) { b->wifi_was_on_ = false; w.StartStation(); } });
        ESP_LOGW("GW-PWR", "deep %s", on ? "on: panel sleep, cpu 80, wifi off" : "off: panel on, cpu 240, wifi on");
    }
    void InitializeButtons() {
        self_ = this;
        NavUiSetHaptic(&M5StackStopwatchBoard::Haptic);
        NavUiSetChatTap([]() { Application::GetInstance().ToggleChatState(); });
        NavUiSetBacklight(&M5StackStopwatchBoard::SetBacklightPct);
        NavUiSetDeep(&M5StackStopwatchBoard::SetDeep);
        // Yellow key (G2) = navigation key: click = next page, double = home (watch face), long = volume
        button1_.OnClick([this]() { NavUiKey(SB_EV_KEY_YELLOW, [this]() {
            if (NavUiPowerPending()) { NavUiPowerMenu(false); return; }   // cancel power-off
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) { EnterWifiConfigMode(); return; }
            NavUiNext();
        }); });
        button1_.OnDoubleClick([this]() { NavUiKey(SB_EV_KEY_YELLOW, []() { NavUiHome(); }); });
        button1_.OnLongPress([this]() { NavUiKey(SB_EV_KEY_YELLOW, [this]() {
            auto* codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            if (volume > 100) volume = 10;
            codec->SetOutputVolume(volume);
            Haptic(25);
            GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ":" + std::to_string(volume) + "%");
        }); });
        // Blue key (G1) = Action button: hold to talk to Grok, release to send. Also confirms power-off.
        button2_.OnPressDown([this]() {
            if (NavUiPowerPending()) { Haptic(80); vTaskDelay(pdMS_TO_TICKS(120)); pmic_.shutdown(); return; }
            NavUiKey(SB_EV_KEY_BLUE, []() { Haptic(15); NavUiShowChat(); Application::GetInstance().StartListening(); });
        });
        button2_.OnPressUp([this]() {
            if (NavUiPowerPending()) return;
            Application::GetInstance().StopListening();
        });
        // Power key (PMIC): click = standby clock on/off, double = power menu (needs Blue to confirm). Hardware long-press is untouched.
        // 2.6.20: 2.6.19 never saw a red click (no GW-SB "red" line on 10 min of serial). The PMIC button IRQ bits are
        // masked by default, so IRQ_STATUS3 stays 0 and the poll returned early. Unmask click+double explicitly, log
        // the results once, and also accept the PMIC "was pressed" flag as a click fallback.
        { int e1 = pmic_.setSingleResetDisable(true), e2 = pmic_.setDoubleOffDisable(true);
          int e3 = pmic_.irqSetBtnMask(M5PM1_IRQ_BTN_CLICK, M5PM1_IRQ_MASK_DISABLE);
          int e4 = pmic_.irqSetBtnMask(M5PM1_IRQ_BTN_DOUBLE, M5PM1_IRQ_MASK_DISABLE); pmic_.irqSetBtnMask(M5PM1_IRQ_BTN_WAKE, M5PM1_IRQ_MASK_DISABLE);
          uint8_t st0 = 0; pmic_.irqGetBtnStatus(&st0, M5PM1_CLEAN_ALL); bool f0 = false; pmic_.btnGetFlag(&f0);
          ESP_LOGW("GW-KEY", "red(PMIC) init: rst_dis=%d off_dis=%d unmask_click=%d unmask_double=%d (0=ok) stale=0x%02x", e1, e2, e3, e4, st0); }
        esp_timer_create_args_t pa = {};
        pa.callback = [](void* arg) {
            auto* b = static_cast<M5StackStopwatchBoard*>(arg); uint8_t st = 0;
            static int errs = 0; int e = b->pmic_.irqGetBtnStatus(&st, M5PM1_CLEAN_ALL);
            if (e != M5PM1_OK) { if (errs++ < 3) ESP_LOGW("GW-KEY", "red(PMIC) read err=%d", e); return; }
            // 2.6.22: BTN_FLAG is the press source (one per press, every state); IRQ bits only confirm a double.
            // 2.6.21 used IRQ bits once they had been seen and ignored the flag, so presses while awake (flag only)
            // were dropped -> red worked only to wake from standby. Decoder + tests: gw_redkey.c, sim/test_redkey.c.
            static gw_redkey_t rk; static bool rk_init = false; if (!rk_init) { gw_redkey_init(&rk); rk_init = true; }
            bool fl = false; b->pmic_.btnGetFlag(&fl);
            int64_t now = esp_timer_get_time() / 1000;
            if (st || fl) ESP_LOGW("GW-KEY", "red raw irq=0x%02x flag=%d", st, (int)fl);
            int r = gw_redkey_poll(&rk, now, st, fl ? 1 : 0);
            if (r == RK_NONE) return;
            ESP_LOGW("GW-KEY", "red -> %s", r == RK_DOUBLE ? "double" : "click");
            Application::GetInstance().Schedule([r]() {
                if (r == RK_DOUBLE) NavUiKey(SB_EV_RED_DOUBLE, []() { NavUiPowerMenu(true); });
                else NavUiKey(SB_EV_RED_CLICK, nullptr);   // toggles standby (power menu open -> closes it)
            });
        };
        pa.arg = this; pa.name = "pwrkey"; esp_timer_handle_t pt; esp_timer_create(&pa, &pt); esp_timer_start_periodic(pt, 120 * 1000);
    }
    void InitializeTouch() {
        // CST820 is register-compatible with CST816S; reset line is on M5IOE1_G4
        ioe_.pinMode(IOE_PIN_TOUCH_RST, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_TOUCH_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_TOUCH_RST, LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        ioe_.digitalWrite(IOE_PIN_TOUCH_RST, HIGH);
        vTaskDelay(pdMS_TO_TICKS(50));

        esp_lcd_panel_io_handle_t tp_io = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
        tp_io_config.scl_speed_hz = 400000;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.flags.disable_control_phase = 1;
        if (esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io) != ESP_OK) {
            ESP_LOGE(TAG, "touch io init failed");
            return;
        }
        esp_lcd_touch_config_t tp_cfg = {};
        tp_cfg.x_max = DISPLAY_WIDTH;
        tp_cfg.y_max = DISPLAY_HEIGHT;
        tp_cfg.rst_gpio_num = GPIO_NUM_NC;
        tp_cfg.int_gpio_num = TOUCH_INT_PIN;
        esp_lcd_touch_handle_t tp = nullptr;
        if (esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp) != ESP_OK) {
            ESP_LOGE(TAG, "CST820 init failed");
            return;
        }
        tp_ = tp;
        const lvgl_port_touch_cfg_t touch_cfg = {.disp = lv_display_get_default(), .handle = tp};
        NavUiSetTouchIndev(lvgl_port_add_touch(&touch_cfg));

        // taps/swipes are handled in nav_ui.cc (swipe = pages, tap = chat only on the chat page)
        ESP_LOGI(TAG, "Touch ready");
    }

    // 2.6.34: BMI270 (I2C 0x68) -> auto 180° flip when the watch hangs upside down (e.g. held by the lanyard).
    // Flip is done in hardware: CO5300 MADCTL + address gap, touch mirrored by esp_lcd_touch. LVGL and gw_slide's
    // direct push keep drawing in logical coordinates, so the UI, slide strips and swipe directions need no change.
    // Knobs (NVS "gw", read at boot, settable live with the console "rot" command): rot=0 off; rot_ax 0/1/2 = accel
    // axis toward the screen top, rot_sg +1/-1; rot_mad MADCTL for 180° (CO5300: standard MY 0x80 | MX 0x40; verified on
    // hardware, Arduino_GFX's 0x02/0x05 left it mirrored); rot_gx/rot_gy address gap while flipped (upright: 6/0).
    // Calibrated on hardware 2026-10-26: screen top = -x (upright x~-0.9, hanging by the lanyard x~+1.0).
    int rot_on_ = 1, rot_ax_ = 0, rot_sg_ = -1, rot_mad_ = 0xC0, rot_gx_ = 0, rot_gy_ = 0;
    bool flipped_ = false;
    float acc_[3] = {};
    void LoadRotKnobs() {
        Settings s("gw", false);
        rot_on_ = s.GetInt("rot", 1); rot_ax_ = s.GetInt("rot_ax", 0) % 3; rot_sg_ = s.GetInt("rot_sg", -1) < 0 ? -1 : 1;
        rot_mad_ = s.GetInt("rot_mad", 0xC0); rot_gx_ = s.GetInt("rot_gx", 0); rot_gy_ = s.GetInt("rot_gy", 0);
    }
    // UI context (main task). Skipped while a finger is down or a slide runs; the IMU task asks again 100 ms later.
    void SetFlip(bool f) {
        DisplayLockGuard g(display_);
        if (f == flipped_ || NavUiBusy()) return;
        flipped_ = f;
        uint8_t m = f ? rot_mad_ : 0;   // upright MADCTL is 0 (esp_lcd_panel_mirror(false,false) at init)
        esp_lcd_panel_io_tx_param(panel_io_, (LCD_OPCODE_WRITE_CMD << 24) | (0x36 << 8), &m, 1);
        esp_lcd_panel_set_gap(panel_, f ? rot_gx_ : 0x06, f ? rot_gy_ : 0);
        if (tp_) { esp_lcd_touch_set_mirror_x(tp_, f); esp_lcd_touch_set_mirror_y(tp_, f); }
        lv_obj_invalidate(lv_screen_active()); lv_obj_invalidate(lv_layer_top());
        ESP_LOGW("GW-ROT", "%s (acc %.2f %.2f %.2f, mad=0x%02x gap=%d,%d)", f ? "flipped" : "upright", acc_[0], acc_[1], acc_[2], m, f ? rot_gx_ : 6, f ? rot_gy_ : 0);
    }
    static void ImuTask(void* arg) {
        auto* b = static_cast<M5StackStopwatchBoard*>(arg);
        i2c_config_t c = {}; c.mode = I2C_MODE_MASTER; c.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN; c.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        c.sda_pullup_en = true; c.scl_pullup_en = true; c.master.clk_speed = 400000;
        i2c_bus_handle_t bus = i2c_bus_create(I2C_NUM_0, &c);   // wraps the board's existing I2C_NUM_0 master bus
        bmi270_handle_t h = nullptr;
        if (!bus || bmi270_sensor_create(bus, &h, bmi270_config_file, BMI2_GYRO_CROSS_SENS_ENABLE | BMI2_CRT_RTOSK_ENABLE) != ESP_OK || !h) {
            ESP_LOGE("GW-ROT", "BMI270 init failed, auto-rotate off"); h = nullptr; }   // 2.6.37: task stays for the battery log
        if (h) { uint8_t acc = BMI2_ACCEL; bmi270_sensor_enable(&acc, 1, h);
        bmi2_sens_config cfg = {}; cfg.type = BMI2_ACCEL; bmi270_get_sensor_config(&cfg, 1, h);
        cfg.cfg.acc.odr = BMI2_ACC_ODR_25HZ; cfg.cfg.acc.range = BMI2_ACC_RANGE_2G; bmi270_set_sensor_config(&cfg, 1, h); }
        gw_orient_t o = {}; int n = 0;
        b->BatLoad(); b->BatTick(true);
        for (;;) {
            int pl = NavUiPowerLevel(); int ms = pl ? 1000 : 100;   // 2.6.37: 100 ms awake, 1 s on the standby clock
            vTaskDelay(pdMS_TO_TICKS(ms)); b->bat_ms_ += ms; b->BatTick(false);
            if (!h || NavUiPowerLevel() == 2) continue;              // deep standby: screen off, no auto-rotate
            bmi2_sens_data d = {};
            if (bmi2_get_sensor_data(&d, h) != BMI2_OK) continue;
            float* a = b->acc_; a[0] = d.acc.x / 16384.f; a[1] = d.acc.y / 16384.f; a[2] = d.acc.z / 16384.f;
            if (n++ == 10) ESP_LOGW("GW-ROT", "BMI270 ok, acc %.2f %.2f %.2f (ax=%d sg=%d on=%d)", a[0], a[1], a[2], b->rot_ax_, b->rot_sg_, b->rot_on_);
            int st = b->rot_on_ ? gw_orient_feed(&o, b->rot_sg_ * a[b->rot_ax_], sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])) : 0;
            if (st != (int)b->flipped_) Application::GetInstance().Schedule([b, st]() { b->SetFlip(st); });
        }
    }
    void InitializeImu() { LoadRotKnobs(); xTaskCreate(ImuTask, "gw_imu", 4096, this, 2, nullptr); }

    // 2.6.19: PowerSaveTimer removed (it entered standby on its own after 60 s on battery and light-slept the CPU,
    // which also muted the wake word). Standby is owned by the nav_ui state machine; Wi-Fi/BLE stay up in standby.

    // 2.6.37 battery log: GW-BAT every 5 min; every 15 min one entry into a 96-slot NVS ring (gw/batlog, ~24 h).
    struct BatEnt { uint32_t t; uint16_t mv; uint8_t pwr, chg; };
    struct BatRing { uint16_t head, n; BatEnt e[96]; };
    BatRing bat_ = {}; int64_t bat_ms_ = 0, bat_log_ms_ = 0, bat_nvs_ms_ = 0;
    void BatLoad() {
        nvs_handle_t h; size_t l = sizeof bat_;
        if (nvs_open("gw", NVS_READONLY, &h) != ESP_OK) return;
        if (nvs_get_blob(h, "batlog", &bat_, &l) != ESP_OK || l != sizeof bat_ || bat_.head >= 96 || bat_.n > 96) bat_ = {};
        nvs_close(h);
    }
    void BatTick(bool now) {
        if (!now && bat_ms_ - bat_log_ms_ < 5 * 60000) return;
        bat_log_ms_ = bat_ms_;
        uint16_t mv = 0; int lvl = 0; bool chg = false, dis = false;
        if (pmic_.readVbat(&mv) != M5PM1_OK || !GetBatteryLevel(lvl, chg, dis)) return;
        int pl = NavUiPowerLevel();
        ESP_LOGW("GW-BAT", "mv=%u lvl=%d chg=%d pwr=%d up=%llds", mv, lvl, (int)chg, pl, (long long)(esp_timer_get_time() / 1000000));
        if (!now && bat_ms_ - bat_nvs_ms_ < 15 * 60000) return;
        bat_nvs_ms_ = bat_ms_;
        bat_.e[bat_.head] = { (uint32_t)time(nullptr), mv, (uint8_t)pl, (uint8_t)chg }; bat_.head = (bat_.head + 1) % 96;
        if (bat_.n < 96) bat_.n++;
        nvs_handle_t h; if (nvs_open("gw", NVS_READWRITE, &h) != ESP_OK) return;
        nvs_set_blob(h, "batlog", &bat_, sizeof bat_); nvs_commit(h); nvs_close(h);
    }
public:
    static int PowerSample(int* level, int* vin, int* pin) {
        *level=-1; *vin=-1; *pin=-1;
        auto* b=self_; if (!b) return 0;
        uint16_t mv=0, input=0;
        if (b->pmic_.readVin(&input)==M5PM1_OK) *vin=input;
        *pin=b->pmic_.digitalRead(PMIC_PIN_CHARGE_STATE);
        if (b->pmic_.readVbat(&mv)!=M5PM1_OK) return 0;
        static gw_battery_t estimate={};
        static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
        taskENTER_CRITICAL(&lock);
        *level=gw_battery_update(&estimate,mv,*vin>4000,esp_timer_get_time()/1000);
        taskEXIT_CRITICAL(&lock);
        return 1;
    }
    static int PowerDiag(char* out,int n){
        int level,vin,pin;
        int valid=PowerSample(&level,&vin,&pin);
        uint8_t cfg=0; int cfg_ok=self_?self_->pmic_.getPowerConfig(&cfg):-1;
        int prog=self_?self_->pmic_.digitalRead(PMIC_PIN_CHARGE_PROG):-1;
        return snprintf(out,n,"POWER valid=%d level=%d vin_mv=%d charge_pin=%d state=%d cfg_ok=%d cfg=%u prog=%d pwr=%d\n",
                        valid,level,vin,pin,valid?gw_charge_status(vin,pin):-1,cfg_ok,cfg,prog,NavUiPowerLevel());
    }
    // Console "bat": current reading + the ring oldest -> newest ("MM-DD HH:MM mv pwr chg"; pwr 0 awake, 1 standby, 2 deep).
    static int BatCmd(char* out, int n) {
        auto* b = self_; if (!b) return snprintf(out, n, "ERR no board\n");
        uint16_t mv = 0; int lvl = 0; bool chg = false, dis = false; b->pmic_.readVbat(&mv); b->GetBatteryLevel(lvl, chg, dis);
        int k = snprintf(out, n, "now mv=%u lvl=%d chg=%d pwr=%d, %d log entries (15 min)\n", mv, lvl, (int)chg, NavUiPowerLevel(), b->bat_.n);
        const BatRing& r = b->bat_;
        for (int i = 0; i < r.n && k < n - 40; i++) { const BatEnt& e = r.e[(r.head + 96 - r.n + i) % 96];
            time_t t = e.t; struct tm tm; localtime_r(&t, &tm);
            k += snprintf(out + k, n - k, "%02d-%02d %02d:%02d %u %d %d\n", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, e.mv, e.pwr, e.chg); }
        return k;
    }
    // Console "rot": no key = status; else set NVS gw/<key> and apply (re-flips on the next IMU sample).
    static int RotCmd(const char* key, int val, char* out, int n) {
        auto* b = self_; if (!b) return snprintf(out, n, "ERR no board\n");
        if (key && key[0]) { { Settings s("gw", true); s.SetInt(key, val); } b->LoadRotKnobs();
            Application::GetInstance().Schedule([b]() { bool f = b->flipped_; b->flipped_ = !f; b->SetFlip(f); }); }   // re-send with new knobs
        return snprintf(out, n, "ROT on=%d flipped=%d acc=%.2f,%.2f,%.2f ax=%d sg=%d mad=0x%02x gap=%d,%d\n", b->rot_on_, (int)b->flipped_,
            b->acc_[0], b->acc_[1], b->acc_[2], b->rot_ax_, b->rot_sg_, b->rot_mad_, b->rot_gx_, b->rot_gy_);
    }
    // GrokWatch: boot record rides along in the OTA check body (board.gw_boot), readable server-side without serial.
    std::string GetBoardJson() override {
        std::string j = WifiBoard::GetBoardJson(); char b[256];
        if (gw_bootrec_json_ex(gw_bootrec_get(), gw_ota_bad_version(), b, sizeof b) > 0 && !j.empty() && j.back() == '}') { j.pop_back(); j += ",\"gw_boot\":"; j += b; j += "}"; }
        return j;
    }
    M5StackStopwatchBoard()
        : i2c_bus_(nullptr),
          button1_(BUTTON1_GPIO),
          button2_(BUTTON2_GPIO),
          display_(nullptr),
          backlight_(nullptr) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeImu();
        { auto& sm=SsidManager::GetInstance();
          if (sizeof(WIFI_HOT_PASS)>1) sm.AddSsid(WIFI_HOT_SSID, WIFI_HOT_PASS);
          if (sizeof(WIFI_HOME_PASS)>1) sm.AddSsid(WIFI_HOME_SSID, WIFI_HOME_PASS); }
        gw_health_start();   // OTA: mark new image valid from local boot health (no server needed)
        gw_ble_start();
        NavUiInit();
        gw_bg_load();   // 2.6.35: saved face background
        gw_console_start();   // USB test console: no-op unless built with GW_TEST_CONSOLE (private/debug builds)
        // [Sunny] anti-buzz: PA on only while codec output is enabled
        { esp_timer_create_args_t a={}; a.name="pa_gate"; a.arg=this;
          a.callback=[](void* p){ auto* b=(M5StackStopwatchBoard*)p; bool on=b->GetAudioCodec()->output_enabled();
            if(on!=b->pa_on_){ b->pa_on_=on; b->ioe_.digitalWrite(IOE_PIN_PA_EN, on?HIGH:LOW);} };
          esp_timer_handle_t h; esp_timer_create(&a,&h); esp_timer_start_periodic(h,100000); }
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA,
            AUDIO_CODEC_ES8311_ADDR,
            false);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        return backlight_;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        int vin=-1,pin=-1;
        bool valid=PowerSample(&level,&vin,&pin);
        int state=gw_charge_status(vin,pin);
        charging=state==1; discharging=state==0||state==2;
        return valid;
    }

    void SetPowerSaveLevel(PowerSaveLevel level) override {
        WifiBoard::SetPowerSaveLevel(level);
    }
};

M5StackStopwatchBoard* M5StackStopwatchBoard::self_ = nullptr;
int GwRotCmd(const char* key, int val, char* out, int n) { return M5StackStopwatchBoard::RotCmd(key, val, out, n); }
int GwPowerDiag(char* out,int n) { return M5StackStopwatchBoard::PowerDiag(out,n); }
int GwPowerSample(int* level, int* vin, int* pin) { return M5StackStopwatchBoard::PowerSample(level,vin,pin); }
int GwBatCmd(char* out, int n) { return M5StackStopwatchBoard::BatCmd(out, n); }
DECLARE_BOARD(M5StackStopwatchBoard);
