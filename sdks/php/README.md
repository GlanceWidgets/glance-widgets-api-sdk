# Glance Widgets PHP SDK

Layer constructors return a `Composite`: `{layers, data}`. Declarations contain only id, type and dataPath; the addressed objects contain layout, content and style. Merge layer results and chart components with the same merge function, then finalize with toResponse (to_response in snake_case APIs). An explicit dataPath chooses the object location; otherwise it is derived from id.

`Matrix.toPart()` returns `{content: MatrixData}`. Place it with `place` / `Place` to give its node a frame and data path. The node's `content` holds `pixels`, `default`, `batch` and `layout`; matrix `style` is empty. Image and image-backed icon nodes use `content.url`. Bind an editor layer to a partial node in `data` without adding a remote declaration. Omitted or null values keep the saved editor value; objects merge field by field, arrays replace, and a supplied frame replaces the whole frame and must include width and height. Each response starts from the saved configuration.

Handwritten PHP 8.1+ SDK for protocol v1. It uses PHP's standard libraries and has no runtime dependencies.
Load `src/GlanceWidgets.php` directly, or load Composer's autoloader.

```php
require 'src/GlanceWidgets.php';
use GlanceWidgets\Box;
use function GlanceWidgets\{pixelProgressBar, place, toResponse};

$canvas = ['width' => 338, 'height' => 158];
$frame = Box::canvas($canvas)->sub(0.1, 0.6, 0.8, 0.2)->toFrame();
$bar = pixelProgressBar(65, ['canvas' => $canvas, 'frame' => $frame]);
$response = toResponse(place($bar, [
    'id' => 'progress', 'frame' => $frame, 'path' => 'data.progress'
]));
echo json_encode($response, JSON_THROW_ON_ERROR);
```

Functions and methods use camelCase and the TypeScript argument order. Protocol and option keys retain
camelCase (`fontSizeRatio`, `dataPath`). Pass associative arrays or `stdClass` for JSON objects and indexed
arrays for lists. Use `new stdClass()` for an empty JSON object; `[]` represents an empty list in response
data. A PHP array is an object only if it is not a list: `[]` is always a JSON array (so `merge([])` and `['data' => []]` are rejected; use `new stdClass()`), and an object whose keys are all sequential integer-like strings (`'0'`, `'1'`, ...) must be a `stdClass` because PHP turns those keys into a list. Omitted options may use `[]`. Returned values use those same native PHP structures.

`new Matrix(columns, rows, options = [])` provides drawing primitives, text, flood fill, blitting and
transforms. Drawing mutates the matrix and returns it; `flipX`, `flipY`, `rotate90`, `crop`, and `clone`
return independent matrices. `toRoot()` exports a root matrix; `toPart()` exports a matrix layer payload.
`Box::canvas(canvas)` provides `sub`, `inset`, `insetPt`, `rows`, `columns`, `toPt`, and `toFrame`, with
static `fromFrame` and `fromPt` constructors. `PixelFont`, `defineFont`, `getFont`, `measureText`, and
`layoutText` handle built-in and custom Unicode bitmap fonts.

The `GlanceWidgets` namespace exposes colors, themes (`THEMES`, `getTheme`, `themeOptions`), icons,
pixelation, geometry, all layer builders, `merge`, `place`, `toResponse`, `validate`, `resolveDataPath`,
and HMAC signing/verification. `validate` returns normally or throws `GlanceValidationError` with a
`path` property. Component functions are `barChart`, `progressBar`, `ringProgress`, `battery`,
`signal` (modern, layer-built), `pixelBarChart`, `pixelProgressBar`, `labeledPixelProgressBar`,
`pixelRingProgress`, `labeledPixelRingProgress` (matrix-cell), `statusStrip`, `labeledStatusStrip`, `heatmap`, `calendarHeatmap`,
`labeledCalendarHeatmap`, `lineChart`, `areaChart`, `sparkline`, `stat`, `pixelTextPanel`, and `pixelTextRoot`.

Run `bash sdks/php/test.sh` from the root of the full source snapshot. Tests include
the shared golden cases, native API assertions, exact binary-double decimal formatting, strict
validation, Unicode, and loading only package sources from a temporary directory. A `throws` fixture
must raise an argument/type/validation exception; missing functions and programming errors fail the test.
Numeric comparison uses relative tolerance 1e-9.

Fonts come from `spec/fonts/*.json`; the 5,287-icon catalog comes from `spec/icons.json`. SF Pro advances
originate in `spec/tools/measure-text-advances.py` and the TypeScript text tables. Validation schemas
come from TypeScript's protocol validator. These pure JSON data files are bundled under `src/data`, so
the installed package reads its bundled data. In the development repository, after installing the TypeScript development
dependencies, run `node sdks/php/scripts/sync-data.cjs` to refresh the data. PHP's SDK algorithms are
maintained directly in their source modules; the data script emits no algorithms or PHP code.

Use `layerPath(id)` for the derived node path, and `validateLayerNode(node, type)` to validate partial editor nodes.
