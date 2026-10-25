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
#include "power_save_timer.h"
#include <esp_lcd_touch_cst816s.h>
#include <esp_lvgl_port.h>

extern "C" void gw_ble_start(void);
void NavUiToggle(); void NavUiNext(); void NavUiHome(); void NavUiShowChat(); void NavUiInit();
void NavUiSetHaptic(void (*f)(int)); void NavUiSetWake(void (*f)()); bool NavUiIsStandby(); void NavUiSetChatTap(void (*f)()); bool NavUiPowerPending(); void NavUiStandby(bool on); void NavUiPowerMenu(bool show);
void NavUiSetTouchIndev(lv_indev_t* i); extern "C" void gw_console_start(void);
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
    PowerSaveTimer* power_save_timer_ = nullptr;
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

        pmic_.setChargeEnable(true);
        pmic_.setBoostEnable(true);
        pmic_.pinMode(PMIC_PIN_CHARGE_STATE, INPUT);
        pmic_.pinMode(PMIC_PIN_CHARGE_PROG, OUTPUT);
        pmic_.digitalWrite(PMIC_PIN_CHARGE_PROG, LOW); // Set charge current to 425mA

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
        io_config.pclk_hz = 40 * 1000 * 1000;
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
    bool screen_off_ = false;
    void WakeScreen() { if (screen_off_) { screen_off_ = false; backlight_->RestoreBrightness(); } if (power_save_timer_) power_save_timer_->WakeUp(); }
    void InitializeButtons() {
        self_ = this;
        NavUiSetHaptic(&M5StackStopwatchBoard::Haptic);
        NavUiSetChatTap([]() { Application::GetInstance().ToggleChatState(); });
        NavUiSetWake([]() { self_->WakeScreen(); });   // 2.6.17: tap on the standby clock wakes like a key press
        // Yellow key (G2) = navigation key: click = next page, double = home (watch face), long = volume
        button1_.OnClick([this]() {
            WakeScreen();
            if (NavUiPowerPending()) { NavUiPowerMenu(false); return; }   // cancel power-off
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) { EnterWifiConfigMode(); return; }
            NavUiNext();
        });
        button1_.OnDoubleClick([this]() { WakeScreen(); NavUiHome(); });
        button1_.OnLongPress([this]() {
            WakeScreen();
            auto* codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            if (volume > 100) volume = 10;
            codec->SetOutputVolume(volume);
            Haptic(25);
            GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ":" + std::to_string(volume) + "%");
        });
        // Blue key (G1) = Action button: hold to talk to Grok, release to send. Also confirms power-off.
        button2_.OnPressDown([this]() {
            WakeScreen();
            if (NavUiPowerPending()) { Haptic(80); vTaskDelay(pdMS_TO_TICKS(120)); pmic_.shutdown(); return; }
            Haptic(15);
            NavUiShowChat();
            Application::GetInstance().StartListening();
        });
        button2_.OnPressUp([this]() {
            if (NavUiPowerPending()) return;
            Application::GetInstance().StopListening();
        });
        // Power key (PMIC): click = standby clock on/off, double = power menu (needs Blue to confirm). Hardware long-press is untouched.
        pmic_.setSingleResetDisable(true);
        pmic_.setDoubleOffDisable(true);
        esp_timer_create_args_t pa = {};
        pa.callback = [](void* arg) {
            auto* b = static_cast<M5StackStopwatchBoard*>(arg); uint8_t st = 0;
            if (b->pmic_.irqGetBtnStatus(&st, M5PM1_CLEAN_ALL) != M5PM1_OK || !st) return;
            Application::GetInstance().Schedule([b, st]() {
                if (st & M5PM1_IRQ_BTN_DOUBLE) { b->WakeScreen(); NavUiPowerMenu(true); }
                else if (st & M5PM1_IRQ_BTN_CLICK) {
                    if (NavUiPowerPending()) { NavUiPowerMenu(false); return; }
                    // 2.6.18: red key click toggles the standby clock (was: backlight off/on, which read as "does nothing"
                    // to enter standby). A screen turned off by an older path is still woken by the same click.
                    if (b->screen_off_ || NavUiIsStandby()) { b->WakeScreen(); NavUiStandby(false); }
                    else { Haptic(15); NavUiStandby(true); }
                }
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
        const lvgl_port_touch_cfg_t touch_cfg = {.disp = lv_display_get_default(), .handle = tp};
        NavUiSetTouchIndev(lvgl_port_add_touch(&touch_cfg));

        // taps/swipes are handled in nav_ui.cc (swipe = pages, tap = chat only on the chat page)
        ESP_LOGI(TAG, "Touch ready");
    }

    void InitializePowerSaveTimer() {
        // Dim AMOLED after 60s idle on battery (AMOLED black pixels draw ~0 power)
        power_save_timer_ = new PowerSaveTimer(-1, 60, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetChatMessage("system", "");
            NavUiStandby(true);                       // minimal black standby clock (AMOLED: black = off pixels)
            GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            NavUiStandby(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->SetEnabled(true);
    }

public:
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
        InitializePowerSaveTimer();
        InitializeTouch();
        InitializeButtons();
        { auto& sm=SsidManager::GetInstance();
          if (sizeof(WIFI_HOT_PASS)>1) sm.AddSsid(WIFI_HOT_SSID, WIFI_HOT_PASS);
          if (sizeof(WIFI_HOME_PASS)>1) sm.AddSsid(WIFI_HOME_SSID, WIFI_HOME_PASS); }
        gw_health_start();   // OTA: mark new image valid from local boot health (no server needed)
        gw_ble_start();
        NavUiInit();
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
        uint16_t voltage_mv = 0;
        static bool last_discharging = true;
        if (pmic_.readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }

        int charge_state_level = pmic_.digitalRead(PMIC_PIN_CHARGE_STATE);
        if (charge_state_level < 0) {
            // Charge status read failed; leave charging/discharging unknown
            charging = false;
            discharging = false;
        } else {
            // M5PM1 charge status is active low: 0 means charging, 1 means not charging
            charging = (charge_state_level == 0);
            discharging = !charging;
        }
        if (power_save_timer_ && discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        const int BATTERY_MIN_VOLTAGE = 3400;
        const int BATTERY_MAX_VOLTAGE = 4200;
        if (voltage_mv < BATTERY_MIN_VOLTAGE) {
            level = 0;
        } else if (voltage_mv > BATTERY_MAX_VOLTAGE) {
            level = 100;
        } else {
            level = ((voltage_mv - BATTERY_MIN_VOLTAGE) * 100) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
        }
        return true;
    }

    void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER && power_save_timer_) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

M5StackStopwatchBoard* M5StackStopwatchBoard::self_ = nullptr;
DECLARE_BOARD(M5StackStopwatchBoard);
