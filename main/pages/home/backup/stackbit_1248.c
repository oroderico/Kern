// Stackbit 1248 mnemonic backup transcription helper

#include "stackbit_1248.h"
#include "../../../core/key.h"
#include "../../../ui/input_helpers.h"
#include "../../../ui/theme_widgets.h"
#include "../../../utils/secure_mem.h"
#include "stackbit_1248_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wally_bip39.h>

#define DEFAULT_WORDS_PER_PAGE 6
#define TALL_SCREEN_WORDS_PER_PAGE 8

static lv_obj_t *stackbit_screen = NULL;
static lv_obj_t *content = NULL;
static lv_obj_t *page_label = NULL;
static lv_obj_t *previous_button = NULL;
static lv_obj_t *next_button = NULL;
static char **mnemonic_words = NULL;
static uint16_t *word_numbers = NULL;
static size_t mnemonic_word_count = 0;
static size_t current_page = 0;
static void (*return_callback)(void) = NULL;

static size_t words_per_page(void) {
  // Portrait displays from 480x800 upwards can show a few more words without
  // crowding. Keep the shorter and landscape displays at six rows.
  return theme_screen_height() >= 800 && !theme_is_landscape()
             ? TALL_SCREEN_WORDS_PER_PAGE
             : DEFAULT_WORDS_PER_PAGE;
}

static void back_cb(lv_event_t *e) {
  (void)e;
  if (return_callback)
    return_callback();
}

static uint16_t bip39_word_number(struct words *wordlist, const char *word) {
  if (!wordlist || !word)
    return 0;

  size_t low = 0;
  size_t high = 2048;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    const char *candidate = bip39_get_word_by_index(wordlist, mid);
    if (!candidate)
      return 0;
    int comparison = strcmp(word, candidate);
    if (comparison == 0)
      return (uint16_t)(mid + 1); // Stackbit uses BIP39 numbers 1-2048.
    if (comparison < 0)
      high = mid;
    else
      low = mid + 1;
  }
  return 0;
}

static bool precompute_word_numbers(void) {
  struct words *wordlist = NULL;
  if (bip39_get_wordlist(NULL, &wordlist) != WALLY_OK || !wordlist)
    return false;

  word_numbers = calloc(mnemonic_word_count, sizeof(*word_numbers));
  if (!word_numbers)
    return false;

  for (size_t i = 0; i < mnemonic_word_count; i++) {
    word_numbers[i] = bip39_word_number(wordlist, mnemonic_words[i]);
    if (word_numbers[i] == 0) {
      free(word_numbers);
      word_numbers = NULL;
      return false;
    }
  }
  return true;
}

static const char *const stackbit_map[] = {"1", "1",  "2", "1", "2", "1",
                                           "2", "\n", "2", "4", "8", "4",
                                           "8", "4",  "8", ""};

