#if 1  /* must be 1 to enable this config file */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ── Colour ──────────────────────────────────────────────────────────────── */
#define LV_COLOR_DEPTH 16

/* ── Memory ──────────────────────────────────────────────────────────────── */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB   /* use system malloc/free */
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/* ── HAL ─────────────────────────────────────────────────────────────────── */
#define LV_DEF_REFR_PERIOD  10    /* ms — display refresh interval */
#define LV_DPI_DEF          130

/* ── Rendering ───────────────────────────────────────────────────────────── */
#define LV_DRAW_BUF_STRIDE_ALIGN   1
#define LV_DRAW_BUF_ALIGN          4
#define LV_DRAW_SW_ASM             LV_DRAW_SW_ASM_NONE  /* no ARM Helium/NEON on Xtensa */

/* ── Logging ─────────────────────────────────────────────────────────────── */
#define LV_USE_LOG          0

/* ── Asserts ─────────────────────────────────────────────────────────────── */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* ── Perf & debug ────────────────────────────────────────────────────────── */
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

/* ── Layer ───────────────────────────────────────────────────────────────── */
#define LV_LAYER_SIMPLE_TELEPORT_TRESHOLD 1

/* ── Fonts ───────────────────────────────────────────────────────────────── */
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   1
#define LV_FONT_UNSCII_8        0
#define LV_FONT_UNSCII_16       0

#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_FONT_PLACEHOLDER 1

/* ── Text ────────────────────────────────────────────────────────────────── */
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " _.,-;:"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI     0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/* ── Widgets ─────────────────────────────────────────────────────────────── */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BUTTON     1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      1
#define LV_USE_LED        0
#define LV_USE_LINE       1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     1
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_SWITCH     1
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    1
#define LV_USE_TEXTAREA   1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/* ── Layouts ─────────────────────────────────────────────────────────────── */
#define LV_USE_FLEX     1
#define LV_USE_GRID     1

/* ── Themes ──────────────────────────────────────────────────────────────── */
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1   /* dark theme base */
#define LV_USE_THEME_SIMPLE     1
#define LV_USE_THEME_MONO       0

/* ── Misc ────────────────────────────────────────────────────────────────── */
#define LV_USE_FRAGMENT         0
#define LV_USE_SNAPSHOT         0
#define LV_USE_MONKEY           0
#define LV_USE_GRIDNAV          0
#define LV_USE_OBSERVER         1
#define LV_USE_IME_PINYIN       0
#define LV_USE_FILE_EXPLORER    0
#define LV_USE_FONT_MANAGER     0
#define LV_USE_RL_STYLE         0
#define LV_USE_VECTOR_GRAPHIC   0
#define LV_USE_LOTTIE           0
#define LV_USE_BARCODE          0
#define LV_USE_QRCODE           0
#define LV_USE_LODEPNG          0
#define LV_USE_LIBPNG           0
#define LV_USE_BMP              0
#define LV_USE_TJPGD            0
#define LV_USE_LIBJPEG_TURBO    0
#define LV_USE_GIF              0
#define LV_USE_FFMPEG           0
#define LV_USE_FREETYPE         0
#define LV_USE_TINY_TTF         0
#define LV_USE_RLOTTIE          0

#endif /* LV_CONF_H */
#endif /* End of file guard */
