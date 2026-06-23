// Decimal BIP39 word-number input, following Krux's 12/24-word flow.

#include "word_numbers_input.h"
#include "../../ui/dialog.h"
#include "../../ui/input_helpers.h"
#include "../../ui/keyboard.h"
#include "../../ui/theme_widgets.h"
#include "../../ui/word_selector.h"
#include "../../utils/secure_mem.h"
#include "../shared/mnemonic_editor.h"
#include "word_numbers_logic.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wally_bip39.h>

#define MAX_WORDS 24
#define MAX_MNEMONIC_LEN 256

static lv_obj_t *word_numbers_screen = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *input_label = NULL;
static lv_obj_t *keypad = NULL;
static struct words *bip39_wordlist = NULL;
static void (*return_callback)(void) = NULL;
static void (*success_callback)(void) = NULL;
static uint16_t entered_numbers[MAX_WORDS];
static size_t target_word_count = 0;
static size_t current_word_index = 0;
static char current_number[6];
static size_t current_number_len = 0;
static uint16_t pending_number = 0;

enum {
  KEYPAD_BACKSPACE_BUTTON = 9,
  KEYPAD_OK_BUTTON = 11,
  KEYPAD_BUTTON_COUNT = 12,
};

static const char *const keypad_map[] = {"1",
                                         "2",
                                         "3",
                                         "\n",
                                         "4",
                                         "5",
                                         "6",
                                         "\n",
                                         "7",
                                         "8",
                                         "9",
                                         "\n",
                                         LV_SYMBOL_BACKSPACE,
                                         "0",
                                         LV_SYMBOL_OK,
                                         ""};

static bool parse_current_number(uint16_t *number_out) {
  return word_numbers_parse_decimal(current_number, number_out);
}

static const char *word_for_number(uint16_t number) {
  if (!bip39_wordlist || number < 1 || number > 2048)
    return NULL;
  return bip39_get_word_by_index(bip39_wordlist, number - 1);
}

static bool digit_can_be_appended(char digit) {
  return word_numbers_digit_can_append(current_number, current_number_len,
                                       digit);
}

static void update_keypad(void) {
  if (!keypad)
    return;

  char display[8];
  snprintf(display, sizeof(display), "%s_",
           current_number_len ? current_number : "");
  lv_label_set_text(input_label, display);

  for (uint32_t button = 0; button < KEYPAD_BUTTON_COUNT; button++) {
    const char *text = lv_buttonmatrix_get_button_text(keypad, button);
    if (!text || text[0] < '0' || text[0] > '9')
      continue;
    if (digit_can_be_appended(text[0]))
      lv_buttonmatrix_clear_button_ctrl(keypad, button,
                                        LV_BUTTONMATRIX_CTRL_DISABLED);
    else
      lv_buttonmatrix_set_button_ctrl(keypad, button,
                                      LV_BUTTONMATRIX_CTRL_DISABLED);
  }

  uint16_t number = 0;
  if (parse_current_number(&number))
    lv_buttonmatrix_clear_button_ctrl(keypad, KEYPAD_OK_BUTTON,
                                      LV_BUTTONMATRIX_CTRL_DISABLED);
  else
    lv_buttonmatrix_set_button_ctrl(keypad, KEYPAD_OK_BUTTON,
                                    LV_BUTTONMATRIX_CTRL_DISABLED);

  if (current_number_len > 0 || current_word_index > 0)
    lv_buttonmatrix_clear_button_ctrl(keypad, KEYPAD_BACKSPACE_BUTTON,
                                      LV_BUTTONMATRIX_CTRL_DISABLED);
  else
    lv_buttonmatrix_set_button_ctrl(keypad, KEYPAD_BACKSPACE_BUTTON,
                                    LV_BUTTONMATRIX_CTRL_DISABLED);
}

static void update_title(void) {
  if (!title_label)
    return;
  char title[32];
  snprintf(title, sizeof(title), "Word %u/%u",
           (unsigned)(current_word_index + 1), (unsigned)target_word_count);
  lv_label_set_text(title_label, title);
}

static void clear_current_number(void) {
  secure_memzero(current_number, sizeof(current_number));
  current_number_len = 0;
}

static bool build_mnemonic(char mnemonic[MAX_MNEMONIC_LEN]) {
  size_t used = 0;
  mnemonic[0] = '\0';

  for (size_t i = 0; i < current_word_index; i++) {
    const char *word = word_for_number(entered_numbers[i]);
    if (!word) {
      secure_memzero(mnemonic, MAX_MNEMONIC_LEN);
      dialog_show_error_timeout("Failed to load wordlist", NULL, 0);
      return false;
    }
    int written = snprintf(mnemonic + used, MAX_MNEMONIC_LEN - used, "%s%s",
                           i ? " " : "", word);
    if (written < 0 || (size_t)written >= MAX_MNEMONIC_LEN - used) {
      secure_memzero(mnemonic, MAX_MNEMONIC_LEN);
      dialog_show_error_timeout("Mnemonic is too long", NULL, 0);
      return false;
    }
    used += (size_t)written;
  }
  return true;
}

static void finish_mnemonic(void) {
  char mnemonic[MAX_MNEMONIC_LEN];
  if (!build_mnemonic(mnemonic))
    return;

  word_numbers_input_page_hide();
  mnemonic_editor_page_create(lv_screen_active(), return_callback,
                              success_callback, mnemonic, false);
  mnemonic_editor_page_show();
  secure_memzero(mnemonic, sizeof(mnemonic));
}

