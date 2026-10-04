# Theming

Two colour sources, and it matters which is which.

**Seed-derived roles** follow `theme.seed`. Change the seed and the whole bar
retones.

**Fixed roles** do not. They mean the same thing on every machine, so tinting
them from the seed would only make them look arbitrary.

```
theme.seed   ->  primary, surfaces, outlines          (follow the seed)
             ->  error, charging, full, tertiary      (fixed: they are states,
                                                       not a mood)
```

---

## Where colours live

`Theme` in `src/core/theme.h` is the C++ generator. It produces **M3 roles** from
one seed colour using OKLCH, so the palette stays perceptually even instead of
just being "somewhere in the neighbourhood".

`src/qml/theme.qml` is the QML-facing singleton. It exists mostly as a
discoverability aid: the real values come from C++, and the literals in
`theme.qml` are the fallback if generation ever fails.

> QML reads a single `theme` object. Do not scatter raw `#rrggbb` through module
> files. Tokens only.

---

## The seed

```json
{ "theme": { "seed": "#89b4fa" } }
```

| Role | L | C | Notes |
|---|---|---|---|
| `primary` | 0.65 | 0.22 | The accent. Readable on the dark surfaces. |
| `onPrimary` | 0.98 | 0.01 | Text on `primary`. |
| `primaryContainer` | 0.35 | 0.15 | Low-key accent fill. |
| `onPrimaryContainer` | 0.95 | 0.05 | Text on `primaryContainer`. |
| `surface` | 0.15 | 0.02 | Bar background at full opacity. |
| `onSurface` | 0.95 | 0.02 | Text on `surface`. |
| `surfaceContainer` | 0.20 | 0.03 | Popup backgrounds. |
| `onSurfaceContainer` | 0.90 | 0.03 | Text inside popups. |
| `outline` | 0.50 | 0.02 | Borders. |
| `surfaceVariant` | 0.25 | 0.04 | Secondary fills. |
| `tertiary` | 0.78 | 0.16 | Amber. Fixed. Battery saver. |
| `error` | 0.62 | 0.19 | Red. Fixed. Low battery. |
| `charging` | 0.72 | 0.17 | Green. Fixed. Gaining charge. |
| `full` | 0.58 | 0.16 | Deep blue. Fixed. At capacity. |

`L` is lightness, `C` is chroma, both 0-1. The hue comes from the seed.

**Every role must come back opaque and in sRGB.** `test_theme.cpp` asserts that
for eight seeds, because a colour outside the gamut renders as black or as
nothing at all, and that is not something you want to discover by looking at a
screenshot.

---

## `barTextColor` is not a theme role

This is the one colour that does not come from the seed.

```json
{ "theme": { "autoContrast": true,
             "textOnLight": "#101010",
             "textOnDark": "#FFFFFF",
             "lightThreshold": 165 } }
```

`ContrastService` samples the strip of wallpaper the bar actually covers,
converts it to linear light, and picks whichever of the two text colours has the
higher **WCAG contrast ratio** against it. The result is pushed onto
`Theme::barTextColor` rather than exposed per-module, so QML has one property to
read.

**Popups never follow the wallpaper.** They paint their own opaque
`surfaceContainer`, so they always use `onSurfaceContainer`. A popup whose text
changed colour with the wallpaper would be unreadable on its own background.

### The threshold

`lightThreshold` is the perceived brightness the background must reach before
light text flips to dark. Pastel wallpapers stay on white even where black would
score a higher ratio, because pastels read as "dark-ish" to a person and
"white" to a formula.

### Two bugs that shaped this

**WCAG ratios are asymmetric.** For text on a background of luminance `L`,
white scores `1.05 / (L + 0.05)` and black scores `(L + 0.05) / (L_black + 0.05)`.
Dividing by the background on both sides inverts the comparison and white always
wins, so the picker never fired. This is easy to get wrong twice.

**Setters run before their signals are connected.** In `main.cpp` the contrast
service is configured, then connected, then the current value is applied once by
hand. Wiring order is load-bearing:

```cpp
QObject::connect(&contrast, &ContrastService::changed,
                 &theme, [&]() { theme.setBarTextColor(contrast.textColorValue()); });
theme.setBarTextColor(contrast.textColorValue());
```

### Sampling order

The wallpaper path is not always where you expect:

1. `SPI_GETDESKWALLPAPER`
2. the registry `Wallpaper` value
3. `TranscodedImageCache`, which holds the real path as UTF-16 inside a binary
   blob
4. a live `BitBlt` of the screen strip 8px below the bar

All three of the first options come back empty while a wallpaper change is in
flight, and on some machines permanently. Chain them or the colour never
updates.

---

## Fonts

