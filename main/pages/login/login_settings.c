// Login Settings Page - Pre-login configuration (PIN, brightness)

#include "login_settings.h"
#include "../../core/pin.h"
#include "../../core/settings.h"
#include "../../ui/input_helpers.h"
#include "../../ui/menu.h"
#include "../../ui/theme_widgets.h"
#include "../../utils/session.h"
#include "../pin/pin_page.h"
#include "../pin/pin_settings.h"
#include <bsp/display.h>
#include <lvgl.h>

// -- Top-level settings menu --
static ui_menu_t *settings_menu = NULL;
static lv_obj_t *settings_screen = NULL;
static void (*return_callback)(void) = NULL;

// -- Brightness detail page --
static lv_obj_t *brightness_screen = NULL;
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_label = NULL;

// Forward declarations
static void show_brightness_page(void);
static void destroy_brightness_page(void);

// ── Screen Brightness detail page ──

static void brightness_apply_level(int32_t level) {
  if (level < 1)
    level = 1;
  else if (level > 10)
    level = 10;

  lv_slider_set_value(brightness_slider, level, LV_ANIM_ON);
  bsp_display_brightness_set(level * 10);
  lv_label_set_text_fmt(brightness_label, "%d", (int)level);
}

static void brightness_decrease_cb(lv_event_t *e) {
  (void)e;
  brightness_apply_level(lv_slider_get_value(brightness_slider) - 1);
}

static void brightness_increase_cb(lv_event_t *e) {
  (void)e;
  brightness_apply_level(lv_slider_get_value(brightness_slider) + 1);
}

static void brightness_back_cb(lv_event_t *e) {
  (void)e;
  int32_t level = lv_slider_get_value(brightness_slider);
  settings_set_brightness((uint8_t)(level * 10));
  destroy_brightness_page();
  ui_menu_show(settings_menu);
}

static void show_brightness_page(void) {
  ui_menu_hide(settings_menu);

  brightness_screen = theme_create_page_container(lv_screen_active());

  ui_create_back_button(brightness_screen, brightness_back_cb);
  theme_create_page_title(brightness_screen, "Screen Brightness");

  // Brightness level (1-10); zero is reserved for display sleep/off.
  uint8_t cur = settings_get_brightness();
  uint8_t level = (cur + 5) / 10;
  if (level < 1)
    level = 1;
  brightness_label = lv_label_create(brightness_screen);
  lv_label_set_text_fmt(brightness_label, "%d", (int)level);
  lv_obj_set_style_text_font(brightness_label, theme_font_medium(), 0);
  lv_obj_set_style_text_color(brightness_label, primary_color(), 0);
  lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, -30);

  // Decrease button, slider and increase button
  lv_obj_t *brightness_row = theme_create_flex_row(brightness_screen);
  lv_obj_set_size(brightness_row, LV_PCT(80), theme_min_touch_size());
  lv_obj_align(brightness_row, LV_ALIGN_CENTER, 0, 20);
  lv_obj_set_style_pad_column(brightness_row, theme_small_padding(), 0);

  lv_obj_t *decrease_btn =
      theme_create_button(brightness_row, LV_SYMBOL_LEFT, false);
  lv_obj_set_size(decrease_btn, theme_min_touch_size(), theme_min_touch_size());
  lv_obj_add_event_cb(decrease_btn, brightness_decrease_cb, LV_EVENT_CLICKED,
                      NULL);

  brightness_slider = lv_slider_create(brightness_row);
  lv_slider_set_range(brightness_slider, 1, 10);
  lv_slider_set_value(brightness_slider, level, LV_ANIM_OFF);
  lv_obj_set_flex_grow(brightness_slider, 1);
  lv_obj_set_height(brightness_slider, 10);
  lv_obj_remove_flag(brightness_slider, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *increase_btn =
      theme_create_button(brightness_row, LV_SYMBOL_RIGHT, false);
  lv_obj_set_size(increase_btn, theme_min_touch_size(), theme_min_touch_size());
  lv_obj_add_event_cb(increase_btn, brightness_increase_cb, LV_EVENT_CLICKED,
                      NULL);

  // Style as a read-only level indicator. The arrow buttons are the only
  // input method, so hide the slider knob to avoid suggesting it can be
  // dragged.
  lv_obj_set_style_bg_color(brightness_slider, highlight_color(),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, panel_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(brightness_slider, 0, LV_PART_KNOB);
}

static void destroy_brightness_page(void) {
  if (brightness_screen) {
    lv_obj_del(brightness_screen);
    brightness_screen = NULL;
  }
  brightness_slider = NULL;
  brightness_label = NULL;
}

// ── PIN setup/settings ──

static void rebuild_menu(void);

static void pin_setup_complete(void) {
  pin_page_destroy();
  // Start session timeout after PIN setup
  uint16_t timeout = pin_get_session_timeout();
  if (timeout > 0)
    session_start(timeout);
  rebuild_menu();
}

static void pin_setup_cancel(void) {
  pin_page_destroy();
  ui_menu_show(settings_menu);
}

static void setup_pin_cb(void) {
  ui_menu_hide(settings_menu);
  pin_page_create(lv_screen_active(), PIN_PAGE_SETUP, pin_setup_complete,
                  pin_setup_cancel);
}

static void pin_settings_return(void) {
  pin_settings_page_destroy();
  rebuild_menu();
}

static void pin_settings_verified(void) {
  pin_page_destroy();
  pin_settings_page_create(lv_screen_active(), pin_settings_return);
}

static void pin_settings_cancel(void) {
  pin_page_destroy();
  ui_menu_show(settings_menu);
}

static void pin_settings_cb(void) {
  ui_menu_hide(settings_menu);
  pin_page_create(lv_screen_active(), PIN_PAGE_UNLOCK, pin_settings_verified,
                  pin_settings_cancel);
}

// ── Category menu callbacks ──

static void brightness_cb(void) { show_brightness_page(); }

static void settings_back_cb(void) {
  if (return_callback)
    return_callback();
}

static void rebuild_menu(void) {
  if (settings_menu) {
    ui_menu_destroy(settings_menu);
    settings_menu = NULL;
  }
  settings_menu = ui_menu_create(settings_screen, "Settings", settings_back_cb);
  if (pin_is_configured()) {
    ui_menu_add_entry(settings_menu, "PIN Settings", pin_settings_cb);
  } else {
    ui_menu_add_entry(settings_menu, "Set Up PIN", setup_pin_cb);
  }
  ui_menu_add_entry(settings_menu, "Screen Brightness", brightness_cb);
}

// ── Public lifecycle ──

void login_settings_page_create(lv_obj_t *parent, void (*return_cb)(void)) {
  return_callback = return_cb;
  settings_screen = theme_create_page_container(parent);
  rebuild_menu();
}

void login_settings_page_show(void) {
  if (settings_menu)
    ui_menu_show(settings_menu);
}

void login_settings_page_hide(void) {
  if (settings_menu)
    ui_menu_hide(settings_menu);
}

void login_settings_page_destroy(void) {
  pin_settings_page_destroy();
  destroy_brightness_page();
  if (settings_menu) {
    ui_menu_destroy(settings_menu);
    settings_menu = NULL;
  }
  if (settings_screen) {
    lv_obj_del(settings_screen);
    settings_screen = NULL;
  }
  return_callback = NULL;
}
