<?php
declare(strict_types=1);
namespace GlanceWidgets;

const LINE_CHART_DEFAULTS = ['id' => 'lineChart', 'interpolation' => 'monotone', 'strokeWidth' => 2,
    'fillOpacity' => 0, 'areaFillOpacity' => .3, 'markerRadius' => 3, 'fontSizeRatio' => .06,
    'axisWidth' => 1, 'gridCellSize' => 8, 'zIndex' => 0,
    'seriesColors' => ['#6BA7FF', '#FF9F0A', '#34C759', '#FF453A', '#BF5AF2', '#5AC8FA'],
    'axisColor' => '#FFFFFF', 'labelColor' => '#FFFFFF', 'axisOpacity' => .5, 'labelOpacity' => .72];

function prepareSeries(array|\stdClass $entry): array
{
    $specification = is_array($entry) && array_is_list($entry) ? ['values' => $entry] : objectFields($entry);
    if (array_key_exists('values', $specification) === array_key_exists('points', $specification)) {
        throw new \InvalidArgumentException('Series needs exactly one of values or points');
    }
    $points = $specification['points'] ?? [];
    if (array_key_exists('values', $specification)) {
        foreach ($specification['values'] as $index => $value) {
            $points[] = [$index, $value];
        }
    }
    if (count($points) < 2 || count($points) > 1024) {
        throw new \InvalidArgumentException('Series needs 2..1024 points');
    }
    foreach ($points as $point) {
        if (!is_array($point) || count($point) !== 2) {
            throw new \InvalidArgumentException('Series points must be pairs');
        }
        finiteNumber($point[0], 'Point x');
        finiteNumber($point[1], 'Point y');
    }
    usort($points, static fn($left, $right) => $left[0] <=> $right[0]);
    for ($index = 1; $index < count($points); $index++) {
        if ($points[$index - 1][0] == $points[$index][0]) {
            throw new \InvalidArgumentException('Series cannot repeat x coordinates');
        }
    }
    return $points;
}

function chartMarkers(array $points, string $color, array $options, int|float $decimals,
    float $radius, string $labelColor): array
{
    $ys = array_column($points, 1);
    $wanted = [];
    if ($options['last'] ?? false) {
        $wanted[] = [count($points) - 1, 'last'];
    }
    if ($options['max'] ?? false) {
        $wanted[] = [array_search(max($ys), $ys), 'max'];
    }
    if ($options['min'] ?? false) {
        $wanted[] = [array_search(min($ys), $ys), 'min'];
    }
    $result = $seen = [];
    foreach ($wanted as [$index, $kind]) {
        if (isset($seen[$index])) {
            continue;
        }
        $seen[$index] = true;
        [$x, $y] = $points[$index];
        $marker = ['at' => ['x' => $x], 'symbol' => ['radiusPt' => $radius, 'fill' => $color]];
        if ($options['labels'] ?? false) {
            $below = $kind === 'min';
            $marker['label'] = Numbers::label($y, $decimals);
            $marker['labelStyle'] = ['color' => $labelColor, 'fontSizePt' => 10,
                'position' => $below ? 'bottom' : 'top'];
            $marker['at'] = ['x' => $x, 'offsetPt' => [0, $below ? $radius + 2 : -($radius + 2)]];
        }
        $result[] = $marker;
    }
    return $result;
}

function planChartPlot(array $canvas, array $options, array $ticks, float $padding): array
{
    $ratio = $options['fontSizeRatio'] ?? .06;
    $points = fontPtOf($ratio, $canvas);
    $metrics = ['ratio' => $ratio, 'fontPt' => $points, 'lineHeightPt' => $points * 1.2, 'gapPt' => $points * .4];
    $axisWidth = $options['axisWidth'] ?? 1;
    $axisPt = $options['showAxes'] ? $axisWidth : 0;
    $labelWidth = max(array_map(static fn($tick) => estimateTextWidthPt($tick, $points), $ticks));
    $halfLabel = $options['showValueLabels'] ? $metrics['lineHeightPt'] * 1.5 / 2 : 0;
    $xRow = !empty($options['xLabels']) ? $metrics['gapPt'] + textBoxHeightPt($points) : 0;
    $need = ['left' => ($options['showValueLabels'] ? $labelWidth + $metrics['gapPt'] : 0) + $axisPt,
        'top' => $halfLabel, 'right' => 0, 'bottom' => max($halfLabel, $axisPt + $xRow)];
    $area = resolveArea($options['frame'] ?? null, $canvas);
    $insets = array_map(static fn($reserve) => max($reserve, $padding) - $padding, $need);
    $region = emittedRegion($area->insetPt($insets), $canvas);
    $plot = ['x' => $region['rect']['x'] + $padding, 'y' => $region['rect']['y'] + $padding,
        'width' => $region['rect']['width'] - 2 * $padding, 'height' => $region['rect']['height'] - 2 * $padding];
    if ($plot['width'] <= 0 || $plot['height'] <= 0) {
        throw new \InvalidArgumentException('Chart area too small for labels');
    }
    return ['canvas' => $canvas, 'area' => $area->toPt(), 'curveFrame' => $region['frame'],
        'plot' => $plot, 'text' => $metrics, 'axisX' => $plot['x'] - $axisWidth / 2];
}

