<?php
declare(strict_types=1);
namespace GlanceWidgets;

const PIXEL_PROGRESS_BAR_DEFAULTS = ['min' => 0, 'max' => 100, 'orientation' => 'horizontal', 'segments' => 10,
    'segmentSize' => 3, 'segmentGap' => 1, 'thickness' => 2];
const LABELED_PIXEL_PROGRESS_BAR_DEFAULTS = ['id' => 'progress', 'labelPosition' => 'end', 'fontSizeRatio' => .07,
    'labelOpacity' => 1, 'zIndex' => 0];
const PIXEL_RING_PROGRESS_DEFAULTS = ['min' => 0, 'max' => 100, 'diameter' => 33, 'thickness' => 4, 'startAngle' => 0];
const LABELED_PIXEL_RING_PROGRESS_DEFAULTS = ['id' => 'ring', 'labelOpacity' => 1, 'zIndex' => 0];

function progressMatrix(float $value, array $options): Matrix
{
    $minimum = $options['min'] ?? 0;
    $maximum = $options['max'] ?? 100;
    requireRange('pixelProgressBar', $minimum, $maximum);
    $segments = positiveInteger($options['segments'] ?? 10, 'segments');
    $size = positiveInteger($options['segmentSize'] ?? 3, 'segmentSize');
    $gap = integerValue($options['segmentGap'] ?? 1, 'segmentGap');
    if ($gap < 0) {
        throw new \InvalidArgumentException('segmentGap must not be negative');
    }
    if (isset($options['thickness'])) {
        positiveInteger($options['thickness'], 'thickness');
    }
    $vertical = ($options['orientation'] ?? 'horizontal') === 'vertical';
    $reverse = $options['reverse'] ?? false;
    $theme = resolveTheme($options['theme'] ?? null);
    $region = regionOf($options, 'pixelProgressBar');
    $length = $segments * $size + ($segments - 1) * $gap;
    $across = $options['thickness'] ?? ($region === null ? 2 : null);
    $gridSize = $vertical ? ['rows' => $length, 'columns' => $across] : ['columns' => $length, 'rows' => $across];
    $plan = planGrid($region, $gridSize, $theme, 'pixelProgressBar');
    $thickness = $plan[$vertical ? 'columns' : 'rows'];
    $looks = pixelLooks($theme, $options['colors'] ?? []);
    $matrix = new Matrix($plan['columns'], $plan['rows'],
        themeOptions($theme, ['default' => EMPTY_PIXEL, 'layout' => $plan['layout']]));
    $lit = jsRound(fractionOf($value, $minimum, $maximum) * $segments);
    for ($index = 0; $index < $segments; $index++) {
        $slot = $vertical !== $reverse ? $segments - 1 - $index : $index;
        $offset = $slot * ($size + $gap);
        $style = $looks[$index < $lit ? 'on' : 'off'];
        if ($vertical) {
            $matrix->rect(0, $offset, $thickness, $size, $style);
        } else {
            $matrix->rect($offset, 0, $size, $thickness, $style);
        }
    }
    return $matrix;
}

function pixelProgressBar(float $value, array|\stdClass $options = []): array
{
    return progressMatrix($value, objectFields($options))->toPart();
}

function labeledPixelProgressBar(float $value, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'labeledPixelProgressBar');
    [$id, $path] = resolveIdentity($options, 'progress', 'labeledPixelProgressBar');
    $minimum = $options['min'] ?? 0;
    $maximum = $options['max'] ?? 100;
    requireRange('labeledPixelProgressBar', $minimum, $maximum);
    $theme = resolveTheme($options['theme'] ?? null);
    $vertical = ($options['orientation'] ?? 'horizontal') === 'vertical';
    $end = ($options['labelPosition'] ?? 'end') === 'end';
    $content = $options['label'] ?? percentLabel(fractionOf($value, $minimum, $maximum));
    $z = $options['zIndex'] ?? 0;
    $area = resolveArea($options['frame'] ?? null, $canvas);
    $rect = $area->toPt();
    $lineBox = 1.2 * 1.5;
    $across = $vertical ? fontPtForWidth($content, $rect['width']) : $rect['height'] / $lineBox;
    $along = $vertical ? $rect['height'] / ($lineBox * 3) : fontPtForWidth($content, $rect['width'] / 3);
    $points = fontPtOf(fontRatioOf(min(fontPtOf($options['fontSizeRatio'] ?? .07, $canvas), $across, $along),
        $canvas), $canvas);
    $slot = $vertical ? $points * $lineBox : estimateTextWidthPt($content, $points);
    $side = $vertical ? ($end ? 'bottom' : 'top') : ($end ? 'right' : 'left');
    $barBox = $area->insetPt([$side => $slot + $points * .4]);
    $labelRect = $vertical ? ['x' => $rect['x'], 'y' => $end ? $rect['y'] + $rect['height'] - $slot : $rect['y'],
        'width' => $rect['width'], 'height' => $slot]
        : ['x' => $end ? $rect['x'] + $rect['width'] - $slot : $rect['x'], 'y' => $rect['y'],
            'width' => $slot, 'height' => $rect['height']];
    $matrix = progressMatrix($value, array_replace($options, ['frame' => $barBox, 'canvas' => $canvas]));
    $placed = place($matrix->toPart(), ['id' => $id . ':bar', 'path' => $path, 'zIndex' => $z,
        'frame' => emittedRegion($barBox, $canvas)['frame']]);
    $label = labelLayer(['id' => $id . ':label', 'box' => Box::fromPt($labelRect, $canvas),
        'content' => $content, 'horizontal' => $vertical ? 'center' : ($end ? 'right' : 'left'),
        'vertical' => 'center', 'fontSizeRatio' => fontRatioOf($points, $canvas),
        'color' => objectFields($options['colors'] ?? [])['label'] ?? $theme['palette']['on'],
        'opacity' => 1, 'zIndex' => $z + 1]);
    return \GlanceWidgets\Internal\combine([$placed, $label]);
}