```json
{ "theme": { "font": "Poppins" } }
```

Poppins ships in the qrc and is registered at startup with
`QFontDatabase::addApplicationFont`, so no system install is needed.

> **Icon font family names are resolved at runtime.** The Material Symbols subset
> reports one family per weight (`ExtraLight`, `Light`, `Medium`, `Regular`,
> `SemiBold`, `Thin`), so a hardcoded name breaks. `loadIconFont()` in
> `main.cpp` takes `applicationFontFamilies(id).first()` and exposes the result
> as the `iconFont` context property. Use `font.family: iconFont` in QML, never a
> literal.

### Sharpness

Text is rendered with Qt's grayscale antialiasing; the taskbar uses ClearType.
Poppins at 10-13px also has fewer stems than the taskbar's face. If you want a
sharper bar, raise the sizes or switch `theme.font` to Segoe UI Variable. Qt
cannot use ClearType for QML text on a layered window, so there is no setting
that fixes it outright.

---

## The icon subset

`resources/fonts/MaterialSymbolsRounded-subset.ttf` is **39 KB**. The full
variable font is 1.2 MB, 88% of which is the `gvar` table holding FILL / GRAD /
opsz / wght variants for 6646 glyphs that are never touched, because every glyph
is drawn at the default weight.

The subset is a **closed list**. Adding a glyph means regenerating the subset, so
codepoints live in config or in a comment next to them, never typed from memory.
Read them back from the font when you are unsure:

```python
from fontTools.ttLib import TTFont
f = TTFont('resources/fonts/MaterialSymbolsRounded-subset.ttf')
for cp in sorted(f.getBestCmap()):
    print('U+%04X' % cp)
```

### Regenerating

```python
# 1. download the full font from Google Fonts
# 2. subset it to the codepoints you need
pyftsubset MaterialSymbolsRounded-full.ttf \
  --unicodes=U+E04D,U+E04F,... \
  --output-file=MaterialSymbolsRounded-subset.ttf
```

**Keep it a variable font.** The old subset is one, and `fvar` / `gvar` / `HVAR`
are what make Qt report the weight-named families. Subset from a *static*
instance and the family name changes, and every icon in the bar silently
disappears.

When you need to add glyphs to an existing variable subset, merge into it rather
than regenerating:

```python
# copy each new glyph's outline, hmtx entry and cmap entry into the base font
base_glyf.glyphs[new_name] = copy.deepcopy(extra_glyf.glyphs[src_name])
base['hmtx'].metrics[new_name] = extra['hmtx'].metrics[src_name]
base_cmap[cp] = new_name
base['maxp'].numGlyphs = len(base.getGlyphOrder())
```

Every glyph returned `numberOfContours > 0`, which is the check that the merge
actually carried outlines rather than empty placeholders.

> Material Symbols has **no ligatures** in the variable font. It is PUA-mapped
> only, so writing `"memory"` in QML renders nothing. Subsetting is therefore
> safe: nothing depends on ligature substitution.

---

## Verified codepoints

Read from the font, not from memory.

| Glyph | Codepoint |
|---|---|
| `volume_up` / `volume_down` / `volume_off` | `U+E050` / `U+E04D` / `U+E04F` |
| `wifi` / `wifi_off` / `lan` | `U+E63E` / `U+E648` / `U+EB2F` |
| `developer_board` (CPU) | `U+E30D` |
| `memory` (RAM) | `U+E322` |
| `data_usage` (network throughput) | `U+E1AF` |
| `bolt` | `U+EA0B` |
| `battery_android_frame_1` … `frame_6` | `U+F257` … `U+F252` |
| `battery_android_frame_full` | `U+F24F` |
| `battery_android_bolt` | `U+F305` |
| `battery_android_frame_bolt` | `U+F250` |
| `battery_android_plus` | `U+F303` |
| `battery_android_frame_plus` | `U+F24E` |
| `battery_android_frame_shield` | `U+F24B` |

The old `battery_0_bar` … `battery_6_bar` family (`U+EBDC` and friends) and
`battery_horiz_*` (`U+F8AE`–`U+F8B0`) are still in the subset but unused. The
`battery_android_*` family is the one in use because it has a `frame_` variant
for every state the bar distinguishes: plain fill, charging, saver, and held at
full.

---

## Verifying a theme change

1. Build and restart. Config hot reload covers values, not code.
2. Check a **light** wallpaper and a **dark** one. The adaptive text colour is
   the part most likely to be wrong, and a single wallpaper hides it.
3. Check a **pastel** one separately. That is where the threshold, not the
   ratio, decides, and it is the case users notice.
4. `ctest --test-dir build -C Release` — `test_theme` covers gamut, determinism
   and the fixed-hue roles.