function chartGrid(string $id, string $path, float $z, array $grid, array $layout): array
{
    $theme = resolveTheme($grid['theme'] ?? null);
    $region = emittedRegion(Box::fromPt($layout['plot'], $layout['canvas']), $layout['canvas']);
    $solved = solveGrid(['region' => $region['rect'], 'cellSize' => $grid['cellSize'] ?? 8,
        'gapMode' => $theme['layout']['gapMode']]);
    $matrix = new Matrix($solved['columns'], $solved['rows'],
        themeOptions($theme, ['default' => $theme['default'], 'layout' => $solved['layout']]));
    return place($matrix->toPart(), ['id' => $id . ':grid', 'frame' => $region['frame'],
        'path' => $path, 'zIndex' => $z]) + ['plot' => $region['rect']];
}

function chartOverlays(string $id, float $z, array $options, array $layout, array $ticks): array
{
    $canvas = $layout['canvas'];
    $area = $layout['area'];
    $plot = $layout['plot'];
    $metrics = $layout['text'];
    $axisX = $layout['axisX'];
    $axisWidth = $options['axisWidth'] ?? 1;
    $bottom = $plot['y'] + $plot['height'];
    $colors = objectFields($options['colors'] ?? []);
    $layers = [];
    if ($options['showAxes']) {
        $style = ['color' => $colors['axis'] ?? '#FFFFFF', 'strokeWidth' => $axisWidth, 'opacity' => .5];
        $layers[] = verticalLine(['id' => $id . ':axis-value', 'at' => $axisX / $canvas['width'],
            'from' => $plot['y'] / $canvas['height'], 'to' => $bottom / $canvas['height'],
            'zIndex' => $z, 'style' => $style]);
        $layers[] = horizontalLine(['id' => $id . ':axis-base',
            'at' => ($bottom + $axisWidth / 2) / $canvas['height'],
            'from' => ($axisX - $axisWidth / 2) / $canvas['width'],
            'to' => ($plot['x'] + $plot['width']) / $canvas['width'], 'zIndex' => $z, 'style' => $style]);
    }
    $add = static function ($role, $rect, $content, $horizontal, $vertical)
        use (&$layers, $id, $canvas, $metrics, $colors, $z): void {
        $layers[] = labelLayer(['id' => $id . ':' . $role, 'box' => Box::fromPt($rect, $canvas),
            'content' => $content, 'horizontal' => $horizontal, 'vertical' => $vertical,
            'fontSizeRatio' => $metrics['ratio'], 'color' => $colors['label'] ?? '#FFFFFF',
            'opacity' => .72, 'zIndex' => $z]);
    };
    if ($options['showValueLabels']) {
        $height = $metrics['lineHeightPt'] * 1.5;
        $right = $axisX - $axisWidth / 2 - $metrics['gapPt'];
        foreach ($ticks as $index => $content) {
            $center = $index === 0 ? $plot['y'] : $bottom;
            $add('value-' . $index, ['x' => $area['x'], 'y' => $center - $height / 2,
                'width' => max(0, $right - $area['x']), 'height' => $height], $content, 'right', 'center');
        }
    }
    $labels = $options['xLabels'] ?? [];
    $axisPt = $options['showAxes'] ? $axisWidth : 0;
    foreach ($labels as $index => $content) {
        $fraction = count($labels) === 1 ? .5 : $index / (count($labels) - 1);
        $width = estimateTextWidthPt($content, $metrics['fontPt']);
        $left = min($area['x'] + $area['width'] - $width,
            max($area['x'], $plot['x'] + $fraction * $plot['width'] - $width / 2));
        $add('x-' . $index, ['x' => $left, 'y' => $bottom + $axisPt + $metrics['gapPt'],
            'width' => $width, 'height' => textBoxHeightPt($metrics['fontPt'])], $content, 'center', 'top');
    }
    return $layers;
}

