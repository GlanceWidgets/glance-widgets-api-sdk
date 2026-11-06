# Go SDK

Layer constructors return a `Composite`: `{layers, data}`. Declarations contain only id, type and dataPath; the addressed objects contain layout, content and style. Merge layer results and chart components with the same merge function, then finalize with toResponse (to_response in snake_case APIs). An explicit dataPath chooses the object location; otherwise it is derived from id.

`Matrix.ToPart()` returns `{content: MatrixData}`. Place it with `place` / `Place` to give its node a frame and data path. The node's `content` holds `pixels`, `default`, `batch` and `layout`; matrix `style` is empty. Image and image-backed icon nodes use `content.url`. Bind an editor layer to a partial node in `data` without adding a remote declaration. Omitted or null values keep the saved editor value; objects merge field by field, arrays replace, and a supplied frame replaces the whole frame and must include width and height. Each response starts from the saved configuration.

Native Go 1.21+, standard library only. Fonts, all 5,287 icons, measured text
advances and validation schemas are embedded in the compiled module.

```go
package main

import (
    "encoding/json"
    "log"
    sdk "github.com/GlanceWidgets/glance-widgets-api-sdk/sdks/go"
)

func main() {
    canvas := sdk.Canvas{Width: 338, Height: 158}
    chart, err := sdk.BarChart([]float64{3, 5, -2, 8}, sdk.Object{"canvas": canvas})
    if err != nil { log.Fatal(err) }
    response, err := sdk.ToResponse(chart)
    if err != nil { log.Fatal(err) }
    body, err := json.Marshal(response)
    if err != nil { log.Fatal(err) }
    log.Print(string(body))
}
```

Functions use exported Go names (`ProgressBar`, `PixelRingProgress`,
`ToResponse`); operations that can fail return `(value, error)`.
`Validate` returns an error, including `*GlanceValidationError` with `Path`.
`Object` is `map[string]any`; option and protocol keys keep their JSON spelling
(`fontSizeRatio`, `dataPath`, `onConflict`). `Canvas` and `Rect` are native structs.
`NewMatrix(columns, rows, options...)` provides every drawing and transformation
operation; `ToRoot` and `ToPart` export JSON objects. `CanvasBox(canvas)` creates
the whole layout area; `BoxFromPt` and `BoxFromFrame` construct subareas.

Components include modern progress/ring/bar-chart/battery/signal (layer-built;
`canvas` required) and their matrix-cell `Pixel*` / `LabeledPixel*` variants
(progress, ring, bar chart), status strips and legends, heatmap/calendar,
line/area/sparkline charts, stat cards
and pixel text. The remaining public API covers colors, themes, font registration,
icons, decoded RGBA pixelation, geometry, layer builders, merge and HMAC signatures.
Line series use `[]any{[]float64{1, 3, 2}, sdk.Object{"points": ...}}`;
nullable status/heat values use `[]any` with `nil` for missing values.

From the root of the full source snapshot, run:

```sh
bash sdks/go/test.sh        # shared golden cases plus native API checks
cd sdks/go && go test -race ./...
```

The test script also executes the compiled embedded-data check from `/tmp`.
`go generate ./...` in the development repository refreshes data from the canonical spec and generated TS schema/
text tables. It requires Python at development time; the Go SDK has no Python or
Node dependency at runtime. After changing TS schemas/text widths, first run
`python3 sdks/python/scripts/generate_data.py` at the repository root.

Font aliases that refer directly to defined glyphs can use a normal `Object` map.
For alias chains, use `[]sdk.FontAlias{{Name: "B", Target: "X"}, {Name: "A", Target: "B"}}`;
aliases are compiled in slice order, and targets must already exist. Indirect chains
in unordered maps are rejected with a request to supply the ordered form.

`LayerPath(id)` returns the derived node path. `ValidateLayerNode(node, type, options...)` validates partial editor nodes; options accept `path` and `requiresFrame`.
