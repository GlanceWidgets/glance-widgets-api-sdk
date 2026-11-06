<?php
declare(strict_types=1);
namespace GlanceWidgets;

const PIXEL_BAR_CHART_DEFAULTS = ['id' => 'pixelBarChart', 'rows' => 10, 'gap' => .16, 'barWidth' => 1,
    'barSpacing' => 1, 'cornerRadius' => .15, 'fontSizeRatio' => .06, 'axisWidth' => 1, 'zIndex' => 0,
    'colors' => ['positive' => '#5EA2FF', 'negative' => '#FF6B6B', 'track' => null,
        'axis' => '#FFFFFF', 'label' => '#FFFFFF'], 'axisOpacity' => .5, 'labelOpacity' => .72];

function pixelBarChart(array $values, array|\stdClass $options): array
{
    $options = objectFields($options);
    if (!$values) {
        throw new \InvalidArgumentException('pixelBarChart needs finite values');
    }
    foreach ($values as $value) {
        finiteNumber($value, 'Bar value');
    }
    $canvas = requireCanvas($options['canvas'] ?? null, 'pixelBarChart');
    [$id, $path] = resolveIdentity($options, 'pixelBarChart', 'pixelBarChart');
    $barWidth = $options['barWidth'] ?? 1;
    $spacing = $options['barSpacing'] ?? 1;
    $z = $options['zIndex'] ?? 0;
    $colors = array_replace(PIXEL_BAR_CHART_DEFAULTS['colors'], objectFields($options['colors'] ?? []));
    $categories = $options['categories'] ?? [];
    $showCategories = $options['showCategoryLabels'] ?? count($categories) > 0;
    $showValues = $options['showValueLabels'] ?? true;
    $showAxes = $options['showAxes'] ?? true;
    if ($showCategories && count($categories) !== count($values)) {
        throw new \InvalidArgumentException('Categories need one entry per value');
    }
    $low = $options['min'] ?? min(array_merge([0], $values));
    $high = $options['max'] ?? max(array_merge([0], $values));
    if (!isset($options['min']) && !isset($options['max']) && $low == $high) {
        $high = $low + 1;
    }
    requireRange('pixelBarChart', $low, $high);
    $zero = clamp(0, $low, $high);
    $decimals = $options['decimals'] ?? defaultDecimals($low, $high);
    $ratio = $options['fontSizeRatio'] ?? .06;
    $points = fontPtOf($ratio, $canvas);
    $lineHeight = $points * 1.2;
    $gapPt = $points * .4;
    $axisWidth = $options['axisWidth'] ?? 1;
    $area = resolveArea($options['frame'] ?? null, $canvas);
    $areaPt = $area->toPt();
    $rows = $options['rows'] ?? 10;
    $gap = $options['gap'] ?? .16;
    $halfGap = $showAxes || $showValues || $showCategories ? $gap * $areaPt['height'] / $rows / 2 : 0;
    $ticks = [];
    foreach ([$high, $zero, $low] as $value) {
        if (!in_array($value, array_column($ticks, 0))) {
            $ticks[] = [$value, Numbers::label($value, $decimals)];
        }
    }
    $labelWidth = max(array_map(static fn($tick) => estimateTextWidthPt($tick[1], $points), $ticks));
    $halfLabel = $showValues ? $lineHeight * 1.5 / 2 : 0;
    $categoryRow = $showCategories ? $gapPt + textBoxHeightPt($points) : 0;
    $plotBox = $area->insetPt([
        'left' => ($showValues ? $labelWidth + $gapPt : 0) + ($showAxes || $showValues ? $axisWidth + $halfGap : 0),
        'top' => $halfLabel + $halfGap, 'bottom' => max($halfLabel, $categoryRow) + $halfGap]);
    $frame = $plotBox->toFrame();
    $region = resolveFrame($frame, $canvas);
    $columns = count($values) * $barWidth + (count($values) - 1) * $spacing;
    $grid = solveGrid(compact('region', 'columns', 'rows', 'gap'));
    $geometry = fitMatrix($region, $grid, $grid['layout']);
    $baseline = jsRound($grid['rows'] * ($high - $zero) / ($high - $low));
    $matrix = new Matrix($columns, $grid['rows'], ['default' => $colors['track'] === null
        ? EMPTY_PIXEL : normalizeColor($colors['track']), 'base' => ['opacity' => 1,
            'cornerRadius' => $options['cornerRadius'] ?? .15], 'layout' => $grid['layout']]);
    foreach ($values as $index => $value) {
        $x = $index * ($barWidth + $spacing);
        $value = clamp($value, $low, $high);
        if ($value > $zero) {
            $cells = jsRound(($value - $zero) / ($high - $zero) * $baseline);
            $matrix->rect($x, $baseline - $cells, $barWidth, $cells, $colors['positive']);
        } elseif ($value < $zero) {
            $cells = jsRound(($zero - $value) / ($zero - $low) * ($grid['rows'] - $baseline));
            $matrix->rect($x, $baseline, $barWidth, $cells, $colors['negative']);
        }
    }
    $plot = place($matrix->toPart(), ['id' => $id . ':plot', 'frame' => $frame, 'path' => $path, 'zIndex' => $z]);
    $layers = [$plot];
    $axisX = min(columnBoundary($geometry, 0), $geometry['originX'] - $axisWidth / 2);
    if ($showAxes) {
        $style = ['color' => $colors['axis'], 'strokeWidth' => $axisWidth, 'opacity' => .5];
        $layers[] = verticalLine(['id' => $id . ':axis-value', 'at' => $axisX / $canvas['width'],
            'from' => rowBoundary($geometry, 0) / $canvas['height'],
            'to' => rowBoundary($geometry, $grid['rows']) / $canvas['height'], 'zIndex' => $z + 1, 'style' => $style]);
        $layers[] = horizontalLine(['id' => $id . ':axis-base',
            'at' => rowBoundary($geometry, $baseline) / $canvas['height'],
            'from' => ($axisX - $axisWidth / 2) / $canvas['width'],
            'to' => columnBoundary($geometry, $grid['columns']) / $canvas['width'],
            'zIndex' => $z + 1, 'style' => $style]);
    }
    $add = static function ($role, $rect, $content, $horizontal, $vertical)
        use (&$layers, $id, $canvas, $ratio, $colors, $z): void {
        $layers[] = labelLayer(['id' => $id . ':' . $role, 'box' => Box::fromPt($rect, $canvas),
            'content' => $content, 'horizontal' => $horizontal, 'vertical' => $vertical,
            'fontSizeRatio' => $ratio, 'color' => $colors['label'], 'opacity' => .72, 'zIndex' => $z + 1]);
    };
    if ($showValues) {
        $height = $lineHeight * 1.5;
        $right = $axisX - $axisWidth / 2 - $gapPt;
        foreach ($ticks as $index => [$value, $content]) {
            $boundary = $value == $high ? 0 : ($value == $low ? $grid['rows'] : $baseline);
            $center = rowBoundary($geometry, $boundary);
            $add('value-' . $index, ['x' => $areaPt['x'], 'y' => $center - $height / 2,
                'width' => max(0, $right - $areaPt['x']), 'height' => $height], $content, 'right', 'center');
        }
    }
    if ($showCategories) {
        $pitch = ($barWidth + $spacing) * ($geometry['cellWidth'] + $geometry['gapX']);
        foreach ($categories as $index => $category) {
            $start = $index * ($barWidth + $spacing);
            $center = (columnBoundary($geometry, $start) + columnBoundary($geometry, $start + $barWidth)) / 2;
            $width = min($pitch, 2 * ($center - $areaPt['x']), 2 * ($areaPt['x'] + $areaPt['width'] - $center));
            $add('category-' . $index, ['x' => $center - $width / 2,
                'y' => rowBoundary($geometry, $grid['rows']) + $gapPt,
                'width' => $width, 'height' => textBoxHeightPt($points)], $category, 'center', 'top');
        }
    }
    return \GlanceWidgets\Internal\combine($layers);
}