function buildChart(string $operation, array $inputSeries, array $options): array
{
    if (!$inputSeries) {
        throw new \InvalidArgumentException('Chart needs at least one series');
    }
    [$id, $path] = resolveIdentity($options, 'lineChart', $operation);
    $z = $options['zIndex'] ?? 0;
    $stroke = $options['strokeWidth'] ?? 2;
    $markers = objectFields($options['markers'] ?? []);
    $radius = $markers['radius'] ?? 3;
    $hasMarkers = ($markers['last'] ?? false) || ($markers['min'] ?? false) || ($markers['max'] ?? false);
    $colors = objectFields($options['colors'] ?? []);
    $palette = $colors['series'] ?? LINE_CHART_DEFAULTS['seriesColors'];
    $series = $every = [];
    foreach ($inputSeries as $index => $entry) {
        $entryOptions = isObject($entry) ? objectFields($entry) : [];
        $points = prepareSeries($entry);
        $series[] = ['points' => $points,
            'color' => normalizeColor($entryOptions['color'] ?? $palette[$index % count($palette)])];
        $every = array_merge($every, $points);
    }
    $domain = [min(array_column($every, 0)), max(array_column($every, 0))];
    $low = $options['min'] ?? min(array_column($every, 1));
    $high = $options['max'] ?? max(array_column($every, 1));
    if ($low == $high && !isset($options['min']) && !isset($options['max'])) {
        $pad = abs($low) < .001 ? 1 : abs($low) * .1;
        $low -= $pad;
        $high += $pad;
    }
    requireRange($operation, $low, $high);
    $decimals = $options['decimals'] ?? defaultDecimals($low, $high);
    $reach = 0;
    foreach ($series as &$entry) {
        $entry['points'] = array_map(static fn($point) => [$point[0], clamp($point[1], $low, $high)], $entry['points']);
        $entry['markers'] = chartMarkers($entry['points'], $entry['color'], $markers, $decimals,
            $radius, $colors['label'] ?? '#FFFFFF');
        foreach ($entry['markers'] as $marker) {
            if (isset($marker['label'])) {
                $reach = max($reach, $radius + 2 + textBoxHeightPt(10), estimateTextWidthPt($marker['label'], 10) / 2);
            }
        }
    }
    unset($entry);
    $padding = min(48, max($stroke / 2, $hasMarkers ? $radius : 0, $reach));
    $grid = $options['grid'] ?? null;
    $grid = $grid === true ? [] : ($grid === false ? null : ($grid === null ? null : objectFields($grid)));
    $ticks = [Numbers::label($high, $decimals), Numbers::label($low, $decimals)];
    $needsCanvas = $options['showAxes'] || $options['showValueLabels'] || !empty($options['xLabels']) || $grid !== null;
    $layout = $needsCanvas ? planChartPlot(requireCanvas($options['canvas'] ?? null, $operation),
        $options, $ticks, $padding) : null;
    $area = $options['frame'] ?? null;
    $frame = $layout['curveFrame'] ?? ($area instanceof Box ? $area->toFrame()
        : ($area ?? ['left' => 0, 'top' => 0, 'width' => 1, 'height' => 1]));
    $layers = [];
    if ($layout !== null && $grid !== null) {
        $background = chartGrid($id, $path, $z, $grid, $layout);
        $layers[] = $background;
        $layout['plot'] = $background['plot'];
    }
    foreach ($series as $index => $entry) {
        $layers[] = curve(['id' => $id . ':series-' . $index, 'frame' => $frame, 'zIndex' => $z + 1,
            'data' => ['mode' => 'samples', 'points' => $entry['points'], 'domain' => $domain,
                'yDomain' => [$low, $high], 'interpolation' => $options['interpolation'] ?? 'monotone',
                'markers' => $entry['markers']], 'style' => ['color' => $entry['color'], 'fillColor' => $entry['color'],
                'fillOpacity' => $options['fillOpacity'] ?? 0, 'opacity' => 1,
                'strokeWidth' => $stroke, 'plotPadding' => $padding]]);
    }
    if ($layout !== null) {
        $layers = array_merge($layers, chartOverlays($id, $z + 2, $options, $layout, $ticks));
    }
    return \GlanceWidgets\Internal\combine($layers);
}

function lineChart(array $series, array|\stdClass $options = []): array
{
    return buildChart('lineChart', $series, withDefaults(objectFields($options), ['showAxes' => true, 'showValueLabels' => true]));
}

function areaChart(array $series, array|\stdClass $options = []): array
{
    return buildChart('areaChart', $series,
        withDefaults(objectFields($options), ['fillOpacity' => .3, 'showAxes' => true, 'showValueLabels' => true]));
}

function sparkline(array $values, array|\stdClass $options = []): array
{
    $options = objectFields($options);
    $rest = array_diff_key($options, array_flip(['color', 'fill', 'fillOpacity']));
    $series = ['values' => $values];
    if (isset($options['color'])) {
        $series['color'] = $options['color'];
    }
    return buildChart('sparkline', [$series], array_replace($rest, ['id' => $options['id'] ?? 'sparkline',
        'markers' => $options['markers'] ?? ['last' => true],
        'fillOpacity' => ($options['fill'] ?? false) === true ? ($options['fillOpacity'] ?? .3) : 0,
        'showAxes' => false, 'showValueLabels' => false]));
}
