<?php
declare(strict_types=1);
namespace GlanceWidgets;

const GAP_MODE_RATIOS = ['zero' => 0, 'near' => .08, 'default' => .16, 'far' => .28, 'extraFar' => .42];
const FRAME_DECIMALS = 6;

function requireCanvas(array|\stdClass|null $canvas, string $operation = 'Operation'): array
{
    $canvas = $canvas === null ? [] : objectFields($canvas);
    if (!(($canvas['width'] ?? 0) > 0) || !(($canvas['height'] ?? 0) > 0)) {
        throw new \InvalidArgumentException($operation . ' needs a positive canvas size in points');
    }
    return $canvas;
}

function resolveFrame(array|\stdClass $frame, array|\stdClass $canvas): array
{
    $frame = objectFields($frame);
    $canvas = requireCanvas($canvas, 'resolveFrame');
    $axis = static function ($start, $end, $size, $extent, $default): array {
        $first = $start === null ? null : $start * $extent;
        $last = $end === null ? null : $end * $extent;
        $length = $first !== null && $last !== null
            ? max(0, $extent - $first - $last) : clamp($size ?? $default, .02, 1.5) * $extent;
        return [$first ?? ($last === null ? 0 : $extent - $last - $length), $length];
    };
    [$x, $width] = $axis($frame['left'] ?? null, $frame['right'] ?? null,
        $frame['width'] ?? null, $canvas['width'], .4);
    [$y, $height] = $axis($frame['top'] ?? null, $frame['bottom'] ?? null,
        $frame['height'] ?? null, $canvas['height'], .16);
    return compact('x', 'y', 'width', 'height');
}

function fitMatrix(array|\stdClass $region, array|\stdClass $grid, array|\stdClass $layout): array
{
    $region = objectFields($region);
    $grid = objectFields($grid);
    $layout = objectFields($layout);
    $columns = $grid['columns'];
    $rows = $grid['rows'];
    $aspect = clamp($layout['cellAspectRatio'] ?? 1, .1, 10);
    $scale = min($aspect, 1);
    $gap = GAP_MODE_RATIOS[$layout['gapMode'] ?? 'default'] ?? .16;
    $gapX = clamp($layout['columnGapRatio'] ?? $gap, 0, 10) * $scale;
    $gapY = clamp($layout['rowGapRatio'] ?? $gap, 0, 10) * $scale;
    $inset = clamp($layout['edgeInsetRatio'] ?? 0, -.45, .45) * $scale;
    $cellHeight = min($region['width'] / ($columns * $aspect + ($columns - 1) * $gapX + 2 * $inset),
        $region['height'] / ($rows + ($rows - 1) * $gapY + 2 * $inset));
    $cellWidth = $cellHeight * $aspect;
    $gapX *= $cellHeight;
    $gapY *= $cellHeight;
    $inset *= $cellHeight;
    $originX = $region['x'] + ($region['width'] - $columns * $cellWidth - ($columns - 1) * $gapX) / 2;
    $originY = $region['y'] + ($region['height'] - $rows * $cellHeight - ($rows - 1) * $gapY) / 2;
    return compact('columns', 'rows', 'cellWidth', 'cellHeight', 'gapX', 'gapY', 'originX', 'originY');
}

function cellRect(array $geometry, float $x, float $y): array
{
    return ['x' => $geometry['originX'] + $x * ($geometry['cellWidth'] + $geometry['gapX']),
        'y' => $geometry['originY'] + $y * ($geometry['cellHeight'] + $geometry['gapY']),
        'width' => $geometry['cellWidth'], 'height' => $geometry['cellHeight']];
}

function columnBoundary(array $geometry, float $index): float
{
    return $geometry['originX'] + $index * ($geometry['cellWidth'] + $geometry['gapX']) - $geometry['gapX'] / 2;
}

function rowBoundary(array $geometry, float $index): float
{
    return $geometry['originY'] + $index * ($geometry['cellHeight'] + $geometry['gapY']) - $geometry['gapY'] / 2;
}

function toRatioRect(array|\stdClass $rect, array|\stdClass $canvas): array
{
    $rect = objectFields($rect);
    $canvas = requireCanvas($canvas);
    return ['x' => $rect['x'] / $canvas['width'], 'y' => $rect['y'] / $canvas['height'],
        'width' => $rect['width'] / $canvas['width'], 'height' => $rect['height'] / $canvas['height']];
}

function solveGrid(array|\stdClass $options): array
{
    $options = objectFields($options);
    $region = objectFields($options['region']);
    if (!($region['width'] > 0) || !($region['height'] > 0)) {
        throw new \InvalidArgumentException('solveGrid requires a positive region');
    }
    $mode = $options['gapMode'] ?? 'default';
    if (!is_string($mode) || !array_key_exists($mode, GAP_MODE_RATIOS)) {
        throw new \InvalidArgumentException('solveGrid: unknown gapMode ' . json_encode($mode)
            . '; expected one of ' . implode(', ', array_keys(GAP_MODE_RATIOS)));
    }
    $gap = $options['gap'] ?? GAP_MODE_RATIOS[$mode];
    $inset = $options['edgeInsetRatio'] ?? 0;
    if ($gap < 0 || $gap > 10 || abs($inset) > .45) {
        throw new \InvalidArgumentException('Invalid grid gap/inset');
    }
    $count = static fn($extent, $cell) => max(1, jsRound(($extent / $cell + $gap - 2 * $inset) / (1 + $gap)));
    $square = static fn($extent, $count) => $extent / ($count + ($count - 1) * $gap + 2 * $inset);
    $rows = $options['rows'] ?? null;
    $columns = $options['columns'] ?? null;
    if ($rows === null && $columns === null) {
        if (!(($options['cellSize'] ?? 0) > 0)) {
            throw new \InvalidArgumentException('Give rows, columns or a positive cellSize');
        }
        $rows = $count($region['height'], $options['cellSize']);
    }
    $rows ??= $count($region['height'], $square($region['width'], $columns));
    $columns ??= $count($region['width'], $square($region['height'], $rows));
    $columns = positiveInteger($columns, 'columns');
    $rows = positiveInteger($rows, 'rows');
    if ($columns * $rows > MAX_MATRIX_CELLS) {
        throw new \InvalidArgumentException('Grid exceeds 4096 cells');
    }
    $ratio = $region['width'] / $region['height'];
    $extraX = ($columns - 1) * $gap + 2 * $inset;
    $extraY = ($rows - 1) * $gap + 2 * $inset;
    $aspect = ($ratio * ($rows + $extraY) - $extraX) / $columns;
    if ($aspect < 1) {
        $denominator = $columns + $extraX - $ratio * $extraY;
        $aspect = $denominator > 0 ? $ratio * $rows / $denominator : null;
        if ($aspect !== null && ($aspect <= 0 || $aspect > 1)) {
            $aspect = null;
        }
    }
    if (($options['clampAspect'] ?? false) === true) {
        $aspect = clamp($aspect ?? .1, .1, 10);
    }
    if ($aspect === null || $aspect < .1 || $aspect > 10) {
        throw new \InvalidArgumentException('Grid cannot fill region within cell aspect limits');
    }
    $layout = ['gapMode' => $mode, 'edgeInsetRatio' => $inset, 'cellAspectRatio' => $aspect];
    if (($options['gap'] ?? null) !== null) {
        $layout += ['columnGapRatio' => $gap, 'rowGapRatio' => $gap];
    }
    return compact('columns', 'rows', 'layout');
}