function ringMatrix(float $value, array $options): Matrix
{
    $minimum = $options['min'] ?? 0;
    $maximum = $options['max'] ?? 100;
    requireRange('pixelRingProgress', $minimum, $maximum);
    $diameter = positiveInteger($options['diameter'] ?? 33, 'diameter');
    $thickness = positiveInteger($options['thickness'] ?? 4, 'thickness');
    if ($diameter < 3 || $diameter % 2 === 0) {
        throw new \InvalidArgumentException('Ring diameter must be odd and at least three');
    }
    $theme = resolveTheme($options['theme'] ?? null);
    $looks = pixelLooks($theme, $options['colors'] ?? []);
    $radius = intdiv($diameter - 1, 2);
    $matrix = new Matrix($diameter, $diameter, themeOptions($theme, ['default' => EMPTY_PIXEL]));
    $sweep = fractionOf($value, $minimum, $maximum) * 360;
    $matrix->arc($radius, $radius, $radius, 0, 360, $looks['off'], ['thickness' => $thickness]);
    if ($sweep > 0) {
        $start = $options['startAngle'] ?? 0;
        [$first, $last] = ($options['counterClockwise'] ?? false) ? [$start - $sweep, $start]
            : [$start, $start + $sweep];
        $matrix->arc($radius, $radius, $radius, $first, $last, $looks['on'], ['thickness' => $thickness]);
    }
    return $matrix;
}

function pixelRingProgress(float $value, array|\stdClass $options = []): array
{
    return ringMatrix($value, objectFields($options))->toPart();
}

function labeledPixelRingProgress(float $value, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'labeledPixelRingProgress');
    [$id, $path] = resolveIdentity($options, 'ring', 'labeledPixelRingProgress');
    $theme = resolveTheme($options['theme'] ?? null);
    $z = $options['zIndex'] ?? 0;
    $content = $options['label'] ?? percentLabel(fractionOf($value, $options['min'] ?? 0, $options['max'] ?? 100));
    $area = resolveArea($options['frame'] ?? null, $canvas)->toPt();
    $side = min($area['width'], $area['height']);
    $box = Box::fromPt(['x' => $area['x'] + ($area['width'] - $side) / 2,
        'y' => $area['y'] + ($area['height'] - $side) / 2, 'width' => $side, 'height' => $side], $canvas);
    $region = emittedRegion($box, $canvas);
    $part = ringMatrix($value, $options)->toPart();
    $placed = place($part, ['id' => $id . ':ring', 'frame' => $region['frame'], 'path' => $path, 'zIndex' => $z]);
    $diameter = count($part['content']['pixels'][0]);
    $geometry = fitMatrix($region['rect'], ['columns' => $diameter, 'rows' => $diameter], $part['content']['layout']);
    $center = cellRect($geometry, ($diameter - 1) / 2, ($diameter - 1) / 2);
    $holeCells = max(1, ($diameter - 1) / 2 - ($options['thickness'] ?? 4));
    $radius = $holeCells * ($geometry['cellWidth'] + $geometry['gapX']);
    $width = 1.6 * $radius;
    $height = 1.2 * $radius;
    $requested = isset($options['fontSizeRatio']) ? fontPtOf($options['fontSizeRatio'], $canvas)
        : min($height / (1.2 * 1.5), fontPtForWidth($content, $width));
    $points = fontPtOf(fontRatioOf($requested, $canvas), $canvas);
    $boxWidth = max($width, estimateTextWidthPt($content, $points));
    $boxHeight = max($height, textBoxHeightPt($points));
    $labelBox = Box::fromPt(['x' => $center['x'] + $center['width'] / 2 - $boxWidth / 2,
        'y' => $center['y'] + $center['height'] / 2 - $boxHeight / 2,
        'width' => $boxWidth, 'height' => $boxHeight], $canvas);
    $label = labelLayer(['id' => $id . ':label', 'box' => $labelBox, 'content' => $content,
        'horizontal' => 'center', 'vertical' => 'center', 'fontSizeRatio' => fontRatioOf($points, $canvas),
        'color' => objectFields($options['colors'] ?? [])['label'] ?? $theme['palette']['on'],
        'opacity' => 1, 'zIndex' => $z + 1]);
    return \GlanceWidgets\Internal\combine([$placed, $label]);
}