static void word_confirmation_cb(bool confirmed, void *user_data) {
  (void)user_data;
  if (!confirmed) {
    pending_number = 0;
    clear_current_number();
    update_keypad();
    return;
  }

  entered_numbers[current_word_index++] = pending_number;
  pending_number = 0;
  clear_current_number();

  if (current_word_index >= target_word_count) {
    finish_mnemonic();
  } else {
    update_title();
    update_keypad();
  }
}

static void confirm_current_number(void) {
  uint16_t number = 0;
  if (!parse_current_number(&number))
    return;
  const char *word = word_for_number(number);
  if (!word)
    return;

  pending_number = number;
  char message[64];
  snprintf(message, sizeof(message), "Word %u\n%u: %s",
           (unsigned)(current_word_index + 1), (unsigned)number, word);
  dialog_show_confirm(message, word_confirmation_cb, NULL,
                      DIALOG_STYLE_OVERLAY);
}

static void keypad_event_cb(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED)
    return;
  uint32_t button = lv_buttonmatrix_get_selected_button(keypad);
  const char *text = lv_buttonmatrix_get_button_text(keypad, button);
  if (!text)
    return;

  if (strcmp(text, LV_SYMBOL_BACKSPACE) == 0) {
    if (current_number_len > 0) {
      current_number[--current_number_len] = '\0';
    } else if (current_word_index > 0) {
      uint16_t previous = entered_numbers[--current_word_index];
      entered_numbers[current_word_index] = 0;
      snprintf(current_number, sizeof(current_number), "%u",
               (unsigned)previous);
      current_number_len = strlen(current_number);
      update_title();
    }
    update_keypad();
    return;
  }

  if (strcmp(text, LV_SYMBOL_OK) == 0) {
    confirm_current_number();
    return;
  }

  char digit = text[0];
  if (!digit_can_be_appended(digit))
    return;
  current_number[current_number_len++] = digit;
  current_number[current_number_len] = '\0';
  update_keypad();

  if (word_numbers_should_autocomplete(current_number, current_number_len))
    confirm_current_number();
}

static void exit_confirmation_cb(bool confirmed, void *user_data) {
  (void)user_data;
  if (confirmed && return_callback)
    return_callback();
}

static void back_cb(lv_event_t *event) {
  (void)event;
  dialog_show_confirm("Are you sure?", exit_confirmation_cb, NULL,
                      DIALOG_STYLE_OVERLAY);
}

static void word_count_back_cb(void) {
  if (return_callback)
    return_callback();
}

static void create_number_input_ui(void) {
  ui_create_back_button(word_numbers_screen, back_cb);

  title_label = theme_create_page_title(word_numbers_screen, "");

  input_label = lv_label_create(word_numbers_screen);
  lv_label_set_text(input_label, "_");
  lv_obj_set_style_text_color(input_label, highlight_color(), 0);
  lv_obj_set_style_text_font(input_label, theme_font_medium(), 0);
  ui_keyboard_align_input_label(input_label);

  keypad = lv_buttonmatrix_create(word_numbers_screen);
  lv_buttonmatrix_set_map(keypad, keypad_map);
  ui_keyboard_align_key_matrix(keypad);
  theme_apply_btnmatrix(keypad);
  lv_obj_add_event_cb(keypad, keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  update_title();
  update_keypad();
}

static void word_count_selected_cb(int word_count) {
  target_word_count = (size_t)word_count;
  current_word_index = 0;
  clear_current_number();
  secure_memzero(entered_numbers, sizeof(entered_numbers));
  create_number_input_ui();
}

void word_numbers_input_page_create(lv_obj_t *parent, void (*return_cb)(void),
                                    void (*success_cb)(void)) {
  if (!parent)
    return;

  return_callback = return_cb;
  success_callback = success_cb;
  secure_memzero(entered_numbers, sizeof(entered_numbers));
  clear_current_number();
  target_word_count = 0;
  current_word_index = 0;
  pending_number = 0;

  if (bip39_get_wordlist(NULL, &bip39_wordlist) != WALLY_OK ||
      !bip39_wordlist) {
    dialog_show_error_timeout("Failed to load wordlist", return_cb, 0);
    return;
  }

  word_numbers_screen = theme_create_page_container(parent);
  ui_word_count_selector_create(word_numbers_screen, word_count_back_cb,
                                word_count_selected_cb);
}

void word_numbers_input_page_show(void) {
  if (word_numbers_screen)
    lv_obj_clear_flag(word_numbers_screen, LV_OBJ_FLAG_HIDDEN);
}

void word_numbers_input_page_hide(void) {
  if (word_numbers_screen)
    lv_obj_add_flag(word_numbers_screen, LV_OBJ_FLAG_HIDDEN);
}

void word_numbers_input_page_destroy(void) {
  if (word_numbers_screen) {
    lv_obj_del(word_numbers_screen);
    word_numbers_screen = NULL;
  }
  title_label = NULL;
  input_label = NULL;
  keypad = NULL;
  bip39_wordlist = NULL;
  return_callback = NULL;
  success_callback = NULL;
  target_word_count = 0;
  current_word_index = 0;
  current_number_len = 0;
  pending_number = 0;
  secure_memzero(current_number, sizeof(current_number));
  secure_memzero(entered_numbers, sizeof(entered_numbers));
}