static lv_obj_t *create_pattern(lv_obj_t *parent, uint16_t number,
                                int cell_size) {
  lv_obj_t *matrix = lv_buttonmatrix_create(parent);
  lv_obj_remove_style_all(matrix);
  lv_buttonmatrix_set_map(matrix, stackbit_map);
  // One-pixel inset on every side keeps the outer item borders inside the
  // matrix clip area instead of shaving off the bottom/right edge.
  lv_obj_set_size(matrix, 7 * cell_size + 2, 2 * cell_size + 2);
  lv_obj_clear_flag(matrix, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_set_style_pad_all(matrix, 1, 0);
  lv_obj_set_style_pad_row(matrix, 0, 0);
  lv_obj_set_style_pad_column(matrix, 0, 0);
  lv_obj_set_style_border_width(matrix, 0, 0);
  lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, 0);

  lv_obj_set_style_radius(matrix, 0, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(matrix, panel_color(), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_color(matrix, secondary_color(), LV_PART_ITEMS);
  lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
  lv_obj_set_style_text_color(matrix, secondary_color(), LV_PART_ITEMS);
  lv_obj_set_style_text_font(matrix, theme_font_small(), LV_PART_ITEMS);

  lv_obj_set_style_bg_color(matrix, highlight_color(),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(matrix, LV_OPA_40, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(matrix, highlight_color(),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(matrix, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(matrix, primary_color(),
                              LV_PART_ITEMS | LV_STATE_CHECKED);

  bool marks[STACKBIT_1248_MARK_COUNT];
  if (!stackbit_1248_encode_number(number, marks))
    return matrix;
  lv_buttonmatrix_ctrl_t controls[STACKBIT_1248_MARK_COUNT] = {0};
  for (size_t i = 0; i < STACKBIT_1248_MARK_COUNT; i++)
    controls[i] = marks[i] ? LV_BUTTONMATRIX_CTRL_CHECKED : 0;
  lv_buttonmatrix_set_ctrl_map(matrix, controls);
  return matrix;
}

static void add_word_pattern(lv_obj_t *parent, size_t word_index,
                             int cell_size) {
  uint16_t number = word_numbers[word_index];

  lv_obj_t *row = theme_create_flex_row(parent);
  lv_obj_set_size(row, LV_PCT(100), 2 * cell_size + 2);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, theme_small_padding(), 0);

  lv_obj_t *position = lv_label_create(row);
  lv_label_set_text_fmt(position, "%u", (unsigned)(word_index + 1));
  int32_t position_width = theme_min_dim() / 14;
  if (position_width < 22)
    position_width = 22;
  lv_obj_set_width(position, position_width);
  lv_obj_set_style_text_align(position, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(position, theme_font_small(), 0);
  lv_obj_set_style_text_color(position, secondary_color(), 0);

  create_pattern(row, number, cell_size);

  lv_obj_t *details = theme_create_flex_column(row);
  // Reserve enough room for the widest eight-letter BIP39 words on the
  // 320-pixel display without pushing the centered row beyond the screen.
  lv_obj_set_width(details, theme_min_dim() / 4);
  lv_obj_set_flex_align(details, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  lv_obj_t *number_label = lv_label_create(details);
  lv_label_set_text_fmt(number_label, "%04u", (unsigned)number);
  lv_obj_set_style_text_font(number_label, theme_font_small(), 0);
  lv_obj_set_style_text_color(number_label, primary_color(), 0);

  lv_obj_t *word_label = lv_label_create(details);
  lv_label_set_text_static(word_label, mnemonic_words[word_index]);
  lv_label_set_long_mode(word_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(word_label, LV_PCT(100));
  lv_obj_set_style_text_font(word_label, theme_font_small(), 0);
  lv_obj_set_style_text_color(word_label, primary_color(), 0);
}

static void render_page(void) {
  if (!content || mnemonic_word_count == 0)
    return;

  lv_obj_clean(content);
  int cell_size = theme_min_dim() / 12;
  int max_cell_size = theme_screen_height() >= 1000 ? 40 : 32;
  if (cell_size < 26)
    cell_size = 26;
  else if (cell_size > max_cell_size)
    cell_size = max_cell_size;

  size_t page_size = words_per_page();
  size_t first = current_page * page_size;
  size_t last = first + page_size;
  if (last > mnemonic_word_count)
    last = mnemonic_word_count;

  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(content, theme_key_gap(), 0);
  lv_obj_set_style_pad_column(content, 0, 0);
  for (size_t i = first; i < last; i++)
    add_word_pattern(content, i, cell_size);

  size_t page_count = (mnemonic_word_count + page_size - 1) / page_size;
  lv_label_set_text_fmt(page_label, "%u/%u", (unsigned)(current_page + 1),
                        (unsigned)page_count);

  if (current_page == 0)
    lv_obj_add_state(previous_button, LV_STATE_DISABLED);
  else
    lv_obj_remove_state(previous_button, LV_STATE_DISABLED);

  if (current_page + 1 >= page_count)
    lv_obj_add_state(next_button, LV_STATE_DISABLED);
  else
    lv_obj_remove_state(next_button, LV_STATE_DISABLED);
}

static void previous_cb(lv_event_t *e) {
  (void)e;
  if (current_page > 0) {
    current_page--;
    render_page();
  }
}

static void next_cb(lv_event_t *e) {
  (void)e;
  size_t page_size = words_per_page();
  size_t page_count = (mnemonic_word_count + page_size - 1) / page_size;
  if (current_page + 1 < page_count) {
    current_page++;
    render_page();
  }
}

void stackbit_1248_page_create(lv_obj_t *parent, void (*return_cb)(void)) {
  if (!parent || !key_is_loaded())
    return;

  if (!key_get_mnemonic_words(&mnemonic_words, &mnemonic_word_count))
    return;

  if (!precompute_word_numbers()) {
    for (size_t i = 0; i < mnemonic_word_count; i++)
      SECURE_FREE_STRING(mnemonic_words[i]);
    free(mnemonic_words);
    mnemonic_words = NULL;
    mnemonic_word_count = 0;
    if (return_cb)
      return_cb();
    return;
  }

  return_callback = return_cb;
  current_page = 0;
  stackbit_screen = theme_create_page_container(parent);
  ui_create_back_button(stackbit_screen, back_cb);
  theme_create_page_title(stackbit_screen, "Stackbit 1248");

  content = theme_create_flex_column(stackbit_screen);
  lv_obj_set_width(content, LV_PCT(98));
  lv_obj_set_style_pad_row(content, theme_key_gap(), 0);
  lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *nav = theme_create_flex_row(stackbit_screen);
  lv_obj_set_size(nav, LV_PCT(55), theme_min_touch_size());
  lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, -theme_small_padding());

  previous_button = theme_create_button(nav, LV_SYMBOL_LEFT, false);
  lv_obj_set_size(previous_button, theme_min_touch_size(),
                  theme_min_touch_size());
  lv_obj_add_event_cb(previous_button, previous_cb, LV_EVENT_CLICKED, NULL);

  page_label = theme_create_label(nav, "", true);
  lv_obj_set_style_text_font(page_label, theme_font_small(), 0);

  next_button = theme_create_button(nav, LV_SYMBOL_RIGHT, false);
  lv_obj_set_size(next_button, theme_min_touch_size(), theme_min_touch_size());
  lv_obj_add_event_cb(next_button, next_cb, LV_EVENT_CLICKED, NULL);

  render_page();
}

void stackbit_1248_page_show(void) {
  if (stackbit_screen)
    lv_obj_clear_flag(stackbit_screen, LV_OBJ_FLAG_HIDDEN);
}

void stackbit_1248_page_hide(void) {
  if (stackbit_screen)
    lv_obj_add_flag(stackbit_screen, LV_OBJ_FLAG_HIDDEN);
}

void stackbit_1248_page_destroy(void) {
  if (stackbit_screen) {
    lv_obj_del(stackbit_screen);
    stackbit_screen = NULL;
  }
  for (size_t i = 0; i < mnemonic_word_count; i++)
    SECURE_FREE_STRING(mnemonic_words[i]);
  free(mnemonic_words);
  mnemonic_words = NULL;
  free(word_numbers);
  word_numbers = NULL;
  mnemonic_word_count = 0;
  content = NULL;
  page_label = NULL;
  previous_button = NULL;
  next_button = NULL;
  current_page = 0;
  return_callback = NULL;
}
