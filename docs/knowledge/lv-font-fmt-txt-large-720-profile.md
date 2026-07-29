# The 1280×720 (3×) profile requires `LV_FONT_FMT_TXT_LARGE`

## Symptom
Adding the 1280×720 landscape display profile (height 720 → `PX_MULTIPLIER_300`,
`_300x` font tier at 3× the 240-height base) fails to compile the moment the
largest baked icon font is included:

```
seedsigner_icons_48_4bpp_300x.c: warning: unsigned conversion from 'int' to
  'short unsigned int:12' changes value from '4608' to '512'
  {.bitmap_index = 206280, .adv_w = 4608, .box_w = 216, .box_h = 109, ...}
seedsigner_icons_48_4bpp_300x.c: error: #error "Too large font or glyphs in
  SEEDSIGNER_ICONS_48_4BPP_300X. Enable LV_FONT_FMT_TXT_LARGE in lv_conf.h"
```

## Root cause
LVGL's `lv_font_fmt_txt` glyph descriptor packs `adv_w` and `bitmap_index` into
narrow bitfields by default (`LV_FONT_FMT_TXT_LARGE == 0`): `adv_w` is 12 bits
(max 4095) and `bitmap_index` is 20 bits. These are sized for "normal" UI fonts.

The `icon_primary_screen` role (the hero status/error icon) is a 48 px **base**
glyph. At the 3× profile it bakes at **144 px**, where a full-width PUA icon has
`adv_w = 4608` (overflows the 12-bit field) and the packed bitmap runs push
`bitmap_index` past the small-field range. The 2× (`_200x`, 96 px) variant still
fits, which is why the overflow first appears only at 3×. `lv_font_conv` detects
this at bake time and emits the `#error` guard into the generated `.c`.

## Fix
Define **`LV_FONT_FMT_TXT_LARGE=1`** binary-wide. It widens `adv_w`/`bitmap_index`
in the glyph-descriptor struct so the 144 px glyphs fit. It is a global LVGL struct
setting: every font `.c` in the binary shares the struct, so it must be defined for
the whole target, not per-font. Harmless for the smaller fonts (their values fit
either field width; the cost is a few bytes per glyph descriptor).

In this repo the desktop tools build with `LV_CONF_SKIP`, so the flag is a compile
definition in each of the four tool `CMakeLists.txt`
(`screenshot_generator`, `screen_runner`, `web_runner`, `runner_core/test`),
added next to `LV_USE_QRCODE=1`.

## Downstream (hardware builds) — REQUIRED when targeting 720
The ESP32 (`seedsigner-micropython-builder`) and Pi Zero (`seedsigner-raspi-lvgl`)
builds carry their own `lv_conf.h`. Any hardware build that compiles the
`SUPPORT_DISPLAY_HEIGHT_720` profile (and thus links `seedsigner_icons_48_4bpp_300x`)
**must set `LV_FONT_FMT_TXT_LARGE 1` in its `lv_conf.h`**, or it hits the same
`#error`. A build that targets only the smaller heights does not include the 144 px
font and does not need the flag.

## Why it's not obvious
- The overflow is invisible at 1×/1.333×/2× — it only crosses the field limit at 3×,
  so nothing about the existing profiles hints at it.
- The failing line is in a *generated* font file, and the guard is a bake-time
  `#error`, not a runtime bug — easy to misread as a corrupt asset rather than a
  global LVGL config gap.
