# C++17 SDK

Header-only, handwritten C++17 implementation with no external runtime or library dependencies.
Include `glance_widgets/glance_widgets.hpp` (or the short `glance_widgets.hpp` entry).
Copy the entire `include/` directory when distributing the SDK.

```cpp
#include <glance_widgets/glance_widgets.hpp>
#include <iostream>
using namespace glance_widgets;

int main() {
  Matrix pixels(12, 5);
  pixels.line(0, 0, 11, 4, std::string("#4cf")).text("OK", 1, 0);
  std::cout << to_response(pixels.to_root()).to_json() << '\n';

  PixelProgressBarOptions options;
  options.segments = 10;
  options.thickness = 3;
  auto part = pixel_progress_bar(70, options);
  PlaceOptions placement;
  placement.id = "progress";
  placement.path = "data.progress";
  placement.frame = Box::canvas(Canvas{300, 150}).frame();
  std::cout << to_response(place(part, placement)).to_json() << '\n';
}
```

Compile with `c++ -std=c++17 -ffp-contract=off -I /path/to/sdks/cpp/include example.cpp`
(`-ffp-contract=off` keeps floating-point results identical to the TypeScript reference;
the CMake target adds it for GCC/Clang). Minimum toolchains: GCC 9+, Clang 10+, MSVC 2019.
No C++20 or `std::to_chars(double)` support is required; number formatting is locale-independent.
Layer constructors return a `Composite` containing `layers` and `data`. Each declaration has only id, type and dataPath; its addressed object contains layout, content and style. Use the standard merge function to combine individual layers and chart components, then finalize the response.
A node holds layout (`frame`, `zIndex`, `rotation`, `transform`), `content` and `style`.
`layer_path(id)` derives the default node path; set `LayerOptions.data_path` for an explicit location.
`Matrix::to_part()` returns `{content: matrixData}`; `place` adds node layout.
Editor layers bind to partial nodes in `data` without a remote declaration.
Use `validate_layer_node(node, type)` for partial-node validation and `validate(response)` for complete responses.
Missing or null node fields retain editor settings, objects merge recursively and arrays replace whole.
A supplied frame replaces the entire saved frame and needs `width` and `height`.
Matrix pixels, default, batch and layout belong to `content`; matrix `style` has no fields.
Image artwork uses `content.url`; a glyph icon uses `icon`, `iconCodePoint` and `iconFont`.

CMake exports the `GlanceWidgets::SDK` interface target and installs all headers:

```sh
cmake -S sdks/cpp -B build/cpp
cmake --build build/cpp
cmake --install build/cpp --prefix /your/install/prefix
```

Consumers can use `find_package(GlanceWidgets CONFIG REQUIRED)` and link `GlanceWidgets::SDK`.

## API map

Functions use snake_case; protocol JSON keys retain the TypeScript spelling.
`rotate90` retains its numeric name. Matrix drawing mutates and returns `Matrix&`;
transforms return independent copies. JSON copies also own independent values.

| TypeScript module | C++ header / API |
| --- | --- |
| protocol | `protocol/validate.hpp`: `validate`, `resolve_data_path`, constants |
| color | `color.hpp`: `Rgba`, parse/format/normalize, `rgb`, `with_alpha`, `mix_colors` |
| output | `output.hpp`: `merge`, `place`, `to_response`, `layer_id`, `nest_at_path`, `empty_root` |
| geometry / box | `geometry.hpp`, `box.hpp`: `Canvas`, `Frame`, `Rect`, `MatrixLayout`, `solve_grid`, `Box` |
| matrix / raster | `matrix.hpp`, `raster.hpp`: `Matrix`, `PixelStyle`, `PixelOverrides`, `PixelInput`, drawing options |
| font / text-matrix | `font.hpp`, `text_matrix.hpp`: `PixelFont`, `define_font`, `get_font`, `layout_text`, `measure_text`, `text_matrix` |
| themes / pixelate | `themes.hpp`, `pixelate.hpp`: `themes`, `get_theme`, `theme_options`, `pixelate` |
| icons / signature | `icons.hpp`, `signature.hpp`: `icon_glyph`, `has_icon`, `icon_names`, `sign`, `verify_signature` |
| layers | `layers.hpp`: text, line, horizontal/vertical line, shape, icon, image, curve |
| components | `components/`: progress (modern + pixel), ring, battery, signal, status strip, heatmaps, bar/line/area charts, sparkline, stat, pixel text |

Layer and component functions produce ordered `Json` protocol values. Typed options
are available alongside JSON adapters; component adapters preserve protocol key names.
Component algorithms consume native option structures; JSON adapters parse nullable
fields before calling them. Layer placement uses `Frame`, and style fields use
`LayerStyle` with `std::optional` members. `solve_grid` returns `SolvedGrid`; use
its `to_json()` method when a protocol value is needed. `Box::frame()` returns
a native `Frame`, while `Box::to_frame()` returns the protocol object.

For a labeled component, provide a point-sized canvas and use the native options:

```cpp
LabeledPixelProgressBarOptions progress;
progress.canvas = Canvas{300, 150};
progress.theme = themes.led;
progress.label = "70%";
auto response = to_response(labeled_pixel_progress_bar(70, progress));

TextOptions title;
title.id = "title";
title.frame = Box(0, 0, 1, 0.2).frame();
title.text = "Status";
title.style.font_size_ratio = 0.08;
auto layer = text(title);
```

`Json::object` and `Json::array` distinguish empty objects from arrays. Use `parse`,
`to_json`/`dump`, `number`, `string`, `boolean`, `size` and `is_*` for protocol data.
Missing const object lookups return null; negative/out-of-range array indices throw.
Strings use UTF-8; lone escaped UTF-16 surrogates round-trip as JSON escapes.
Numbers follow binary64 shortest formatting; non-finite values serialize as null,
while protocol validation rejects them with a field path.

Input errors throw `GlanceError`; `GlanceValidationError` adds the `path` member.
Font registration is synchronized and returned fonts do not alias registry state.

## Data and verification

`generated/fonts.hpp`, `generated/icons.hpp` and `generated/advances.hpp` contain
only data from `spec/fonts/*.json`, `spec/icons.json` and `spec/text-advances.json`.
In the development repository, regenerate them with:

```sh
python3 sdks/cpp/scripts/sync-data.py
```

Validation schemas are handwritten constant tables. The shared text advance snapshot
contains the SF Pro measurements produced by `spec/tools/measure-text-advances.py`,
also used by the reference implementation. Updating measurements is separate from
embedding the snapshot and requires the font/tool dependencies documented there.

The golden runner calls public C++ APIs and compares against `spec/golden/*.json`.
The SDK never loads golden fixtures. To run the checks when desired:

```sh
bash sdks/cpp/test.sh
ctest --test-dir build/cpp --output-on-failure
```

`bash sdks/cpp/test.sh` builds every test target with
`-Wall -Wextra -Wconversion -Wshadow -ffp-contract=off` and runs them. Set
`SANITIZE=1` (for example `SANITIZE=1 bash sdks/cpp/test.sh`) to build with
`-fsanitize=address,undefined`. The suite includes shared protocol fixtures and
native API, color, signature and geometry checks.
