// Real LVGL + production page fonts + the exact patched SetTextFont method.
// Only the ESP lock/theme shell is replaced; no LVGL/font stubs are used.
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
#include "gw_faces.h"
#include "lvgl_font.h"
extern "C" const lv_font_t* gw_f20(void);
extern "C" const lv_font_t* gw_f21(void);

struct TestTheme {
    std::shared_ptr<LvglFont> value;
    std::shared_ptr<LvglFont> text_font() { return value; }
    void set_text_font(std::shared_ptr<LvglFont> f) { value = f; }
};
struct LvglThemeManager {
    TestTheme light, dark;
    static LvglThemeManager& GetInstance() { static LvglThemeManager m; return m; }
    TestTheme* GetTheme(const char* name) { return !strcmp(name,"light") ? &light : &dark; }
};
static bool lock_ok = true;
static int lock_depth = 0;
static std::weak_ptr<LvglFont> previous_owner;
struct DisplayLockGuard {
    bool held;
    explicit DisplayLockGuard(void*) : held(lock_ok) { if(held) ++lock_depth; }
    ~DisplayLockGuard() { if(held) --lock_depth; }
    explicit operator bool() const { return lock_ok; }
};
struct LvglDisplay {
    TestTheme* current_theme_ = &LvglThemeManager::GetInstance().dark;
    bool SetTextFont(std::shared_ptr<LvglFont> text_font);
    using TextFontChangedCallback = void (*)(const lv_font_t*);
    static inline TextFontChangedCallback text_font_changed_cb_ = nullptr;
    void SetTheme(TestTheme*) {} // custom labels keep F20/F21; these must be rebound separately
};
#include "set_text_font.inc"
static bool built=true;
#include "font_changed.inc"

static bool glyph(const lv_font_t* font, lv_font_glyph_dsc_t* d, uint32_t c, uint32_t next) {
    (void)next;
    if(c != 0x9f98) return false; // 龘: intentionally absent from compact gz_20
    memset(d, 0, sizeof *d);
    d->adv_w = font->line_height;
    return true;
}
static std::shared_ptr<LvglFont> make_font(int width) {
    lv_font_t f = {};
    f.get_glyph_dsc = glyph; f.line_height = width;
    return std::make_shared<LvglBuiltInFont>(&f);
}
int main() {
    lv_init();
    lv_display_t* display = lv_display_create(466,466);
    assert(display);
    lv_obj_t* label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(label, gw_f20(), 0);
    lv_label_set_text(label, "龘龘");
    auto& themes = LvglThemeManager::GetInstance();
    auto initial = make_font(20);
    themes.light.value = themes.dark.value = initial;
    gw_faces_set_fallback(initial->font());
    initial.reset();
    LvglDisplay controller;
    LvglDisplay::text_font_changed_cb_ = [](const lv_font_t* font) {
        assert(lock_depth > 0);
        assert(!previous_owner.expired());
        on_text_font_changed(font);
    };
    for(int i=0; i<200; ++i) {
        auto next = make_font(21 + i%7);
        const auto old = themes.dark.value->font();
        std::weak_ptr<LvglFont> weak = themes.dark.value;
        previous_owner = weak;
        assert(controller.SetTextFont(next));
        assert(weak.expired()); // old wrapper really freed, not accidentally retained by fixture
        previous_owner.reset();
        weak.reset(); // make_shared allocation is released only after the weak owner is gone
        (void)old;
        // With the unpatched method ASan must diagnose a heap-use-after-free here.
        assert(lv_font_get_glyph_width(gw_f20(),0x9f98,0) == next->font()->line_height);
        assert(lv_font_get_glyph_width(gw_f21(),0x9f98,0) == next->font()->line_height);
        lv_obj_update_layout(label);
        assert(lv_obj_get_width(label) == 2 * next->font()->line_height);
    }
    auto saved = gw_f20()->fallback;
    assert(!controller.SetTextFont(nullptr));
    auto owner_before_failed_lock = themes.dark.value;
    lock_ok = false;
    assert(!controller.SetTextFont(make_font(29)));
    assert(gw_f20()->fallback == saved);
    assert(themes.dark.value == owner_before_failed_lock);
    lock_ok = true;
    gw_faces_set_fallback(nullptr);
    assert(!gw_f20()->fallback && !gw_f21()->fallback);
    lv_display_delete(display);
    puts("font lifetime: 200 replacements, CJK width/layout, detach/lock checks passed");
}
